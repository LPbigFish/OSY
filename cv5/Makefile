CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -g

TARGET := chat_demo

all: $(TARGET)

$(TARGET): src/main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGET) out.txt

.PHONY: all clean
