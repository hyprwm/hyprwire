{
  lib,
  stdenv,
  cmake,
  pkg-config,
  hyprutils,
  libffi,
  version ? "git",
}:
stdenv.mkDerivation {
  pname = "hyprwire";
  inherit version;

  src = ../.;

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    hyprutils
    libffi
  ];

  meta = {
    homepage = "https://github.com/hyprwm/hyprwire";
    description = "A fast and consistent wire protocol for IPC";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
  };
}
