#include "PokemonHome_HomeEnvironment.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_RawOCR.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonHome/Inference/PokemonHome_HomeApplicationDetector.h"
#include "PokemonHome/Inference/PokemonHome_FilterMenuReader.h"
#include "PokemonHome/Inference/PokemonHome_FilterMenuConfirmReader.h"
#include "PokemonHome/Inference/PokemonHome_SummaryDetector.h"
#include <chrono>
#include <queue>
#include <cmath>
#include <algorithm>
#include <unordered_set>


struct HomeSaveFailedError : public std::exception {
    const char* what() const noexcept override {
        return "HOME had errors saving during return to main menu.";
    }
};

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;

static int MAX_RETRIES = 5;

std::string sanitize_OCR2(std::string str){
    char chars[] = "\n\r—.,";
    for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
    return str;
}

std::string to_string(PageID page) {
    switch (page) {
    case PageID::TITLE_SCREEN: return "Title Screen";
    case PageID::MAIN_MENU: return "Main Menu";
    case PageID::GAME_SELECTION: return "Game Selection";
    case PageID::BOX_VIEW: return "Box View";
    case PageID::SUMMARY_VIEW: return "Summary View";
    case PageID::MARKINGS_VIEW: return "Markings View";
    case PageID::LIST_VIEW: return "List View";
    case PageID::UNKNOWN: return "Unknown";
    default: return "Invalid PageID";
    }
}

std::string to_string(GameStatus game) {
    switch (game) {

    case GameStatus::NONE: return "none";
    case GameStatus::POKEMON_HOME: return "Pokémon HOME";
    case GameStatus::POKEMON_PLA: return "Pokémon Legends: Arceus";
    case GameStatus::POKEMON_PIKACHU: return "Pokémon Let's Go! Pikachu";
    case GameStatus::POKEMON_EEVEE: return "Pokémon Let's Go! Eevee";
    case GameStatus::POKEMON_DIAMOND: return "Pokémon Brilliant Diamond";
    case GameStatus::POKEMON_PEARL: return "Pokémon Shining Pearl";
    case GameStatus::POKEMON_SWORD: return "Pokémon Sword";
    case GameStatus::POKEMON_SHIELD: return "Pokémon Shield";
    case GameStatus::POKEMON_SCARLET: return "Pokémon Scarlet";
    case GameStatus::POKEMON_VIOLET: return "Pokémon Violet";
    case GameStatus::CURRENT: return "Current Game";
    case GameStatus::UNKNOWN: return "Unknown";
    default: return "ERROR: DEFAULT CASE";
    }
}


HomeCursor::HomeCursor(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool single_page, bool secondary_open){
    holding_pokemon = false;
    this->secondary_open = secondary_open;

    locate_position(env, context);
    if(!single_page){
        identify_page(env, context, false);
        if(box==0){
            identify_page(env,context, true, UINT_MAX);
        }
    }

}

HomeCursor::HomeCursor(int row, int col)
    : secondary_open(false), row(row), col(col), box(0), holding_pokemon(false) {}

HomeCursor::HomeCursor(HomeCursor other, int box)
    : secondary_open(false), row(other.row), col(other.col), box(box), holding_pokemon(false) {}

HomeCursor::HomeCursor(int row, int col, int box)
    : secondary_open(false), row(row), col(col), box(box), holding_pokemon(false) {}

HomeCursor::HomeCursor(std::pair<int,int>& other)
    : secondary_open(false), row(other.first), col(other.second), box(0), holding_pokemon(false) {}

HomeCursor::HomeCursor()
    : secondary_open(false), row(0), col(0), box(0) {}




CursorActionResponse HomeCursor::move_cursor_to(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const HomeCursor& dest_cursor){
    // TODO: Implement cursor movement logic

    auto response = position_cursor(env, context, dest_cursor);
    if(response.result!=CursorActionResult::SUCCESS){
        return {response.result, response.message+" in movement to ("+std::to_string(dest_cursor.row)+", "+std::to_string(dest_cursor.col)+"), box "+std::to_string(dest_cursor.box)};
    }

    if(dest_cursor.box!=0){
        response = navigate_to_page(env, context, dest_cursor);
        if(response.result!=CursorActionResult::SUCCESS){
            return {response.result, response.message+" in movement to ("+std::to_string(dest_cursor.row)+", "+std::to_string(dest_cursor.col)+"), box "+std::to_string(dest_cursor.box)};
        }
    }

    return {CursorActionResult::SUCCESS, "Successfully navigated to ("+std::to_string(dest_cursor.row)+", "+std::to_string(dest_cursor.col)+"), box "+std::to_string(dest_cursor.box)};
}

CursorActionResponse HomeCursor::pick_up_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    pbf_press_button(context, BUTTON_Y, 10, 70);    // press y button
    holding_pokemon = true;
    return {CursorActionResult::SUCCESS, "Function still undefined"};

}

CursorActionResponse HomeCursor::pick_up_pokemon_multi(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    pbf_press_button(context, BUTTON_A, 10, 70);    // press y button
    holding_pokemon = true;
    // locate_position(env, context);
    return {CursorActionResult::SUCCESS, "Function still undefined"};

}

CursorActionResponse HomeCursor::put_down_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    pbf_press_button(context, BUTTON_Y, 10, 70);    // press y button
    holding_pokemon = false;
    return {CursorActionResult::SUCCESS, "Function still undefined"};
}

CursorActionResponse HomeCursor::put_down_pokemon_multi(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    pbf_press_button(context, BUTTON_A, 10, 70);    // press y button
    holding_pokemon = true;
    return {CursorActionResult::SUCCESS, "Function still undefined"};
}

void HomeCursor::align_col(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const int& new_col){
    int total_cols = (secondary_open ? 12 : 6);
    int dist = (new_col > col) ? new_col - col : col - new_col;

    if (dist <= total_cols / 2) {
        // direct movement
        if (new_col > col){
            for (int i = 0; i < new_col - col; ++i)
                pbf_press_dpad(context, DPAD_RIGHT, 10, 30);
        } else {
            for (int i = 0; i < col - new_col; ++i)
                pbf_press_dpad(context, DPAD_LEFT, 10, 30);
        }
    } else {
        // wrap-around
        if (new_col > col){
            for (int i = 0; i < total_cols - (new_col - col); ++i)
                pbf_press_dpad(context, DPAD_LEFT, 10, 30);
        } else {
            for (int i = 0; i < total_cols - (col - new_col); ++i)
                pbf_press_dpad(context, DPAD_RIGHT, 10, 30);
        }
    }

    col = new_col;

}

void HomeCursor::align_row(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const int& new_row){

    // direct nav up or down through rows
    if (!(row == 0 && new_row == 4) && !(new_row == 0 && row == 4)) {
        for (int i = row; i < new_row; ++i) {
            pbf_press_dpad(context, DPAD_DOWN, 10, 30);
        }
        for (int i = new_row; i < row; ++i) {
            pbf_press_dpad(context, DPAD_UP, 10, 30);
        }
    } else { // wrap around is faster to move between row or last row
        if (row == 0 && new_row == 4) {
            for (int i = 0; i <= 2; ++i) {
                pbf_press_dpad(context, DPAD_UP, 10, 30);
            }
        } else {
            for (size_t i = 0; i <= 2; ++i) {
                pbf_press_dpad(context, DPAD_DOWN, 10, 30);
            }
        }
    }

    row = new_row;

}

CursorActionResponse HomeCursor::position_cursor(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const HomeCursor& dest_cursor, int retry_count){

    if(dest_cursor.row>MAX_ROWS){
        return {CursorActionResult::ERROR_RECOVERABLE, "row "+std::to_string(dest_cursor.row)+" out of scope"};
    }
    if(dest_cursor.col>(secondary_open?2:1)*MAX_COLUMNS){
        return {CursorActionResult::ERROR_RECOVERABLE, "column "+std::to_string(dest_cursor.col)+" out of scope"};
    }


    if (retry_count > MAX_RETRIES) {
        return {CursorActionResult::FAILURE, "Reached maximum retry attempts"};
    }


    if(retry_count > 3){ // Last resort for checking that the cursor did not end up at the bottom or top row.
        // TODO: Add detection for top and bottom row buttons that don't appear in the normal grid
        ImageFloatBox box_name_button = {0.3, 0.1, 0.01, 0.02};
        ImageFloatBox box_spaces_button = {0.3, 0.72, 0.01, 0.02};
        ImageFloatBox newest_30_button = {0.46, 0.72, 0.01, 0.02};
        context.wait_for_all_requests();

        VideoSnapshot screen = env.console.video().snapshot();
        if(euclidean_distance(image_stats(extract_box_reference(screen, box_name_button)).average, FloatPixel(255, 215.5,0))<=15){
            pbf_press_button(context, BUTTON_DOWN, 10, 60);
            return locate_position(env, context, true);
        }
        if(euclidean_distance(image_stats(extract_box_reference(screen, box_spaces_button)).average, FloatPixel(255, 215.5,0))<=15 || euclidean_distance(image_stats(extract_box_reference(screen, newest_30_button)).average, FloatPixel(255, 215.5,0))<=15){
            pbf_press_button(context, BUTTON_UP, 10, 60);
            return locate_position(env, context, true);
        }
    }

    if(row < 0 || col < 0 || row > MAX_ROWS || col > MAX_COLUMNS){
        locate_position(env, context, false);
    }


    // Align column if not already aligned
    if(dest_cursor.col!=col){
        align_col(env, context, dest_cursor.col);
    }

    // Align row if not already aligned
    if(dest_cursor.row!=row){
        align_row(env, context, dest_cursor.row);
    }


    // if(!(dest_cursor.row==row&&dest_cursor.col==col)){
        locate_position(env, context);
    // }

    if(row!=dest_cursor.row||col!=dest_cursor.col || row > MAX_ROWS || col > (secondary_open?2:1)*MAX_COLUMNS){
        return position_cursor(env, context, dest_cursor, retry_count+1);
    }

    return {CursorActionResult::SUCCESS, "Successfully moved cursor to ("+std::to_string(row)+", "+std::to_string(col)+")"};
}

