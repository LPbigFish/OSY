#include <arpa/inet.h>
#include <array>
#include <cstdint>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <print>
#include <string>

constexpr int MAP_SIZE = 10;
constexpr int CLIENT_TIMEOUT_MS = 60000;
static bool g_debug = false;

namespace {

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

auto send_text(int fd, const std::string& text) -> bool {
    return write_all(fd, text.data(), text.size());
}

auto valid_word(const std::string& word) -> bool {
    return !word.empty() && word.find_first_of(" \t") == std::string::npos;
}

auto send_info(int client, const std::string& animal) -> bool {
    std::array<int, 2> channel{};
    if (pipe(channel.data()) < 0) {
        perror("pipe");
        return send_text(client, "ERROR nelze spustit zoo_info\n");
    }

    pid_t child = fork();
    if (child == 0) {
        close(channel[0]);
        if (dup2(channel[1], STDOUT_FILENO) < 0) {
            _exit(1);
        }
        close(channel[1]);
        execl("./zoo_info", "zoo_info", animal.c_str(), nullptr);
        _exit(1);
    }
    if (child < 0) {
        perror("fork");
        close(channel[0]);
        close(channel[1]);
        return send_text(client, "ERROR nelze spustit zoo_info\n");
    }

    close(channel[1]);
    bool success = true;
    std::array<char, 512> buffer{};
    ssize_t count = 0;
    while ((count = read(channel[0], buffer.begin(), sizeof(buffer))) > 0) {
        if (success) {
            success
                = write_all(client, buffer.begin(), static_cast<size_t>(count));
        }
    }
    if (count < 0) {
        perror("read");
        success = false;
    }
    close(channel[0]);
    while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
    }
    return success;
}

auto handle_command(
    int client, const std::string& command, std::string& name, int& x, int& y
) -> bool {
    if (command.starts_with("HELLO ")) {
        std::string requested = command.substr(6);
        if (!name.empty() || !valid_word(requested)) {
            return send_text(client, "ERROR neplatny prikaz HELLO\n");
        }
        name = requested;
        x = rand() % MAP_SIZE;
        y = rand() % MAP_SIZE;
        return send_text(
            client,
            "WELCOME " + name + " (" + std::to_string(x) + ","
                + std::to_string(y) + ")\n"
        );
    }

    if (name.empty()) {
        return send_text(client, "ERROR nejprve pouzijte HELLO name\n");
    }
    if (command == "WHERE") {
        return send_text(
            client, "POS " + std::to_string(x) + " " + std::to_string(y) + "\n"
        );
    }
    if (command.starts_with("MOVE ")) {
        std::string direction = command.substr(5);
        if (direction == "NORTH" && y > 0) {
            --y;
        } else if (direction == "SOUTH" && y < MAP_SIZE - 1) {
            ++y;
        } else if (direction == "WEST" && x > 0) {
            --x;
        } else if (direction == "EAST" && x < MAP_SIZE - 1) {
            ++x;
        } else if (direction != "NORTH" && direction != "SOUTH"
                   && direction != "WEST" && direction != "EAST") {
            return send_text(client, "ERROR neznamy smer\n");
        }
        return send_text(
            client, "POS " + std::to_string(x) + " " + std::to_string(y) + "\n"
        );
    }
    if (command.starts_with("INFO ")) {
        std::string animal = command.substr(5);
        if (!valid_word(animal)) {
            return send_text(client, "ERROR neplatny prikaz INFO\n");
        }
        return send_info(client, animal);
    }
    if (command == "QUIT") {
        send_text(client, "BYE " + name + "\n");
        return false;
    }
    return send_text(client, "ERROR neznamy prikaz\n");
}

auto serve_player(int client) -> void {
    signal(SIGCHLD, SIG_DFL);
    srand(
        static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid())
    );

    pollfd event{.fd = client, .events = POLLIN, .revents = 0};
    std::string input;
    std::string name;
    int x = 0;
    int y = 0;

    while (true) {
        int ready = poll(&event, 1, CLIENT_TIMEOUT_MS);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready < 0) {
            perror("poll");
            break;
        }
        if (ready == 0) {
            send_text(client, "TIMEOUT\n");
            break;
        }
        if (!(event.revents & (POLLIN | POLLHUP | POLLERR))) {
            continue;
        }

        std::array<char, 1024> buffer{};
        ssize_t count = read(client, buffer.begin(), sizeof(buffer));
        if (count <= 0) {
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0) {
                perror("read");
            }
            break;
        }
        input.append(buffer.begin(), static_cast<size_t>(count));
        if (input.size() > 4096) {
            send_text(client, "ERROR prikaz je prilis dlouhy\n");
            break;
        }

        size_t newline = 0;
        while ((newline = input.find('\n')) != std::string::npos) {
            std::string command = input.substr(0, newline);
            input.erase(0, newline + 1);
            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }
            if (g_debug) {
                std::println(stderr, "[{}] {}", getpid(), command);
            }
            if (!handle_command(client, command, name, x, y)) {
                return;
            }
        }
    }
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
    int port = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "-d") == 0) {
            g_debug = true;
        } else {
            port = parse_port(argv[index]);
        }
    }
    if (port == 0) {
        std::fprintf(stderr, "Pouziti: %s [-d] <port>\n", argv[0]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return 1;
    }
    int reuse = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
        < 0) {
        perror("setsockopt");
        close(listener);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address))
            < 0
        || listen(listener, SOMAXCONN) < 0) {
        perror("bind/listen");
        close(listener);
        return 1;
    }
    std::printf("Zoo server nasloucha na portu %d\n", port);
    std::fflush(stdout);

    while (true) {
        int client = accept(listener, nullptr, nullptr);
        if (client < 0 && errno == EINTR) {
            continue;
        }
        if (client < 0) {
            perror("accept");
            break;
        }

        pid_t child = fork();
        if (child == 0) {
            close(listener);
            serve_player(client);
            close(client);
            _exit(0);
        }
        if (child < 0) {
            perror("fork");
        }
        close(client);
    }

    close(listener);
    return 1;
}
