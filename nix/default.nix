{
  lib,
  stdenv,
  cmake,
  pkg-config,
  hyprutils,
  libffi,
  pugixml,
  version ? "git",
  doCheck ? false,
}:
stdenv.mkDerivation {
  pname = "hyprwire";
  inherit version doCheck;

  src = ../.;

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    hyprutils
    libffi
    pugixml
  ];

  meta = {
    homepage = "https://github.com/hyprwm/hyprwire";
    description = "A fast and consistent wire protocol for IPC";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
  };
}
