CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -g

TARGETS := zoo_server zoo_client zoo_info

LIB := libzoomap.so

all: $(TARGETS)

zoo_server: src/zoo_server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

zoo_client: src/zoo_client.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $< -L. -lzoomap -o $@

zoo_info: src/zoo_info.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean
