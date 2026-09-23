#include "../src/transform.hpp"

auto transform_char(char c) -> char {
    char r{};

    if (c >= 'A' && c <= 'Z') {
        r = static_cast<char>(((c - 'A' + 13 + 26) % 26) + 'A');
    } else if (c >= 'a' && c <= 'z') {
        r = static_cast<char>(((c - 'a' + 13 + 26) % 26) + 'a');
    } else {
        r = c;
    }

    return r;
}
