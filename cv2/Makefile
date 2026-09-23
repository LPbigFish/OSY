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
SND_LIB_NAME = charutil

# STATIC
STATIC_LIB = lib$(LIBNAME).a
STATIC_OBJ = src/transform.o

STATIC_UTIL_LIB = lib$(SND_LIB_NAME).a
STATIC_UTIL_OBJ = src/charutil.o

#DYNAMIC
LOWER_OBJ = dynamic_lower/transform.o
UPPER_OBJ = dynamic_upper/transform.o
ROT13_OBJ = dynamic_rot13/transform.o

LOWER_LIB = dynamic_lower/lib$(LIBNAME).so
UPPER_LIB = dynamic_upper/lib$(LIBNAME).so
ROT13_LIB = dynamic_rot13/lib$(LIBNAME).so

DYNAMIC_TARGET = dynamic_transform
STATIC_TARGET = static_transform

all: static dynamic

# STATIC LIB
$(STATIC_OBJ): src/transform.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(STATIC_LIB): $(STATIC_OBJ)
	ar rcs $@ $^

# CHARUTIL

$(STATIC_UTIL_OBJ): src/charutil.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(STATIC_UTIL_LIB): $(STATIC_UTIL_OBJ)
	ar rcs $@ $^

# DYNAMIC LIB

$(LOWER_OBJ): dynamic_lower/transform.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@
 
$(UPPER_OBJ): dynamic_upper/transform.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

$(ROT13_OBJ): dynamic_rot13/transform.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

# ---

$(LOWER_LIB): $(LOWER_OBJ)
	$(CXX) -shared $^ -o $@

$(UPPER_LIB): $(UPPER_OBJ)
	$(CXX) -shared $^ -o $@

$(ROT13_LIB): $(ROT13_OBJ)
	$(CXX) -shared $^ -o $@


# STATIC TARGET
$(STATIC_TARGET): src/main.cpp $(STATIC_LIB) $(STATIC_UTIL_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -l$(SND_LIB_NAME) -l$(LIBNAME) -o $@


# DYNAMIC TARGET
$(DYNAMIC_TARGET): src/main.cpp $(UPPER_LIB) $(LOWER_LIB) $(ROT13_LIB) $(STATIC_UTIL_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -l$(SND_LIB_NAME) -Ldynamic_upper -l$(LIBNAME) -o $@ 

# FINALE
static: $(STATIC_TARGET)

dynamic: $(DYNAMIC_TARGET)

clean:
	rm -f $(STATIC_TARGET) $(DYNAMIC_TARGET)
	rm -f $(STATIC_LIB) $(STATIC_UTIL_LIB)
	rm -f $(STATIC_OBJ) $(STATIC_UTIL_OBJ)
	rm -f $(UPPER_OBJ) $(LOWER_OBJ) $(ROT13_OBJ)
	rm -f $(UPPER_LIB) $(LOWER_LIB) $(ROT13_LIB)