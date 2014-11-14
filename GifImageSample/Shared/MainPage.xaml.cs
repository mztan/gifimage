using System;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

namespace GifImageSample
{
    public sealed partial class MainPage : Page
    {
        public MainPage()
        {
            InitializeComponent();
        }

        private void Load_Click(object sender, RoutedEventArgs e)
        {
            _gifImage.UriSource = new Uri("ms-appx:///kitty.gif");
        }

        private void Unload_Click(object sender, RoutedEventArgs e)
        {
            _gifImage.UriSource = null;
        }

        private void Play_Click(object sender, RoutedEventArgs e)
        {
            _gifImage.IsAnimating = true;
        }

        private void Pause_Click(object sender, RoutedEventArgs e)
        {
            _gifImage.IsAnimating = false;
        }
    }
}
