#include "log.hpp"
#include <print>
#include <unistd.h>

auto main(int argc, char** argv) -> int {
    std::println();

    if (argc != 3) {
        std::println(stderr, "Bad usage");
        return 1;
    }

    return 0;
}