CursorActionResponse HomeCursor::navigate_to_page(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const HomeCursor& dest_cursor){
    if(dest_cursor.box>200 || dest_cursor.box < 0){
        return {CursorActionResult::FAILURE, "box "+std::to_string(dest_cursor.box)+" out of scope"};
    }

    if(box == 0){
        identify_page(env, context, false, UINT_MAX);
    }

    CursorActionResponse last_move;

    env.console.log("moving to box " + std::to_string(dest_cursor.box)+" from box "+std::to_string(box));

    if(dest_cursor.box == box){ // on current page
        last_move = {CursorActionResult::SUCCESS, "Starting on correct page"};
    }else if(dest_cursor.box>box){ // Navigating right
        while(box<dest_cursor.box){
            pbf_press_button(context, BUTTON_R, 10, 0);
            HomePageRightMoveWatcher rightWatcher(COLOR_BLUE);
            int ret = wait_until(env.console, context, 2000ms,
                {
                    rightWatcher
                }
            );
            pbf_wait(context, 12);
            context.wait_for_all_requests();
            switch (ret){
            case 0:
                last_move = {CursorActionResult::SUCCESS, "Watched page turn"};
                break;
            default:
                return {CursorActionResult::FAILURE, "Could not observe page traversal to the right"};
                break;
            }
            box++;
        }
    }else{ // navigating left
        while(box>dest_cursor.box){
            pbf_press_button(context, BUTTON_L, 10, 0);
            HomePageLeftMoveWatcher leftWatcher(COLOR_BLUE);
            int ret = wait_until(env.console, context, 2000ms,
                {
                    leftWatcher
                }
            );
            pbf_wait(context, 12);
            context.wait_for_all_requests();
            switch (ret){
            case 0:
                last_move = {CursorActionResult::SUCCESS, "Watched page turn"};
                break;
            default:
                return {CursorActionResult::FAILURE, "Could not observe page traversal to the left"};
                break;
            }
            box--;
        }
    }

    return {last_move.result, last_move.message+" while navigating to page "+std::to_string(dest_cursor.box)};
}


CursorActionResponse HomeCursor::locate_position(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool retry){
    // Special waterfill case for if holding pokemon, very reliable
    context.wait_for_all_requests();

    ImageFloatBox hand_region = {0.03, 0.15, 0.93, 0.5};
    HomeCursorWatcher handWatcher(holding_pokemon?HomeCursorType::GRABBING:HomeCursorType::RED, hand_region, COLOR_WHITE);

    int ret = wait_until(env.console, context, 2s, {handWatcher});
    if (ret == 0){
        auto [x, y] = handWatcher.location();
        row = y;
        col = x;
        env.console.log("HERE at ("+std::to_string(y)+", "+std::to_string(x)+")");
        return {CursorActionResult::SUCCESS, "Found cursor at ("+std::to_string(y)+", "+std::to_string(x)+")"};
    }else{
        ImageFloatBox box_name_button = {0.3, 0.1, 0.01, 0.02};
        ImageFloatBox box_spaces_button = {0.3, 0.72, 0.01, 0.02};
        ImageFloatBox newest_30_button = {0.46, 0.72, 0.01, 0.02};
        context.wait_for_all_requests();

        VideoSnapshot screen = env.console.video().snapshot();
        if(euclidean_distance(image_stats(extract_box_reference(screen, box_name_button)).average, FloatPixel(255, 187,0))<=15){
            pbf_press_button(context, BUTTON_DOWN, 10, 60);
            return locate_position(env, context, true);
        }
        if(euclidean_distance(image_stats(extract_box_reference(screen, box_spaces_button)).average, FloatPixel(255, 187,0))<=15 || euclidean_distance(image_stats(extract_box_reference(screen, newest_30_button)).average, FloatPixel(255, 187,0))<=15){
            pbf_press_button(context, BUTTON_UP, 10, 60);
            return locate_position(env, context, true);
        }

        env.console.log(std::to_string(holding_pokemon));

        env.console.log("HERE, FAILED AT FINDING CURSOR");

        HomeCursorWatcher handWatcher2(!holding_pokemon?HomeCursorType::GRABBING:HomeCursorType::RED, hand_region, COLOR_WHITE);
        ret = wait_until(env.console, context, 2000ms, {handWatcher2});
        if (ret == 0){
            auto [x, y] = handWatcher2.location();
            row = y;
            col = x;
            holding_pokemon = !holding_pokemon;
            return locate_position(env, context, true);
        }
        return {CursorActionResult::FAILURE, "Could not locate cursor"};
    }
}



/**
* Identifies the current page in the game by analyzing the screen and extracting relevant information.
* This function is used by the PokemonHome class via the HomeCursor class.
* It captures screen data from the bottom-left display and the box name,
* and updates the HomeCursor's `box` attribute with the consensus result.
*
* Note: For optimal performance, ensure the cursor is positioned at (0, 0) before calling this function.
*
* Optional Parameter:
* - `hard_check`: When enabled, the function collects data from at least two separate pages
* and determines the current page based on a >50% consensus.
*/

CursorActionResponse HomeCursor::identify_page(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool hard_check, int expected) {
    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();
    VideoOverlaySet box_render(env.console);
    std::ostringstream ss;

    ImageFloatBox box_name_box(0.135, 0.105, 0.24, 0.04);
    ImageFloatBox home_box_checker(0.075, 0.72, 0.03205, 0.04);

    int box_name_top = 0, box_name_bottom = 0;

    if(col>MAX_COLUMNS){ // in second box, pick good wraparound to get to the main box
        if(col<=8){
            move_cursor_to(env, context, {row, 5});
        }else{
            move_cursor_to(env, context, {row, 0});
        }
    }

    auto extract_box_data = [&](VideoSnapshot& snapshot) {
        std::string temp;
        try {
            temp = sanitize_OCR2(OCR::ocr_read(Language::English, extract_box_reference(snapshot, box_name_box)));
            box_name_top = std::stoi(temp.substr(temp.find_last_of(' ') + 1));
        } catch (...) {
            box_name_top = 0;
        }

        try {
            temp = sanitize_OCR2(OCR::ocr_read(Language::English, extract_box_reference(snapshot, home_box_checker)));
            box_name_bottom = std::stoi(temp.substr(0, temp.find_last_of('/')));
        } catch (...) {
            box_name_bottom = 0;
        }
    };

    extract_box_data(screen);
    env.console.log(std::to_string(box_name_top) + "/" + std::to_string(box_name_bottom));

    if (!hard_check) {
        if(expected!=INT_MAX){
            if (box_name_top == expected || box_name_bottom == expected) {
                box = box_name_top;
                return {CursorActionResult::SUCCESS, "Successfully identified expected page as " + std::to_string(box)};
            } else {
                CursorActionResponse response = identify_page(env, context, true);
                return {response.result, response.message+" after a failed soft check"};
            }
        }else{
            if (box_name_top == box_name_bottom) {
                box = box_name_top;
                return {CursorActionResult::SUCCESS, "Successfully identified page as " + std::to_string(box)};
            } else {
                CursorActionResponse response = identify_page(env, context, true);
                return {response.result, response.message+" after a failed soft check"};
            }
        }
    }

    locate_position(env, context);

    auto normalize_page = [](int page){
        // Assume page range 1..200
        while (page < 1) page += 200;
        while (page > 200) page -= 200;
        return page;
    };

    std::unordered_map<int, size_t> page_counts;
    size_t total_observations = 0;
    int page_offset = 0;

    auto update_page_counts = [&]() {
        int adjusted_top = normalize_page(static_cast<int>(box_name_top) - page_offset);
        int adjusted_bottom = normalize_page(static_cast<int>(box_name_bottom) - page_offset);
        page_counts[adjusted_top]++;
        page_counts[adjusted_bottom]++;
        total_observations += 2;
    };

    update_page_counts();

    while (std::abs(page_offset) < 10) {
        // Find the most common observed page
        int most_common_page = 0;
        size_t most_common_count = 0;
        for (auto& [page, count] : page_counts) {
            if (count > most_common_count) {
                most_common_page = page;
                most_common_count = count;
            }
        }

        // Stop condition
        if ((static_cast<double>(most_common_count) / total_observations) > 0.5 && page_offset != 0) {
            box = normalize_page(most_common_page + page_offset);
            return {CursorActionResult::SUCCESS, "Identified page " + std::to_string(box)};
        }

        // Decide which way to scroll
        if (page_offset < 0 || (page_offset == 0 && (box_name_top == 199 || box_name_bottom == 199))) {
            pbf_press_button(context, BUTTON_L, 10, 50);
            page_offset--;
        } else {
            pbf_press_button(context, BUTTON_R, 10, 50);
            page_offset++;
        }

        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
        extract_box_data(screen);
        env.console.log(std::to_string(box_name_top) + "/" + std::to_string(box_name_bottom));

        update_page_counts();
    }

    CursorActionResponse temp = {CursorActionResult::FAILURE, "Could not identify page after extensive hard check"};

    return box==0?identify_page(env, context, true, UINT_MAX):temp;

}





