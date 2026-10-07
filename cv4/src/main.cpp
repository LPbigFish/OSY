#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <print>
#include <string>
#include <vector>

// Zpráva putující rourami jako celý záznam (fixní velikost = jednoduché rámování)
constexpr int MAX_TEXT = 256;

struct Msg {
    int seq;             // pořadové číslo (vyplní proces 2)
    int line;            // číslo původního řádku (vyplní proces 1)
    int len;             // délka textu zprávy (vyplní proces 3)
    char text[MAX_TEXT]; // text zprávy
};

// Uzavře oba konce roury
static void close_pair(int p[2]) {
    close(p[0]);
    close(p[1]);
}

// Přečte přesně n bajtů; vrátí false při EOF na hranici záznamu
static bool read_full(int fd, void* buf, size_t n) {
    auto* p = static_cast<char*>(buf);
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("read");
            exit(1);
        }
        if (r == 0) {
            if (got > 0) {
                std::println(stderr, "[{}] neúplný záznam v rouře", getpid());
                exit(1);
            }
            return false; // čistý EOF – všechny zapisovací konce jsou zavřené
        }
        got += static_cast<size_t>(r);
    }
    return true;
}

// Zapíše přesně n bajtů
static void write_full(int fd, const void* buf, size_t n) {
    auto* p = static_cast<const char*>(buf);
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = write(fd, p + sent, n - sent);
        if (w < 0) {
            if (errno == EINTR) continue;
            perror("write");
            exit(1);
        }
        sent += static_cast<size_t>(w);
    }
}

