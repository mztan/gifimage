using Em.UI.Xaml.Media;
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Graphics.Imaging;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.Web.Http;

namespace Em.UI.Xaml.Controls
{
    public class GifImage : Control
    {
        public static readonly DependencyProperty UriSourceProperty =
            DependencyProperty.Register("UriSource", typeof(Uri), typeof(GifImage),
                new PropertyMetadata(null, OnUriSourceChanged));

        public static readonly DependencyProperty CanLoadProperty =
            DependencyProperty.Register("CanLoad", typeof(bool), typeof(GifImage),
                new PropertyMetadata(true, OnCanLoadChanged));

        public static readonly DependencyProperty IsAnimatingProperty =
            DependencyProperty.Register("IsAnimating", typeof(bool), typeof(GifImage),
                new PropertyMetadata(true, OnIsAnimatingChanged));

        public event TypedEventHandler<GifImage, ImageOpenedEventArgs> ImageOpened;

        private ImageBrush _brush;
        private GifImageSource _source;
        private Border _surface;
        private TextBlock _progressText;
        private HyperlinkButton _reloadButton;
        private int _progress;

        public GifImage()
        {
            DefaultStyleKey = typeof(GifImage);
            Loaded += GifImage_Loaded;
            Unloaded += GifImage_Unloaded;

            IsWindowActivated = true;
            SizeChanged += GifImage_SizeChanged;
        }

        protected bool HasImage { get; private set; }
        protected bool HasAppliedTemplate { get; private set; }
        protected bool IsLoaded { get; private set; }
        protected bool IsWindowActivated { get; private set; }

        public Uri UriSource
        {
            get { return (Uri)GetValue(UriSourceProperty); }
            set { SetValue(UriSourceProperty, value); }
        }

        public bool CanLoad
        {
            get { return (bool)GetValue(CanLoadProperty); }
            set { SetValue(CanLoadProperty, value); }
        }

        public bool IsAnimating
        {
            get { return (bool)GetValue(IsAnimatingProperty); }
            set { SetValue(IsAnimatingProperty, value); }
        }

        private static void OnCanLoadChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var gifImage = d as GifImage;
            if (gifImage == null) return;
            gifImage.ResetImage();
            gifImage.ShowImage();
        }

