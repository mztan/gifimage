#include "pch.h"

#include <ppltasks.h>
#include <wincodec.h>
#include <shcore.h>

#include "windows.graphics.imaging.h"
#include "windows.ui.xaml.media.imaging.h"

#include "GifImageSource.h"

using namespace Em::UI::Xaml::Media;

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::Foundation;

GifImageSource::GifImageSource(int width, int height)
    : SurfaceImageSource(width, height),
    m_width(width),
    m_height(height),
    m_dwFrameCount(0),
    m_bitmaps(NULL),
    m_offsets(NULL),
    m_delays(NULL),
    m_dwCurrentFrame(0),
    m_completedPrerender(false),
    m_completedLoop(false),
    m_loopCount(0),
    m_isAnimatedGif(false),
    m_prerender(false)
{
    if (width < 0 || height < 0)
        throw ref new Platform::InvalidArgumentException();

    CreateDeviceResources();

    m_timer = ref new DispatcherTimer();
    m_timer->Tick += ref new EventHandler<Object^>(this, &GifImageSource::OnTick);
}

void GifImageSource::ClearResources()
{
    if (m_timer != nullptr)
    {
        m_timer->Stop();
        m_timer = nullptr;
    }

    while (!m_bitmaps.empty())
    {
        m_bitmaps.back() = nullptr;
        m_bitmaps.pop_back();
    }

    m_offsets.clear();
    m_delays.clear();

    m_width = 0;
    m_height = 0;
    m_loopCount = 0;
    m_isAnimatedGif = false;
    m_completedLoop = false;
    m_completedPrerender = false;
    m_prerender = false;
    m_dwFrameCount = 0;
    m_dwCurrentFrame = 0;

    m_surfaceBitmap = nullptr;
    m_sisNative = nullptr;
    m_d2dContext = nullptr;
    m_d2dDevice = nullptr;
    m_d3dDevice = nullptr;
}

Windows::Foundation::IAsyncAction^ GifImageSource::SetSourceAsync(IRandomAccessStream^ pStream)
{
    if (pStream == nullptr)
        throw ref new Platform::InvalidArgumentException();

    return create_async([this, pStream]() -> void
    {
        m_completedPrerender = false;
        m_dwCurrentFrame = 0;
        m_completedLoop = false;

        ComPtr<IStream> pIStream;
        DX::ThrowIfFailed(
            CreateStreamOverRandomAccessStream(
            reinterpret_cast<IUnknown*>(pStream),
            IID_PPV_ARGS(&pIStream)));

        LoadImage(pIStream.Get());
    });
}

bool GifImageSource::RenderFrame()
{
    if (m_prerender && !m_completedPrerender)
    {
        PrerenderBitmaps();
        m_completedPrerender = true;
    }

    auto bCanDraw = BeginDraw();

    if (bCanDraw)
    {
        m_d2dContext->Clear();

        if (m_prerender)
        {
            m_d2dContext->DrawImage(m_bitmaps.at(m_dwCurrentFrame).Get());
        }
        else
        {
            for (UINT i = 0; i <= m_dwCurrentFrame; i++)
            {
                m_d2dContext->DrawImage(m_bitmaps.at(i).Get(), m_offsets.at(i));
            }
        }

        SetNextInterval();
        m_dwCurrentFrame = (m_dwCurrentFrame + 1) % m_dwFrameCount;

        EndDraw();
    }
    else
    {
    }

    // Returns true if we just completed a loop
    return m_dwCurrentFrame == 0;
}

