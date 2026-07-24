CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

wishc: wishc.cpp
	$(CXX) $(CXXFLAGS) wishc.cpp -o wishc

.PHONY: run clean
# An example may name the genie it wants with a `# genie: PATH` line; otherwise
# the built-in genie is used.
run: wishc
	@for f in examples/*.wish; do \
		echo "-----------------------------------------------------------"; \
		g=$$(sed -n 's/^# genie: *//p' $$f | head -1); \
		./wishc $${g:+--genie $$g} $$f; \
	done

clean:
	rm -f wishc
