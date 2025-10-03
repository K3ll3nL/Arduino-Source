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
#include <chrono>
#include <queue>
#include <cmath>
#include <algorithm>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;

static size_t MAX_RETRIES = 5;

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

    locate_position(env, context);
    if(!single_page){
        identify_page(env, context, false);
        if(box==0){
            identify_page(env,context, true, UINT_MAX);
        }
    }
    this->secondary_open = secondary_open;

}

HomeCursor::HomeCursor(size_t row, size_t col)
    : secondary_open(false), row(row), col(col), box(0), holding_pokemon(false) {}

HomeCursor::HomeCursor(HomeCursor other, size_t box)
    : secondary_open(false), row(other.row), col(other.col), box(box), holding_pokemon(false) {}

HomeCursor::HomeCursor(size_t row, size_t col, size_t box)
    : secondary_open(false), row(row), col(col), box(box), holding_pokemon(false) {}

HomeCursor::HomeCursor(std::pair<size_t,size_t>& other)
    : secondary_open(false), row(other.first), col(other.second), box(0), holding_pokemon(false) {}





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
    // TODO: Implement pick up logic
    //return {CursorActionResult::FAILURE, "Function still undefined"};

    pbf_press_button(context, BUTTON_Y, 10, 70);    // press y button
    holding_pokemon = true;
    return {CursorActionResult::SUCCESS, "Function still undefined"};

}

CursorActionResponse HomeCursor::put_down_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // TODO: Implement put down logic
    // return {CursorActionResult::FAILURE, "Function still undefined"};
    pbf_press_button(context, BUTTON_Y, 10, 70);    // press y button
    holding_pokemon = false;
    return {CursorActionResult::SUCCESS, "Function still undefined"};
}

void HomeCursor::align_col(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const size_t& new_col){
    size_t total_cols = (secondary_open ? 12 : 6);
    size_t dist = (new_col > col) ? new_col - col : col - new_col;

    if (dist <= total_cols / 2) {
        // direct movement
        if (new_col > col){
            for (size_t i = 0; i < new_col - col; ++i)
                pbf_press_dpad(context, DPAD_RIGHT, 10, 30);
        } else {
            for (size_t i = 0; i < col - new_col; ++i)
                pbf_press_dpad(context, DPAD_LEFT, 10, 30);
        }
    } else {
        // wrap-around
        if (new_col > col){
            for (size_t i = 0; i < total_cols - (new_col - col); ++i)
                pbf_press_dpad(context, DPAD_LEFT, 10, 30);
        } else {
            for (size_t i = 0; i < total_cols - (col - new_col); ++i)
                pbf_press_dpad(context, DPAD_RIGHT, 10, 30);
        }
    }

    col = new_col;

}

