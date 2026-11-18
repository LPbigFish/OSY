#include "utils.hpp"
#include <array>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <netdb.h>
#include <print>
#include <pthread.h>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

using std::println;

namespace {
// NOLINTNEXTLINE
bool debug = false;

auto kuchar_thread(void* arg) -> void* {
    srand(
        static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid())
    );

    std::unique_ptr<utils::ClientInfo> server{
      static_cast<utils::ClientInfo*>(arg)
    };

    std::println("Pocet jidel:");
    std::fflush(stdout);
    std::string input;
    if (!utils::read_line(STDIN_FILENO, input)) {
        return nullptr;
    }
    char* end = nullptr;
    errno = 0;
    long pocet = strtol(input.c_str(), &end, 10);
    if (errno != 0 || end == input.c_str() || *end != '\0'
        || pocet < 0 || pocet > INT_MAX) {
        std::println(stderr, "Neplatny pocet jidel");
        return nullptr;
    }

    std::string response;
    for (int i = 0; i < pocet; i++) {
        std::string rand_jidlo = utils::JIDLA.at(rand() % utils::JIDLA.size());
        if (!utils::send_text(server->socket, rand_jidlo + '\n')
            || !utils::read_line(server->socket, response)
            || response != "OK") {
            std::println(stderr, "Chyba pri predavani jidla");
            return nullptr;
        }
    }

    if (!utils::send_text(server->socket, "QUIT\n")) {
        std::println(stderr, "Chyba pri ukonceni komunikace");
    }
    return nullptr;
}

auto host_thread(void* arg) -> void* {
    std::unique_ptr<utils::ClientInfo> server{
      static_cast<utils::ClientInfo*>(arg)
    };

    std::string jidlo;
    while (utils::read_line(server->socket, jidlo)) {
        std::println("{}", jidlo);
        std::fflush(stdout);
        if (!utils::send_text(server->socket, "OK\n")) {
            break;
        }
    }
    return nullptr;
}

auto bootstrap_client(const char* host, int port) -> int {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    std::array<char, 6> service{};
    std::snprintf(service.data(), sizeof(service), "%d", port);
    int result = getaddrinfo(host, service.data(), &hints, &addresses);
    if (result != 0) {
        std::println(stderr, "getaddrinfo: {}", gai_strerror(result));
        return -1;
    }

    int server = -1;
    for (auto* address = addresses; address != nullptr;
         address = address->ai_next) {
        server = socket(
            address->ai_family, address->ai_socktype, address->ai_protocol
        );
        if (server < 0) {
            continue;
        }
        if (connect(server, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        close(server);
        server = -1;
    }
    if (server < 0) {
        perror("socket/connect");
    }
    freeaddrinfo(addresses);
    return server;
}
} // namespace

auto main(int argc, char** argv) -> int {
    const char* host = nullptr;
    int port = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == "-d") {
            debug = true;
        } else if (host == nullptr) {
            host = argv[index];
        } else {
            port = utils::parse_port(argv[index]);
        }
    }
    if (host == nullptr || port == 0) {
        println(stderr, "Pouziti: {} [-d] <IP_adresa_serveru> <port>", argv[0]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    int server = bootstrap_client(host, port);
    if (server < 0) {
        return 1;
    }
    if (debug) {
        std::println(stderr, "Pripojeno k {}:{}", host, port);
    }

    std::string prompt;
    if (!utils::read_line(server, prompt)) {
        close(server);
        return 1;
    }
    std::println("{}", prompt);
    std::fflush(stdout);

    std::string name;
    if (!utils::read_line(STDIN_FILENO, name)) {
        close(server);
        return 1;
    }

    bool kuchar = false;
    if (name == "kuchar") {
        kuchar = true;
    } else if (name != "navstevnik") {
        println(stderr, "Špatná role, KONEC!!!");
        close(server);
        return 1;
    }
    if (!utils::send_text(server, name + '\n')) {
        close(server);
        return 1;
    }

    // NOLINTNEXTLINE
    auto* player = new utils::ClientInfo{
      .socket = server,
      .role = kuchar ? utils::Role::Cook : utils::Role::Host,
    };

    pthread_t thread{};
    int error = pthread_create(
        &thread, nullptr, kuchar ? kuchar_thread : host_thread, player
    );
    if (error != 0) {
        std::println(stderr, "pthread_create: {}", strerror(error));
        close(server);

        // NOLINTNEXTLINE
        delete player;
        return 1;
    }

    error = pthread_join(thread, nullptr);
    if (error != 0) {
        std::println(stderr, "pthread_join: {}", strerror(error));
    }
    close(server);
    return error == 0 ? 0 : 1;
}
