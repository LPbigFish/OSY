#include <cstdio>
#include <cstring>

struct Animal {
    const char* key;
    const char* description;
};

constexpr Animal ANIMALS[]{
  {"mouse", "Zvíře: myš\nVýška: 5 cm\nHmotnost: 150 g\nPotrava: všežravec\n"},
  {"lion",
   "Zvíře: lev\nVýška: 120 cm\nHmotnost: 190 kg\nPotrava: masožravec\n"},
  {"elephant",
   "Zvíře: slon\nVýška: 300 cm\nHmotnost: 5000 kg\nPotrava: býložravec\n"},
  {"giraffe",
   "Zvíře: žirafa\nVýška: 500 cm\nHmotnost: 900 kg\nPotrava: býložravec\n"},
};

auto main(int argc, char** argv) -> int {
    if (argc != 2) {
        std::puts("Použití: zoo_info <animal>");
        return 1;
    }
    for (const Animal& animal : ANIMALS) {
        if (std::strcmp(argv[1], animal.key) == 0) {
            std::printf("%s", animal.description);
            return 0;
        }
    }
    std::printf("Neznámé zvíře: %s\n", argv[1]);
    return 1;
}
