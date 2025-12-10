#include "Arena.h"
#include <iostream>

int main() {
    // Create an arena (default constructor sets 10x10, will override in config)
    Arena arena;

    // Load configuration from a file (sets size, obstacles, max rounds, live mode)
    std::string config_file = "arena_config.txt"; // make sure this exists
    arena.load_config(config_file);

    // Place obstacles based on config
    arena.place_obstacles();

    // Load robots dynamically from Robot_*.cpp files
    arena.load_robots();

    // Check if any robots were loaded
    if (arena.count_living_robots() == 0) {
        std::cout << "[Main] No robots loaded. Exiting.\n";
        return 0;
    }

    // Run the game (uses m_max_rounds and m_watch_live from config)
    arena.run(config_file);

    return 0;
}
