#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

static bool write_all(int fd, const char* data, size_t size) {
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

static int parse_port(const char* text) {
    char* end = nullptr;
    errno = 0;
    long port = strtol(text, &end, 10);
    return errno == 0 && end != text && *end == '\0' && port > 0
                && port <= 65535
             ? static_cast<int>(port)
             : 0;
}

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
        std::fprintf(
            stderr, "Pouziti: %s [-d] <IP_adresa_serveru> <port>\n", argv[0]
        );
        return 1;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    char service[6];
    std::snprintf(service, sizeof(service), "%d", port);
    int result = getaddrinfo(host, service, &hints, &addresses);
    if (result != 0) {
        std::fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
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

    if (debug) {
        std::fprintf(stderr, "Pripojeno k %s:%d\n", host, port);
    }
    srand(
        static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid())
    );
    pollfd events[]{
      {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0},
      {.fd = server, .events = POLLIN, .revents = 0},
    };
    auto last_input = std::chrono::steady_clock::now();
    constexpr const char* moves[]{
      "MOVE NORTH\n", "MOVE SOUTH\n", "MOVE WEST\n", "MOVE EAST\n"
    };

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - last_input
        )
                           .count();
        int timeout = events[0].fd < 0 ? -1 : static_cast<int>(5000 - elapsed);
        if (timeout < 0 && events[0].fd >= 0) {
            timeout = 0;
        }

        int ready = poll(events, 2, timeout);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready < 0) {
            perror("poll");
            break;
        }
        if (ready == 0) {
            const char* move = moves[rand() % 4];
            if (debug) {
                std::fprintf(stderr, "AUTO: %s", move);
            }
            if (!write_all(server, move, std::strlen(move))) {
                break;
            }
            last_input = std::chrono::steady_clock::now();
            continue;
        }

        if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            char buffer[1024];
            ssize_t count = read(server, buffer, sizeof(buffer));
            if (count <= 0) {
                if (count < 0) {
                    perror("read");
                }
                break;
            }
            if (!write_all(STDOUT_FILENO, buffer, static_cast<size_t>(count))) {
                break;
            }
        }

        if (events[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            char buffer[1024];
            ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count < 0) {
                perror("read");
                break;
            }
            if (count == 0) {
                shutdown(server, SHUT_WR);
                events[0].fd = -1;
            } else {
                if (!write_all(server, buffer, static_cast<size_t>(count))) {
                    break;
                }
                last_input = std::chrono::steady_clock::now();
            }
        }
    }

    close(server);
    return 0;
}
