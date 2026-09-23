#include "../src/transform.hpp"

auto transform_char(char c) -> char {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}
