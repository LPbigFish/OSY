#include "transform.hpp"
#include <cstdint>
#include <print>
#include <unistd.h>

auto main() -> int {
    char buf{};

    int64_t status{};

    while (status = read(STDIN_FILENO, &buf, 1), status != -1 && status != 0) {
        std::print("{}", transform_char(buf));
    }

    std::println();

    return static_cast<int>(status);
}
