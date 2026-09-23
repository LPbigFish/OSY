#include "charutil.hpp"
#include <cctype>

auto is_letter(char c) -> bool {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
};

auto is_digit(char c) -> bool {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

auto to_upper(char c) -> char {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

auto to_lower(char c) -> char {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}