// Convert raw frames into final displayable frames
void GifImageSource::PrerenderBitmaps()
{
    ComPtr<IWICImagingFactory> pFactory;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory,
        (LPVOID*) &pFactory);

    D2D1_POINT_2U dest = { 0, 0 };
    D2D1_RECT_U src = { 1, 2, m_width + 1, m_height + 2 }; // not sure why this (1,2) offset is needed... 

    BeginDraw();

    m_d2dContext->Clear();

    for (UINT dwIndex = 0; dwIndex < m_dwFrameCount; dwIndex++)
    {
        m_d2dContext->DrawImage(m_bitmaps.at(dwIndex).Get(), m_offsets.at(dwIndex));
        m_d2dContext->Flush();

        ComPtr<IWICBitmap> pWicBitmap;
        hr = pFactory->CreateBitmap(m_width, m_height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &pWicBitmap);

        ComPtr<ID2D1Bitmap> pBitmap;
        hr = m_d2dContext->CreateBitmapFromWicBitmap(pWicBitmap.Get(), &pBitmap);

        hr = pBitmap->CopyFromBitmap(&dest, m_surfaceBitmap.Get(), &src);

        m_bitmaps.at(dwIndex) = pBitmap;
    }

    EndDraw();
}

HRESULT GifImageSource::QueryMetadata(IWICMetadataQueryReader *pQueryReader)
{
    HRESULT hr = S_OK;
    PROPVARIANT var;
    PropVariantInit(&var);

    // Default to non-animated gif
    m_isAnimatedGif = false;

    hr = pQueryReader->GetMetadataByName(L"/appext/Application", &var);
    if (FAILED(hr))
        return hr;

    if (var.vt != (VT_UI1 | VT_VECTOR))
        return S_FALSE;
    if (var.caub.cElems != 0x0B) // must be exactly 11 bytes long
        return S_FALSE;

    static LPCSTR strNetscape = "NETSCAPE2.0";
    for (ULONG i = 0; i < var.caub.cElems; i++)
    {
        // Manual strcmp
        if (*(var.caub.pElems + i) != strNetscape[i])
            return S_FALSE;
    }

    PropVariantClear(&var);
    hr = pQueryReader->GetMetadataByName(L"/appext/Data", &var);
    if (FAILED(hr))
        return hr;

    // Only handle case where the application data block is exactly 4 bytes long
    if (var.caub.cElems != 4)
        return S_FALSE;

    auto count = *var.caub.pElems; // Data length; we generally expect this value to be 3
    if (count >= 1)
    {
        m_isAnimatedGif = *(var.caub.pElems + 1) != 0; // is animated gif; we generally expect this byte to be 1
    }
    if (count == 3)
    {
        // iteration count; 2nd element is LSB, 3rd element is MSB
        // we generally expect this value to be 0
        m_loopCount = (*(var.caub.pElems + 3) << 8) + *(var.caub.pElems + 2);
    }

    return hr;
}