void HomeCursor::align_row(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const size_t& new_row){

    // direct nav up or down through rows
    if (!(row == 0 && new_row == 4) && !(new_row == 0 && row == 4)) {
        for (size_t i = row; i < new_row; ++i) {
            pbf_press_dpad(context, DPAD_DOWN, 10, 30);
        }
        for (size_t i = new_row; i < row; ++i) {
            pbf_press_dpad(context, DPAD_UP, 10, 30);
        }
    } else { // wrap around is faster to move between row or last row
        if (row == 0 && new_row == 4) {
            for (size_t i = 0; i <= 2; ++i) {
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

CursorActionResponse HomeCursor::position_cursor(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const HomeCursor& dest_cursor, size_t retry_count){

    if(dest_cursor.row>MAX_ROWS){
        return {CursorActionResult::ERROR_RECOVERABLE, "row "+std::to_string(dest_cursor.row)+" out of scope"};
    }
    if(dest_cursor.col>(secondary_open?2:1)*MAX_COLUMNS){
        return {CursorActionResult::ERROR_RECOVERABLE, "column "+std::to_string(dest_cursor.col)+" out of scope"};
    }


    if (retry_count > MAX_RETRIES) {
        return {CursorActionResult::FAILURE, "Reached maximum retry attempts"};
    }


    if(retry_count > 3){ // Last resort for checking that the cursor did not end up at the bottom or top row. // TODO: Add detection for top and bottom row buttons that don't appear in the normal grid
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


    // Align column if not already aligned
    if(dest_cursor.col!=col){
        align_col(env, context, dest_cursor.col);
    }

    // Align row if not already aligned
    if(dest_cursor.row!=row){
        align_row(env, context, dest_cursor.row);
    }


    locate_position(env, context);


    if(row!=dest_cursor.row||col!=dest_cursor.col || row > MAX_ROWS || col > (secondary_open?2:1)*MAX_COLUMNS){
        return position_cursor(env, context, dest_cursor, retry_count+1);
    }

    return {CursorActionResult::SUCCESS, "Successfully moved cursor to ("+std::to_string(row)+", "+std::to_string(col)+")"};
}

CursorActionResponse HomeCursor::navigate_to_page(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const HomeCursor& dest_cursor){
    if(dest_cursor.box>200){
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

    int ret = wait_until(env.console, context, 2000ms, {handWatcher});
    if (ret == 0){
        auto [x, y] = handWatcher.location();
        row = y;
        col = x;
        env.console.log("HERE at ("+std::to_string(x)+", "+std::to_string(y)+")");
        return {CursorActionResult::SUCCESS, "Found cursor at ("+std::to_string(x)+", "+std::to_string(y)+")"};
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

CursorActionResponse HomeCursor::identify_page(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool hard_check, size_t expected) {
    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();
    VideoOverlaySet box_render(env.console);
    std::ostringstream ss;

    ImageFloatBox box_name_box(0.135, 0.105, 0.24, 0.04);
    ImageFloatBox home_box_checker(0.075, 0.72, 0.03205, 0.04);

    size_t box_name_top = 0, box_name_bottom = 0;

    auto extract_box_data = [&](VideoSnapshot& snapshot) {
        std::string temp;
        try {
            temp = sanitize_OCR2(OCR::ocr_read(Language::English, extract_box_reference(snapshot, box_name_box)));
            box_name_top = std::stoull(temp.substr(temp.find_last_of(' ') + 1));
        } catch (...) {
            box_name_top = 0;
        }

        try {
            temp = sanitize_OCR2(OCR::ocr_read(Language::English, extract_box_reference(snapshot, home_box_checker)));
            box_name_bottom = std::stoull(temp.substr(0, temp.find_last_of('/')));
        } catch (...) {
            box_name_bottom = 0;
        }
    };

    extract_box_data(screen);
    env.console.log(std::to_string(box_name_top) + "/" + std::to_string(box_name_bottom));

    if (!hard_check) {
        if(expected!=UINT_MAX){
            if (box_name_top == static_cast<size_t>(expected) || box_name_bottom == static_cast<size_t>(expected)) {
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





size_t HomeCursor::get_page() {
    return box;
}

 size_t HomeCursor::get_row() {
    return row;
}

 size_t HomeCursor::get_col() {
    return col;
}

int HomeCursor::distance_to(const HomeCursor& other){
    int x_dist = std::min(std::abs(static_cast<int>(other.col-col)),std::abs(static_cast<int>((secondary_open?12:6)-other.col-col)));
    int y_dist = std::min(static_cast<int>(3), std::abs(static_cast<int>(other.row-row)));
    int box_dist = std::abs(static_cast<int>(other.box-box));

    return x_dist+y_dist+box_dist;
}




PokemonHome_HomeEnvironment::PokemonHome_HomeEnvironment(SingleSwitchProgramEnvironment& env, ProControllerContext& context)
{
    detect_home(env, context);
    initialize_navigation_map(env, context);

    if(current_view==PageID::BOX_VIEW){ cursor.emplace(env, context, false, game_open!=GameStatus::POKEMON_HOME);}

    boxes = HomeStorage();
}

void PokemonHome_HomeEnvironment::navigate_menus_to(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const PageID destination, const GameStatus game){
    if(cursor.has_value()){
        cursor->secondary_open =
            (game != GameStatus::POKEMON_HOME) &&
            !(game == GameStatus::CURRENT && game_open == GameStatus::POKEMON_HOME);
    }

    if(current_view==destination&&(game==game_open||game==GameStatus::CURRENT))return;

    std::vector<PageID> steps;
    if ((game != GameStatus::NONE && game != game_open)) {
        if( game != GameStatus::CURRENT && current_view != PageID::GAME_SELECTION){
            steps = find_navigation_path(env, context, current_view, PageID::GAME_SELECTION);
            perform_navigation_steps(env, context, steps);
            // Open desired game
            game_open = game;
        }
        steps = find_navigation_path(env, context, current_view, destination);
        perform_navigation_steps(env, context, steps);
    }
}

void PokemonHome_HomeEnvironment::navigate_to(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const HomeCursor& dest_cursor){
    if(!cursor.has_value() || cursor->get_page() == 0){
        cursor.emplace(env, context, false, game_open!=GameStatus::POKEMON_HOME);
    }



    auto response = this->cursor->move_cursor_to(env, context, dest_cursor);
    if(response.result!=CursorActionResult::SUCCESS){
        handle_errors(env, context, response);
    }
}

std::vector<PageID> PokemonHome_HomeEnvironment::find_navigation_path(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PageID from, PageID to){
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

void PokemonHome_HomeEnvironment::perform_navigation_steps(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::vector<PageID>& steps){
    // Ensure we have at least two steps to navigate.
    if (steps.size() < 2) {
        throw std::runtime_error("Insufficient steps to perform navigation.");
    }

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
        transition_it->second(env, context);
    }
    current_view = steps.back();
    steps= {};
}

void PokemonHome_HomeEnvironment::identify_game_icon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
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

void PokemonHome_HomeEnvironment::detect_home(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool single_page){
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
        env.console, context, std::chrono::milliseconds(5000),
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

std::string PokemonHome_HomeEnvironment::get_view(){
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

size_t PokemonHome_HomeEnvironment::get_box(){
    return cursor.value().get_page();
}

HomeCursor PokemonHome_HomeEnvironment::get_cursor(){
    return cursor.value();
}

CursorActionResponse PokemonHome_HomeEnvironment::handle_errors(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const CursorActionResponse& response){

    send_program_notification(
        env, NOTIFICATION_ERROR_RECOVERABLE,
        COLOR_RED,
        response.message,
        {}, "",
        env.console.video().snapshot()
        );

    return response;
}

void PokemonHome_HomeEnvironment::initialize_navigation_map(SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
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
                env.console, context, 150000ms, {
                logoutWatcher
            });

            if(ret!=0){
                throw;
            }

            pbf_press_button(context, BUTTON_A, 10, 100);

            context.wait_for_all_requests();

            ret = wait_until(
                env.console, context, 5000ms, {
                    mainMenuWatcher
                });

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

            int ret = wait_until(
                env.console, context, 5000ms, {
                    listWatcher
                });

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

            int ret = wait_until(
                env.console, context, 5000ms, {
                    boxWatcher
                });

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

            int ret = wait_until(
                env.console, context, 5000ms, {
                    boxWatcher
                });

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

            int ret = wait_until(
                env.console, context, 5000ms, {
                    boxWatcher
                });

            while(ret!=0){
                pbf_press_button(context, BUTTON_B, 10, 40);
                ret = wait_until(
                    env.console, context, 5000ms, {
                        boxWatcher
                });
            }

            identify_game_icon(env, context);

        }},
    };
}

std::string PokemonHome_HomeEnvironment::get_filter_menu_read(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
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


void PokemonHome_HomeEnvironment::scroll_filter_menu(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string dest, size_t retry_count) {
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


void PokemonHome_HomeEnvironment::swap_pokemon(SingleSwitchProgramEnvironment &env, ProControllerContext &context, const HomeCursor& slot1, const HomeCursor& slot2){
    if(cursor->distance_to(slot1)<=cursor->distance_to(slot2)){
        navigate_to(env, context, slot1);
        pick_up_pokemon(env, context);
        navigate_to(env, context, slot2);
        put_down_pokemon(env, context);
    }else{
        navigate_to(env, context, slot2);
        pick_up_pokemon(env, context);
        navigate_to(env, context, slot1);
        put_down_pokemon(env, context);
    }
}

void PokemonHome_HomeEnvironment::pick_up_pokemon(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
    if(!cursor.has_value()){
        cursor.emplace(env, context, true, game_open!=GameStatus::POKEMON_HOME);
    }

    cursor->pick_up_pokemon(env, context);
}

void PokemonHome_HomeEnvironment::put_down_pokemon(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
    if(!cursor.has_value()){
        cursor.emplace(env, context, true, game_open!=GameStatus::POKEMON_HOME);
    }

    cursor->put_down_pokemon(env, context);

}

void PokemonHome_HomeEnvironment::bail_out(SingleSwitchProgramEnvironment &env, ProControllerContext &context){
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
