set shell := ["bash", "-uo", "pipefail", "-c"]

default:
	@just --list

# Build host binary + core lib
build-core:
	cargo build -p trimmeh-core --release

# Build CLI
build-cli:
	cargo build -p trimmeh-cli --release

# Build wasm artifacts (requires wasm32 target and wasm-bindgen-cli on PATH)
build-wasm:
	cargo build -p trimmeh-core --release --target wasm32-unknown-unknown --features wasm
	wasm-bindgen --target web --no-typescript --out-dir shell-extension/wasm target/wasm32-unknown-unknown/release/trimmeh_core.wasm

# Bundle the shell extension JS (requires esbuild)
bundle-extension:
	npx esbuild shell-extension/src/extension.ts --bundle --format=esm --platform=browser --external:gi://* --external:resource://* --outfile=shell-extension/extension.js
	npx esbuild shell-extension/src/clipboard.ts --bundle --format=esm --platform=browser --external:gi://* --external:resource://* --outfile=shell-extension/clipboard.js
	npx esbuild shell-extension/src/trimmer.ts --bundle --format=esm --platform=browser --external:gi://* --external:resource://* --outfile=shell-extension/trimmer.js
	npx esbuild shell-extension/src/prefs.ts --bundle --format=esm --platform=browser --external:gi://* --external:resource://* --outfile=shell-extension/prefs.js
	cp shell-extension/metadata.json shell-extension/stylesheet.css shell-extension/ 2>/dev/null || true
	glib-compile-schemas shell-extension/schemas

# Bundle the GI-light clipboard watcher core for standalone gjs tests.
bundle-tests:
	mkdir -p tests/dist
	npx esbuild shell-extension/src/clipboardWatcher.ts --bundle --format=esm --platform=browser --external:gi://* --external:resource://* --outfile=tests/dist/clipboardWatcher.js
	npx esbuild trimmeh-core-js/src/index.ts --bundle --format=esm --platform=browser --outfile=tests/dist/trimCore.js

# Install extension locally (Wayland session)
install-extension: bundle-extension
	mkdir -p "${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/trimmeh@trimmeh.dev"
	cp -r shell-extension/* "${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/trimmeh@trimmeh.dev/"
	gnome-extensions disable trimmeh@trimmeh.dev 2>/dev/null || true
	gnome-extensions enable trimmeh@trimmeh.dev 2>/dev/null || true
	@echo "Trimmeh installed. If it doesn't show up yet, log out/in or enable it in the Extensions app."

# RPM build (expects rpmbuild and fedora tree)
rpm:
	rm -rf .rpmbuild
	mkdir -p .rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS,tmp}
	tar czf gnome-shell-extension-trimmeh-0.1.0.tar.gz \
		--exclude .git --exclude .rpmbuild --exclude target --exclude "**/node_modules" --exclude upstream \
		--exclude gnome-shell-extension-trimmeh-0.1.0.tar.gz \
		--transform 's,^,gnome-shell-extension-trimmeh-0.1.0/,' .
	rpmbuild -ba packaging/fedora/trimmeh.spec \
		--define "_sourcedir $(pwd)" \
		--define "_topdir $(pwd)/.rpmbuild" \
		--define "_tmppath $(pwd)/.rpmbuild/tmp" \
		--define "__os_install_post %{nil}"

# Build a zip suitable for extensions.gnome.org or manual install
extension-zip: bundle-extension
	rm -f trimmeh-extension.zip
	cd shell-extension && zip -r ../trimmeh-extension.zip \
		metadata.json extension.js prefs.js stylesheet.css LICENSE \
		schemas/org.gnome.shell.extensions.trimmeh.gschema.xml schemas/gschemas.compiled

# KDE/Plasma 6 (Klipper DBus)
kde-configure:
	cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Debug

kde-build: kde-configure
	cmake --build build-kde

kde-run: kde-build
	./build-kde/trimmeh-kde

kde-probe-run: kde-build
	./build-kde/trimmeh-kde-probe