// Načte soubor po řádcích (syscall read, žádné streamy)
static std::vector<std::string> read_lines(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        exit(1);
    }

    std::string data;
    char buf[4096];
    for (;;) {
        ssize_t r = read(fd, buf, sizeof buf);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("read");
            exit(1);
        }
        if (r == 0) break;
        data.append(buf, static_cast<size_t>(r));
    }
    close(fd);

    std::vector<std::string> lines;
    size_t start = 0;
    while (start < data.size()) {
        size_t end = data.find('\n', start);
        if (end == std::string::npos) end = data.size();
        if (end > start) lines.push_back(data.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

[[noreturn]] static void process_receive(const std::vector<std::string>& lines, long count,
                                         int p1[2], int p2[2], int pa[2], int pb[2], int pc[2], int ps[2]) {
    close(p1[0]);
    close_pair(p2);
    close_pair(pa);
    close_pair(pb);
    close_pair(pc);
    close_pair(ps);

    for (long i = 0; i < count; ++i) {
        size_t idx = static_cast<size_t>(rand()) % lines.size();

        Msg m{};
        m.line = static_cast<int>(idx) + 1;
        size_t n = std::min(lines[idx].size(), static_cast<size_t>(MAX_TEXT - 1));
        lines[idx].copy(m.text, n);
        m.text[n] = '\0';

        std::println("[{}] Přijímám zprávu: \"{}\" (řádek {})", getpid(), m.text, m.line);
        write_full(p1[1], &m, sizeof m);
    }

    close(p1[1]);
    exit(0);
}

[[noreturn]] static void process_number(int p1[2], int p2[2], int pa[2], int pb[2], int pc[2], int ps[2]) {
    close(p1[1]);
    close(p2[0]);
    close_pair(pa);
    close_pair(pb);
    close_pair(pc);
    close_pair(ps);

    Msg m;
    int seq = 0;
    while (read_full(p1[0], &m, sizeof m)) {
        m.seq = ++seq;
        std::println("[{}] Čísluji zprávu: {}. {} (řádek {})", getpid(), m.seq, m.text, m.line);
        write_full(p2[1], &m, sizeof m);
    }

    close(p1[0]);
    close(p2[1]);
    exit(0);
}

[[noreturn]] static void process_analyze(int p1[2], int p2[2], int pa[2], int pb[2], int pc[2], int ps[2]) {
    close_pair(p1);
    close(p2[1]);
    close(pa[0]);
    close(pb[0]);
    close_pair(pc);
    close_pair(ps);

    Msg m;
    while (read_full(p2[0], &m, sizeof m)) {
        m.len = static_cast<int>(strlen(m.text)); // délka jen z textu zprávy
        std::println("[{}] Analyzuji zprávu: {}. {} ({}) – řádek {}", getpid(), m.seq, m.text, m.len, m.line);
        write_full(m.len % 2 == 0 ? pa[1] : pb[1], &m, sizeof m);
    }

    close(p2[0]);
    close(pa[1]);
    close(pb[1]);
    exit(0);
}

[[noreturn]] static void process_branch(char id, int p1[2], int p2[2], int pa[2], int pb[2], int pc[2], int ps[2]) {
    int in = id == 'A' ? pa[0] : pb[0];
    int inw = id == 'A' ? pa[1] : pb[1];
    int oin = id == 'A' ? pb[0] : pa[0];
    int oinw = id == 'A' ? pb[1] : pa[1];

    close_pair(p1);
    close_pair(p2);
    close(inw);
    close(oin);
    close(oinw);
    close(pc[0]);
    close(ps[0]);

    Msg m;
    int n = 0;
    while (read_full(in, &m, sizeof m)) {
        for (char* c = m.text; *c != '\0'; ++c) {
            *c = static_cast<char>(id == 'A' ? tolower(static_cast<unsigned char>(*c))
                                             : toupper(static_cast<unsigned char>(*c)));
        }
        ++n;
        std::println("[{}] Větev {}: {}. {} ({}) – řádek {}", getpid(), id, m.seq, m.text, m.len, m.line);
        write_full(pc[1], &m, sizeof m);
    }

    close(in);
    close(pc[1]);
    write_full(ps[1], &n, sizeof n);
    close(ps[1]);
    exit(0);
}

auto main(int argc, char** argv) -> int {
    setvbuf(stdout, nullptr, _IOLBF, 0);

    if (argc != 3) {
        std::println(stderr, "Použití: {} <soubor se zprávami> <počet zpráv>", argv[0]);
        return 1;
    }

    long count = strtol(argv[2], nullptr, 10);
    if (count <= 0) {
        std::println(stderr, "Počet zpráv musí být kladné číslo");
        return 1;
    }

    std::vector<std::string> lines = read_lines(argv[1]);
    if (lines.empty()) {
        std::println(stderr, "Soubor neobsahuje žádné zprávy");
        return 1;
    }

    srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));

    int p1[2], p2[2], pa[2], pb[2], pc[2], ps[2];
    if (pipe(p1) < 0 || pipe(p2) < 0 || pipe(pa) < 0 || pipe(pb) < 0 || pipe(pc) < 0 || pipe(ps) < 0) {
        perror("pipe");
        return 1;
    }

    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) process_receive(lines, count, p1, p2, pa, pb, pc, ps);

    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) process_number(p1, p2, pa, pb, pc, ps);

    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) process_analyze(p1, p2, pa, pb, pc, ps);

    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) process_branch('A', p1, p2, pa, pb, pc, ps);

    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) process_branch('B', p1, p2, pa, pb, pc, ps);

    close_pair(p1);
    close_pair(p2);
    close_pair(pa);
    close_pair(pb);
    close(pc[1]);
    close(ps[1]);

    Msg m;
    while (read_full(pc[0], &m, sizeof m)) {
        std::println("[{}] Přijata zpráva: {}. {} ({}) – řádek {}", getpid(), m.seq, m.text, m.len, m.line);
    }
    close(pc[0]);

    int countA = 0, countB = 0;
    read_full(ps[0], &countA, sizeof countA);
    read_full(ps[0], &countB, sizeof countB);
    close(ps[0]);

    while (wait(nullptr) > 0) { }

    std::println("Větev A (sudá délka) zpracovala: {} zpráv", countA);
    std::println("Větev B (lichá délka) zpracovala: {} zpráv", countB);

    return 0;
}
