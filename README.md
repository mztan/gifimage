gifimage
========

Loads, decodes, renders, and animates GIFs for your viewing pleasure! Built using Direct2D and the Windows Runtime. Supports Windows 8+ and Windows Phone 8.1+.

#### Usage
* Add reference to Em.UI.Xaml.Media.GifImageSource in your app
* Copy GifImage and ImageOpenedEventArgs into your app
* Set GifImage.UriSource and watch the magic happen!
* Check out GifImageSample app for a fully functional demo

#### Known Issues
* GIFs are decoded and stored into memory in their entirety. Unusually large GIFs may cause OOM issues on low-memory Windows Phones.
* DirectX usage may be strange or buggy. Forgive me, this is my first time working with DirectX.

#### To Do
* Implement rolling window buffering to better handle unusually large GIFs.

Comments and pull requests are more than welcome.
