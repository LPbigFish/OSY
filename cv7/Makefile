CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -g -pthread

TARGETS := zoo_server zoo_client zoo_info

all: $(TARGETS)

zoo_server: src/zoo_server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

zoo_client: src/zoo_client.cpp libzoomap.so
	$(CXX) $(CXXFLAGS) src/zoo_client.cpp -L. -lzoomap -o $@

zoo_info: src/zoo_info.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)

test: all
	python3 test_chat.py

.PHONY: all clean test
