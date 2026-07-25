CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

# Where `make install` puts the compiler. Override for a home-directory install
# that needs no sudo:  make install PREFIX=$HOME/.local
PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin

loophole: loophole.cpp
	$(CXX) $(CXXFLAGS) loophole.cpp -o loophole

# The browser build. Needs emsdk on PATH:  source ~/emsdk/emsdk_env.sh
# NODERAWFS gives the node build the real filesystem, so `wasm-check` can run
# the very same command lines as the native binary and diff the output.
EMCC ?= emcc
EMFLAGS = -std=c++17 -O2 -fexceptions -sALLOW_MEMORY_GROWTH=1

loophole.node.js: loophole.cpp
	$(EMCC) $(EMFLAGS) -sNODERAWFS=1 loophole.cpp -o loophole.node.js

.PHONY: run check wasm wasm-check install uninstall clean
# An example may name the genie it wants with a `# genie: PATH` line; otherwise
# the built-in genie is used.
run: loophole
	@for f in examples/*.wish; do \
		echo "-----------------------------------------------------------"; \
		g=$$(sed -n 's/^# genie: *//p' $$f | head -1); \
		./loophole $${g:+--genie $$g} $$f || true; \
	done

# The regression suite CI runs. `./ci/check.sh --update` refreshes the goldens
# when a change to the output or the semantics is intentional.
check: loophole
	@./ci/check.sh

# Put it on PATH, so it is `loophole a.wish` rather than `./loophole a.wish`.
wasm: loophole.node.js

# The claim the browser build has to earn: it judges exactly as the native one
# does. Anything less and the playground would be a second implementation that
# quietly disagrees with the compiler the spec describes.
wasm-check: loophole loophole.node.js
	@./ci/wasm-check.sh

install: loophole
	@mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 loophole $(DESTDIR)$(BINDIR)/loophole
	@echo "installed $(DESTDIR)$(BINDIR)/loophole"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/loophole

clean:
	rm -f loophole loophole.node.js loophole.node.wasm
