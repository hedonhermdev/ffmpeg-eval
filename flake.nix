{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    mars-std.url = "git+ssh://git@github.com/mars-research/mars-std";
  };

  outputs = { self, mars-std, nixpkgs }: let 
    supportedSystems = ["x86_64-linux" "aarch64-darwin"];
    in mars-std.lib.eachSystem supportedSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      nativeBuildInputs = with pkgs; [
        pkg-config
        clang-tools
        (ffmpeg.override { debugDeveloper = true; })
      ];
  in {
    devShell = pkgs.mkShell {
      inherit nativeBuildInputs;
    };
  });
}
