#pragma once

#include <ppltasks.h>
#include <wincodec.h>
#include "windows.foundation.h"

namespace Em {
    namespace UI {
        namespace Xaml {
            namespace Media
            {
                public ref class GifImageSource sealed : Windows::UI::Xaml::Media::Imaging::SurfaceImageSource
                {
                public:
                    GifImageSource(int width, int height);
                    Windows::Foundation::IAsyncAction^ SetSourceAsync(Windows::Storage::Streams::IRandomAccessStream^ pStream);

                    void ClearResources();

                    property int Width
                    {
                        int get() { return m_width; }
                    }

                    property int Height
                    {
                        int get() { return m_height; }
                    }

                    property bool EnablePrerender
                    {
                        bool get() { return m_prerender; }
                        void set(bool value) { m_prerender = value; }
                    }

                    void Start();
                    void Stop();
                    void Restart();
                    bool RenderFrame();

                private:
                    void CreateDeviceResources();
                    bool BeginDraw();
                    void EndDraw();

                    void SetNextInterval();
                    void CheckTimer();

                    void LoadImage(IStream *pStream);
                    HRESULT QueryMetadata(IWICMetadataQueryReader *pQueryReader);
                    void PrerenderBitmaps();

                    Microsoft::WRL::ComPtr<ID3D11Device>                m_d3dDevice;
                    Microsoft::WRL::ComPtr<ID2D1Device>                 m_d2dDevice;
                    Microsoft::WRL::ComPtr<ID2D1DeviceContext>          m_d2dContext;
                    Microsoft::WRL::ComPtr<ISurfaceImageSourceNative>   m_sisNative;
                    Microsoft::WRL::ComPtr<ID2D1Bitmap1>				m_surfaceBitmap;

                    UINT m_width;
                    UINT m_height;
                    UINT m_dwFrameCount;
                    UINT m_loopCount;
                    bool m_isAnimatedGif;

                    bool m_prerender;
                    bool m_completedPrerender;

                    std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_bitmaps;
                    std::vector<D2D1_POINT_2F> m_offsets;
                    std::vector<USHORT> m_delays;

                    UINT m_dwCurrentFrame;
                    bool m_completedLoop;

                    Windows::UI::Xaml::DispatcherTimer^ m_timer;
                    void OnTick(Platform::Object ^sender, Platform::Object ^args);
                };
            }
        }
    }
}