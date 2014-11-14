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
                    
                    /// <summary>
                    /// Loads the image from the specified image stream.
                    /// </summary>
                    Windows::Foundation::IAsyncAction^ SetSourceAsync(Windows::Storage::Streams::IRandomAccessStream^ pStream);

                    /// <summary>
                    /// Clears all resources currently held in-memory.
                    /// </summary>
                    void ClearResources();

                    /// <summary>
                    /// Gets the width of the image.
                    /// </summary>
                    property int Width
                    {
                        int get() { return m_width; }
                    }

                    /// <summary>
                    /// Gets the height of the image.
                    /// </summary>
                    property int Height
                    {
                        int get() { return m_height; }
                    }

                    /// <summary>
                    /// Sets whether to pre-compose raw frames into displayable frames.
                    /// </summary>
                    /// <remarks>
                    /// Generally speaking, set this to true.
                    /// </remarks>
                    property bool EnablePrerender
                    {
                        bool get() { return m_prerender; }
                        void set(bool value) { m_prerender = value; }
                    }

                    /// <summary>
                    /// Starts the animation, if image is animated.
                    /// </summary>
                    void Start();
                    
                    /// <summary>
                    /// Stops the animation.
                    /// </summary>
                    void Stop();
                    
                    /// <summary>
                    /// Resets the animation to the first frame.
                    /// </summary>
                    void Restart();
                    
                    /// <summary>
                    /// Renders a single frame and increments the current frame index.
                    /// </summary>
                    /// <returns>
                    /// True if an animation loop was just completed, false otherwise.
                    /// </returns>
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
                    Microsoft::WRL::ComPtr<ID2D1Bitmap1>                m_surfaceBitmap;

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