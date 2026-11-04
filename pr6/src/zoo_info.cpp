#include <cstring>
#include <print>

struct Animal {
    const char* key;
    const char* description;
};

constexpr Animal ANIMALS[]{
  {.key="mouse", .description="Zvíře: myš\nVýška: 5 cm\nHmotnost: 150 g\nPotrava: všežravec\n"},
  {.key="lion",
   .description="Zvíře: lev\nVýška: 120 cm\nHmotnost: 190 kg\nPotrava: masožravec\n"},
  {.key="elephant",
   .description="Zvíře: slon\nVýška: 300 cm\nHmotnost: 5000 kg\nPotrava: býložravec\n"},
  {.key="giraffe",
   .description="Zvíře: žirafa\nVýška: 500 cm\nHmotnost: 900 kg\nPotrava: býložravec\n"},
};

auto main(int argc, char** argv) -> int {
    if (argc != 2) {

        std::println("Použití: zoo_info <animal>");
        return 1;
    }
    for (const Animal& animal : ANIMALS) {
        if (std::strcmp(argv[1], animal.key) == 0) {
            std::println("{}", animal.description);
            return 0;
        }
    }
    std::println("Neznámé zvíře: {}\n", argv[1]);
    return 1;
}
