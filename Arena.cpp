#include "Arena.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <cmath>
#include <filesystem>
#include <dlfcn.h>
#include <unistd.h>
#include <set>

Arena::Arena() {
    m_current_round = 0;
    m_max_rounds = 100;
    m_watch_live = false;

    m_height = 10;
    m_width = 10;

    m_num_flamethrowers = 0;
    m_num_mounds = 0;
    m_num_pits = 0;

    m_board.resize(m_height, std::vector<char>(m_width, '.'));
}

Arena::~Arena() {
    for (RobotBase* r : m_robots)
        delete r;

    for (void* h : m_robot_handles)
        dlclose(h);
}

bool Arena::is_valid_position(int row, int col) const {
    return row >= 0 && row < m_height && col >= 0 && col < m_width;
}

char Arena::get_cell(int row, int col) const {
    if (!is_valid_position(row, col))
        return '?';
    return m_board[row][col];
}

void Arena::set_cell(int row, int col, char c) {
    if (is_valid_position(row, col))
        m_board[row][col] = c;
}

void Arena::load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Could not open config file: " << config_file << std::endl;
        return;
    }

    std::string key;
    int value;
    while (file >> key >> value) {
        if (key == "width") m_width = value;
        else if (key == "height") m_height = value;
        else if (key == "mounds") m_num_mounds = value;
        else if (key == "pits") m_num_pits = value;
        else if (key == "flamethrowers") m_num_flamethrowers = value;
        else if (key == "max_rounds") m_max_rounds = value;
        else if (key == "watch_live") m_watch_live = (value != 0);
    }

    file.close();
    m_board.assign(m_height, std::vector<char>(m_width, '.'));
}

