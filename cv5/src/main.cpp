#include <array>
#include <cctype>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <print>

using Pipe = std::array<int, 2>;
using Buffer = std::array<char, 256>;

namespace {
void close_pipe(Pipe pipefd) {
    close(pipefd[0]);
    close(pipefd[1]);
}

[[noreturn]] void generate_messages(
    const char* path,
    Pipe first,
    Pipe second,
    Pipe third,
    Pipe forth,
    Pipe fifth
) {
    close(first[0]);
    close_pipe(second);
    close_pipe(third);
    close_pipe(forth);
    close_pipe(fifth);

    // NOLINTNEXTLINE
    int input = open(path, O_RDONLY);
    if (input < 0) {
        perror(path);
        _exit(1);
    }

    Buffer buffer{};
    ssize_t count{};
    while ((count = read(input, buffer.begin(), buffer.size())) > 0) {
        if (write(first[1], buffer.begin(), count) != count) {
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

[[noreturn]] void
sort_messages(Pipe first, Pipe second, Pipe third, Pipe forth, Pipe fifth) {
    close(first[1]);
    close(second[0]);
    close_pipe(third);
    close_pipe(forth);
    close_pipe(fifth);

    if (dup2(first[0], STDIN_FILENO) < 0
        || dup2(second[1], STDOUT_FILENO) < 0) {
        perror("dup2");
        _exit(1);
    }
    close(first[0]);
    close(second[1]);

    // NOLINTNEXTLINE
    execlp("sort", "sort", nullptr);
    perror("sort");
    _exit(1);
}

[[noreturn]] void filter_length(
    const std::string& min_len,
    const std::string& max_len,
    Pipe first,
    Pipe second,
    Pipe third,
    Pipe forth,
    Pipe fifth
) {
    close_pipe(first);
    close(second[1]);
    close(third[0]);
    close_pipe(forth);
    close_pipe(fifth);

    if (dup2(second[0], STDIN_FILENO) < 0
        || dup2(third[1], STDOUT_FILENO) < 0) {
        perror("dup2");
        _exit(1);
    }

    close(second[0]);
    close(third[1]);

    std::string filter_str
        = "length($0) > " + min_len + " && length($0) < " + max_len;
        
    // NOLINTNEXTLINE
    execlp("awk", "awk", filter_str.c_str(), nullptr);
    perror("execlp");
    _exit(1);
}

[[noreturn]] void uppercase_messages(
    Pipe first, Pipe second, Pipe third, Pipe forth, Pipe fifth
) {
    close_pipe(first);
    close_pipe(second);
    close(third[1]);
    close(forth[0]);
    close_pipe(fifth);

    if (dup2(third[0], STDIN_FILENO) < 0
        || dup2(forth[1], STDOUT_FILENO) < 0) {
        perror("dup2");
        _exit(1);
    }
    close(third[0]);
    close(forth[1]);

    // NOLINTNEXTLINE
    execlp("tr", "tr", "a-z", "A-Z", nullptr);
    perror("tr");
    _exit(1);
}

[[noreturn]] void grep_filter(
    const std::string& filter,
    Pipe first,
    Pipe second,
    Pipe third,
    Pipe forth,
    Pipe fifth
) {
    close_pipe(first);
    close_pipe(second);
    close_pipe(third);
    close(forth[1]);
    close(fifth[0]);

    if (dup2(forth[0], STDIN_FILENO) < 0
        || dup2(fifth[1], STDOUT_FILENO) < 0) {
        perror("dup2");
        _exit(1);
    }
    close(forth[0]);
    close(fifth[1]);

    // NOLINTNEXTLINE
    execlp("grep", "grep", "-i", filter.c_str(), nullptr);
    perror("grep");
    _exit(1);
}

auto to_upper(std::string_view text) -> std::string {
    std::string upper;
    upper.reserve(text.size());
    for (char character : text) {
        upper.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(character))
        ));
    }
    return upper;
}

[[noreturn]] void print_statistics(
    const std::string& filter,
    const char* output_path,
    Pipe first,
    Pipe second,
    Pipe third,
    Pipe forth,
    Pipe fifth
) {
    close_pipe(first);
    close_pipe(second);
    close_pipe(third);
    close_pipe(forth);
    close(fifth[1]);

    if (output_path != nullptr) {
        // NOLINTNEXTLINE
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

    const std::string filter_upper = to_upper(filter);
    int messages = 0;
    int matching = 0;
    int line_length = 0;
    int total_length = 0;
    bool has_line = false;
    std::string line;
    Buffer buffer{};
    ssize_t count{};

    auto finish_message = [&]() -> void {
        ++messages;
        total_length += line_length;
        if (!filter_upper.empty()
            && to_upper(line).find(filter_upper) != std::string::npos) {
            ++matching;
        }
        line.clear();
        line_length = 0;
        has_line = false;
    };

    while ((count = read(fifth[0], buffer.begin(), buffer.size())) > 0) {
        for (ssize_t index = 0; index < count; ++index) {
            auto character = static_cast<unsigned char>(buffer.at(index));
            if (character == '\n') {
                finish_message();
            } else {
                has_line = true;
                line.push_back(static_cast<char>(character));
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
        finish_message();
    }

    int average = messages == 0 ? 0 : total_length / messages;
    if (filter_upper.empty()) {
        std::println(
            "Chat 4: prijato {} zprav, prumerna delka {} znaku", messages, average
        );
    } else {
        std::println(
            "Chat 4: prijato {} zprav, prumerna delka {} znaku, {} zprav"
            " obsahuje {}",
            messages,
            average,
            matching,
            filter_upper
        );
    }
    _exit(0);
}
} // namespace

auto main(int argc, char** argv) -> int {
    setvbuf(stdout, nullptr, _IOLBF, 0);

    std::string min_len = "0";
    std::string max_len = "999999";
    std::string filter{};
    std::string output_file{};

    if (argc < 2) {
        std::println(
            stderr,
            "Použití: {} <data.txt> [-o out.txt] [min_len max_len] [filtr]",
            argv[0]
        );
        return 1;
    }

    for (int index = 2; index < argc; ++index) {
        std::string_view argument{argv[index]};
        if (argument == "-o") {
            if (++index >= argc) {
                std::println(stderr, "Chybí název souboru po -o");
                return 1;
            }
            output_file = argv[index];
        } else if (!argument.empty()
                   && std::isdigit(static_cast<unsigned char>(argument.front()))
                          != 0) {
            if (index + 1 >= argc) {
                std::println(stderr, "min_len potřebuje i max_len");
                return 1;
            }
            min_len = argv[index];
            max_len = argv[++index];
        } else {
            filter = argument;
        }
    }

    Pipe first;
    Pipe second;
    Pipe third;
    Pipe forth;
    Pipe fifth;
    if (pipe(first.data()) < 0 || pipe(second.data()) < 0
        || pipe(third.data()) < 0 || pipe(forth.data()) < 0
        || pipe(fifth.data()) < 0) {
        perror("pipe");
        return 1;
    }

    std::array<pid_t, 6> children{};

    children[0] = fork();
    if (children[0] == 0) {
        generate_messages(argv[1], first, second, third, forth, fifth);
    }
    if (children[0] < 0) {
        perror("fork");
        return 1;
    }

    children[1] = fork();
    if (children[1] == 0) {
        sort_messages(first, second, third, forth, fifth);
    }
    if (children[1] < 0) {
        perror("fork");
        return 1;
    }

    // tu $ awk 'length($0) > min_len && length($0) < max_len'
    children[2] = fork();
    if (children[2] == 0) {
        filter_length(min_len, max_len, first, second, third, forth, fifth);
    }
    if (children[2] < 0) {
        perror("fork");
        return 1;
    }

    children[3] = fork();
    if (children[3] == 0) {
        uppercase_messages(first, second, third, forth, fifth);
    }
    if (children[3] < 0) {
        perror("fork");
        return 1;
    }

    // tu GREP FILTER
    children[4] = fork();
    if (children[4] == 0) {
        grep_filter(filter, first, second, third, forth, fifth);
    }
    if (children[4] < 0) {
        perror("fork");
        return 1;
    }

    children[5] = fork();
    if (children[5] == 0) {
        print_statistics(
            filter,
            output_file.empty() ? nullptr : output_file.c_str(),
            first,
            second,
            third,
            forth,
            fifth
        );
    }
    if (children[5] < 0) {
        perror("fork");
        return 1;
    }

    close_pipe(first);
    close_pipe(second);
    close_pipe(third);
    close_pipe(forth);
    close_pipe(fifth);

    while (wait(nullptr) > 0) {
    }
    for (int index = 0; index < 6; ++index) {
        std::println("Chat {} skoncil (PID {})", index + 1, children.at(index));
    }
    return 0;
}
