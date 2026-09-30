#include "../src/log.hpp"
#include <array>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <print>
#include <string>
#include <sys/dir.h>
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
    struct stat s{};

    int fid = open(path.c_str(), O_RDONLY);
    fstat(fid, &s);
    close(fid);

    std::println(
        "Soubor: {}\n"
        "Velikost: {} B\n"
        "Inode: {}",
        path.c_str(),
        s.st_size,
        s.st_ino
    );
}

auto log_dirs(const std::filesystem::path& path) -> void {
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        std::println(stderr, "Nelze otevřít adresář: {}", path.c_str());
        return;
    }

    while (auto* entry = readdir(dir)) {
        if (entry->d_name[0] == '.'
            && (entry->d_name[1] == '\0'
                || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        auto entry_path = path / entry->d_name;
        struct stat entry_stat{};
        if (lstat(entry_path.c_str(), &entry_stat) != 0) {
            std::println("{}", entry_path.c_str());
            continue;
        }

        if (S_ISLNK(entry_stat.st_mode)) {
            std::array<char, 4096> target{};
            auto length = readlink(
                entry_path.c_str(), target.data(), target.size() - 1
            );

            if (length >= 0) {
                target.at(static_cast<std::size_t>(length)) = '\0';
                std::println("{} -> {}", entry_path.c_str(), target.data());
            } else {
                std::println("{} -> [readlink failed]", entry_path.c_str());
            }

            continue;
        }

        std::println(
            "{}    Velikost: {} B Inode: {} Čitelný: {}, Spustitelný: {} "
            "Poslední změna: {}",
            entry_path.c_str(),
            entry_stat.st_size,
            entry_stat.st_ino,
            access(entry_path.c_str(), R_OK) == 0 ? "ANO" : "NE",
            access(entry_path.c_str(), X_OK) == 0 ? "ANO" : "NE",
            entry_stat.st_atim.tv_sec
        );

        if (S_ISDIR(entry_stat.st_mode)) {
            log_dirs(entry_path);
        }
    }

    closedir(dir);
}

auto log_user_readable(const std::filesystem::path& path) -> void {
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        std::println(stderr, "Nelze otevřít složka: {}", path.c_str());
        return;
    }

    while (auto* entry = readdir(dir)) {
        if (entry->d_name[0] == '.'
            && (entry->d_name[1] == '\0'
                || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        auto entry_path = path / entry->d_name;
        struct stat entry_stat{};
        if (lstat(entry_path.c_str(), &entry_stat) != 0) {
            continue;
        }

        if (S_ISDIR(entry_stat.st_mode)) {
            log_user_readable(entry_path);
            continue;
        }

        if (!S_ISREG(entry_stat.st_mode)
            || access(entry_path.c_str(), R_OK) != 0) {
            continue;
        }

        std::println(
            "{}    Velikost: {} B",
            entry_path.c_str(),
            entry_stat.st_size
        );
    }

    closedir(dir);
}

auto log_head(const std::filesystem::path& path) -> void {
    struct stat entry_stat{};
    if (lstat(path.c_str(), &entry_stat) != 0) {
        std::println(stderr, "Nelze načíst: {}", path.c_str());
        return;
    }

    if (S_ISDIR(entry_stat.st_mode)) {
        DIR* dir = opendir(path.c_str());
        if (dir == nullptr) {
            std::println(stderr, "Nelze otevřít adresář: {}", path.c_str());
            return;
        }

        while (auto* entry = readdir(dir)) {
            if (entry->d_name[0] == '.'
                && (entry->d_name[1] == '\0'
                    || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
                continue;
            }

            log_head(path / entry->d_name);
        }

        closedir(dir);
        return;
    }

    if (!S_ISREG(entry_stat.st_mode)) {
        return;
    }

    int file = open(path.c_str(), O_RDONLY);
    if (file == -1) {
        std::println(stderr, "Nelze číst soubor: {}", path.c_str());
        return;
    }

    std::println("{}:", path.c_str());
    std::string line;
    std::array<char, 4096> buffer{};
    int lines_printed = 0;
    bool read_failed = false;

    while (lines_printed < 5) {
        auto bytes = read(file, buffer.data(), buffer.size());
        if (bytes == 0) {
            break;
        }
        if (bytes < 0) {
            read_failed = true;
            break;
        }

        for (ssize_t index = 0; index < bytes; ++index) {
            if (buffer.at(static_cast<std::size_t>(index)) == '\n') {
                std::println("{}", line);
                line.clear();
                ++lines_printed;
                if (lines_printed == 5) {
                    break;
                }
            } else {
                line.push_back(buffer.at(static_cast<std::size_t>(index)));
            }
        }
    }

    if (!read_failed && lines_printed < 5 && !line.empty()) {
        std::println("{}", line);
    }
    if (read_failed) {
        std::println(stderr, "Chyba při čtení souboru: {}", path.c_str());
    }
    close(file);
}
