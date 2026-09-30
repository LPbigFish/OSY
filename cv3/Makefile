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

LIBNAME = log

#DYNAMIC
LOG_OBJ = log_lib/log.o

LOG_LIB = log_lib/lib$(LIBNAME).so

DYNAMIC_TARGET = access

all: $(DYNAMIC_TARGET)

# DYNAMIC LIB

$(LOG_OBJ): log_lib/log.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

$(LOG_LIB): $(LOG_OBJ)
	$(CXX) -shared $^ -o $@

# DYNAMIC TARGET
$(DYNAMIC_TARGET): src/main.cpp $(LOG_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -Llog_lib -l$(LIBNAME) -o $@ 

clean:
	rm -f $(DYNAMIC_TARGET)
	rm -f $(LOG_OBJ)
	rm -f $(LOG_LIB)