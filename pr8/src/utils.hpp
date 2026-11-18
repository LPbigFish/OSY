#pragma once

#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

namespace utils {

constexpr size_t MAX_LINE_SIZE = 4096;

constexpr auto JIDLA = std::array{
  "Smazeny syr",
  "Kureci rizek s hranolky",
  "Spagety Bolognese",
  "Zeleninovy salat",
  "Hamburger",
  "Hot dog",
  "Palacinky",
  "Ovocny pohar"
};

enum class Role : uint8_t {
    Cook,
    Host,
};

struct ClientInfo {
    int socket;
    Role role;
};

inline auto write_all(int fd, const char* data, size_t size) -> bool {
    size_t sent = 0;
    while (sent < size) {
        ssize_t count = write(fd, data + sent, size - sent);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        sent += static_cast<size_t>(count);
    }
    return true;
}

inline auto read_line(int fd, std::string& line) -> bool {
    line.clear();
    while (true) {
        char character{};
        ssize_t count = read(fd, &character, sizeof(character));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        if (character == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        }
        if (line.size() == MAX_LINE_SIZE) {
            errno = EMSGSIZE;
            return false;
        }
        line += character;
    }
}

inline auto send_text(int fd, const std::string& text) -> bool {
    return write_all(fd, text.data(), text.size());
}

inline auto parse_port(const char* text) -> int {
    char* end = nullptr;
    errno = 0;
    int64_t port = strtol(text, &end, 10);
    return errno == 0 && end != text && *end == '\0' && port > 0
                && port <= 65535
             ? static_cast<int>(port)
             : 0;
}
} // namespace utils
