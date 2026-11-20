{
  rev,
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  qt6,
  ffmpeg,
  wayland,
  wayland-protocols,
  m3colours,
  debug ? false,
}: let
  version = "0.0.1";

  cmakeBuildType =
    if debug
    then "Debug"
    else "RelWithDebInfo";
in
  stdenv.mkDerivation {
    inherit version cmakeBuildType;
    pname = "caelestia-lib${lib.optionalString debug "-debug"}";
    src = ./.;

    nativeBuildInputs = [cmake ninja pkg-config];
    buildInputs = [
      qt6.qtbase
      qt6.qtdeclarative
      qt6.qtwayland
      ffmpeg
      wayland
      wayland-protocols
    ];

    cmakeFlags = [
      (lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_M3COLOURS" (toString m3colours))
      (lib.cmakeFeature "VERSION" version)
      (lib.cmakeFeature "GIT_REVISION" rev)
      (lib.cmakeFeature "DISTRIBUTOR" "nix-flake")
      (lib.cmakeFeature "INSTALL_QMLDIR" qt6.qtbase.qtQmlPrefix)
      (lib.cmakeFeature "INSTALL_SUDOERS" "OFF")
    ];

    dontStrip = debug;
    dontWrapQtApps = true;

    passthru = {
      inherit stdenv;
    };

    meta = {
      description = "Shared C++ library for Caelestia";
      homepage = "https://github.com/caelestia-dots/lib";
      license = lib.licenses.gpl3Only;
      platforms = lib.platforms.linux;
    };
  }
