CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -g -pthread
LDLIBS   := -lrt

all: client server

client: src/client.cpp src/utils.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

server: src/server.cpp src/utils.hpp src/semafor.hpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

test: all
	python3 tests/test_restaurant.py

clean:
	rm -f client server

.PHONY: all test clean
