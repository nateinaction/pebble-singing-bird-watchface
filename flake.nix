{
  description = "Texas Songbird — Pebble watchface dev environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      # Systems we support a dev shell for.
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
    in
    {
      devShells = forAllSystems (pkgs:
        let
          # Python used by the asset-generation tooling in tools/.
          pythonEnv = pkgs.python3.withPackages (ps: [ ps.pillow ]);
        in
        {
          # mkShellNoCC: do NOT pull in a host C compiler. The Pebble `waf`
          # build must use the SDK's bundled arm-none-eabi toolchain; a stdenv
          # gcc in PATH (and an exported CC=gcc) shadows it and fails on the
          # ARM-only `-mthumb`/`-mcpu` flags.
          default = pkgs.mkShellNoCC {
            packages = [
              pythonEnv        # tools/make_birds.py, tools/make_placeholder_audio.py (Pillow)
              pkgs.ffmpeg      # tools/fetch_audio.sh — call recordings -> mono 8kHz 8-bit PCM
              pkgs.uv          # installs/runs the coredevices `pebble` CLI (uv tool)
              pkgs.nodejs      # `pebble build` needs npm >= 3.0.0 for pkjs/clay JS
              pkgs.curl        # xeno-canto downloads in tools/fetch_audio.sh
              pkgs.jq          # parsing xeno-canto API responses
            ];

            shellHook = ''
              # Belt-and-suspenders: ensure no host compiler is advertised to
              # waf, so it falls back to the SDK's arm-none-eabi cross-compiler.
              unset CC CXX

              # The Pebble SDK CLI (coredevices/repebble fork) is distributed as a
              # `uv` tool rather than a nixpkgs package. Install it once, locally,
              # without touching the base OS. It self-manages the SDK download.
              if ! command -v pebble >/dev/null 2>&1; then
                echo "pebble CLI not found. Install it with:"
                echo "    uv tool install pebble-tool"
                echo "(uv is provided by this dev shell)."
              fi
            '';
          };
        });
    };
}