int HomeCursor::get_page() {
    return box;
}

int HomeCursor::get_page() const{
    return box;
}

int HomeCursor::get_row() {
    return row;
}

int HomeCursor::get_row() const {
    return row;
}

int HomeCursor::get_col() {
    return col;
}

int HomeCursor::get_col() const{
    return col;
}

 int HomeCursor::distance_to(const HomeCursor& other) const{
    int x_dist = std::min(std::abs(static_cast<int>(other.col-col)),std::abs(static_cast<int>((secondary_open?12:6)-other.col-col)));
    int y_dist = std::min(static_cast<int>(3), std::abs(static_cast<int>(other.row-row)));
    int box_dist = std::abs(static_cast<int>(other.box-box));

    return x_dist+y_dist+box_dist+(box_dist>0?1:0);
}




HomeEnvironment::HomeEnvironment(SingleSwitchProgramEnvironment& env, ProControllerContext& context)
{
    detect_home(env, context);
    initialize_navigation_map(env, context);

    if(current_view==PageID::BOX_VIEW){ cursor.emplace(env, context, false, game_open!=GameStatus::POKEMON_HOME);}

    boxes = HomeStorage();
    best_map.clear();
}


bool HomeEnvironment::navigate_menus_to(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const PageID destination, const GameStatus game){
    if(cursor.has_value()){
        cursor->secondary_open =
            (game != GameStatus::POKEMON_HOME) &&
            !(game == GameStatus::CURRENT && game_open == GameStatus::POKEMON_HOME);
    }

    if(current_view==destination&&(game==game_open||game==GameStatus::CURRENT))return true;

    bool succeeded = true;
    std::vector<PageID> steps;
    if ((game != GameStatus::NONE && game != game_open)) {
        if( game != GameStatus::CURRENT && current_view != PageID::GAME_SELECTION){
            steps = find_navigation_path(env, context, current_view, PageID::GAME_SELECTION);
            succeeded = perform_navigation_steps(env, context, steps) && succeeded;
            // Open desired game
            game_open = game;
        }
        steps = find_navigation_path(env, context, current_view, destination);
        perform_navigation_steps(env, context, steps);
    }

    return succeeded;
}

void HomeEnvironment::navigate_to(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const HomeCursor& dest_cursor){
    context.wait_for_all_requests();

    if(!cursor.has_value() || cursor->get_page() == 0){
        cursor.emplace(env, context, false, game_open!=GameStatus::POKEMON_HOME);
    }



    auto response = this->cursor->move_cursor_to(env, context, dest_cursor);
    if(response.result!=CursorActionResult::SUCCESS){
        handle_errors(env, context, response);
    }
}


std::vector<PageID> HomeEnvironment::find_navigation_path(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PageID from, PageID to){
    if (to == PageID::CURRENT){
        return {};
    }

    // Check the cache for a precomputed path
    std::pair<PageID, PageID> key = {from, to};
    auto it = navigation_cache.find(key);
    if (it != navigation_cache.end()) {
        return it->second; // Return the cached path
    }

    // BFS for shortest path
    std::queue<std::pair<PageID, std::vector<PageID>>> queue;
    queue.push({from, {}});

    while (!queue.empty()) {
        auto [current, current_path] = queue.front();
        queue.pop();

        // env.console.log("Looking at moves from " + to_string(current));


        // If we've reached the target page, cache and return the path
        if (current == to) {
            current_path.push_back(current);
            navigation_cache[key] = current_path; // Cache the result
            return current_path;
        }

        // Explore all transitions from the current page
        const auto& transitions = navigation_map[current];
        for (const auto& [next, _] : transitions) {
            // env.console.log("considering moving to " + to_string(next));

            // Push the next node and path without using a visited set
            std::vector<PageID> new_path = current_path;
            new_path.push_back(current);
            queue.push({next, new_path});
        }
    }

    // If no path is found, cache and return an empty vector
    navigation_cache[key] = {};
    return {};
}

bool HomeEnvironment::perform_navigation_steps(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::vector<PageID>& steps){
    // Ensure we have at least two steps to navigate.
    if (steps.size() < 2) {
        throw std::runtime_error("Insufficient steps to perform navigation.");
    }

    bool no_errors = true;

    // Iterate over the steps in pairs.
    for (size_t i = 0; i < steps.size() - 1; ++i) {
        PageID current = steps[i];
        PageID next = steps[i + 1];

        // Check if the current page has navigation entries.
        auto it = navigation_map.find(current);
        if (it == navigation_map.end()) {
            throw std::runtime_error("No navigation entries for current page: " + std::string(to_string(current)));
        }

        // Search for the pair (current, next) in the navigation vector.
        const auto& transitions = it->second;
        auto transition_it = std::find_if(transitions.begin(), transitions.end(),
                                          [next](const std::pair<PageID, NavigationFunction>& pair) {
                                              return pair.first == next;
                                          });

        // If no valid transition is found, throw an error.
        if (transition_it == transitions.end()) {
            throw std::runtime_error("No valid transition from " + std::string(to_string(current)) + " to " + std::string(to_string(next)));
        }

        // Call the navigation function for the transition, passing the game status as well.
        try{
            transition_it->second(env, context);
        }catch(HomeSaveFailedError){
            no_errors = false;
        }
    }
    current_view = steps.back();
    steps= {};

    return no_errors;
}

void HomeEnvironment::identify_game_icon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ImageFloatBox game_icon_box_prim(0.9255, 0.715, 0.035, 0.057);

    context.wait_for_all_requests();
    VideoSnapshot screen = env.console.video().snapshot();

    FloatPixel game_icon_prim = image_stats(extract_box_reference(screen, game_icon_box_prim)).average;

    if(game_icon_prim.r>115&&game_icon_prim.r<125&&game_icon_prim.g>80&&game_icon_prim.g<85&&game_icon_prim.b>100&&game_icon_prim.b<105){
        game_open = GameStatus::POKEMON_VIOLET;
    }else if(game_icon_prim.r>155&&game_icon_prim.r<165&&game_icon_prim.g>250&&game_icon_prim.g<=255&&game_icon_prim.b>200&&game_icon_prim.b<205){
        game_open = GameStatus::POKEMON_HOME;
    }else if(game_icon_prim.r>240&&game_icon_prim.r<245&&game_icon_prim.g>230&&game_icon_prim.g<235&&game_icon_prim.b>170&&game_icon_prim.b<175){
        game_open = GameStatus::POKEMON_PLA;
    }else{
        game_open = GameStatus::UNKNOWN;
    }
}

void HomeEnvironment::preserve_placeholders(int start, int end){
    for(int i = start; i <= end; i++){
        HomeBox box = boxes.at(i);
        for (int row = 0; row < 5; row++){
            for (int col = 0; col < 6; col++){
                HomeSlot pokemon = box.at(row, col);
                if(!pokemon.isEmpty()&&!pokemon.isOccupied()){
                    placeholder_list.push_back(pokemon.getPokemon().value());
                }
            }
        }
    }
}

