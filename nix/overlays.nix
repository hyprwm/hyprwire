{
  lib,
  inputs,
  self,
}: let
  mkDate = longDate: (lib.concatStringsSep "-" [
    (builtins.substring 0 4 longDate)
    (builtins.substring 4 2 longDate)
    (builtins.substring 6 2 longDate)
  ]);

  version = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in {
  default = inputs.self.overlays.hyprwire;

  hyprwire = lib.composeManyExtensions [
    inputs.hyprutils.overlays.default
    (final: prev: {
      hyprwire = prev.callPackage ./default.nix {
        stdenv = prev.gcc15Stdenv;
        version =
          version
          + "+date="
          + (mkDate (inputs.self.lastModifiedDate or "19700101"))
          + "_"
          + (inputs.self.shortRev or "dirty");
      };
      hyprwire-with-tests = final.hyprwire.override {doCheck = true;};
    })
  ];
}
