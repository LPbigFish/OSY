#include "include/zoo_lib.h"

#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::vector<std::string> map;
int player_x = -1;
int player_y = -1;
} // namespace

void map_init(int width, int height) {
    map.clear();
    if (width > 0 && height > 0) {
        map.assign(height, std::string(width, '?'));
    }
    player_x = player_y = -1;
}

void draw_map() {
    std::println("Mapa ZOO (@ = vy, ? = nezname):");
    for (size_t y = 0; y < map.size(); ++y) {
        std::string row = map[y];
        if (static_cast<int>(y) == player_y) {
            row.at(player_x) = '@';
        }
        std::println("{}", row);
    }
}

void update_map(int px, int py, const char* area_str) {
    if (map.empty() || px < 0 || py < 0 || py >= static_cast<int>(map.size())
        || px >= static_cast<int>(map.front().size()) || area_str == nullptr) {
        return;
    }
    std::string_view area{area_str};
    if (area.size() != 17) {
        return;
    }
    for (size_t i = 0; i < area.size(); ++i) {
        if (i % 2 == 0 ? area[i] < '0' || area[i] > '9' : area[i] != ',') {
            return;
        }
    }
    player_x = px;
    player_y = py;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int x = px + dx;
            int y = py + dy;
            if (x >= 0 && y >= 0 && y < static_cast<int>(map.size())
                && x < static_cast<int>(map.front().size())) {
                map[y][x] = area[((dy + 1) * 3 + dx + 1) * 2];
            }
        }
    }
}
