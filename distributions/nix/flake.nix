{
  description = "Nix flake for HackMatrix - A 3D Linux desktop environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
  };

  outputs = { self, nixpkgs }:
    let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
    in {
      packages.x86_64-linux.hackmatrix = pkgs.stdenv.mkDerivation {
        pname = "hackmatrix";
        version = "master";

        src = pkgs.fetchFromGitHub {
          owner = "collinalexbell";
          repo = "HackMatrix";
          rev = "master";
          sha256 = "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="; 
          # nix-prefetch-git https://github.com/collinalexbell/HackMatrix and relace the hash
        };

        nativeBuildInputs = [ pkgs.pkg-config pkgs.cmake ];
        buildInputs = [
          pkgs.zmq
          pkgs.libX11
          pkgs.libXcomposite
          pkgs.libXtst
          pkgs.libXext
          pkgs.libXfixes
          pkgs.protobuf
          pkgs.spdlog
          pkgs.fmt
          pkgs.glfw
          pkgs.opengl
          pkgs.pthread
          pkgs.assimp
          pkgs.sqlite
          pkgs.xdotool
          pkgs.dmenu
          pkgs.x11utils
        ];

        buildPhase = ''
          make
        '';

        installPhase = ''
          mkdir -p $out/bin
          cp matrix trampoline $out/bin/
        '';

        meta = with pkgs.lib; {
          description = "A 3D Linux desktop environment with game engine capabilities.";
          license = licenses.mit;
          maintainers = [ maintainers.anonymous ];
        };
      };
    };
}
