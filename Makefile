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
# -fwasm-exceptions, not -fexceptions: the latter emulates exceptions in
# JavaScript and instruments every call to do it, which cost the searcher a 2x
# slowdown for a feature used on exactly one path (a diagnostic). Native wasm
# exceptions bring it back to within 15% of the C++ binary. Needs Chrome 95,
# Firefox 100 or Safari 15.2, all from 2021-22.
EMFLAGS = -std=c++17 -O2 -fwasm-exceptions -sALLOW_MEMORY_GROWTH=1 -lembind

# Two wasm builds of the same source, for two different hosts.
#
#   loophole.node.js  a command line with the real filesystem, so wasm-check can
#                     run the very same arguments as the native binary.
#   web/loophole.js   a module with no filesystem and no argv, exporting judge()
#                     for the page. INVOKE_RUN=0 because Emscripten would
#                     otherwise call main() at load and print the usage text.
loophole.node.js: loophole.cpp
	$(EMCC) $(EMFLAGS) -sNODERAWFS=1 loophole.cpp -o loophole.node.js

web/loophole.js: loophole.cpp
	@mkdir -p web
	$(EMCC) $(EMFLAGS) -sMODULARIZE=1 -sEXPORT_NAME=createLoophole \
	        -sENVIRONMENT=web,worker,node -sINVOKE_RUN=0 loophole.cpp -o web/loophole.js

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
	@./ci/keywords-check.sh

# Put it on PATH, so it is `loophole a.wish` rather than `./loophole a.wish`.
wasm: loophole.node.js web/loophole.js

# The claim the browser build has to earn: it judges exactly as the native one
# does. Anything less and the playground would be a second implementation that
# quietly disagrees with the compiler the spec describes.
# `loophole.node.js` is a prerequisite, so a failed wasm build stops here rather
# than letting the suite pass against whatever was left in the directory from
# last time. A check that can succeed on a stale artifact is not a check.
#
# The node line goes through `sh -c` on purpose. emsdk puts its own root on
# PATH, and that root contains a subdirectory literally named `node`. A shell
# searching PATH sees the match is not executable and keeps looking; GNU make 4
# execs a simple recipe line directly and stops at the first match, reporting
# "node: Permission denied". It only bites when emsdk comes first on PATH, which
# is what `setup-emsdk` does on CI and what `emsdk_env.sh` does for anyone who
# sources it in a fresh shell -- so this is not a CI workaround.
wasm-check: loophole loophole.node.js web/loophole.js
	@./ci/wasm-check.sh
	@sh -c 'node ci/embed-check.mjs'
	@sh -c 'node ci/lessons-check.mjs'

install: loophole
	@mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 loophole $(DESTDIR)$(BINDIR)/loophole
	@echo "installed $(DESTDIR)$(BINDIR)/loophole"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/loophole

clean:
	rm -f loophole loophole.node.js loophole.node.wasm
	rm -f web/loophole.js web/loophole.wasm
