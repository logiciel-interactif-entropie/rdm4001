let
	nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-25.05";
	pkgs = import nixpkgs { config = {}; overlay = []; };
in

pkgs.mkShell {
	packages = with pkgs; [
		sdl3
		bullet
		libsndfile
		curl
		mpv
		icu
		luajit
		openal
		libtomcrypt
		ninja
		meson
		xz	
		stdenv
		pkg-config
		assimp
		enet
		editline
		mesa
		libGL
		shaderc
		glm
		readline
		xorg.libX11.dev
	];
}
