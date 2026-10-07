CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -g

TARGET := chat

all: $(TARGET)

$(TARGET): src/main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean