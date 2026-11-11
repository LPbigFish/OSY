{
  description = "OSY course dev shell (g++, make, bear, lldb, clangd)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      system2 = "aarch64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      pkgs2 = nixpkgs.legacyPackages.${system2};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          gcc
          clang
          gnumake
          bear
          lldb
          clang-tools # clangd
        ];
      };
      devShells.${system2}.default = pkgs2.mkShell {
        packages = with pkgs2; [
          gcc
          clang
          gnumake
          bear
          lldb
          clang-tools # clangd
        ];
      };
    };
}
