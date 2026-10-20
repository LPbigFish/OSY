#include <arpa/inet.h>
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
#include <string>

constexpr int MAP_SIZE = 10;
constexpr int CLIENT_TIMEOUT_MS = 60000;

static bool g_debug = false;

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

static bool send_text(int fd, const std::string& text) {
    return write_all(fd, text.data(), text.size());
}

static bool valid_word(const std::string& word) {
    return !word.empty() && word.find_first_of(" \t") == std::string::npos;
}

static bool send_info(int client, const std::string& animal) {
    int channel[2];
    if (pipe(channel) < 0) {
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
    char buffer[512];
    ssize_t count;
    while ((count = read(channel[0], buffer, sizeof(buffer))) > 0) {
        if (success) {
            success = write_all(client, buffer, static_cast<size_t>(count));
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

static bool handle_command(
    int client, const std::string& command, std::string& name, int& x, int& y
) {
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

static void serve_player(int client) {
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

        char buffer[1024];
        ssize_t count = read(client, buffer, sizeof(buffer));
        if (count <= 0) {
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0) {
                perror("read");
            }
            break;
        }
        input.append(buffer, static_cast<size_t>(count));
        if (input.size() > 4096) {
            send_text(client, "ERROR prikaz je prilis dlouhy\n");
            break;
        }

        size_t newline;
        while ((newline = input.find('\n')) != std::string::npos) {
            std::string command = input.substr(0, newline);
            input.erase(0, newline + 1);
            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }
            if (g_debug) {
                std::fprintf(stderr, "[%d] %s\n", getpid(), command.c_str());
            }
            if (!handle_command(client, command, name, x, y)) {
                return;
            }
        }
    }
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
