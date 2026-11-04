#include <arpa/inet.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <format>
#include <list>
#include <memory>
#include <poll.h>
#include <print>
#include <pthread.h>
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

constexpr int MAP_SIZE = 19;
constexpr int CLIENT_TIMEOUT_MS = 10'000;
static bool g_debug = false;

class ZooMap {
    std::array<std::array<unsigned char, MAP_SIZE>, MAP_SIZE> map{};

  public:
    ZooMap() {
        srand(time(nullptr));

        for (auto& col : map) {
            for (auto& cel : col) {
                cel = static_cast<unsigned char>(rand() % 9) + '1';
            }
        }
    }

    auto get_area(int x, int y) -> std::array<unsigned char, 9> {
        std::array<unsigned char, 9> to_send{};
        std::size_t ptr = 0;
        for (int a = y - 1; a < y + 2; a++) {
            for (int b = x - 1; b < x + 2; b++) {
                if (a < 0 || a >= MAP_SIZE || b < 0 || b >= MAP_SIZE) {
                    to_send.at(ptr++) = '0';
                } else {
                    to_send.at(ptr++) = this->map.at(a).at(b);
                }
            }
        }

        return to_send;
    }
};

namespace {
ZooMap zoo{};

struct Client {
    int socket;
    std::string name;
    int x = 0;
    int y = 0;
};

std::list<Client*> clients;

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

auto broadcast(const std::string& text, const Client* sender = nullptr)
    -> void {
    for (const Client* client : clients) {
        if (client != sender) {
            send_text(client->socket, text);
        }
    }
}

auto broadcast_chat(
    const std::string& name,
    const std::string& text,
    const Client* sender = nullptr
) -> void {
    std::string message = std::format("{}: {}\n", name, text);
    std::print(stderr, "{}", message);
    broadcast(message, sender);
}

auto position(const Client& client) -> std::string {
    return std::format("POS {} {} {}\n", client.name, client.x, client.y);
}

auto send_area(int fd, int x, int y) -> bool {
    auto area = zoo.get_area(x, y);

    std::string to_send{};

    for (const auto c : area) {
        to_send += static_cast<char>(c);
        to_send += ",";
    }

    return send_text(
        fd,
        std::format(
            "POS {} {} AREA {}\n", x, y, to_send.substr(0, to_send.length() - 1)
        )
    );
}

auto valid_word(const std::string& word) -> bool {
    return !word.empty() && word.find_first_of(" \t") == std::string::npos;
}

auto send_info(int client, const std::string& animal) -> bool {
    std::array<int, 2> channel{};
    if (pipe2(channel.data(), O_CLOEXEC) < 0) {
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
    while (true) {
        count = read(channel[0], buffer.begin(), sizeof(buffer));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            break;
        }
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

auto handle_command(Client& player, const std::string& command) -> bool {
    auto& [client, name, x, y] = player;
    if (command.starts_with("HELLO ")) {
        std::string requested = command.substr(6);
        if (!name.empty() || !valid_word(requested)) {
            return send_text(client, "ERROR neplatny prikaz HELLO\n");
        }
        name = requested;
        x = rand() % MAP_SIZE;
        y = rand() % MAP_SIZE;
        bool ok = send_text(
            client,
            "WELCOME " + name + " (" + std::to_string(x) + ","
                + std::to_string(y) + ")\n"
        );
        if (!ok || !send_area(client, x, y)) {
            return false;
        }
        for (const Client* other : clients) {
            if (other != &player && !other->name.empty()) {
                if (!send_text(client, position(*other))) {
                    return false;
                }
            }
        }
        broadcast(position(player), &player);
        return true;
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
        bool ok = send_area(client, x, y);
        broadcast(position(player), &player);
        return ok;
    }
    if (command == "MSG" || command.starts_with("MSG ")) {
        std::string text = command.size() > 4 ? command.substr(4) : "";
        if (text.find_first_not_of(" \t") == std::string::npos) {
            return send_text(client, "ERROR neplatny prikaz MSG\n");
        }
        broadcast_chat(name, text, &player);
        return true;
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

auto serve_player(void* argument) -> void* {
    std::unique_ptr<Client> player{static_cast<Client*>(argument)};
    int client = player->socket;
    clients.push_back(player.get());
    pollfd event{.fd = client, .events = POLLIN, .revents = 0};
    std::string input;
    bool connected = true;
    std::println(stderr, "[{}] klient pripojen", client);

    while (connected) {
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
        size_t newline = 0;
        while ((newline = input.find('\n')) != std::string::npos) {
            if (newline > 4096) {
                send_text(client, "ERROR prikaz je prilis dlouhy\n");
                connected = false;
                break;
            }
            std::string command = input.substr(0, newline);
            input.erase(0, newline + 1);
            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }
            if (g_debug) {
                std::println(
                    stderr,
                    "[{}] DEBUG jmeno='{}' pozice=({},{})",
                    client,
                    player->name,
                    player->x,
                    player->y
                );
            }
            if (!command.starts_with("MSG ")) {
                std::println(
                    stderr,
                    "[{}] {}: {}",
                    client,
                    player->name.empty() ? "anonym" : player->name,
                    command
                );
            }
            if (!handle_command(*player, command)) {
                connected = false;
                break;
            }
        }
        if (connected && input.size() > 4096) {
            send_text(client, "ERROR prikaz je prilis dlouhy\n");
            break;
        }
    }
    clients.remove(player.get());
    close(client);
    std::println(stderr, "[{}] klient odpojen", client);
    return nullptr;
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
        std::println(stderr, "Pouziti: {} [-d] <port>", argv[0]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    int listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
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
    std::println("Zoo server nasloucha na portu {}", port);
    std::fflush(stdout);

    std::array<pollfd, 2> events{{
      {.fd = listener, .events = POLLIN, .revents = 0},
      {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0},
    }};
    std::string console_input;
    while (true) {
        int ready = poll(events.data(), events.size(), -1);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready < 0) {
            perror("poll");
            break;
        }
        if (events[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            break;
        }
        if (events[0].revents & POLLIN) {
            int client = accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
            if (client < 0) {
                if (errno != EINTR) {
                    perror("accept");
                }
                continue;
            }
            auto* player = new Client{.socket = client};
            pthread_t thread{};
            int error = pthread_create(&thread, nullptr, serve_player, player);
            if (error != 0) {
                std::println(stderr, "pthread_create: {}", strerror(error));
                close(client);
                delete player;
            } else {
                pthread_detach(thread);
            }
        }
        if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            std::array<char, 1024> buffer{};
            ssize_t count = read(STDIN_FILENO, buffer.data(), buffer.size());
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                if (count < 0) {
                    perror("read stdin");
                }
                if (!console_input.empty()) {
                    broadcast_chat("Server", console_input);
                    console_input.clear();
                }
                events[1].fd = -1;
                continue;
            }
            console_input.append(buffer.data(), static_cast<size_t>(count));
            size_t newline = 0;
            while ((newline = console_input.find('\n')) != std::string::npos) {
                std::string text = console_input.substr(0, newline);
                console_input.erase(0, newline + 1);
                if (!text.empty() && text.back() == '\r') {
                    text.pop_back();
                }
                if (!text.empty()) {
                    broadcast_chat("Server", text);
                }
            }
        }
    }

    close(listener);
    return 1;
}