void Arena::print_arena() const {
    std::cout << "=========== Round " << m_current_round << " ===========" << std::endl;
    for (int row = 0; row < m_height; ++row) {
        for (int col = 0; col < m_width; ++col) {
            bool printed = false;
            for (size_t i = 0; i < m_robots.size(); ++i) {
                int r, c;
                m_robots[i]->get_current_location(r, c);
                if (r == row && c == col && m_robots[i]->get_health() > 0) {
                    std::cout << m_robots[i]->m_character;
                    printed = true;
                    break;
                }
            }
            if (!printed) std::cout << m_board[row][col];
            std::cout << ' ';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void Arena::place_obstacles() {
    auto place = [&](int count, char symbol) {
        for (int i = 0; i < count; ++i) {
            int r, c;
            do {
                r = rand() % m_height;
                c = rand() % m_width;
            } while (get_cell(r, c) != '.');
            set_cell(r, c, symbol);
        }
    };

    place(m_num_mounds, 'M');
    place(m_num_pits, 'P');
    place(m_num_flamethrowers, 'F');
}

void Arena::load_robots() {
    // Delete old robots and close handles if any
    for (RobotBase* r : m_robots) delete r;
    for (void* h : m_robot_handles) dlclose(h);
    m_robots.clear();
    m_robot_handles.clear();

    std::vector<std::string> robot_files = {
        "Robot_Ratboy.so",
        "Robot_Flame_e_o.so"
    };

    std::vector<char> available_chars = {'@', '$', '#', '%', '&', '!', '^', '*', '~', '+'};
    int char_index = 0;

    for (const auto& filename : robot_files) {
        void* handle = dlopen(("./" + filename).c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "dlopen failed for " << filename << std::endl;
            continue;
        }

        RobotFactory factory = (RobotFactory)dlsym(handle, "create_robot");
        if (!factory) {
            std::cerr << "Missing create_robot in " << filename << std::endl;
            dlclose(handle);
            continue;
        }

        RobotBase* robot = factory();
        if (!robot) {
            dlclose(handle);
            continue;
        }

        robot->set_boundaries(m_height, m_width);
        robot->m_character = available_chars[char_index++ % available_chars.size()];
        robot->m_name = filename.substr(6, filename.size() - 9);

        int r, c;
        do {
            r = rand() % m_height;
            c = rand() % m_width;
        } while (get_cell(r, c) != '.');
        robot->move_to(r, c);
        set_cell(r, c, robot->m_character);

        m_robots.push_back(robot);
        m_robot_handles.push_back(handle);
    }
}

int Arena::count_living_robots() const {
    int count = 0;
    for (RobotBase* r : m_robots)
        if (r->get_health() > 0) count++;
    return count;
}

bool Arena::check_winner() const {
    return count_living_robots() == 1;
}

std::vector<RadarObj> Arena::scan_radar(int r, int c, int direction) const {
    std::vector<RadarObj> results;
    int dr = 0, dc = 0;

    if (direction >= 1 && direction <= 8) {
        dr = directions[direction].first;
        dc = directions[direction].second;
    }

    int max_dist = std::max(m_height, m_width);
    for (int d = 1; d <= max_dist; ++d) {
        int check_r = r + dr * d;
        int check_c = c + dc * d;
        if (!is_valid_position(check_r, check_c)) break;

        bool found = false;
        for (RobotBase* rob : m_robots) {
            int rr, cc;
            rob->get_current_location(rr, cc);
            if (rr == check_r && cc == check_c) {
                results.emplace_back(rob->get_health() > 0 ? 'R' : 'X', rr, cc);
                found = true;
                break;
            }
        }
        if (!found) {
            char cell = get_cell(check_r, check_c);
            if (cell != '.') results.emplace_back(cell, check_r, check_c);
        }
    }
    return results;
}

void Arena::handle_movement(RobotBase* robot, int dir, int dist) {
    int r, c;
    robot->get_current_location(r, c);
    int dr = directions[dir].first;
    int dc = directions[dir].second;

    for (int step = 0; step < dist; ++step) {
        int nr = r + dr;
        int nc = c + dc;

        if (!is_valid_position(nr, nc)) break;

        char next_cell = get_cell(nr, nc);

        if (next_cell == '.') {
            set_cell(r, c, '.');
            r = nr; c = nc;
            set_cell(r, c, robot->m_character);
        }
        else if (next_cell == 'M') break; // stop at mound
        else if (next_cell == 'P') {
            set_cell(r, c, '.'); r = nr; c = nc;
            set_cell(r, c, robot->m_character);
            robot->disable_movement();
            break;
        }
        else if (next_cell == 'F') {
            set_cell(r, c, '.');
            r = nr; c = nc;
            set_cell(r, c, robot->m_character);
            int dmg = rand() % 21 + 30;
            robot->take_damage(dmg);
            robot->reduce_armor(1);
        }
        else break; // blocked by robot or unknown obstacle
    }
    robot->move_to(r, c);
}

void Arena::handle_shot(RobotBase* shooter, int target_r, int target_c) {
    WeaponType weapon = shooter->get_weapon();
    int s_r, s_c;
    shooter->get_current_location(s_r, s_c);

    if (weapon == railgun) {
        double delta_r = target_r - s_r;
        double delta_c = target_c - s_c;
        int steps = std::max(std::abs((int)delta_r), std::abs((int)delta_c));
        double step_r = delta_r / steps;
        double step_c = delta_c / steps;

        double cur_r = s_r, cur_c = s_c;
        for (int i = 0; i <= steps * 3; ++i) {
            cur_r += step_r; cur_c += step_c;
            int rr = std::round(cur_r), cc = std::round(cur_c);
            if (!is_valid_position(rr, cc)) break;

            for (RobotBase* rob : m_robots) {
                int r, c;
                rob->get_current_location(r, c);
                if (r == rr && c == cc && rob != shooter && rob->get_health() > 0) {
                    int dmg = rand() % 11 + 10;
                    rob->take_damage(dmg);
                    rob->reduce_armor(1);
                    if (rob->get_health() <= 0) set_cell(r, c, 'X');
                    break;
                }
            }
        }
    }
    else if (weapon == hammer) {
        for (RobotBase* rob : m_robots) {
            int r, c;
            rob->get_current_location(r, c);
            if (r == target_r && c == target_c && rob->get_health() > 0) {
                int dmg = rand() % 11 + 50;
                rob->take_damage(dmg);
                rob->reduce_armor(1);
                if (rob->get_health() <= 0) set_cell(r, c, 'X');
                break;
            }
        }
    }
    else if (weapon == grenade && shooter->get_grenades() > 0) {
        shooter->decrement_grenades();
        for (int rr = target_r - 1; rr <= target_r + 1; ++rr) {
            for (int cc = target_c - 1; cc <= target_c + 1; ++cc) {
                if (!is_valid_position(rr, cc)) continue;
                for (RobotBase* rob : m_robots) {
                    int r, c;
                    rob->get_current_location(r, c);
                    if (r == rr && c == cc && rob->get_health() > 0) {
                        int dmg = rand() % 31 + 10;
                        rob->take_damage(dmg);
                        rob->reduce_armor(1);
                        if (rob->get_health() <= 0) set_cell(r, c, 'X');
                        break;
                    }
                }
            }
        }
    }
    else if (weapon == flamethrower) {
        int dr = target_r - s_r;
        int dc = target_c - s_c;
        if (dr != 0) dr /= abs(dr);
        if (dc != 0) dc /= abs(dc);
        int perp_r = -dc, perp_c = dr;

        for (int depth = 1; depth <= 4; ++depth) {
            for (int width = -1; width <= 1; ++width) {
                int rr = s_r + dr * depth + perp_r * width;
                int cc = s_c + dc * depth + perp_c * width;
                if (!is_valid_position(rr, cc)) continue;
                for (RobotBase* rob : m_robots) {
                    int r, c;
                    rob->get_current_location(r, c);
                    if (r == rr && c == cc && rob->get_health() > 0) {
                        int dmg = rand() % 21 + 30;
                        rob->take_damage(dmg);
                        rob->reduce_armor(1);
                        if (rob->get_health() <= 0) set_cell(r, c, 'X');
                        break;
                    }
                }
            }
        }
    }
}

void Arena::run(const std::string& config_file) {
    srand(time(nullptr));

    // Load config
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[Arena] Config file '" << config_file 
                  << "' not found. Using default settings." << std::endl;
    } else {
        load_config(config_file);
    }

    // Place obstacles and load robots
    place_obstacles();
    load_robots();

    // Initialize robot characters vector
    m_robot_characters.clear();
    for (RobotBase* robot : m_robots)
        m_robot_characters.push_back(robot->m_character);

    m_current_round = 0;

    while (m_current_round <= m_max_rounds) {
        // Print arena grid
        print_arena();

        // If no robots loaded, exit gracefully
        if (m_robots.empty()) {
            std::cout << "[Arena] No robots loaded. Exiting." << std::endl;
            break;
        }

        for (size_t i = 0; i < m_robots.size(); ++i) {
            RobotBase* robot = m_robots[i];
            char robot_char = m_robot_characters[i];

            int cur_r, cur_c;
            robot->get_current_location(cur_r, cur_c);

            if (robot->get_health() <= 0) {
                std::cout << robot->m_name << " " << robot_char 
                          << " (" << cur_r << "," << cur_c << ") - is out\n" << std::endl;
                continue;
            }

            // Robot stats
            std::cout << robot->m_name << " " << robot_char 
                      << " (" << cur_r << "," << cur_c << ") "
                      << "Health: " << robot->get_health() 
                      << " Armor: " << robot->get_armor() << std::endl;

            // Radar
            int radar_dir = 0;
            robot->get_radar_direction(radar_dir);
            std::vector<RadarObj> radar_results = scan_radar(cur_r, cur_c, radar_dir);
            robot->process_radar_results(radar_results);

            std::cout << " radar scan returned ";
            if (!radar_results.empty()) {
                for (size_t j = 0; j < radar_results.size(); j++) {
                    RadarObj &obj = radar_results[j];
                    char type = obj.m_type;
                    int r = obj.m_row;
                    int c = obj.m_col;
                    if (type == 'R') {
                        for (size_t k = 0; k < m_robots.size(); k++) {
                            int rr, cc;
                            m_robots[k]->get_current_location(rr, cc);
                            if (rr == r && cc == c && m_robots[k]->get_health() > 0) {
                                std::cout << "R" << m_robot_characters[k] << " at " << r << "," << c;
                                break;
                            }
                        }
                    } else {
                        std::cout << type << " at " << r << "," << c;
                    }
                    if (j < radar_results.size() - 1) std::cout << " and ";
                }
                std::cout << std::endl;
            } else {
                std::cout << "nothing" << std::endl;
            }

            // Shooting
            int shot_row, shot_col;
            if (robot->get_shot_location(shot_row, shot_col)) {
                std::cout << " firing ";
                WeaponType w = robot->get_weapon();
                if (w == railgun) std::cout << "railgun";
                else if (w == hammer) std::cout << "hammer";
                else if (w == grenade) std::cout << "grenade";
                else if (w == flamethrower) std::cout << "flamethrower";
                std::cout << " at " << shot_row << "," << shot_col;

                // Check if robot hit
                bool hit_robot = false;
                for (size_t k = 0; k < m_robots.size(); k++) {
                    RobotBase* target = m_robots[k];
                    int tr, tc;
                    target->get_current_location(tr, tc);
                    if (tr == shot_row && tc == shot_col && target->get_health() > 0) {
                        std::cout << " Hits Robot " << target->m_name 
                                  << " at " << tr << "," << tc << std::endl;
                        hit_robot = true;
                        break;
                    }
                }
                if (!hit_robot) std::cout << std::endl;

                handle_shot(robot, shot_row, shot_col);
            } else {
                std::cout << " not firing" << std::endl;
                int move_dir, move_dist;
                robot->get_move_direction(move_dir, move_dist);
                if (move_dir != 0 && move_dist > 0) {
                    handle_movement(robot, move_dir, move_dist);
                    int new_r, new_c;
                    robot->get_current_location(new_r, new_c);
                    std::cout << " moving to (" << new_r << "," << new_c << ")" << std::endl;
                } else {
                    std::cout << " not moving" << std::endl;
                }
            }

            std::cout << std::endl;
        }

        if (check_winner()) {
            std::cout << "Game Over! Winner found!" << std::endl;
            break;
        }

        m_current_round++;
        if (m_watch_live) sleep(1);
    }
}
