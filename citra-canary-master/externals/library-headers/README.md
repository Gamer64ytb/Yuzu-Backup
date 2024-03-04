# ext-library-headers

Set of headers used by Citra for dynamically linked libraries. Compared to including the original library repositories as submodules, this keeps the clone size low by providing only the headers.

## Base Libraries

* fdk-aac
  * Version: 2.0.2
  * Source: https://github.com/mstorsjo/fdk-aac/tree/v2.0.2
  * Packages: https://archlinux.org/packages/extra/x86_64/libfdk-aac
* FFmpeg
  * Version: 6.0
  * Source: https://github.com/FFmpeg/FFmpeg/tree/n6.0
  * Packages: https://archlinux.org/packages/extra/x86_64/ffmpeg

## Updating

The simplest way to get all of the final headers is to use the linked Arch packages.

1. Download the latest version of the package you want to update from the linked page.
2. Extract the package from the downloaded file.
3. Browse to the ```usr/include``` directory within the package and copy all of the headers over the current set in this repository.
