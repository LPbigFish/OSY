CXX      := g++
CXXFLAGS := -std=c++23 -g -O0 \
            -Wall -Wcast-align -Wextra -Wformat -Wformat-security \
            -Wno-shadow -Wno-unused-result -Wnull-dereference \
            -Wredundant-decls -Wreturn-type -Wstrict-overflow \
            -Wuninitialized -Wvla \
            -fstack-protector-strong \
            -fsanitize=address,undefined
LDFLAGS  := -fsanitize=address,undefined

.PHONY: all clean

all: encode decode

# argv[0] basename selects encode/decode behaviour, so build one and copy.
encode: src/main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

decode: encode
	cp encode decode

clean:
	$(RM) encode decode