bool HomeEnvironment::reconcile_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int box_num, bool retry = false){
    int moves = 0;
    if(cursor.value().get_row()<3){
        navigate_to(env, context, {0,0, box_num});
    }else{
        for(int i = 5-cursor.value().get_row(); i>0; i--, moves++){
            pbf_press_dpad(context, DPAD_DOWN, 10, 50);
        }
    }

    pbf_wait(context, 125ms);
    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();

    int mismatches = 0;

    bool succeeded = true;

    HomeBox& box = boxes.at(box_num);

    for (int row = 0; row < 5; row++){
        for (int col = 0; col < 6; col++){
            ImageFloatBox slot_box(0.06 + (0.072 * col), 0.2 + (0.1035 * row), 0.03, 0.057);
            ImageFloatBox slot_box2(0.059400 + (0.071861 * col), 0.1987 + (0.105544 * row), 0.03, 0.057);
            int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();
            FloatPixel color_value = image_stats(extract_box_reference(screen, slot_box2)).average;

            if(!box.at(row,col).isOccupied()){ // blank pokemon space
                if(current_box_value>=5){
                    env.console.log("Box " + std::to_string(box_num)+" was not reconciled");
                    succeeded = succeeded && false;
                    mismatches++;
                }
            }else{
                double euc_dist = euclidean_distance(box.at(row,col).quick_color(),color_value);
                if(euc_dist>=6.5f){
                    env.console.log("Box " + std::to_string(box_num)+" was not reconciled at {" + std::to_string(row) + ", " + std::to_string(col) + "}. Euclidian distance was "+std::to_string(euc_dist));
                    succeeded = succeeded && false;
                    mismatches++;
                }
            }
        }
    }
    if(succeeded)env.console.log("Box " + std::to_string(box_num)+" successfully reconciled");

    if(moves>0){
        for( ; moves>0; moves--){
            pbf_press_dpad(context, DPAD_UP, 10, 50);
        }
        context.wait_for_all_requests();
    }

    // If it didn't succeed, just double check that we aren't at the wrong box.
    if(!succeeded && mismatches >= 25 && !retry){
        bail_out(env, context);

        navigate_to(env, context, {0,0,box_num});

        return reconcile_box(env, context, box_num, true);

    }
    return succeeded;
}

bool HomeEnvironment::sort_into_correct_boxes(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    int left_num,
    int right_num
    ){
    if(left_num==107||right_num==107){
        env.console.log("DELETE");
    }


    if (current_view != PageID::BOX_VIEW)
        navigate_menus_to(env, context, PageID::BOX_VIEW);

    if (!boxes.at(left_num).loaded)  build_box(env, context, left_num);
    if (!boxes.at(right_num).loaded) build_box(env, context, right_num);

    HomeBox& left  = boxes.at(left_num);
    HomeBox& right = boxes.at(right_num);

    // Flatten both boxes
    std::vector<PokemonData*> combined = left.flatten();
    {
        std::vector<PokemonData*> right_flat = right.flatten();
        combined.insert(combined.end(), right_flat.begin(), right_flat.end());
    }

    if (combined.empty()) return false;

    // Sort pointers by dereferenced value
    std::sort(combined.begin(), combined.end(),
        [](const PokemonData* a, const PokemonData* b) {
            return *a < *b;
        }
    );

    // Assign Pokémon to target boxes (keep pointers)
    std::vector<PokemonData*> left_target;
    std::vector<PokemonData*> right_target;

    size_t total = combined.size();
    size_t left_count = std::min<size_t>(30, total);

    // First 30 go to left
    left_target.assign(combined.begin(), combined.begin() + left_count);

    // Remaining go to right
    if (total > 30)
        right_target.assign(combined.begin() + 30, combined.end());

    // Helpers
    auto in_target = [](PokemonData* p, const std::vector<PokemonData*>& target) {
        return std::find(target.begin(), target.end(), p) != target.end();
    };

    // Queues for misplaced and empty slots
    std::vector<HomeCursor> left_incorrect, left_empty, left_unoccupied;
    std::vector<HomeCursor> right_incorrect, right_empty, right_unoccupied;

    for (int r = 0; r < HomeBox::MAX_ROWS; r++) {
        for (int c = 0; c < HomeBox::MAX_COLS; c++) {
            auto& slot = left.at(r, c);
            if (!slot.isOccupied()) {
                left_unoccupied.push_back({r, c, left_num});
            }
            if (slot.isEmpty()) {
                left_empty.push_back({r, c, left_num});
            } else {
                PokemonData* p = &(*slot.getPokemon());
                if (!in_target(p, left_target))
                    left_incorrect.push_back({r, c, left_num});
            }

            auto& rslot = right.at(r, c);
            if (!rslot.isOccupied()) {
                right_unoccupied.push_back({r, c, right_num});
            }
            if (rslot.isEmpty()) {
                right_empty.push_back({r, c, right_num});
            } else {
                PokemonData* p = &(*rslot.getPokemon());
                if (!in_target(p, right_target))
                    right_incorrect.push_back({r, c, right_num});
            }
        }
    }

    bool did_swap = false;

    // === MAIN SWAP LOGIC ===

    // Special Case for all can move to the right
    // if(((left_incorrect.size()==30&&right_incorrect.size()==30)||right_empty.size()==30)&&right_unoccupied.size()==30&&left_unoccupied.size()!=30){
    //     cursor.value().move_cursor_to(env, context, {0,0,left_num});

    //     context.wait_for_all_requests();
    //     pbf_press_button(context, BUTTON_ZR,10, 50);
    //     pbf_press_button(context, BUTTON_A,10, 50);
    //     pbf_press_dpad(context, DPAD_DOWN,10, 30);
    //     pbf_press_dpad(context, DPAD_DOWN,10, 30);
    //     pbf_press_dpad(context, DPAD_DOWN,10, 30);
    //     pbf_press_dpad(context, DPAD_DOWN,10, 30);
    //     pbf_press_dpad(context, DPAD_RIGHT,10, 30);
    //     pbf_press_dpad(context, DPAD_RIGHT,10, 30);
    //     pbf_press_dpad(context, DPAD_RIGHT,10, 30);
    //     pbf_press_dpad(context, DPAD_RIGHT,10, 30);
    //     pbf_press_dpad(context, DPAD_RIGHT,10, 30);
    //     pbf_press_button(context, BUTTON_A,10, 50);
    //     context.wait_for_all_requests();

    //     cursor.value().move_cursor_to(env, context, {0,0,right_num});
    //     pbf_press_button(context, BUTTON_A,10, 50);
    //     pbf_press_button(context, BUTTON_ZL,10, 50);

    //     std::swap(left, right);

    //     context.wait_for_all_requests();

    //     return true;
    // }

    // Fill left box first (prioritize filling blanks)
    while (!left_empty.empty() && !right_incorrect.empty()) {
        HomeCursor dest = left_empty.back();  left_empty.pop_back();
        HomeCursor src  = right_incorrect.back(); right_incorrect.pop_back();

        swap_pokemon(env, context, src, dest);
        did_swap = true;
    }

    // Direct swap between incorrect slots if both have extras
    while (!left_incorrect.empty() && !right_incorrect.empty()) {
        HomeCursor left_pos = left_incorrect.back();  left_incorrect.pop_back();
        HomeCursor right_pos = right_incorrect.back(); right_incorrect.pop_back();

        swap_pokemon(env, context, left_pos, right_pos);
        did_swap = true;
    }

    return did_swap;
}



void HomeEnvironment::scan_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int box_num){
    navigate_to(env, context, {0, 0, box_num});

    HomeCursorWatcher handWatcher(HomeCursorType::RED, {0.03, 0.15, 0.93, 0.5}, COLOR_WHITE);
    int ret = wait_until(env.console, context, 2s, {handWatcher});

    if(ret!=0&&handWatcher.location().first!=0&&handWatcher.location().second!=0){
        pbf_mash_button(context, BUTTON_B, 2s);
        bail_out(env, context);
        navigate_to(env, context, {0, 0, box_num});
    }



    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();

    HomeBox& box = boxes.at(box_num);
    std::vector<std::pair<int, int>> occupied_slots;
    std::optional<std::pair<int, int>> first_pokemon_slot;


    // Step 1: Visual scan
    for (int row = 0; row < HomeBox::MAX_ROWS; ++row) {
        for (int col = 0; col < HomeBox::MAX_COLS; ++col) {
            ImageFloatBox slot_box(0.06 + (0.072 * col), 0.2 + (0.1035 * row), 0.03, 0.057);
            ImageFloatBox slot_box2(0.059400 + (0.071861 * col), 0.198700 + (0.105544 * row), 0.03, 0.057);
            // For some reason, it needs both of these. The first one accurately checks if the slots are empty and the second one accurately captures the rgb values
            double stddev_sum = image_stddev(extract_box_reference(screen, slot_box)).sum();
            if (stddev_sum < 5) {
                box.at(row, col).clear();
                continue;
            }

            FloatPixel avg_color = image_stats(extract_box_reference(screen, slot_box2)).average;
            box.at(row, col).m_quick_color = avg_color;
            occupied_slots.emplace_back(row, col);

            if (!first_pokemon_slot.has_value()) {
                first_pokemon_slot = std::make_pair(row, col);
            }
        }
    }

    // Step 2: Handle empty box
    if (occupied_slots.empty()) {
        return;
    }

    // Step 3: Move to first Pokémon and open Summary View
    if (*first_pokemon_slot != std::pair<int, int>{0, 0}) {
        navigate_to(env, context, *first_pokemon_slot);
    }
    navigate_menus_to(env, context, PageID::SUMMARY_VIEW);

    // Step 4: Read summaries only for occupied slots
    for (size_t i = 0; i < occupied_slots.size(); ++i) {
        const auto& [row, col] = occupied_slots[i];
        HomeSlot& slot = box.at(row, col);

        context.wait_for_all_requests();
        screen = env.console.video().snapshot();

        std::optional<PokemonData> pokemon = scan_pokemon(env, context);

        if (pokemon) {
            slot.setPokemon(*pokemon);
        }
        else {
            env.console.log("Failed to read Pokémon at {" + std::to_string(row) + ", " + std::to_string(col) + "}.");
            slot.clear();
        }

        if (i + 1 < occupied_slots.size()) {
            pbf_press_button(context, BUTTON_R, 10, 80);  // Move to next Pokémon
        }
    }


    // Step 5: Return to Box View
    pbf_press_button(context, BUTTON_R, 10, 80);  // Move to first Pokémon again
    navigate_menus_to(env, context, PageID::BOX_VIEW);

    box.loaded = true;
    boxes.at(box_num) = std::move(box);
}

