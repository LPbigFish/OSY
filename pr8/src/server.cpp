#include <arpa/inet.h>
#include <array>
#include <cstdint>
#include <poll.h>
#include <print>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <csignal>
#include <cstdio>
#include <cstring>

#include "semafor.hpp"
#include "utils.hpp"

using std::println;

namespace {
// NOLINTNEXTLINE
bool g_debug = false;

// NOLINTNEXTLINE
sem::SharedState s_state{};
// NOLINTNEXTLINE
volatile sig_atomic_t s_running = 1;

// NOLINTNEXTLINE
int s_client = -1;
// NOLINTNEXTLINE
size_t s_item = 0;

void handle_signal(int number) {
    if (number != SIGCHLD) {
        s_running = 0;
    }
}

auto bootstrap_signals() -> bool {
    struct sigaction action{};
    action.sa_handler = handle_signal;
    action.sa_flags = SA_NOCLDSTOP;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, nullptr) < 0
        || sigaction(SIGTERM, &action, nullptr) < 0
        || sigaction(SIGCHLD, &action, nullptr) < 0) {
        perror("sigaction");
        return false;
    }
    signal(SIGPIPE, SIG_IGN);
    return true;
}

void reap_children(std::vector<pid_t>& children) {
    pid_t child = 0;
    while ((child = waitpid(-1, nullptr, WNOHANG)) > 0) {
        std::erase(children, child);
    }
}

void producer(char* jidlo) {
    sem::down(s_state.empty);
    sem::down(s_state.mutex);

    auto* buffer = s_state.buffer;

    // NOLINTNEXTLINE
    std::strcpy(buffer->data[buffer->tail], jidlo);
    buffer->tail = (buffer->tail + 1) % sem::N;
    ++buffer->size;
    ++s_item;
    if (g_debug) {
        println(
            stderr,
            "[{}] kuchar, jidlo {}: {}, pult {}/{}",
            s_client,
            s_item,
            jidlo,
            buffer->size,
            sem::N
        );
    }

    sem::up(s_state.mutex);
    sem::up(s_state.full);
}

void consumer(char* jidlo_out) {
    sem::down(s_state.full);
    sem::down(s_state.mutex);

    auto* buffer = s_state.buffer;

    // NOLINTNEXTLINE
    std::strcpy(jidlo_out, buffer->data[buffer->head]);
    buffer->head = (buffer->head + 1) % sem::N;
    --buffer->size;
    ++s_item;
    if (g_debug) {
        println(
            stderr,
            "[{}] navstevnik, jidlo {}: {}, pult {}/{}",
            s_client,
            s_item,
            jidlo_out,
            buffer->size,
            sem::N
        );
    }

    sem::up(s_state.mutex);
    sem::up(s_state.empty);
}

void handle_kuchar(int client) {
    std::string command;
    while (utils::read_line(client, command)) {
        if (command == "QUIT") {
            break;
        }
        if (command.size() > sem::MAX_JIDLO
            || command.find('\0') != std::string::npos) {
            utils::send_text(client, "ERROR neplatne jidlo\n");
            break;
        }
        producer(command.data());
        if (!utils::send_text(client, "OK\n")) {
            break;
        }
    }
}

void handle_navstevnik(int client) {
    std::array<char, sem::MAX_JIDLO + 1> jidlo{};
    std::string command;
    while (true) {
        consumer(jidlo.data());
        if (!utils::send_text(client, std::string{jidlo.data()} + '\n')) {
            break;
        }
        if (!utils::read_line(client, command) || command != "OK") {
            break;
        }
    }
}

void handle_client(int client) {
    s_client = getpid();
    println(stderr, "[{}] klient pripojen", s_client);

    std::string role;
    if (utils::send_text(client, "Role?\n") && utils::read_line(client, role)) {
        if (role == "kuchar") {
            handle_kuchar(client);
        } else if (role == "navstevnik") {
            handle_navstevnik(client);
        } else {
            utils::send_text(client, "ERROR neplatna role\n");
        }
    }

    close(client);
    println(stderr, "[{}] klient odpojen", s_client);
}

auto bootstrap_server(int port) -> int {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return -1;
    }
    int reuse = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
        < 0) {
        perror("setsockopt");
        close(listener);
        return -1;
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
        return -1;
    }
    return listener;
}
} // namespace

auto main(int argc, char** argv) -> int {
    int port = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == "-d") {
            g_debug = true;
        } else {
            port = utils::parse_port(argv[index]);
        }
    }
    if (port == 0) {
        std::println(stderr, "Pouziti: {} [-d] <port>", argv[0]);
        return 1;
    }

    if (!bootstrap_signals()) {
        return 1;
    }

    int listener = bootstrap_server(port);
    if (listener < 0) {
        return 1;
    }

    // MARK: SEMAFORY

    if (!sem::init(&s_state, port)) {
        close(listener);
        return 1;
    }

    std::println("Restaurace server nasloucha na portu {}", port);
    std::fflush(stdout);

    pollfd event{.fd = listener, .events = POLLIN, .revents = 0};
    std::vector<pid_t> children;
    int result = 0;

    sigset_t blocked{};
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGCHLD);

    // MARK: LOOP

    while (s_running) {
        reap_children(children);
        // The timeout also covers signals arriving just before poll().
        int ready = poll(&event, 1, 250);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready < 0) {
            perror("poll");
            result = 1;
            break;
        }
        if (event.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            result = 1;
            break;
        }
        if (event.revents & POLLIN) {
            int client = accept(listener, nullptr, nullptr);
            if (client < 0 && errno == EINTR) {
                continue;
            }
            if (client < 0) {
                perror("accept");
                result = 1;
                break;
            }

            sigset_t previous{};
            if (sigprocmask(SIG_BLOCK, &blocked, &previous) < 0) {
                perror("sigprocmask");
                close(client);
                result = 1;
                break;
            }

            pid_t child = fork();
            if (child == 0) {
                close(listener);
                signal(SIGINT, SIG_DFL);
                signal(SIGTERM, SIG_DFL);
                signal(SIGCHLD, SIG_DFL);
                sigprocmask(SIG_SETMASK, &previous, nullptr);

                handle_client(client);
                sem::release(&s_state);
                std::fflush(stdout);
                std::fflush(stderr);
                _exit(0);
            }
            if (child < 0) {
                perror("fork");
            } else {
                children.push_back(child);
            }
            close(client);
            sigprocmask(SIG_SETMASK, &previous, nullptr);
        }
    }

    for (pid_t child : children) {
        kill(child, SIGTERM);
    }
    for (pid_t child : children) {
        while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
        }
    }
    sem::release(&s_state);
    if (!sem::unlink_names(&s_state)) {
        result = 1;
    }
    close(listener);
    return result;
}
