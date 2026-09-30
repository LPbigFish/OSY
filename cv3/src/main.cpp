#include "log.hpp"
#include <print>
#include <string_view>
#include <unistd.h>

auto main(int argc, char** argv) -> int {
    if (argc != 3) {
        std::println(stderr, "Bad usage");
        return 1;
    }

    auto flag = std::string_view{argv[1]};

    if (flag == "-a") {
        log_access(argv[2]);
    } else if (flag == "-s") {
        log_stat(argv[2]);
    } else if (flag == "-l") {
        log_lstat(argv[2]);
    } else if (flag == "-f") {
        log_fstat(argv[2]);
    } else if (flag == "-d") {
        log_dirs(argv[2]);
    } else if (flag == "-r") {
        log_user_readable(argv[2]);
    } else if (flag == "-h") {
        log_head(argv[2]);
    } else {
        std::println(stderr, "Wrong flag: {}", flag);
        return 1;
    }

    return 0;
}