        private static void OnUriSourceChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var gifImage = d as GifImage;
            if (gifImage == null) return;
            gifImage.ResetImage(); // Re-show the image
            gifImage.ShowImage();
        }

        private static void OnIsAnimatingChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var gifImage = d as GifImage;
            if (gifImage == null) return;
            gifImage.CheckTimer();
        }

        private void CheckTimer()
        {
            if (_source == null)
            {
                return;
            }

            if (IsAnimating && IsLoaded && IsWindowActivated)
            {
                _source.Start();
            }
            else
            {
                _source.Stop();
            }
        }

        private void GifImage_Loaded(object sender, RoutedEventArgs e)
        {
            IsLoaded = true;
            Window.Current.Activated += Window_Activated;

            CheckTimer();
            ShowImage();
        }

        private void GifImage_Unloaded(object sender, RoutedEventArgs e)
        {
            IsLoaded = false;
            Window.Current.Activated -= Window_Activated;

            CheckTimer();
            ResetImage();
        }

        protected override void OnApplyTemplate()
        {
            HasAppliedTemplate = true;

            _surface = (Border)GetTemplateChild("Surface");
            _progressText = (TextBlock)GetTemplateChild("ProgressText");
            _reloadButton = (HyperlinkButton)GetTemplateChild("ReloadButton");
            _reloadButton.Tapped += _reloadButton_Tapped;

            ShowImage();
        }

        private async void ShowImage()
        {
            if (!CanLoad || !HasAppliedTemplate || !IsLoaded || HasImage)
                return;

            ResetImage();
            if (UriSource == null)
                return;

            SetProgress(0);
            VisualStateManager.GoToState(this, "Unloaded", true);

            // For thread safety, save current value of UriSource
            var uriSource = UriSource;

            HasImage = true;
            using (var stream = await GetImageStreamAsync(uriSource)) // This is not thread-safe!
            {
                // Technically, at each await exists the possibility that UriSource can be changed.
                // We need to ensure that this hasn't happened, otherwise we run the risk of leaking resources
                // or doing other bad things.
                if (uriSource != UriSource)
                    return;

                if (stream == null)
                {
                    VisualStateManager.GoToState(this, "Failed", true);
                    return;
                }

                try
                {
                    var decoder = await BitmapDecoder.CreateAsync(stream);
                    var source = new GifImageSource((int)decoder.PixelWidth, (int)decoder.PixelHeight)
                    {
                        EnablePrerender = true
                    };
                    await source.SetSourceAsync(stream);

                    // It's possible that the URI changed, or the control has been unloaded, etc. If so,
                    // clear resources and quit.
                    if (uriSource != UriSource || !IsLoaded || !CanLoad)
                    {
                        // Clean up GifImageSource and return
                        source.ClearResources();
                        return;
                    }

                    // Must wait for async operations to complete before setting _source
                    _source = source;
                }
                catch (Exception)
                {
                    VisualStateManager.GoToState(this, "Failed", true);
                    return;
                }
            }

            _brush = new ImageBrush { ImageSource = _source, Stretch = Stretch.None };
            _surface.Background = _brush;

            _surface.Height = _source.Height;
            _surface.Width = _source.Width;

            // Render one frame so that we show something even if the gif is paused
            _source.RenderFrame();

            VisualStateManager.GoToState(this, "Loaded", true);

            CheckTimer();

            _fireImageOpened = true;
        }

        // The ImageOpened event must be fired after the control's size has been calculated. This way,
        // we guarantee that the control's ActualHeight/ActualWidth match the image's height and width.
        private bool _fireImageOpened;
        void GifImage_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (_fireImageOpened)
            {
                _fireImageOpened = false;
                if (ImageOpened != null)
                    ImageOpened(this, new ImageOpenedEventArgs(e.NewSize.Height, e.NewSize.Width));
            }
        }

        private void ResetImage()
        {
            if (!HasImage)
                return;

            _surface.Background = null;

            if (_brush != null)
            {
                _brush.ImageSource = null;
                _brush = null;
            }

            if (_source != null)
            {
                _source.ClearResources();
                _source = null;
            }

            HasImage = false;
        }

        private void SetProgress(int progress)
        {
            _progress = progress;
            _progressText.Text = _progress > 0 ? _progress.ToString() : "...";
        }

        private async Task<IRandomAccessStream> GetImageStreamAsync(Uri uriSource)
        {
            switch (uriSource.Scheme)
            {
                case "ms-appx":
                    var file =
                        await StorageFile.GetFileFromApplicationUriAsync(uriSource).AsTask().ConfigureAwait(false);
                    return await file.OpenReadAsync().AsTask().ConfigureAwait(false);

                case "http":
                case "https":
                    var client = new HttpClient();
                    IAsyncOperationWithProgress<IBuffer, HttpProgress> getOperation = null;
                    try
                    {
                        getOperation = client.GetBufferAsync(uriSource);
                        getOperation.Progress = OnProgress;

                        var buffer = await getOperation.AsTask().ConfigureAwait(false);
                        return buffer.AsStream().AsRandomAccessStream();
                    }
                    catch (Exception ex)
                    {
                        Debug.WriteLine("Failed to get GIF image");
                        Debug.WriteLine(ex);
                        return null;
                    }
                    finally
                    {
                        client.Dispose();
                        if (getOperation != null)
                        {
                            getOperation.Progress = null;
                        }
                    }

                default:
                    throw new InvalidOperationException("unsupported uri scheme");
            }
        }

        private async void OnProgress(IAsyncOperationWithProgress<IBuffer, HttpProgress> info, HttpProgress progressInfo)
        {
            if (progressInfo.Stage != HttpProgressStage.ReceivingContent || !progressInfo.TotalBytesToReceive.HasValue ||
                progressInfo.TotalBytesToReceive == 0)
                return;

            // Fire out an event only if we have made progress in downloading the image
            var progress = (int)(progressInfo.BytesReceived * 100 / progressInfo.TotalBytesToReceive);
            if (_progress == progress)
                return;

            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
                SetProgress(progress));
        }

        private void _reloadButton_Tapped(object sender, Windows.UI.Xaml.Input.TappedRoutedEventArgs e)
        {
            // Don't allow this Tapped event to bubble up
            e.Handled = true;

            // Force reload
            ResetImage();
            ShowImage();

            VisualStateManager.GoToState(this, "Unloaded", true);
        }

        private void Window_Activated(object sender, WindowActivatedEventArgs e)
        {
            IsWindowActivated = e.WindowActivationState == CoreWindowActivationState.CodeActivated ||
                                e.WindowActivationState == CoreWindowActivationState.PointerActivated;
            CheckTimer();
        }
    }
}