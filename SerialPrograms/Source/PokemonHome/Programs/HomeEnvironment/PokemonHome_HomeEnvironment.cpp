#include "PokemonHome_HomeEnvironment.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_RawOCR.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonHome/Inference/PokemonHome_HomeApplicationDetector.h"
#include <chrono>
#include <queue>


namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

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

HomeCursor::HomeCursor(SingleSwitchProgramEnvironment& env, ProControllerContext&){

}

void HomeCursor::locate_position(){
    // TODO: Implement locate_position logic
}

void HomeCursor::move_cursor_to(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const std::pair<size_t, size_t> cursor){
    // TODO: Implement cursor movement logic
}

void HomeCursor::pick_up_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // TODO: Implement pick up logic
}

void HomeCursor::put_down_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // TODO: Implement put down logic
}



PokemonHome_HomeEnvironment::PokemonHome_HomeEnvironment(SingleSwitchProgramEnvironment& env, ProControllerContext& context)
    : cursor(env, context)
{
    detect_home(env, context);
    initialize_navigation_map(env, context);
}

void PokemonHome_HomeEnvironment::navigate_to(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const PageID destination, const GameStatus game, const std::pair<size_t, size_t> cursor, const size_t box){
    std::vector<PageID> steps;
    if ((game != GameStatus::CURRENT && game != GameStatus::NONE && game != game_open) && current_view != PageID::GAME_SELECTION) {
        steps = find_navigation_path(env, context, current_view, PageID::GAME_SELECTION);
        perform_navigation_steps(env, context, steps);
        // Open desired game
        game_open = game;
    }
    steps = find_navigation_path(env, context, current_view, destination);
    perform_navigation_steps(env, context, steps);
}

std::vector<PageID> PokemonHome_HomeEnvironment::find_navigation_path(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PageID from, PageID to){
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

        env.console.log("Looking at moves from " + to_string(current));


        // If we've reached the target page, cache and return the path
        if (current == to) {
            current_path.push_back(current);
            navigation_cache[key] = current_path; // Cache the result
            return current_path;
        }

        // Explore all transitions from the current page
        const auto& transitions = navigation_map[current];
        for (const auto& [next, _] : transitions) {
            env.console.log("considering moving to " + to_string(next));

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


void PokemonHome_HomeEnvironment::detect_home(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    VideoSnapshot screen = env.console.video().snapshot();

    HomeTitleScreenWatcher titleWatcher(COLOR_BLUE);
    HomeMainMenuWatcher mainMenuWatcher(COLOR_BLUE);
    HomeGameSelectWatcher gameSelectWatcher(COLOR_BLUE);
    HomeListViewWatcher listWatcher(COLOR_BLUE);
    HomeSummaryViewWatcher summaryWatcher(COLOR_BLUE);
    HomeMarkingsViewWatcher markingsWatcher(COLOR_BLUE);
    HomeBoxViewWatcher boxWatcher(COLOR_BLUE);
    int ret = wait_until(
        env.console, context, std::chrono::milliseconds(5000),
        {
            titleWatcher,
            mainMenuWatcher,
            gameSelectWatcher,
            listWatcher,
            summaryWatcher,
            markingsWatcher,
            boxWatcher
        }
        );

    switch(ret){
    case 0:
        current_view = PageID::TITLE_SCREEN;
        game_open = GameStatus::NONE;
        env.console.log("At Title Screen");
        break;
    case 1:
        current_view = PageID::MAIN_MENU;
        game_open = GameStatus::NONE;
        env.console.log("At Main Menu");
        break;
    case 2:
        current_view = PageID::GAME_SELECTION;
        game_open = GameStatus::NONE;
        env.console.log("At Game Selection");
        break;
    case 3:
        current_view = PageID::LIST_VIEW;
        game_open = GameStatus::UNKNOWN;
        env.console.log("At List View");
        break;
    case 4:
        current_view = PageID::SUMMARY_VIEW;
        // TODO: Break out summary view into box version and list version
        game_open = GameStatus::UNKNOWN;
        env.console.log("At Summary View");
        break;
    case 5:
        current_view = PageID::MARKINGS_VIEW;
        // TODO: Break out Markings view into box version and list version
        game_open = GameStatus::UNKNOWN;
        env.console.log("At Markings View");
        break;
    case 6:
        current_view = PageID::BOX_VIEW;
        game_open = GameStatus::UNKNOWN;
        // TODO: Implement game status detection
        // TODO: Get current home box as well as secondary box (if applicable)
        env.console.log("At Box View");
        break;
    default:
        current_view = PageID::UNKNOWN;
        game_open = GameStatus::UNKNOWN;
        env.console.log("At Unknown");
        break;
    }
}

std::string PokemonHome_HomeEnvironment::get_view(){
    switch(current_view){
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

                // If true, wait for LoginDialogueDetector!=0 (Finished logging in)
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
            env.console.log("Press A");
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
            env.console.log("Press B");
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
                case GameStatus::POKEMON_VIOLET: target_name = "Pokémon Violet";
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
                env.console.log("Found game " + text + OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), game_checker)));
                pbf_press_dpad(context, DPAD_RIGHT, 10, 20);

                context.wait_for_all_requests();
                text = OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), game_checker));
                for(auto a:chars){text.erase(std::remove(text.begin(),text.end(), a),text.end());}
            } ;


            // Press A twice, then verify we are at the Box View screen
            env.console.log("Press A");
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
            env.console.log("Press Plus");
            env.console.log("Press A");
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
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeSummaryViewWatcher summaryWatcher(COLOR_BLUE);

            // Open menu, then go to summary. Assumes cursor is in the right position. (Use other code to check if the correct button is orange later)
            env.console.log("Press A, then down, then A");
            pbf_press_button(context, BUTTON_A, 10, 40);
            pbf_press_button(context, BUTTON_DOWN, 10, 40);
            pbf_press_button(context, BUTTON_A, 10, 40);

            context.wait_for_all_requests();

            int ret = wait_until(
                env.console, context, 5000ms, {
                    summaryWatcher
                });

            if(ret!=0){
                throw;
            }
        }},
        {PageID::MARKINGS_VIEW,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeMarkingsViewWatcher markingsWatcher(COLOR_BLUE);

            // Open menu, then go to markings. Assumes cursor is in the right position. (Use other code to check if the correct button is orange later)
            env.console.log("Press A, then down, then down, then a");

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
            env.console.log("Press X");

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
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeBoxViewWatcher boxWatcher(COLOR_BLUE);

            // Press B
            env.console.log("Press B");

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

    navigation_map[PageID::MARKINGS_VIEW] = {
        {PageID::BOX_VIEW,
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeBoxViewWatcher boxWatcher(COLOR_BLUE);

             //Press B
            env.console.log("Press B");

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
         [](SingleSwitchProgramEnvironment& env, ProControllerContext& context) {
            HomeBoxViewWatcher boxWatcher(COLOR_BLUE);

            //Press B
            env.console.log("Press B");

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
}

}
}
}
