#ifndef ARENA_H
#define ARENA_H

#include <vector>
#include <string>
#include "RobotBase.h"
#include "RadarObj.h"

// Arena class
class Arena {
public:
    Arena();
    ~Arena();

    // Main game loop
    void run(const std::string& config_file);

    // Board management
    bool is_valid_position(int row, int col) const;
    char get_cell(int row, int col) const;
    void set_cell(int row, int col, char c);
    void print_arena() const;
    void place_obstacles();
    void load_robots();
    int count_living_robots() const;

    // Configuration
    void load_config(const std::string& config_file);

private:

    // Game mechanics
    bool check_winner() const;

    // Radar and combat
    std::vector<RadarObj> scan_radar(int r, int c, int direction) const;
    void handle_movement(RobotBase* robot, int dir, int dist);
    void handle_shot(RobotBase* shooter, int target_r, int target_c);

    int m_height;
    int m_width;

    int m_num_mounds;
    int m_num_pits;
    int m_num_flamethrowers;

    int m_current_round;
    int m_max_rounds;
    bool m_watch_live;

    std::vector<std::vector<char>> m_board;

    std::vector<RobotBase*> m_robots;
    std::vector<void*> m_robot_handles;
    std::vector<char> m_robot_characters;

    // Directions for movement/radar (1-8)
    const std::pair<int,int> directions[9] = {
        {0,0},   // unused
        {-1,0},  // N
        {-1,1},  // NE
        {0,1},   // E
        {1,1},   // SE
        {1,0},   // S
        {1,-1},  // SW
        {0,-1},  // W
        {-1,-1}  // NW
    };
};

// Robot factory function type
typedef RobotBase* (*RobotFactory)();

#endif // ARENA_H
