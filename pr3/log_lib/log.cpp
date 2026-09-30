#include "../src/log.hpp"
#include <fcntl.h>
#include <print>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

auto log_access(const std::filesystem::path& path) -> void {
    int ac = access(path.c_str(), F_OK);

    std::println(
        "Soubor {} lze číst: {}", path.c_str(), ac == 0 ? "ANO" : "NE"
    );
}

auto log_stat(const std::filesystem::path& path) -> void {
    struct stat s{};
    stat(path.c_str(), &s);

    std::println(
        "Soubor: {}\n"
        "Velikost: {} B\n"
        "UID: {}\n"
        "GID: {}\n"
        "Hardlinky: {}\n",
        path.c_str(),
        s.st_size,
        s.st_uid,
        s.st_gid,
        s.st_nlink
    );
}

auto log_lstat(const std::filesystem::path& path) -> void {
    struct stat s{};
    lstat(path.c_str(), &s);

    std::println(
        "Soubor: {}\n"
        "Velikost: {} B\n"
        "Číslo inode: {}",
        path.c_str(),
        s.st_size,
        s.st_ino
    );
}

auto log_fstat(const std::filesystem::path& path) -> void {
    open(path.c_str(), O_PATH);
}
