CXX      := g++
CXXFLAGS := -std=c++23 -g -O0 \
            -Wall -Wcast-align -Wextra -Wformat -Wformat-security \
            -Wno-shadow -Wno-unused-result -Wnull-dereference \
            -Wredundant-decls -Wreturn-type -Wstrict-overflow \
            -Wuninitialized -Wvla \
            -fstack-protector-strong \
            -fsanitize=address,undefined
LDFLAGS  := -fsanitize=address,undefined -L.
# ---

TARGET = transform
LIBNAME = transforms
LIBFILE = lib$(LIBNAME).a

LIBSRC   := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
LIBOBJ   := $(LIBSRC:.cpp=.o)

LDLIBS := -l$(LIBNAME)

all: $(TARGET)

clean:
	rm -rf $(TARGET) $(LIBFILE) *.o

$(LIBFILE): $(LIBOBJ)
	ar rcs $@ $^
	@echo "\nStatic library created: '$(LIBFILE)'.\n"

$(TARGET): src/main.cpp $(LIBFILE)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
