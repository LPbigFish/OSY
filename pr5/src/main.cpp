#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <print>

static void close_pipe(int pipefd[2]) {
    close(pipefd[0]);
    close(pipefd[1]);
}

[[noreturn]] static void
generate_messages(const char* path, int first[2], int second[2], int third[2]) {
    close(first[0]);
    close_pipe(second);
    close_pipe(third);

    int input = open(path, O_RDONLY);
    if (input < 0) {
        perror(path);
        _exit(1);
    }

    char buffer[256];
    ssize_t count;
    while ((count = read(input, buffer, sizeof(buffer))) > 0) {
        if (write(first[1], buffer, count) != count) {
            perror("write");
            _exit(1);
        }
    }
    if (count < 0) {
        perror("read");
        _exit(1);
    }

    close(input);
    close(first[1]);
    _exit(0);
}

[[noreturn]] static void
sort_messages(int first[2], int second[2], int third[2]) {
    close(first[1]);
    close(second[0]);
    close_pipe(third);

    if (dup2(first[0], STDIN_FILENO) < 0
        || dup2(second[1], STDOUT_FILENO) < 0) {
        perror("dup2");
        _exit(1);
    }
    close(first[0]);
    close(second[1]);

    execlp("sort", "sort", nullptr);
    perror("sort");
    _exit(1);
}

[[noreturn]] static void
uppercase_messages(int first[2], int second[2], int third[2]) {
    close_pipe(first);
    close(second[1]);
    close(third[0]);

    if (dup2(second[0], STDIN_FILENO) < 0
        || dup2(third[1], STDOUT_FILENO) < 0) {
        perror("dup2");
        _exit(1);
    }
    close(second[0]);
    close(third[1]);

    execlp("tr", "tr", "a-z", "A-Z", nullptr);
    perror("tr");
    _exit(1);
}

[[noreturn]] static void print_statistics(
    const char* output_path, int first[2], int second[2], int third[2]
) {
    close_pipe(first);
    close_pipe(second);
    close(third[1]);

    if (output_path != nullptr) {
        int output = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output < 0) {
            perror(output_path);
            _exit(1);
        }
        if (dup2(output, STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(1);
        }
        close(output);
    }

    int messages = 0;
    int line_length = 0;
    int total_length = 0;
    bool has_line = false;
    char buffer[256];
    ssize_t count;

    while ((count = read(third[0], buffer, sizeof(buffer))) > 0) {
        for (ssize_t index = 0; index < count; ++index) {
            unsigned char character = static_cast<unsigned char>(buffer[index]);
            if (character == '\n') {
                ++messages;
                total_length += line_length;
                line_length = 0;
                has_line = false;
            } else {
                has_line = true;
                if (character != '\r' && (character & 0xC0) != 0x80) {
                    ++line_length;
                }
            }
        }
    }
    if (count < 0) {
        perror("read");
        _exit(1);
    }
    if (has_line) {
        ++messages;
        total_length += line_length;
    }

    int average = messages == 0 ? 0 : total_length / messages;
    std::println(
        "Chat 4: prijato {} zprav, prumerna delka {} znaku", messages, average
    );
    close(third[0]);
    _exit(0);
}

auto main(int argc, char** argv) -> int {
    setvbuf(stdout, nullptr, _IOLBF, 0);

    if (argc != 2 && argc != 3) {
        std::println(stderr, "Použití: {} <data.txt> [out.txt]", argv[0]);
        return 1;
    }

    int first[2];
    int second[2];
    int third[2];
    if (pipe(first) < 0 || pipe(second) < 0 || pipe(third) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t children[4];

    children[0] = fork();
    if (children[0] == 0) {
        generate_messages(argv[1], first, second, third);
    }
    if (children[0] < 0) {
        perror("fork");
        return 1;
    }

    children[1] = fork();
    if (children[1] == 0) {
        sort_messages(first, second, third);
    }
    if (children[1] < 0) {
        perror("fork");
        return 1;
    }

    children[2] = fork();
    if (children[2] == 0) {
        uppercase_messages(first, second, third);
    }
    if (children[2] < 0) {
        perror("fork");
        return 1;
    }

    children[3] = fork();
    if (children[3] == 0) {
        print_statistics(argc == 3 ? argv[2] : nullptr, first, second, third);
    }
    if (children[3] < 0) {
        perror("fork");
        return 1;
    }

    close_pipe(first);
    close_pipe(second);
    close_pipe(third);

    while (wait(nullptr) > 0) {
    }
    for (int index = 0; index < 4; ++index) {
        std::println("Chat {} skoncil (PID {})", index + 1, children[index]);
    }
    return 0;
}
