CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

wishc: wishc.cpp
	$(CXX) $(CXXFLAGS) wishc.cpp -o wishc

.PHONY: run clean
run: wishc
	@for f in examples/*.wish; do \
		echo "-----------------------------------------------------------"; \
		./wishc $$f; \
	done

clean:
	rm -f wishc