void HomeEnvironment::set_prime(int target_id, int target_form){
    // Flatten all Pokémon into a single list.
    auto all_pokemon = boxes.flatten();

            // Filter for those matching ID + form.
    std::vector<PokemonData*> matches;
    for (auto& slot : all_pokemon){
        if (slot->id == target_id && slot->form_id == target_form){
            matches.push_back(slot);
        }
    }

    std::sort(matches.begin(), matches.end(),
        [](const PokemonData* a, const PokemonData* b){
            if (a->shiny != b->shiny) return a->shiny > b->shiny;      // shiny first
            if (a->level != b->level) return a->level > b->level;      // higher level first
            return false;                            // lower form next
        }
    );

            // Mark prime_example correctly.
    for (size_t i = 0; i < matches.size(); ++i){
        matches[i]->prime_example = (i == 0);
    }
};


void HomeEnvironment::build_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int box_num){
    std::vector<std::tuple<HomeCursor, HomeCursor, PokemonData*>> pending_swaps;

    // auto process_best = [&](PokemonData& p_in_slot)->void{
    //     auto key = std::make_pair(p_in_slot.id, p_in_slot.form_id);
    //     auto it = best_map.find(key);

    //     if (it == best_map.end()) { // No current best
    //         best_map[key] = &p_in_slot;
    //         p_in_slot.prime_example = true;
    //         return;
    //     } else{
    //         set_prime(p_in_slot.id, p_in_slot.form_id);
    //     }
    // };

    // First, try to load from the .json files. If there is a failure, manually scan box
    if(!boxes.load(box_num) || !reconcile_box(env, context, box_num)){
        scan_box(env, context, box_num);
    }

    HomeBox& box = boxes.at(box_num);

    for (int row = 0; row < HomeBox::MAX_ROWS; ++row) {
        for (int col = 0; col < HomeBox::MAX_COLS; ++col) {
            if(!box.at(row, col).isEmpty()){
                auto pokemon = box.at(row, col).getPokemon();
                set_prime(pokemon->id, pokemon->form_id);
            }else if(placeholder_list.size()>0){
                box.at(row,col).setPokemon(placeholder_list.front());
                placeholder_list.pop_front();
            }
        }
    }


    // TODO: This is where we can do the experimental quick swap prime logic

    boxes.store(box_num);
}


bool HomeEnvironment::sort_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int box_num) {
    if(current_view!=PageID::BOX_VIEW)navigate_menus_to(env, context, PageID::BOX_VIEW);

    navigate_to(env, context, {0, 0, box_num});

    HomeBox& box = boxes.at(box_num);

    constexpr int total_slots = HomeBox::MAX_ROWS * HomeBox::MAX_COLS;

    bool swapped = false;

    for (int i = 0; i < total_slots - 1; ++i) {
        int min_idx = i;
        int min_row = i / HomeBox::MAX_COLS;
        int min_col = i % HomeBox::MAX_COLS;

        // Find the "smallest" Pokémon
        for (int j = i; j < total_slots; ++j) {
            int row = j / HomeBox::MAX_COLS;
            int col = j % HomeBox::MAX_COLS;

            HomeSlot& candidate = box.at(row, col);
            HomeSlot& current_min = box.at(min_row, min_col);

            if (current_min.isEmpty() ||
                (!candidate.isEmpty() && *candidate.getPokemon() < *current_min.getPokemon())) {
                min_idx = j;
                min_row = row;
                min_col = col;
            }
        }

        if (min_idx != i) {
            // Extend min_idx for equal Pokémon
            while (min_idx + 1 < total_slots) {
                int next_idx = min_idx + 1;
                int cur_row = min_idx / HomeBox::MAX_COLS;
                int cur_col = min_idx % HomeBox::MAX_COLS;
                int next_row = next_idx / HomeBox::MAX_COLS;
                int next_col = next_idx % HomeBox::MAX_COLS;

                HomeSlot& cur_slot = box.at(cur_row, cur_col);
                HomeSlot& next_slot = box.at(next_row, next_col);

                if (cur_slot.isOccupied() && next_slot.isOccupied() &&
                    *cur_slot.getPokemon() == *next_slot.getPokemon()) {
                    min_idx = next_idx;
                    min_row = next_row;
                    min_col = next_col;
                } else {
                    break;
                }
            }

            int swap_row = i / HomeBox::MAX_COLS;
            int swap_col = i % HomeBox::MAX_COLS;

            // Skip if both slots are empty
            if (box.at(swap_row, swap_col).isEmpty() && box.at(min_row, min_col).isEmpty())
                continue;

            // Swap visually
            swap_pokemon(env, context, {swap_row, swap_col, box_num}, {min_row, min_col, box_num});

            swapped = true;
        }
    }

    context.wait_for_all_requests();

    return swapped;
}


void HomeEnvironment::sort_all_boxes(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int start, int end){
    auto pad_with_placeholders = [&]() {
        // --- Step 1: Build set of existing Pokemon keys ---
        std::unordered_set<uint64_t> existing;
        std::vector<PokemonData*> all_pokemon = boxes.flatten();
        existing.reserve(all_pokemon.size() * 2);

        for (PokemonData* p : all_pokemon) {
            if (!p || p->placeholder) continue;
            existing.insert((uint64_t(p->id) << 32) | uint64_t(p->form_id));
        }

                // --- Step 2: Collect all empty slots ---
        std::vector<std::tuple<int,int,int>> empty_slots;
        empty_slots.reserve((end - start + 1) * HomeBox::MAX_ROWS * HomeBox::MAX_COLS);
        for (int i = start; i <= end; ++i) {
            HomeBox& box = boxes.at(i);
            for (int r = 0; r < HomeBox::MAX_ROWS; ++r) {
                for (int c = 0; c < HomeBox::MAX_COLS; ++c) {
                    auto& slot = box.at(r, c);
                    if (slot.isEmpty()) {
                        empty_slots.emplace_back(i, r, c);
                    }else if (!slot.isOccupied()) {
                        slot.clear();
                        empty_slots.emplace_back(i, r, c);
                    }
                }
            }
        }

        size_t next_empty = 0;
        const auto& pokedex = pokedex_information.get_pokedex();

                // --- Helper: place a single placeholder safely ---
        auto place_placeholder = [&](int id, int form_id, const std::string& form_name) {
            if (next_empty >= empty_slots.size()) return; // safety guard
            auto [bi, br, bc] = empty_slots[next_empty++];
            HomeBox& box = boxes.at(bi);
            box.at(br, bc).setPokemon({
                id, form_id, form_name,
                StatsHuntGenderFilter::Genderless,
                PokemonType::NONE, PokemonType::NONE,
                Region::UNKNOWN,
                0, -1, false, false,
                "Any", PokemonType::NONE,
                true, true
            });
        };

                // --- Pre-scan: track prime placeholders per region ---
        std::unordered_map<Region, int> special_case_count;
        for (PokemonData* p : all_pokemon) {
            if (p && p->form_id == -1 && p->prime_example) {
                special_case_count[p->region]++;
            }
        }

                // --- Add generation spacers, bounded by available empties ---
        auto add_spacers = [&](Region region, int id, int count, const char* label) {
            int adjustment = special_case_count[region];
            int effective_count = std::max(0, count - adjustment);
            for (int i = 0; i < effective_count && next_empty < empty_slots.size(); ++i) {
                place_placeholder(id, i + 100, label);
            }
            special_case_count[region] = 0;
        };

                // --- Step 3: Fill missing Pokédex entries ---
        for (const auto& group : pokedex) {
            for (const auto& pokemon : group) {
                uint64_t key = (uint64_t(pokemon.id.value()) << 32) | uint64_t(pokemon.form_id.value());
                if (existing.find(key) != existing.end()) continue;
                if (next_empty >= empty_slots.size()) return; // stop cleanly instead of crashing

                auto [bi, br, bc] = empty_slots[next_empty++];
                HomeBox& box = boxes.at(bi);
                box.at(br, bc).setPokemon({
                    pokemon.id.value(),
                    pokemon.form_id.value(),
                    pokemon.form.value(),
                    pokemon.gender.value(),
                    pokemon.type1.value(),
                    pokemon.type2.value(),
                    pokemon.region.value(),
                    0, -1, false, false,
                    pokemon.ability[0],
                    PokemonType::NONE,
                    true, true
                });
                existing.insert(key);
            }

            if(group.size()>0){
                switch (group[0].id.value()) {
                    case 151:
                        add_spacers(Region::KANTO, 151, 22, "gen 1 spacer"); break;
                    case 251:
                        add_spacers(Region::JOHTO, 251, 24, "gen 2 spacer"); break;
                    case 386:
                        add_spacers(Region::HOENN, 386, 22, "gen 3 spacer"); break;
                    case 721:
                        add_spacers(Region::KALOS, 721, 22, "gen 6 spacer"); break;
                    case 809:
                        add_spacers(Region::ALOLA, 809, 18, "gen 7 spacer"); break;
                    case 905:
                        add_spacers(Region::GALAR, 905, 13, "gen 8 spacer"); break;
                    case 1025:
                        add_spacers(Region::PALDEA, 1025, 19, "gen 9 spacer"); break;
                    default: break;

                }
            }
        }
    };

    auto left_comb = [&](int start, int end) -> bool{
        bool swapped = false;
        for(int i = start; i <= end - 1; i++){
            bool temp = sort_into_correct_boxes(env, context, i, i+1);
            if(!reconcile_box(env, context,i+1)){
                build_box(env, context, i);
                build_box(env, context, i+1);
                pad_with_placeholders();
                i--;
                continue;
            }if(temp){
                swapped = true;
                boxes.store(i);
                boxes.store(i+1);
            }
        }
        return swapped;
    };
    auto right_comb = [&](int start, int end) -> bool{
        bool swapped = false;
        for(int i = end - 1; i >= start; i--){
            bool temp = sort_into_correct_boxes(env, context, i, i+1);
            if(!reconcile_box(env, context,i)){
                build_box(env, context, i);
                build_box(env, context, i+1);
                pad_with_placeholders();
                i++;
                continue;
            }if(temp){
                swapped = true;
                boxes.store(i);
                boxes.store(i+1);
            }
        }
        return swapped;
    };
    auto sort_each = [&](int start, int end) -> void{
        for(int i = start; i <= end; i++){
            bool temp = sort_box(env, context, i);
            if(!reconcile_box(env, context,i)){
                build_box(env, context, i);
                pad_with_placeholders();
                i--;
                continue;
            }if(temp){
                boxes.store(i);
            }
        }
    };

    auto pause_to_save = [&]() -> bool{
        bool succeeded = true;
        GameStatus return_game = game_open;

        succeeded = navigate_menus_to(env, context, PageID::MAIN_MENU);
        navigate_menus_to(env, context, PageID::BOX_VIEW, return_game);

        context.wait_for_all_requests();
        return succeeded;
    };



    // auto save_temp_status








    navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_HOME);


    WallClock start_time = WallClock::min();

    for(int i = start; i <= end; i++){
        build_box(env, context, i);
    }

    pad_with_placeholders();

    do{
        while(true){
            // We are starting at the right side, going left

            if(!right_comb(start, end))break;

            // Every 25 minutes or so, save the game to make sure
            if(WallClock::min()-start_time>=std::chrono::minutes(25)){

                start_time = WallClock::min();

                if(!pause_to_save()){
                    boxes = HomeStorage();
                }
            }

            if(!left_comb(start, end))break;


                    // Every 25 minutes or so, save the game to make sure
            if(WallClock::min()-start_time>=std::chrono::minutes(25)){

                start_time = WallClock::min();

                if(!pause_to_save()){
                    boxes = HomeStorage();
                }
            }

        }
    }while(!pause_to_save());

    sort_each(start, end);

    pause_to_save();
}



