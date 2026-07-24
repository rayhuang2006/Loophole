CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

wishc: wishc.cpp
	$(CXX) $(CXXFLAGS) wishc.cpp -o wishc

.PHONY: run check clean
# An example may name the genie it wants with a `# genie: PATH` line; otherwise
# the built-in genie is used.
run: wishc
	@for f in examples/*.wish; do \
		echo "-----------------------------------------------------------"; \
		g=$$(sed -n 's/^# genie: *//p' $$f | head -1); \
		./wishc $${g:+--genie $$g} $$f || true; \
	done

# The regression suite CI runs. `./ci/check.sh --update` refreshes the goldens
# when a change to the output or the semantics is intentional.
check: wishc
	@./ci/check.sh

clean:
	rm -f wishc