void GifImageSource::LoadImage(IStream *pStream)
{
    HRESULT hr = S_OK;
    PROPVARIANT var;
    PropVariantInit(&var);

    ComPtr<IWICImagingFactory> pFactory;
    hr = CoCreateInstance(CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory,
        (LPVOID*) &pFactory);

    ComPtr<IWICBitmapDecoder> pDecoder;
    hr = pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);

    ComPtr<IWICMetadataQueryReader> pQueryReader;
    hr = pDecoder->GetMetadataQueryReader(&pQueryReader);

    // Get image metadata
    hr = QueryMetadata(pQueryReader.Get());

    UINT dwFrameCount = 0;
    hr = pDecoder->GetFrameCount(&dwFrameCount);

    m_dwFrameCount = dwFrameCount;
    m_bitmaps = std::vector<ComPtr<ID2D1Bitmap>>(m_dwFrameCount);
    m_offsets = std::vector<D2D1_POINT_2F>(m_dwFrameCount);
    m_delays = std::vector<USHORT>(m_dwFrameCount);

    for (UINT dwFrameIndex = 0; dwFrameIndex < dwFrameCount; dwFrameIndex++)
    {
        ComPtr<IWICBitmapFrameDecode> pFrameDecode;
        hr = pDecoder->GetFrame(dwFrameIndex, &pFrameDecode);

        ComPtr<IWICMetadataQueryReader> pFrameQueryReader;
        hr = pFrameDecode->GetMetadataQueryReader(&pFrameQueryReader);

        FLOAT fOffsetX = 0.0;
        FLOAT fOffsetY = 0.0;
        USHORT dwDelay = 10; // default to 10 hundreths of a second (100 ms)

        PropVariantClear(&var);
        hr = pFrameQueryReader->GetMetadataByName(L"/grctlext/Delay", &var);
        dwDelay = var.uiVal;

        if (dwFrameIndex == 0)
        {
            hr = pFrameDecode->GetSize((UINT*) &m_width, (UINT*) &m_height);
        }
        else
        {
            PropVariantClear(&var);
            hr = pFrameQueryReader->GetMetadataByName(L"/imgdesc/Left", &var);
            fOffsetX = var.uiVal;

            PropVariantClear(&var);
            hr = pFrameQueryReader->GetMetadataByName(L"/imgdesc/Top", &var);
            fOffsetY = var.uiVal;
        }

        ComPtr<IWICFormatConverter> pConvertedBitmap;
        hr = pFactory->CreateFormatConverter(&pConvertedBitmap);

        hr = pConvertedBitmap->Initialize(pFrameDecode.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);

        ComPtr<IWICBitmap> pWicBitmap;
        hr = pFactory->CreateBitmapFromSource(pConvertedBitmap.Get(), WICBitmapCacheOnDemand, &pWicBitmap);

        ComPtr<ID2D1Bitmap> pBitmap;
        hr = m_d2dContext->CreateBitmapFromWicBitmap(pWicBitmap.Get(), &pBitmap);

        // Push raw frames into bitmaps array. These need to be processed into proper frames before being drawn to screen.
        m_bitmaps[dwFrameIndex] = pBitmap;
        m_offsets[dwFrameIndex] = { fOffsetX, fOffsetY };
        m_delays[dwFrameIndex] = dwDelay;
    }

    PropVariantClear(&var);
}

void GifImageSource::CreateDeviceResources()
{
    // This flag adds support for surfaces with a different color channel ordering 
    // than the API default. It is required for compatibility with Direct2D. 
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)     
    // If the project is in a debug build, enable debugging via SDK Layers. 
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif 

    // This array defines the set of DirectX hardware feature levels this app will support. 
    // Note the ordering should be preserved. 
    // Don't forget to declare your application's minimum required feature level in its 
    // description.  All applications are assumed to support 9.1 unless otherwise stated. 
    const D3D_FEATURE_LEVEL featureLevels [] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    // Create the Direct3D 11 API device object. 
    DX::ThrowIfFailed(
        D3D11CreateDevice(
        nullptr,                        // Specify nullptr to use the default adapter. 
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,                  // Set debug and Direct2D compatibility flags. 
        featureLevels,                  // List of feature levels this app can support. 
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,              // Always set this to D3D11_SDK_VERSION for Windows Store apps. 
        &m_d3dDevice,                   // Returns the Direct3D device created. 
        nullptr,
        nullptr
        )
        );

    // Get the Direct3D 11.1 API device. 
    ComPtr<IDXGIDevice> dxgiDevice;
    DX::ThrowIfFailed(
        m_d3dDevice.As(&dxgiDevice));

    // Create the Direct2D device object and a corresponding context. 
    DX::ThrowIfFailed(
        D2D1CreateDevice(
        dxgiDevice.Get(),
        nullptr,
        &m_d2dDevice));

    DX::ThrowIfFailed(
        m_d2dDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &m_d2dContext));

    // Query for ISurfaceImageSourceNative interface. 
    DX::ThrowIfFailed(
        reinterpret_cast<IUnknown*>(this)->QueryInterface(IID_PPV_ARGS(&m_sisNative)));

    // Associate the DXGI device with the SurfaceImageSource. 
    DX::ThrowIfFailed(
        m_sisNative->SetDevice(dxgiDevice.Get()));
}

