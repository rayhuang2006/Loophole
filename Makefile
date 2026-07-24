CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

# Where `make install` puts the compiler. Override for a home-directory install
# that needs no sudo:  make install PREFIX=$HOME/.local
PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin

loophole: loophole.cpp
	$(CXX) $(CXXFLAGS) loophole.cpp -o loophole

.PHONY: run check install uninstall clean
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
install: loophole
	@mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 loophole $(DESTDIR)$(BINDIR)/loophole
	@echo "installed $(DESTDIR)$(BINDIR)/loophole"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/loophole

clean:
	rm -f loophole
