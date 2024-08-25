# pkg-config-wrap

This is a small application (for Windows) which can take `pkg-config` calls such as from CMake based build toolchain and print the requested arguments as well as returned result.

The application would typically sit first on the search path, and it will automatically find next one (such as for example [`pkgconfiglite`](https://stackoverflow.com/a/25605631/868014)), and the application will forward the calls to such second real `pkg-config`.

The application is also capable to convert extra `PKG_CONFIG_PATH` command line argument, such as set by [`PKG_CONFIG_ARGN`](https://cmake.org/cmake/help/latest/module/FindPkgConfig.html) on CMake side, and convert it into environment variable before forwarding the call. Although, this was no longer necessary for mt specific problem once I figured out the original cross-compilation issue.