void HomeEnvironment::detect_home(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool single_page){
    context.wait_for_all_requests();

    HomeTitleScreenWatcher titleWatcher(COLOR_BLUE);
    HomeMainMenuWatcher mainMenuWatcher(COLOR_BLUE);
    HomeGameSelectWatcher gameSelectWatcher(COLOR_BLUE);
    HomeListViewWatcher listWatcher(COLOR_BLUE);
    HomeSummaryViewWatcher summaryWatcher(COLOR_BLUE);
    HomeMarkingsViewWatcher markingsWatcher(COLOR_BLUE);
    HomeBoxViewWatcher boxWatcher(COLOR_BLUE);
    NameBoxWatcher nameBoxWatcher(COLOR_BLUE);
    int ret = wait_until(
        env.console, context, 30s,
        {
            titleWatcher,
            mainMenuWatcher,
            gameSelectWatcher,
            listWatcher,
            summaryWatcher,
            markingsWatcher,
            boxWatcher,
            nameBoxWatcher
        }
    );

    context.wait_for_all_requests();
    VideoSnapshot screen = env.console.video().snapshot();

    switch(ret){
    case 0:
        current_view = PageID::TITLE_SCREEN;
        game_open = GameStatus::NONE;
        // env.console.log("At Title Screen");
        break;
    case 1:
        current_view = PageID::MAIN_MENU;
        game_open = GameStatus::NONE;
        // env.console.log("At Main Menu");
        break;
    case 2:
        current_view = PageID::GAME_SELECTION;
        game_open = GameStatus::NONE;
        // env.console.log("At Game Selection");
        break;
    case 3:
        current_view = PageID::LIST_VIEW;
        game_open = GameStatus::UNKNOWN;
        // env.console.log("At List View");
        break;
    case 4:
        current_view = PageID::SUMMARY_VIEW;
        // TODO: Break out summary view into box version and list version
        game_open = GameStatus::UNKNOWN;
        // env.console.log("At Summary View");
        break;
    case 5:
        current_view = PageID::MARKINGS_VIEW;
        // TODO: Break out Markings view into box version and list version
        game_open = GameStatus::UNKNOWN;
        // env.console.log("At Markings View");
        break;
    case 6:
        current_view = PageID::BOX_VIEW;
        identify_game_icon(env, context);
        cursor.emplace(env, context, single_page, game_open != GameStatus::POKEMON_HOME);
        // TODO: Implement primitive game status detection: Is the game icon slot green? if so, Home, else Unknown.
        // TODO: Implement game status detection
        // TODO: Get current home box as well as secondary box (if applicable)
        // env.console.log("At Box View");
        break;
    case 7:
        pbf_mash_button(context, BUTTON_B, 5*TICKS_PER_SECOND);
        context.wait_for_all_requests();
        detect_home(env, context, single_page);
        break;
    default:
        current_view = PageID::UNKNOWN;
        game_open = GameStatus::UNKNOWN;
        // env.console.log("At Unknown");
        break;
    }

    context.wait_for_all_requests();
}

std::string HomeEnvironment::get_view(){
    switch(current_view){
        case PageID::CURRENT: return "Current";
        case PageID::TITLE_SCREEN: return "Title Screen";
        case PageID::MAIN_MENU: return "Main Menu";
        case PageID::GAME_SELECTION: return "Game Selection";
        case PageID::BOX_VIEW: return "Box View";
        case PageID::SUMMARY_VIEW: return "Summary View";
        case PageID::MARKINGS_VIEW: return "Markings View";
        case PageID::LIST_VIEW: return "List View";
        case PageID::UNKNOWN: return "Unknown";
        break;
    }
        return "failed";
}

size_t HomeEnvironment::get_box(){
    return cursor.value().get_page();
}

HomeCursor HomeEnvironment::get_cursor(){
    return cursor.value();
}

void HomeEnvironment::scan_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeCursor location){
    PokemonData pokemon;

    SummaryWatcher summary_page(COLOR_CYAN);

    int ret = -1;
    try{
        ret = wait_until(
            env.console, context,
            std::chrono::milliseconds(5*TICKS_PER_SECOND),
            {summary_page}
        );
    }catch(std::exception& e){
        send_program_notification(
            env, NOTIFICATION_ERROR_FATAL,
            COLOR_RED,
            e.what(),
            {}, "",
            env.console.video().snapshot()
            );
        pbf_wait(context, 2s);
        context.wait_for_all_requests();
        throw;
    }catch(...){
        send_program_notification(
            env, NOTIFICATION_ERROR_FATAL,
            COLOR_RED,
            "unknown type caught.",
            {}, "",
            env.console.video().snapshot()
            );
        pbf_wait(context, 2s);
        context.wait_for_all_requests();
        throw;
    }


    if(ret==0){
        pokemon = summary_page.get_pokemon(env.console, env.console.video().snapshot(), pokedex_information);
        env.console.log(pokemon.to_string());
    }

    boxes.at(location.get_page()).at(location.get_row(),location.get_col()) = HomeSlot(location.get_row(),location.get_col(),pokemon);

}

PokemonData HomeEnvironment::scan_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    PokemonData pokemon;

    SummaryWatcher summary_page(COLOR_CYAN);

    context.wait_for_all_requests();

    int ret = wait_until(
        env.console, context,
        std::chrono::milliseconds(10*TICKS_PER_SECOND),
        {summary_page}
        );

    try{
        if(ret==0){
            pokemon = summary_page.get_pokemon(env.console, env.console.video().snapshot(), pokedex_information);
            env.console.log(pokemon.to_string());
        }
    }catch(Exception& e){
        context.wait_for_all_requests();
        send_program_notification(
            env, NOTIFICATION_ERROR_RECOVERABLE,
            COLOR_RED,
            e.message(),
            {}, "",
            env.console.video().snapshot()
            );
        throw;
    }

    return pokemon;
}

