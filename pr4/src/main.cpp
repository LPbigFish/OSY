#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <print>
#include <string>
#include <vector>

constexpr int TEXT_SIZE = 256;

struct Message {
    int number;
    int line;
    int length;
    // ponytail: messages over 255 bytes are truncated; use length-prefixed
    // packets if long input matters.
    char text[TEXT_SIZE];
};

static void close_pipe(int pipefd[2]) {
    close(pipefd[0]);
    close(pipefd[1]);
}

// Message fits PIPE_BUF, so one write sends one complete record.
static bool read_message(int fd, Message& message) {
    ssize_t count = read(fd, &message, sizeof(message));
    if (count == 0) {
        return false;
    }
    if (count != sizeof(message)) {
        if (count < 0) {
            perror("read");
        } else {
            std::println(stderr, "Neúplná zpráva v rouře");
        }
        _exit(1);
    }
    return true;
}

static void write_message(int fd, const Message& message) {
    ssize_t count = write(fd, &message, sizeof(message));
    if (count != sizeof(message)) {
        if (count < 0) {
            perror("write");
        } else {
            std::println(stderr, "Neúplná zpráva do roury");
        }
        _exit(1);
    }
}

static std::vector<std::string> read_lines(const char* path) {
    int file = open(path, O_RDONLY);
    if (file < 0) {
        perror(path);
        _exit(1);
    }

    std::string content;
    char buffer[1024];
    ssize_t count;
    while ((count = read(file, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, static_cast<size_t>(count));
    }
    if (count < 0) {
        perror("read");
        _exit(1);
    }
    close(file);

    std::vector<std::string> lines;
    size_t start = 0;
    while (start < content.size()) {
        size_t end = content.find('\n', start);
        if (end == std::string::npos) {
            end = content.size();
        }
        lines.push_back(content.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

static int utf8_length(const char* text) {
    int length = 0;
    for (const auto* character = reinterpret_cast<const unsigned char*>(text);
         *character != '\0';
         ++character) {
        if ((*character & 0xC0) != 0x80) {
            ++length;
        }
    }
    return length;
}

[[noreturn]] static void receive_messages(
    const char* path,
    long message_count,
    int first[2],
    int second[2],
    int third[2]
) {
    close(first[0]);
    close_pipe(second);
    close_pipe(third);

    std::vector<std::string> lines = read_lines(path);
    if (lines.empty()) {
        std::println(stderr, "Soubor neobsahuje žádné zprávy");
        _exit(1);
    }

    srand(
        static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid())
    );
    for (long index = 0; index < message_count; ++index) {
        size_t selected = static_cast<size_t>(rand()) % lines.size();
        Message message{};
        message.line = static_cast<int>(selected) + 1;
        snprintf(
            message.text, sizeof(message.text), "%s", lines[selected].c_str()
        );

        std::println(
            "[PID {}] Přijímám zprávu: \"{}\" (řádek {})",
            getpid(),
            message.text,
            message.line
        );
        write_message(first[1], message);
    }

    close(first[1]);
    _exit(0);
}

[[noreturn]] static void
number_messages(int first[2], int second[2], int third[2]) {
    close(first[1]);
    close(second[0]);
    close_pipe(third);

    Message message;
    int number = 0;
    while (read_message(first[0], message)) {
        message.number = ++number;
        std::println(
            "[PID {}] Čísluji zprávu: {}. {} (řádek {})",
            getpid(),
            message.number,
            message.text,
            message.line
        );
        write_message(second[1], message);
    }

    close(first[0]);
    close(second[1]);
    _exit(0);
}

[[noreturn]] static void
analyze_messages(int first[2], int second[2], int third[2]) {
    close_pipe(first);
    close(second[1]);
    close(third[0]);

    Message message;
    while (read_message(second[0], message)) {
        message.length = utf8_length(message.text);
        std::println(
            "[PID {}] Analyzuji zprávu: {}. {} ({}) - řádek {}",
            getpid(),
            message.number,
            message.text,
            message.length,
            message.line
        );
        write_message(third[1], message);
    }

    close(second[0]);
    close(third[1]);
    _exit(0);
}

auto main(int argc, char** argv) -> int {
    setvbuf(stdout, nullptr, _IOLBF, 0);

    if (argc != 3) {
        std::println(
            stderr, "Použití: {} <soubor se zprávami> <počet zpráv>", argv[0]
        );
        return 1;
    }

    char* end = nullptr;
    errno = 0;
    long message_count = strtol(argv[2], &end, 10);
    if (errno != 0 || end == argv[2] || *end != '\0' || message_count <= 0) {
        std::println(stderr, "Počet zpráv musí být kladné celé číslo");
        return 1;
    }

    int first[2];
    int second[2];
    int third[2];
    if (pipe(first) < 0 || pipe(second) < 0 || pipe(third) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t child = fork();
    if (child == 0) {
        receive_messages(argv[1], message_count, first, second, third);
    }
    if (child < 0) {
        perror("fork");
        return 1;
    }

    child = fork();
    if (child == 0) {
        number_messages(first, second, third);
    }
    if (child < 0) {
        perror("fork");
        return 1;
    }

    child = fork();
    if (child == 0) {
        analyze_messages(first, second, third);
    }
    if (child < 0) {
        perror("fork");
        return 1;
    }

    close_pipe(first);
    close_pipe(second);
    close(third[1]);

    Message message;
    while (read_message(third[0], message)) {
        std::println(
            "[PID {}] Přijata zpráva: {}. {} ({}) - řádek {}",
            getpid(),
            message.number,
            message.text,
            message.length,
            message.line
        );
    }
    close(third[0]);

    while (wait(nullptr) > 0) {
    }
    return 0;
}
