#include <array>
#include <cstdint>
#include <cstdio>
#include <print>
#include <string>
#include <vector>

#include <unistd.h>

namespace {
    auto char_list_gen() -> std::vector<char> {
        std::vector<char> list(256);
        for (int i{'A'}; i < 'z' + 1; ++i) {
            list[i] = static_cast<char>(i);
        }
        return list;
    }
    
    const std::vector<char> char_list{char_list_gen()};

    auto read_string() -> std::string {
        std::string str;
        std::array<char, 1024> buffer{};
        while (std::fgets(buffer.data(), buffer.size(), stdin)) {
            str += buffer.data();
        }
        return str;
    }

    auto ceasar(const std::string &input, int64_t shift) -> std::string {
        std::string result{};

        for (char c : input) {
            if (c >= 'A' && c <= 'Z') {
                result += static_cast<char>(((c - 'A' + shift + 26) % 26) + 'A');
            } else if (c >= 'a' && c <= 'z') {
                result += static_cast<char>(((c - 'a' + shift + 26) % 26) + 'a');
            } else {
                result += c;
            }
        }

        return result;
    }
} // namespace
    
auto main(int argc, char* argv[]) -> int {
    if (argc != 2) {
        std::println(stderr, "Usage: {} <non-zero shift>", argv[0]);
        return 1;
    }

    std::string operation{argv[0]};
    operation.erase(0, 2);

    int64_t shift{std::strtol(argv[1], nullptr, 10)};

    if (shift == 0) {
        std::println(stderr, "Invalid shift {}", argv[1]);
        return 1;
    }

    shift %= 26;

    std::string input{read_string()};

    if (operation == "encode") {
        std::print("{}", ceasar(input, shift));
    } else if (operation == "decode") {
        std::print("{}", ceasar(input, -shift));
    } else {
        std::println(stderr, "Invalid operation: {}", operation);
        return 1;
    }

    return 0;
}