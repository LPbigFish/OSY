#include "transform.hpp"

auto transform_char(char c) -> char {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}