std::optional<HomeCursor> HomeEnvironment::locate_pokemon(PokemonData& to_locate){
    auto temp = boxes.find_pokemon(to_locate);

    if (temp.has_value()){
        auto [row, col, box] = temp.value();
        return HomeCursor(row, col, box);
    } else {
        return std::nullopt;
    }
}



CursorActionResponse HomeEnvironment::handle_errors(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const CursorActionResponse& response){

    send_program_notification(
        env, NOTIFICATION_ERROR_RECOVERABLE,
        COLOR_RED,
        response.message,
        {}, "",
        env.console.video().snapshot()
        );

    return response;
}

void HomeEnvironment::initialize_navigation_map(SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
    navigation_map[PageID::TITLE_SCREEN] = {
        {PageID::MAIN_MENU,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {

            HomeLoginDialogueWatcher loginWatcher(COLOR_BLUE);
            HomeMainMenuWatcher mainMenuWatcher(COLOR_BLUE);

            // Press A
            env.console.log("Press A");
            pbf_press_button(context, BUTTON_A, 10, 100);

            context.wait_for_all_requests();

            // Check for LoginDialogueDetector == 0
            int ret = wait_until(
                env.console, context, 5000ms, {
                    loginWatcher
                });

            // If true, wait for Main Menu (Finished logging in, we can wait 2.5 mins)
            if(ret==0){
                ret = wait_until(
                    env.console, context, 150000ms, {
                        mainMenuWatcher
                    });

                // Can't find main menu
                if(ret!=0){
                    throw;
                }
            }
            // Else, error out
            else{
                throw;
            }

        }},
    };

    navigation_map[PageID::MAIN_MENU] = {
        {PageID::GAME_SELECTION,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {

            HomeGameSelectWatcher gameSelectWatcher(COLOR_BLUE);

            // Press A, then verify we are at the game selection screen
            // env.console.log("Press A");
            pbf_press_button(context, BUTTON_A, 10, 100);

            context.wait_for_all_requests();

            int ret = wait_until(
                env.console, context, 5000ms, {
                    gameSelectWatcher
                });

            // If true, wait for LoginDialogueDetector!=0 (Finished logging in)
            if(ret!=0){
                throw;
            }
        }},
    };

    navigation_map[PageID::GAME_SELECTION] = {
        {PageID::MAIN_MENU,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {

            HomeMainMenuWatcher mainMenuWatcher(COLOR_BLUE);

            // Press B, then verify we are at the game selection screen
            // env.console.log("Press B");
            pbf_press_button(context, BUTTON_B, 10, 100);

            context.wait_for_all_requests();

            int ret = wait_until(
                env.console, context, 5000ms, {
                    mainMenuWatcher
                });

            // If true, wait for LoginDialogueDetector!=0 (Finished logging in)
            if(ret!=0){
                throw;
            }
        }},
        {PageID::BOX_VIEW,
         [this](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeBoxViewWatcher boxWatcher(COLOR_BLUE);

            // Cycle through games until we see the correct game on screen, then press A and wait for login.
            std::string target_name;
            switch(game_open){
                case GameStatus::NONE: target_name = "Start without connecting a game"; break;
                case GameStatus::POKEMON_HOME: target_name = "Start without connecting a game"; break;
                case GameStatus::POKEMON_PLA: target_name = "Pokémon Legends: Arceus"; break;
                case GameStatus::POKEMON_PIKACHU: target_name = "Pokémon: Let's Go, Pikachu!"; break;
                case GameStatus::POKEMON_EEVEE: target_name = "Pokémon: Let's Go, Eevee!"; break;
                case GameStatus::POKEMON_DIAMOND: target_name = "Pokémon Brilliant Diamond"; break;
                case GameStatus::POKEMON_PEARL: target_name = "Pokémon Shining Pearl"; break;
                case GameStatus::POKEMON_SWORD: target_name = "Pokémon Sword"; break;
                case GameStatus::POKEMON_SHIELD: target_name = "Pokémon Shield"; break;
                case GameStatus::POKEMON_SCARLET: target_name = "Pokémon Scarlet"; break;
                case GameStatus::POKEMON_VIOLET: target_name = "Pokémon Violet"; break;
                default: throw;
                    break;
            }

            // Navigate to correct game
            char chars[] = "\n\r—";
            ImageFloatBox game_checker(0.0455, 0.244, 0.435, 0.057);
            std::string text = OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), game_checker));
            for(auto a:chars){text.erase(std::remove(text.begin(),text.end(), a),text.end());}
            VideoOverlaySet box_render(env.console);

            while (text != target_name){
                // env.console.log("Found game " + text + OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), game_checker)));
                pbf_press_dpad(context, DPAD_RIGHT, 10, 20);

                context.wait_for_all_requests();
                text = OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), game_checker));
                for(auto a:chars){text.erase(std::remove(text.begin(),text.end(), a),text.end());}
            };

            // env.console.log("Found game " + text + OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), game_checker)));

            // Press A twice, then verify we are at the Box View screen
            // env.console.log("Press A");
            pbf_press_button(context, BUTTON_A, 10, 100);
            pbf_press_button(context, BUTTON_A, 10, 100);

            context.wait_for_all_requests();

            int ret = wait_until(
                env.console, context, 150000ms, {
                    boxWatcher
                });

            if(ret!=0){
                throw;
            }
        }},
    };

    navigation_map[PageID::BOX_VIEW] = {
        {PageID::MAIN_MENU,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeLogoutDialogueWatcher logoutWatcher(COLOR_BLUE);
            HomeMainMenuWatcher mainMenuWatcher(COLOR_BLUE);

            // Press Plus, then trigger logout sequence
            // env.console.log("Press Plus");
            // env.console.log("Press A");
            pbf_press_button(context, BUTTON_PLUS, 10, 150);
            pbf_press_button(context, BUTTON_A, 10, 100);

            context.wait_for_all_requests();

            int ret = wait_until(
                env.console, context, 60s, {
                logoutWatcher
            });

            if(ret!=0){
                pbf_press_button(context, BUTTON_A, 10, 150);
                throw HomeSaveFailedError{};
            }

            pbf_press_button(context, BUTTON_A, 10, 100);

            context.wait_for_all_requests();

            // MASH B until main menu just in case the A did not register (B is safer to mash here)
            ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_mash_button(context, BUTTON_B, 15s);
                },
                {
                    mainMenuWatcher
                }
            );

            if(ret!=0){
                throw;
            }

            context.wait_for_all_requests();

        }},
        {PageID::SUMMARY_VIEW,
         [this](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeSummaryViewWatcher summaryWatcher(COLOR_BLUE);

            // Open menu, then go to summary. Assumes cursor is in the right position. (Use other code to check if the correct button is orange later)
            // env.console.log("Press A, then down, then A");
            pbf_press_button(context, BUTTON_A, 10, 40);
            pbf_press_button(context, BUTTON_DOWN, 10, 40);
            pbf_press_button(context, BUTTON_A, 10, 40);

            context.wait_for_all_requests();

            int ret = wait_until(
                env.console, context, 5000ms, {
                    summaryWatcher
                });

            if(ret!=0){
                detect_home(env, context);
                navigate_menus_to(env, context,PageID::SUMMARY_VIEW);
            }
        }},
        {PageID::MARKINGS_VIEW,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeMarkingsViewWatcher markingsWatcher(COLOR_BLUE);

            // Open menu, then go to markings. Assumes cursor is in the right position. (Use other code to check if the correct button is orange later)
            // env.console.log("Press A, then down, then down, then a");

            pbf_press_button(context, BUTTON_A, 10, 40);
            pbf_press_button(context, BUTTON_DOWN, 10, 40);
            pbf_press_button(context, BUTTON_DOWN, 10, 40);
            pbf_press_button(context, BUTTON_A, 10, 40);

            context.wait_for_all_requests();

            int ret = wait_until(
                env.console, context, 5000ms, {
                    markingsWatcher
                });

            if(ret!=0){
                throw;
            }
        }},
        {PageID::LIST_VIEW,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeListViewWatcher listWatcher(COLOR_BLUE);

            // Press X
            // env.console.log("Press X");

            pbf_press_button(context, BUTTON_X, 10, 40);

            context.wait_for_all_requests();

            int ret = run_until<ProControllerContext>( // Press X 5 times in case it didn't register
                env.console, context,
                [](ProControllerContext& context){
                    for(int i = 0; i < 5; i++){
                        pbf_press_button(context, BUTTON_X, 500ms, 2s);
                    }
                },
                {
                    listWatcher
                }
            );

            if(ret!=0){
                throw;
            }
        }},
    };

    navigation_map[PageID::SUMMARY_VIEW] = {
        {PageID::BOX_VIEW,
         [this](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeBoxViewWatcher boxWatcher(COLOR_BLUE);

            // Press B
            // env.console.log("Press B");

            pbf_press_button(context, BUTTON_B, 10, 40);

            context.wait_for_all_requests();

            int ret = run_until<ProControllerContext>( // Press B 5 times in case it didn't register
                env.console, context,
                [](ProControllerContext& context){
                    for(int i = 0; i < 5; i++){
                        pbf_press_button(context, BUTTON_B, 500ms, 2s);
                    }
                },
                {
                    boxWatcher
                }
            );

            if(ret!=0){
                throw;
            }

            identify_game_icon(env, context);

         }},
    };

    navigation_map[PageID::MARKINGS_VIEW] = {
        {PageID::BOX_VIEW,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeBoxViewWatcher boxWatcher(COLOR_BLUE);

             //Press B
            // env.console.log("Press B");

            pbf_press_button(context, BUTTON_B, 10, 40);

            context.wait_for_all_requests();

            int ret = run_until<ProControllerContext>( // Press B 5 times in case it didn't register
                env.console, context,
                [](ProControllerContext& context){
                    for(int i = 0; i < 5; i++){
                        pbf_press_button(context, BUTTON_B, 500ms, 2s);
                    }
                },
                {
                    boxWatcher
                }
            );

            if(ret!=0){
                throw;
            }
        }},
    };

    navigation_map[PageID::LIST_VIEW] = {
        {PageID::BOX_VIEW,
         [this](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeBoxViewWatcher boxWatcher(COLOR_BLUE);

            //Press B
            // env.console.log("Press B");

            pbf_press_button(context, BUTTON_B, 10, 40);

            context.wait_for_all_requests();

            int ret = run_until<ProControllerContext>( // Press B 5 times in case it didn't register
                env.console, context,
                [](ProControllerContext& context){
                    for(int i = 0; i < 5; i++){
                        pbf_press_button(context, BUTTON_B, 500ms, 2s);
                    }
                },
                {
                    boxWatcher
                }
            );

            if(ret!=0){
                throw;
            }

            identify_game_icon(env, context);

        }},
    };
}

