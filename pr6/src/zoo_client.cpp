#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <poll.h>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <print>
#include <string>

#include "include/zoo_lib.h"

using std::println;

static bool moves = false;

namespace {

auto update_area(std::string_view str) -> bool {
    constexpr std::string_view PREFIX{"POS "};
    if (!str.starts_with(PREFIX)) {
        return false;
    }
    const char* first = str.data() + PREFIX.size();
    const char* last = str.data() + str.size();

    int x = 0;
    auto [ptr_x, ec_x] = std::from_chars(first, last, x);
    if (ec_x != std::errc{} || ptr_x == last || *ptr_x != ' ') {
        return false;
    }
    int y = 0;
    auto [ptr_y, ec_y] = std::from_chars(ptr_x + 1, last, y);
    if (ec_y != std::errc{}) {
        return false;
    }

    constexpr std::string_view MARKER{" AREA "};
    if (!std::string_view{ptr_y, last}.starts_with(MARKER)) {
        return false;
    }
    update_map(x, y, ptr_y + MARKER.size());
    return true;
}

auto write_all(int fd, const char* data, size_t size) -> bool {
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

auto parse_port(const char* text) -> int {
    char* end = nullptr;
    errno = 0;
    int64_t port = strtol(text, &end, 10);
    return errno == 0 && end != text && *end == '\0' && port > 0
                && port <= 65535
             ? static_cast<int>(port)
             : 0;
}
} // namespace

auto main(int argc, char** argv) -> int {
    bool debug = false;
    const char* host = nullptr;
    int port = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "-d") == 0) {
            debug = true;
        } else if (host == nullptr) {
            host = argv[index];
        } else {
            port = parse_port(argv[index]);
        }
    }
    if (host == nullptr || port == 0) {
        println(stderr, "Pouziti: {} [-d] <IP_adresa_serveru> <port>", argv[0]);
        return 1;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    std::array<char, 6> service{};
    std::snprintf(service.data(), sizeof(service), "%d", port);
    int result = getaddrinfo(host, service.data(), &hints, &addresses);
    if (result != 0) {
        std::println(stderr, "getaddrinfo: {}", gai_strerror(result));
        return 1;
    }

    int server = socket(
        addresses->ai_family, addresses->ai_socktype, addresses->ai_protocol
    );
    if (server < 0
        || connect(server, addresses->ai_addr, addresses->ai_addrlen) < 0) {
        perror(server < 0 ? "socket" : "connect");
        if (server >= 0) {
            close(server);
        }
        freeaddrinfo(addresses);
        return 1;
    }
    freeaddrinfo(addresses);
    signal(SIGPIPE, SIG_IGN);

    std::print("Zadejte jmeno navstevnika: ");
    std::fflush(stdout);
    std::string name;
    char letter = 0;
    // Read only the nickname; piped commands must remain available to poll().
    while (name.size() <= 33) {
        ssize_t count = read(STDIN_FILENO, &letter, 1);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0) {
            perror("read");
            close(server);
            return 1;
        }
        if (count == 0 || letter == '\n') {
            break;
        }
        name += letter;
    }
    if (!name.empty() && name.back() == '\r') {
        name.pop_back();
    }

    if (name.empty() || name.size() > 32
        || name.find_first_of(" \t") != std::string::npos) {
        println(stderr, "Neplatne jmeno");
        close(server);
        return 1;
    }
    std::string hello = std::format("HELLO {}\n", name);
    if (!write_all(server, hello.data(), hello.size())) {
        close(server);
        return 1;
    }
    println(
        "Prikazy: MSG <zprava>, WHERE, MOVE NORTH|SOUTH|WEST|EAST, "
        "INFO <zvire>, QUIT"
    );

    if (debug) {
        std::println(stderr, "Pripojeno k {}:{}", host, port);
    }
    srand(
        static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid())
    );
    std::array<pollfd, 2> events{{
      {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0},
      {.fd = server, .events = POLLIN, .revents = 0},
    }};
    auto last_input = std::chrono::steady_clock::now();
    constexpr std::array MOVES{
      "MOVE NORTH\n", "MOVE SOUTH\n", "MOVE WEST\n", "MOVE EAST\n"
    };
    std::string response;

    map_init(19, 19);
    draw_map();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - last_input
        )
                           .count();
        int timeout = !moves || events[0].fd < 0
                        ? -1
                        : static_cast<int>(1000 - elapsed);
        if (moves && timeout < 0 && events[0].fd >= 0) {
            timeout = 0;
        }

        int ready
            = poll(events.data(), static_cast<nfds_t>(events.size()), timeout);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready < 0) {
            perror("poll");
            break;
        }
        if (ready == 0 && moves) {
            const char* move = MOVES.at(rand() % 4);
            if (debug) {
                std::print(stderr, "AUTO: {}", move);
            }
            if (!write_all(server, move, std::strlen(move))) {
                break;
            }
            last_input = std::chrono::steady_clock::now();
            continue;
        }

        if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            std::array<char, 1024> buffer{};
            ssize_t count = read(server, buffer.begin(), sizeof(buffer));
            if (count <= 0) {
                if (count < 0 && errno == EINTR) {
                    continue;
                }
                if (count < 0) {
                    perror("read");
                }
                break;
            }

            response.append(buffer.data(), static_cast<size_t>(count));
            size_t newline = 0;
            while ((newline = response.find('\n')) != std::string::npos) {
                std::string line = response.substr(0, newline);
                response.erase(0, newline + 1);
                if (update_area(line)) {
                    draw_map();
                }
                println("{}", line);
            }
        }

        if (events[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            std::array<char, 1024> buffer{};
            ssize_t count = read(STDIN_FILENO, buffer.begin(), sizeof(buffer));
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("read");
                break;
            }
            if (count == 0) {
                shutdown(server, SHUT_WR);
                events[0].fd = -1;
            } else {
                if (!write_all(
                        server, buffer.data(), static_cast<size_t>(count)
                    )) {
                    break;
                }
                last_input = std::chrono::steady_clock::now();
            }
        }
    }

    close(server);
    return 0;
}
