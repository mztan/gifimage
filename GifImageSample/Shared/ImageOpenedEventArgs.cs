using System;

namespace Em.UI.Xaml
{
    public class ImageOpenedEventArgs : EventArgs
    {
        public double PixelHeight { get; set; }
        public double PixelWidth { get; set; }

        public ImageOpenedEventArgs(double pixelHeight, double pixelWidth)
        {
            PixelHeight = pixelHeight;
            PixelWidth = pixelWidth;
        }
    }
}