std::string HomeEnvironment::get_filter_menu_read(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();


    ImageFloatBox dialog_box_secondary(0.7, 0.325, 0.21, 0.05);
    ImageViewRGB32 dialog_image = extract_box_reference(screen, dialog_box_secondary);
    auto result = FilterMenuConfirmReader::instance().read_substring(
        env.console, Language::English, dialog_image,
        OCR::BLACK_TEXT_FILTERS()
        );
    if (!result.results.empty()){ // program is entered specifically into Markings
        return result.results.cbegin()->second.token;
    }
    ImageFloatBox dialog_box_top(0.7, 0.1, 0.21, 0.05);
    dialog_image = extract_box_reference(screen, dialog_box_top);
    result = FilterMenuConfirmReader::instance().read_substring(
        env.console, Language::English, dialog_image,
        OCR::BLACK_TEXT_FILTERS()
        );
    if (result.results.empty()){ // Make sure we are in the main menu, not in anything
        return "";
    }else {
        return result.results.cbegin()->second.token;
    }

}


void HomeEnvironment::scroll_filter_menu(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string dest, int retry_count) {
    env.console.log("Scrolling to "+dest);

    if(retry_count >= MAX_RETRIES){
        throw std::runtime_error("Hit max failures on filter menu navigation");
    }

    int scrolls = 0;


    std::string menu_read = get_filter_menu_read(env, context);

    if(menu_read == ""){ // Could not find an open menu. Try to recover by going to main menu and returning here.
        pbf_mash_button(context, BUTTON_B, 3000ms);

        detect_home(env, context);
        navigate_menus_to(env, context, PageID::LIST_VIEW);
        pbf_wait(context, 1000ms);

        pbf_press_button(context, BUTTON_X, 10, 80);
    }else if(menu_read != "main"){ // Filter menu open, but at a bad position. One b press will work.
        pbf_press_button(context, BUTTON_B, 10, 80);
    }

    // Expected behavior: sitting in main menu with "Filter" at the top of the screen

    context.wait_for_all_requests();
    VideoSnapshot screen = env.console.video().snapshot();

    FilterCursorWatcher filterWatcher({0,0,1,1}, COLOR_WHITE);
    if(wait_until(env.console, context, 2000ms, {filterWatcher})==-1){
        // TODO: Do error correction for no cursor found
    }

    // Check top box
    ImageFloatBox dialog_box_selected(0.7, filterWatcher.location().second-0.06, 0.2, 0.05);
    ImageViewRGB32 dialog_image = extract_box_reference(screen, dialog_box_selected);
    auto result = FilterMenuReader::instance().read_substring(
        env.console, Language::English, dialog_image,
        OCR::WHITE_TEXT_FILTERS()
        );
    scrolls = result.results.empty()?1:FilterMenuReader::instance().distance_to(result.results.cbegin()->second.token, dest);

    // Expected behavior: Should have scanned all four possible buttons to determine which one is moused over (black text). Also should know distance to target button.

    if(scrolls<0){
        while(scrolls++<0){
            pbf_press_dpad(context, DPAD_UP, 10, 35);
        }
    }else if(scrolls>0){
        while(scrolls-->0){
            pbf_press_dpad(context, DPAD_DOWN, 10, 35);
        }
    }else if(retry_count==MAX_RETRIES-1){
        pbf_press_dpad(context, DPAD_DOWN, 10, 35);
    }

    // Expected behavior: should be moused over the target button, press A to navigate into it if so. Otherwise, reset.
    pbf_press_button(context, BUTTON_A, 10, 60);

    context.wait_for_all_requests();

    menu_read = get_filter_menu_read(env, context);
    if(menu_read != dest){
        scroll_filter_menu(env, context, dest, retry_count+1);
    }

}


void HomeEnvironment::swap_pokemon(SingleSwitchProgramEnvironment &env, ProControllerContext &context, const HomeCursor& slot1, const HomeCursor& slot2){

    if (!boxes.at(slot1.get_page()).at(slot1.get_row(), slot1.get_col()).isOccupied() && !boxes.at(slot2.get_page()).at(slot2.get_row(), slot2.get_col()).isOccupied()) {

    }else if (!boxes.at(slot1.get_page()).at(slot1.get_row(), slot1.get_col()).isOccupied()) {
        navigate_to(env, context, slot2);
        pick_up_pokemon(env, context);
        navigate_to(env, context, slot1);
        put_down_pokemon(env, context);
    } else if (!boxes.at(slot2.get_page()).at(slot2.get_row(), slot2.get_col()).isOccupied()) {
        navigate_to(env, context, slot1);
        pick_up_pokemon(env, context);
        navigate_to(env, context, slot2);
        put_down_pokemon(env, context);
    }else if (cursor->distance_to(slot1) <= cursor->distance_to(slot2)) {
        navigate_to(env, context, slot1);
        pick_up_pokemon(env, context);
        navigate_to(env, context, slot2);
        put_down_pokemon(env, context);
    } else {
        navigate_to(env, context, slot2);
        pick_up_pokemon(env, context);
        navigate_to(env, context, slot1);
        put_down_pokemon(env, context);
    }

    std::swap(boxes.at(slot1.get_page()).at(slot1.get_row(), slot1.get_col()).getPokemon(),
              boxes.at(slot2.get_page()).at(slot2.get_row(), slot2.get_col()).getPokemon());
    std::swap(boxes.at(slot1.get_page()).at(slot1.get_row(), slot1.get_col()).m_quick_color,
              boxes.at(slot2.get_page()).at(slot2.get_row(), slot2.get_col()).m_quick_color);

}



void HomeEnvironment::pick_up_pokemon(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
    if(!cursor.has_value()){
        cursor.emplace(env, context, true, game_open!=GameStatus::POKEMON_HOME);
    }

    cursor->pick_up_pokemon(env, context);
}

void HomeEnvironment::pick_up_pokemon_multi(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
    if(!cursor.has_value()){
        cursor.emplace(env, context, true, game_open!=GameStatus::POKEMON_HOME);
    }

    cursor->pick_up_pokemon_multi(env, context);
}

void HomeEnvironment::put_down_pokemon(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
    if(!cursor.has_value()){
        cursor.emplace(env, context, true, game_open!=GameStatus::POKEMON_HOME);
    }

    cursor->put_down_pokemon(env, context);

}

void HomeEnvironment::put_down_pokemon_multi(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
    if(!cursor.has_value()){
        cursor.emplace(env, context, true, game_open!=GameStatus::POKEMON_HOME);
    }

    cursor->put_down_pokemon_multi(env, context);

}


void HomeEnvironment::bail_out(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
    context.wait_for_all_requests();

    NameBoxWatcher nameBoxWatcher(COLOR_BLUE);
    int ret = wait_until(
        env.console, context, std::chrono::milliseconds(5000),
        {
            nameBoxWatcher
        }
    );

    if(ret==0){
        pbf_mash_button(context, BUTTON_B, 5*TICKS_PER_SECOND);
        context.wait_for_all_requests();
        detect_home(env, context, false);

    }


    cursor.value().identify_page(env, context, true);
}

}
}
}
