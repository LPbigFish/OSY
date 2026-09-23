CXX      := clang++
CXXFLAGS := -std=c++23 -g -O0 \
            -Wall -Wcast-align -Wextra -Wformat -Wformat-security \
            -Wno-shadow -Wno-unused-result -Wnull-dereference \
            -Wredundant-decls -Wreturn-type -Wstrict-overflow \
            -Wuninitialized -Wvla \
            -fstack-protector-strong \
            -fsanitize=address,undefined
LDFLAGS  := -fsanitize=address,undefined -L.
# ---

LIBNAME = transform


# STATIC
STATIC_LIB = lib$(LIBNAME).a
STATIC_OBJ = src/transform.o

#DYNAMIC
LOWER_OBJ = dynamic_lower/transform.o
UPPER_OBJ = dynamic_upper/transform.o

LOWER_LIB = dynamic_lower/lib$(LIBNAME).so
UPPER_LIB = dynamic_upper/lib$(LIBNAME).so

DYNAMIC_TARGET = dynamic_transform
STATIC_TARGET = static_transform

all: static dynamic

# STATIC LIB
$(STATIC_OBJ): src/transform.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(STATIC_LIB): $(STATIC_OBJ)
	ar rcs $@ $^

# DYNAMIC LIB

$(LOWER_OBJ): dynamic_lower/transform.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@
 
$(UPPER_OBJ): dynamic_upper/transform.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

$(LOWER_LIB): $(LOWER_OBJ)
	$(CXX) -shared $^ -o $@

$(UPPER_LIB): $(UPPER_OBJ)
	$(CXX) -shared $^ -o $@

# STATIC TARGET
$(STATIC_TARGET): src/main.cpp $(STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB)


# DYNAMIC TARGET
$(DYNAMIC_TARGET): src/main.cpp $(UPPER_LIB) $(LOWER_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -Ldynamic_upper -l$(LIBNAME) -o $@ 

# FINALE
static: $(STATIC_TARGET)

dynamic: $(DYNAMIC_TARGET)

clean:
	rm -f $(STATIC_TARGET) $(DYNAMIC_TARGET)
	rm -f $(STATIC_LIB)
	rm -f $(STATIC_OBJ)
	rm -f $(UPPER_OBJ) $(LOWER_OBJ)
	rm -f $(UPPER_LIB) $(LOWER_LIB)