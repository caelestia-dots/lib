# caelestia-lib

A shared C++ library for [Caelestia](https://github.com/caelestia-dots).

## Modules

| Module      | Contains                                                  |
| ----------- | --------------------------------------------------------- |
| `config`    | Basic access to `~/.config/caelestia/config.json` + paths |
| `ipc`       | CLI to shell requests over a local socket                 |
| `media`     | Image/video colour analysis                               |
| `record`    | Screen recorder lifecycle and state                       |
| `scheme`    | Material 3 scheme generation                              |
| `theme`     | Apply schemes to apps                                     |
| `utils`     | File and blob caches, component registry                  |
| `wallpaper` | Current wallpaper state and wallpaper-driven schemes      |

## Dependencies

- [Qt 6](https://github.com/qt/qtbase)
- [FFmpeg](https://github.com/ffmpeg/ffmpeg)

## Installation

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

> [!IMPORTANT]
> The `theme` module has 2 targets which require access to `sudo`. A sudoers file
> is [installed by default](src/theme/helper/CMakeLists.txt) and allows passwordless
> access to the helpers. This can be disabled with `-DINSTALL_SUDOERS=OFF`, or the
> location of the sudoers directory can be changed with `-DSUDOERS_DIR=...`.

## Testing

```sh
cmake -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build -j4
```

## Usage

This library is designed to be consumed via `CMake`'s `find_package` like so:

```cmake
find_package(caelestia REQUIRED)
caelestia_setup()
target_link_libraries(myapp PRIVATE caelestia::caelestia)
```

An optional `caelestia_setup_warnings()` macro is available for enabling strict
warnings (Qt cast disables + strict build warnings).
