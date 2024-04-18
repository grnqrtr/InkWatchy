{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    pkgs.freetype
  ];
  shellHook = ''
    export C_INCLUDE_PATH="${pkgs.freetype.dev}/include"
    export CPLUS_INCLUDE_PATH="${pkgs.freetype.dev}/include"
  '';
}