bool GifImageSource::BeginDraw()
{
    ComPtr<IDXGISurface> surface;
    RECT updateRect = { 0, 0, m_width, m_height };
    POINT offset = { 0 };

    // Begin drawing - returns a target surface and an offset to use as the top left origin when drawing. 
    HRESULT beginDrawHR = m_sisNative->BeginDraw(updateRect, &surface, &offset);

    if (SUCCEEDED(beginDrawHR))
    {
        // Create render target. 
        DX::ThrowIfFailed(
            m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), nullptr, &m_surfaceBitmap));

        // Set context's render target. 
        m_d2dContext->SetTarget(m_surfaceBitmap.Get());

        // Begin drawing using D2D context. 
        m_d2dContext->BeginDraw();

        // Apply a clip and transform to constrain updates to the target update area. 
        // This is required to ensure coordinates within the target surface remain 
        // consistent by taking into account the offset returned by BeginDraw, and 
        // can also improve performance by optimizing the area that is drawn by D2D. 
        // Apps should always account for the offset output parameter returned by  
        // BeginDraw, since it may not match the passed updateRect input parameter's location. 
        m_d2dContext->PushAxisAlignedClip(
            D2D1::RectF(
            static_cast<float>(offset.x),
            static_cast<float>(offset.y),
            static_cast<float>(offset.x + updateRect.right),
            static_cast<float>(offset.y + updateRect.bottom)
            ),
            D2D1_ANTIALIAS_MODE_ALIASED);

        m_d2dContext->SetTransform(
            D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x), static_cast<float>(offset.y)));
    }
    else if (beginDrawHR == DXGI_ERROR_DEVICE_REMOVED || beginDrawHR == DXGI_ERROR_DEVICE_RESET)
    {
        // If the device has been removed or reset, attempt to recreate it and continue drawing. 
        CreateDeviceResources();
        BeginDraw();
    }
    else
    {
        return false;
    }

    return true;
}

void GifImageSource::EndDraw()
{
    // Remove the transform and clip applied in BeginDraw since 
    // the target area can change on every update. 
    m_d2dContext->SetTransform(D2D1::IdentityMatrix());
    m_d2dContext->PopAxisAlignedClip();

    // Remove the render target and end drawing. 
    DX::ThrowIfFailed(
        m_d2dContext->EndDraw());

    m_d2dContext->SetTarget(nullptr);
    m_surfaceBitmap = nullptr;

    DX::ThrowIfFailed(
        m_sisNative->EndDraw());
}


void GifImageSource::Restart()
{
    m_dwCurrentFrame = 0;
    m_completedLoop = false;
}

void GifImageSource::SetNextInterval()
{
    auto delay = m_delays.at(m_dwCurrentFrame);
    if (delay < 3)
    {
        delay = 10; // default to 100ms if delay is too short
    }

    auto timespan = TimeSpan();
    timespan.Duration = 80000L * delay; // 8ms * delay. (should be 10ms, but this gives the impression of awesome perf)

    m_timer->Interval = timespan;
}

void GifImageSource::Start()
{
    if (m_timer->IsEnabled)
        return;

    if (m_isAnimatedGif || !m_completedLoop)
    {
        m_timer->Start();
    }
}

void GifImageSource::Stop()
{
    m_timer->Stop();
}

void GifImageSource::OnTick(Object ^sender, Object ^args)
{
    // Timer might've been stopped just prior to OnTick
    if (m_timer == nullptr || !m_timer->IsEnabled)
    {
        return;
    }

    try
    {
        auto completedLoop = RenderFrame();
        if (completedLoop)
        {
            // Allow gif to play through at least once, no matter what
            m_completedLoop = true;

            // Stop animation if this is not a looping gif
            if (!m_isAnimatedGif)
            {
                m_timer->Stop();
            }
        }
    }
    catch (Platform::Exception^)
    {
    }
}
