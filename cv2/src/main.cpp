#include "charutil.hpp"
#include "transform.hpp"
#include <cstdint>
#include <cstring>
#include <format>
#include <print>
#include <string>
#include <unistd.h>

struct Stats {
    bool collect = false;
    uint32_t chars = 0;
    uint32_t letters = 0;
    uint32_t digits = 0;
    uint32_t other = 0;

    auto print() -> void {
        if (collect) {
            std::println(
                "\n"
                "Characters: {}\n"
                "Letters: {}\n"
                "Digits: {}\n"
                "Others: {}",
                chars,
                letters,
                digits,
                other
            );
        }
    }
};

auto main(int argc, char** argv) -> int {
    Stats stats{};

    if (argc == 2 && strcmp(argv[1], "-s") == 0) {
        stats.collect = true;
    }

    char buf{};

    int64_t status{};
    while (status = read(STDIN_FILENO, &buf, 1), status != -1 && status != 0) {
        if (stats.collect) {
            stats.chars++;

            if (is_digit(buf)) {
                stats.digits++;
            } else if (is_letter(buf)) {
                stats.letters++;
            } else {
                stats.other++;
            }
        }

        std::print("{}", transform_char(buf));
    }

    std::println();

    stats.print();

    return static_cast<int>(status);
}
