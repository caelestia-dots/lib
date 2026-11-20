{
  description = "Shared C++ library for Caelestia";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

    m3colours = {
      url = "github:soramanew/m3colours/f38b9185357d22bd1b63e7dd14c0c044c268c4cd";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    ...
  } @ inputs: let
    inherit (nixpkgs.lib) genAttrs platforms lists systems;

    pkgsOf = nixpkgs.legacyPackages;
    systems' = lists.intersectLists platforms.linux systems.flakeExposed;
    eachSystem = genAttrs systems';
  in {
    formatter = eachSystem (system: pkgsOf.${system}.alejandra);

    packages = eachSystem (system: rec {
      caelestia-lib = pkgsOf.${system}.callPackage ./default.nix {
        rev = self.rev or self.dirtyRev or "unknown";
        stdenv = pkgsOf.${system}.clangStdenv;
        m3colours = inputs.m3colours;
      };
      debug = caelestia-lib.override {debug = true;};
      default = caelestia-lib;
    });

    devShells = eachSystem (system: {
      default = let
        pkgs = pkgsOf.${system};
        library = self.packages.${system}.caelestia-lib;
        mkShell = pkgs.mkShell.override {stdenv = library.stdenv;};
      in
        mkShell {
          inputsFrom = [library];
          packages = with pkgs; [clazy ccache];
        };
    });
  };
}
