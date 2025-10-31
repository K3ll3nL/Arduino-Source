#include "PokemonHome_Enrichment.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/Images/ImageManip.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonTools/OCR/OCR_RawOCR.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/Pokemon_Types.h"
#include "Pokemon/Options/Pokemon_StatsHuntFilter.h"
#include "PokemonHome/Inference/PokemonHome_HomeApplicationDetector.h"
#include "PokemonHome/Inference/PokemonHome_SVItemReader.h"
#include "PokemonHome/Inference/PokemonHome_SummaryDetector.h"
#include "PokemonHome/Inference/PokemonHome_PokemonData.h"
#include "PokemonHome/Programs/Enrichment_Tools.h"
#include "PokemonHome/Programs/HomeEnvironment/PokemonHome_HomeEnvironment.h"
#include "PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_DirectionDetector.h"
#include "PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV/Programs/Battles/PokemonSV_SinglesBattler.h"
#include "PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include <qdir.h>
#include <qobject.h>
#include <cmath>
#include <ctime>
#include <windows.h>
#include <string>
#include <sstream>




struct BattleFailedException : public std::exception {
    const char* what() const noexcept override {
        return "Calyrex Fainted.";
    }
};

struct NoPokemonAvailableException : public std::exception {
    const char* what() const noexcept override {
        return "No Pokemon Found.";
    }
};
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;

const int MAX_RETRIES = 5;

Enrichment_Descriptor::Enrichment_Descriptor()
    : SingleSwitchProgramDescriptor(
          "PokemonHome:Enrichment",
          STRING_POKEMON + " Home", "Enrichment",
          "ComputerControl/blob/master/Wiki/Programs/PokemonHome/Enrichment.md",
          "Order boxes of " + STRING_POKEMON + ", run various XP grinding events across games, and deal with " + STRING_POKEMON + " from " + STRING_POKEMON + " Go.",
          ProgramControllerClass::StandardController_NoRestrictions,
          FeedbackType::REQUIRED,
          AllowCommandsWhenRunning::DISABLE_COMMANDS
          )
{}
struct Enrichment_Descriptor::Stats : public StatsTracker{
    Stats()

    {

    }

};
std::unique_ptr<StatsTracker> Enrichment_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

Enrichment::Enrichment()
    : HOME_FIRST_BOX(
          "<b>First dedicated box in Home:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          1, 1, 200
          )
    , HOME_LAST_BOX(
          "<b>Last dedicated box in Home:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          1, 1, 200
          )
    , PLA_FIRST_BOX(
          "<b>First Dedicated box in Legends Arceus:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          16, 1, 32
          )
    , PLA_LAST_BOX(
          "<b>Last Dedicated box in Legends Arceus:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          28, 1, 32
          )
    , SV_BOX_NAME(
          false,
          "<b>S/V Box Name:</b><br>Name of the box in Scarlet/Violet to use for storage",
          LockMode::LOCK_WHILE_RUNNING,
          "To Home",
          "box_order"
          )
    , WIPE_MARKINGS(
          "<b>Wipe Markings:</b><br>Clear markings from home or pick up where the program last left off.",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , DISPOSE_GOS(
          "<b>Go Disposal:</b><br>Get rid of Pokémon with a Go origin mark (compatible with PLA). Will not include shinies",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , STARTING_AT_DESK(
          "<b>Starting at Desk:</b><br>Check this if you are absolutely sure the save is starting inside the<br>Uva Academy at the Academy Ace Tournament desk.",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , EMERGENCY_DELOAD(
          "<b>DEBUG Emergency Deload:</b><br>Dump Pokemon into Home",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , NORMAL_DELOAD(
          "<b>DEBUG Normal Deload:</b><br>Avoid Second marking on home movements",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , SKIP_SETUP(
          "<b>DEBUG Skip Setup:</b><br>Skip loading pokemon into home boxes",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , SKIP_SORT(
          "<b>DEBUG Skip Sorting boxes:</b><br>Skip sorting pokemon in home boxes",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , NOTIFICATIONS({
          &NOTIFICATION_PROGRAM_FINISH
      })
{
    PA_ADD_OPTION(HOME_FIRST_BOX);            //number of first box to check and sort in HOME
    PA_ADD_OPTION(HOME_LAST_BOX);             //number of last box to check and sort in HOME
    PA_ADD_OPTION(PLA_FIRST_BOX);            //number of first box to check and sort in PLA
    PA_ADD_OPTION(PLA_LAST_BOX);             //number of last box to check and sort in PLA
    PA_ADD_OPTION(SV_BOX_NAME);         //box to use for s/v
    PA_ADD_OPTION(WIPE_MARKINGS);        //should wipe markings on home
    PA_ADD_OPTION(DISPOSE_GOS);         //Dispose of Go marked Pokémon
    PA_ADD_OPTION(STARTING_AT_DESK);     //pokemon sv start at desk
    PA_ADD_OPTION(EMERGENCY_DELOAD);     //pokemon sv start at desk
    PA_ADD_OPTION(NORMAL_DELOAD);     //pokemon sv start at desk
    PA_ADD_OPTION(SKIP_SETUP);          //pokemon sv start at desk
    PA_ADD_OPTION(SKIP_SORT);          //pokemon sv start at desk
    PA_ADD_OPTION(NOTIFICATIONS);
    QDir().mkpath("Home Storage");

    item_counts = {
        {"auspicious-armor",0},
        {"berry-sweet",0},
        {"chipped-pot",0},
        {"clover-sweet",0},
        {"cracked-pot",0},
        {"dawn-stone",0},
        {"dragon-scale",0},
        {"dubious-disc",0},
        {"dusk-stone",0},
        {"electirizer",0},
        {"fire-stone",0},
        {"flower-sweet",0},
        {"galarica-cuff",0},
        {"galarica-wreath",0},
        {"ice-stone",0},
        {"kings-rock",0},
        {"leaders-crest",0},
        {"leaf-stone",0},
        {"love-sweet",0},
        {"magmarizer",0},
        {"malicious-armor",0},
        {"masterpiece-teacup",0},
        {"metal-alloy",0},
        {"metal-coat",0},
        {"moon-stone",0},
        {"oval-stone",0},
        {"prism-scale",0},
        {"protector",0},
        {"razor-claw",0},
        {"razor-fang",0},
        {"reaper-cloth",0},
        {"ribbon-sweet",0},
        {"scroll-of-darkness",0},
        {"scroll-of-waters",0},
        {"shiny-stone",0},
        {"star-sweet",0},
        {"strawberry-sweet",0},
        {"sun-stone",0},
        {"sweet-apple",0},
        {"syrupy-apple",0},
        {"tart-apple",0},
        {"thunderstone",0},
        {"unremarkable-teacup",0},
        {"upgrade",0},
        {"water-stone",0}
    };

}

#include <string>
#include <sstream>
#include <memory>


std::string type_to_string(PokemonType type){
    const char * types[] = {
        "None",
        "Normal",
        "Fire",
        "Fighting",
        "Water",
        "Flying",
        "Grass",
        "Poison",
        "Electric",
        "Ground",
        "Psychic",
        "Rock",
        "Ice",
        "Bug",
        "Dragon",
        "Ghost",
        "Dark",
        "Steel",
        "Fairy",
    };
    return types[int(type)];
}

void home_clear_marking(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager){
    ImageFloatBox home_circle_marking_big(0.75, 0.5, 0.0025, 0.005);
    ImageFloatBox home_triangle_marking_big(0.87, 0.5, 0.0025, 0.005);
    ImageFloatBox home_square_marking_big(0.75, 0.62, 0.0025, 0.005);
    ImageFloatBox home_heart_marking_big(0.87, 0.62, 0.0025, 0.005);
    ImageFloatBox home_star_marking_big(0.75, 0.75, 0.0025, 0.005);
    ImageFloatBox home_diamond_marking_big(0.87, 0.75, 0.0025, 0.005);

    VideoSnapshot screen;
    VideoOverlaySet box_render(env.console);

    pbf_press_button(context, BUTTON_A, 10, 180);

    home_manager.detect_home(env, context, true);

    int row = home_manager.get_cursor().get_row();
    int col = home_manager.get_cursor().get_col();

    pbf_press_button(context, BUTTON_A, 10, 18);
    pbf_press_dpad(context, DPAD_DOWN,10, 40);
    pbf_press_dpad(context, DPAD_DOWN,10, 40);

    // Check can be marked in the first place
    context.wait_for_all_requests();
    screen = env.console.video().snapshot();
    ImageFloatBox can_mark(0.16 + (col * .0705), std::min(0.58, 0.4 + (row*0.1)), 0.0075, 0.01);
    FloatPixel scan_val = image_stats(extract_box_reference(screen, can_mark)).average;
    box_render.add(COLOR_RED, can_mark);
    env.console.log(std::to_string(scan_val.r)+" "+std::to_string(scan_val.g)+" "+std::to_string(scan_val.b));
    if(!(scan_val.r==255&&scan_val.b==0)){
        pbf_press_button(context, BUTTON_B, 10, 27);
        context.wait_for_all_requests();
        return;
    }

    pbf_press_button(context, BUTTON_A, 10, 27);

    std::vector<ImageFloatBox> marking_list_big = {home_circle_marking_big,home_triangle_marking_big,home_square_marking_big,home_heart_marking_big,home_star_marking_big,home_diamond_marking_big};

    for(int i = 0; i<3; i++){
        for(int j=0;j<2;j++){
            while(image_stats(extract_box_reference(screen, marking_list_big[2*i+j])).average.r==255||image_stats(extract_box_reference(screen, marking_list_big[2*i+j])).average.b==255){
                pbf_press_button(context, BUTTON_A, 10, 27);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
            }
            pbf_press_dpad(context, DPAD_RIGHT, 10, 18);
        }
        pbf_press_dpad(context, DPAD_DOWN,10 ,18);
    }
    pbf_press_button(context, BUTTON_A, 10,27);

    context.wait_for_all_requests();

}

PokemonType closest_type(const FloatPixel& color_box){
    // Use string literals directly as keys
    static const std::pair<PokemonType, Color> type_color_list[] = {
        {PokemonType::GRASS, Color(62, 180, 86)}, {PokemonType::FIRE, Color(201, 106, 83)}, {PokemonType::WATER, Color(31, 161, 243)},
        {PokemonType::ELECTRIC, Color(202, 207, 66)}, {PokemonType::ROCK, Color(164, 201, 169)}, {PokemonType::GROUND, Color(145, 130, 78)},
        {PokemonType::POISON, Color(142, 125, 234)}, {PokemonType::DARK, Color(86, 118, 113)}, {PokemonType::STEEL, Color(91, 188, 211)},
        {PokemonType::FLYING, Color(112, 206, 242)}, {PokemonType::NORMAL, Color(146, 190, 186)}, {PokemonType::FIGHTING, Color(203, 173, 82)},
        {PokemonType::GHOST, Color(116, 125, 157)}, {PokemonType::DRAGON, Color(80, 145, 241)}, {PokemonType::ICE, Color(44, 224, 243)},
        {PokemonType::FAIRY, Color(202, 166, 242)}, {PokemonType::BUG, Color(140, 188, 87)}, {PokemonType::PSYCHIC, Color(202, 128, 156)},
        {PokemonType::NONE, Color(20, 191, 195)}
    };

    double min_distance = std::numeric_limits<double>::max();
    PokemonType closest_type = PokemonType::NONE;

    for (const auto& [type, color] : type_color_list) {
        double distance = euclidean_distance(color_box, color);
        if (distance < min_distance) {
            min_distance = distance;
            closest_type = type;
        }
    }

    return closest_type;
}

void home_navigate_to_box_secondary(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string target) {
    ImageFloatBox home_box_checker_secondary(0.62, 0.105, 0.255, 0.04);
    VideoOverlaySet box_render(env.console);

    // Add overlays for debugging
    box_render.add(COLOR_RED, home_box_checker_secondary);
    box_render.add(COLOR_GREEN, home_box_checker_secondary);

    context.wait_for_all_requests();

    std::string box_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary)));

    // Store the last 5 reads
    std::deque<std::string> prev_reads;

    while (target != box_name) {
        // Add current box_name to prev_reads
        prev_reads.push_back(box_name);
        if (prev_reads.size() > 5) {
            prev_reads.pop_front();
        }

        // Check if the last 5 reads are all the same
        if (prev_reads.size() == 5 && std::all_of(prev_reads.begin(), prev_reads.end(), [&](const std::string& val) {
                return val == prev_reads.front();
            })) {
            // All 5 reads are the same, press DPAD_LEFT
            pbf_press_button(context, BUTTON_LEFT, 10, 80);
        }

        // Navigate to the next box
        pbf_press_button(context, BUTTON_L, 10, 80);
        context.wait_for_all_requests();
        box_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary)));
    }
}

void home_navigate_to_box_secondary(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int target){
    ImageFloatBox home_box_checker_secondary(0.85, 0.723, 0.06, 0.03);
    VideoOverlaySet box_render(env.console);

    // Go to the first box in the program
    box_render.add(COLOR_RED, home_box_checker_secondary);

    std::string temp = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary)));
    int home_box = std::stoi(temp.substr(0, temp.find_last_of('/')));

    // Store the last 5 reads
    std::deque<int> prev_reads;

    while (target != home_box) {
        // Add current box_name to prev_reads
        prev_reads.push_back(home_box);
        if (prev_reads.size() > 5) {
            prev_reads.pop_front();
        }

        // Check if the last 5 reads are all the same
        if (prev_reads.size() == 5 && std::all_of(prev_reads.begin(), prev_reads.end(), [&](const int val) {
                return val == prev_reads.front();
            })) {
            // All 5 reads are the same, press DPAD_LEFT
            pbf_press_button(context, BUTTON_LEFT, 10, 80);
        }

        // Navigate to the next box
        pbf_press_button(context, BUTTON_L, 10, 80);
        context.wait_for_all_requests();
        temp = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary)));
        home_box = std::stoi(temp.substr(0, temp.find_last_of('/')));
    }
}

void home_remove_markings(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager){

}

HomeCursor home_locate_empty_position(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int* current_box, int last_box){
    VideoOverlaySet box_render(env.console);

    VideoSnapshot screen = env.console.video().snapshot();

    while(*current_box<last_box){
        for (int row = 0; row < 5; row++){
            for (int column = 0; column < 6; column++){
                ImageFloatBox slot_box(0.06 + (0.072 * column), 0.2 + (0.1035 * row), 0.03, 0.057);
                int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();

                //checking color to know if a pokemon is on the slot or not
                if(current_box_value < 5){
                    return {row, column};
                }
            }
        }
        pbf_press_button(context, BUTTON_R, 10, 80);
        (*current_box)++;
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
    }
    throw;
}

HomeCursor home_locate_empty_position_secondary(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int* current_box, int last_box, bool transpose=false){
    VideoOverlaySet box_render(env.console);

    VideoSnapshot screen = env.console.video().snapshot();


    while(*current_box<last_box){
        for (int row = 0; row < 5; row++){
            for (int column = 0; column < 6; column++){
                ImageFloatBox slot_box(0.55 + (0.072 * column), 0.2 + (0.1035 * row), 0.03, 0.057);
                int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();

                //checking color to know if a pokemon is on the slot or not
                if(current_box_value < 5){
                    return {row, column+6};
                }
            }
        }
        pbf_press_button(context, BUTTON_R, 10, 80);
        (*current_box)++;
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
    }
    throw;
}

void sv_get_evo_items(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::unordered_map<std::string, int>& item_counts){

    pbf_wait(context, 5000ms);
    context.wait_for_all_requests();

    PokemonSV::enter_menu_from_overworld(env.program_info(), env.console, context, 0);

    env.console.log("entered menu");
    context.wait_for_all_requests();

    // PokemonSV::enter_bag_from_menu(env.program_info(), env.console, context);

    env.console.log("Made it here");
    context.wait_for_all_requests();

    ImageFloatBox other_icon(0.485, 0.11, 0.005, 0.005);
    while(euclidean_distance(image_stats(extract_box_reference(env.console.video().snapshot(), other_icon)).average, FloatPixel(255, 215.5,0))>5){
        pbf_press_dpad(context, DPAD_RIGHT, 10, 20);
        context.wait_for_all_requests();
    }

    auto set_item_count = [&](std::unordered_map<std::string, int>& items, const std::string key, int value) -> void{
        items[key] = value;
    };

    auto get_item_row_y = [&](VideoSnapshot& screen) -> float {
        PokemonSV::GradientArrowDetector arrow_detector(COLOR_RED, PokemonSV::GradientArrowType::RIGHT, {0.1, 0.17, 0.05, 0.75});
        ImageFloatBox item_box;

        arrow_detector.detect(item_box, screen);

        return item_box.y;
    };

    auto get_selected_bag_quantity = [&](VideoSnapshot& screen, float row_y = -1.0) -> int{
        ImageFloatBox bag_quant(0.49, row_y==-1.0?get_item_row_y(screen):row_y, 0.045, 0.069);

        return OCR::read_number_waterfill(env.logger(),extract_box_reference(screen, bag_quant),0xff000000, 0xff404050, true);
    };

    auto get_selected_bag_item = [&](VideoSnapshot& screen) -> std::pair<std::string, int> {
        float row_y = get_item_row_y(screen);

        ImageFloatBox bag_text(0.175, row_y, 0.2, 0.069);

        auto result = SVItemReader::instance().read_substring(
            env.console, Language::English, extract_box_reference(screen, bag_text),
            OCR::BLACK_TEXT_FILTERS()
            );

        if(result.results.size()==0)return {};

        return {result.results.cbegin()->second.token,get_selected_bag_quantity(screen, row_y)};
    };

    ImageFloatBox first_selected(0.46, 0.175, 0.001, 0.002);
    ImageFloatBox item_large(0.595, 0.5, 0.31, 0.051);
    bool failed_match = false;
    std::pair<std::string, int> item;

    pbf_wait(context, 250ms);
    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();

    do{
        try{
            item = get_selected_bag_item(screen);
            env.console.log(item.first+" "+std::to_string(item.second));

            // Double check we are on the right row, if not, don't move.
            failed_match = item.first != SVItemReader::instance().read_substring(env.console, Language::English, extract_box_reference(screen, item_large),OCR::WHITE_TEXT_FILTERS()).results.cbegin()->second.token;
        }catch(...){
            failed_match = true;
        }

        if(!failed_match){
            pbf_press_button(context, BUTTON_DOWN, 16, 35);
            set_item_count(item_counts, item.first, item.second);
        }else{
            pbf_wait(context, 20ms);
        }

        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
    }while(failed_match || euclidean_distance(image_stats(extract_box_reference(screen, first_selected)).average, FloatPixel(255, 215.5,0))>15);
}

Pokemon home_request_next_simple_item_evo(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, Game& game, std::unordered_map<std::string, int>& item_counts){
    ImageFloatBox home_filter_reader(0.4, 0.41, 0.2, 0.045);
    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.049, 0.04); //pokemon national dex number pos
    ImageFloatBox nature_box(0.157, 0.783, 0.212, 0.042); // Nature box
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046); // OT ID box
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041); // Level box

    VideoSnapshot screen;

    VideoOverlaySet box_render(env.console);

    env.console.log("navigating to list");
    home_manager.navigate_menus_to(env, context, PageID::LIST_VIEW);
    env.console.log("successfully navigated to list");

    pbf_press_button(context, BUTTON_Y, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 60);
    pbf_press_button(context, BUTTON_X, 10, 60);
    home_manager.scroll_filter_menu(env, context, "labels");
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "compatible-games");

    int temp = game.index;
    env.console.log(std::to_string(temp));
    do{
        pbf_press_button(context, BUTTON_DOWN, 10, 60);
        env.console.log(std::to_string(temp));
    }while(--temp==0);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "markings");
    pbf_press_button(context, BUTTON_A, 10, 60);
    pbf_press_button(context, BUTTON_UP, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 75);
    pbf_press_button(context, BUTTON_B, 10, 60);

    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
    screen = env.console.video().snapshot();
    std::string filter_text = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, home_filter_reader)));
    env.console.log(filter_text);

    if(filter_text=="No matches found!")return Pokemon();



    pbf_press_button(context, BUTTON_A, 10, 80);
    pbf_press_button(context, BUTTON_DOWN, 10, 80);
    pbf_press_button(context, BUTTON_A, 10, 200); // Navigate into first pokemon to start finding something that evolves by leveling up

    Pokemon first_pokemon = home_manager.scan_pokemon(env, context);

    Pokemon curr_pokemon = first_pokemon;

    std::vector<std::tuple<double, std::string, std::vector<double>>> item_list = {{25.0, "thunder-stone", {}},{27.0, "ice-stone", {0.01}},{30.0, "moon-stone", {}},{33.0, "moon-stone", {}},{35.0, "moon-stone", {}},{37.0, "fire-stone", {0}},{37.0, "ice-stone", {0.01}},{39.0, "moon-stone", {}},{58.0, "fire-stone", {}},{61.0, "water-stone", {}},{70.0, "leaf-stone", {}},{82.0, "thunder-stone", {}},{90.0, "water-stone", {}},{100.0, "leaf-stone", {0.1}},{102.0, "leaf-stone", {}},{120.0, "water-stone", {}},{176.0, "shiny-stone", {}},{191.0, "sun-stone", {}},{198.0, "dusk-stone", {}},{200.0, "dusk-stone", {}},{271.0, "water-stone", {}},{274.0, "leaf-stone", {}},{299.0, "thunder-stone", {}},{300.0, "moon-stone", {}},{315.0, "shiny-stone", {}},{511.0, "leaf-stone", {}},{513.0, "fire-stone", {}},{515.0, "water-stone", {}},{517.0, "moon-stone", {}},{546.0, "sun-stone", {}},{554.0, "ice-stone", {0}},{572.0, "shiny-stone", {}},{603.0, "thunder-stone", {}},{608.0, "dusk-stone", {}},{670.0, "shiny-stone", {}},{680.0, "dusk-stone", {}},{694.0, "sun-stone", {}},{737.0, "thunder-stone", {}},{739.0, "ice-stone", {}},{951.0, "fire-stone", {}},{938.0, "thunder-stone", {}},{974.0, "ice-stone", {}},{935.0, "auspicious-armor", {}},{935.0, "malicious-armor", {}},{884.0, "metal-alloy", {}}};
    // removed sinistea and poltchageist


    auto matches_pokemon = [&](const Pokemon& curr_pokemon) -> bool {
        for (const auto& [dex_number, item_name, form_list] : item_list){
            if (dex_number == curr_pokemon.national_dex_number && item_counts[item_name] > 0){
                if (!form_list.empty()){
                    if(std::find(form_list.begin(), form_list.end(), curr_pokemon.form_id) != form_list.end()){
                        item_counts[item_name]--;
                        return true;
                    }else{
                        return false;
                    }
                } else {
                    item_counts[item_name]--;
                    return true;
                }
            }
        }
        return false;
    };



    do {
        env.console.log("Running search");
        if (matches_pokemon(curr_pokemon)) {
            pbf_press_button(context, BUTTON_B, 10, 270);
            break;  // Found the first pokemon that can evolve by level up, break out of the do-while loop.
        }

        pbf_press_button(context, BUTTON_R, 10, 80);
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
        curr_pokemon = home_manager.scan_pokemon(env, context);
    } while (curr_pokemon.national_dex_number != first_pokemon.national_dex_number
             || curr_pokemon.level != first_pokemon.level
             || curr_pokemon.ot_id != first_pokemon.ot_id);

    if (!matches_pokemon(curr_pokemon)) {
        return Pokemon();
    }

    pbf_press_button(context, BUTTON_A, 10, 40);
    pbf_press_button(context, BUTTON_A, 10, 40);

    HomeBoxViewWatcher boxWatcher(COLOR_BLUE);


    context.wait_for_all_requests();

    int ret = wait_until(
        env.console, context, 5000ms, {
            boxWatcher
        });

    while(ret!=0){
        pbf_press_button(context, BUTTON_A, 10, 40);

        context.wait_for_all_requests();

        ret = wait_until(
            env.console, context, 2000ms, {
                boxWatcher
            });
    }

    home_manager.detect_home(env, context, true);

    return curr_pokemon;
}

std::vector<PokemonData> home_request_next_level_evo(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, Game& game, int slots_full){
    ImageFloatBox home_filter_reader(0.4, 0.41, 0.2, 0.045);
    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.049, 0.04); //pokemon national dex number pos
    ImageFloatBox nature_box(0.157, 0.783, 0.212, 0.042); // Nature box
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046); // OT ID box
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041); // Level box


    VideoSnapshot screen;

    VideoOverlaySet box_render(env.console);

    home_manager.navigate_menus_to(env, context, PageID::LIST_VIEW);

    context.wait_for_all_requests();
    pbf_press_button(context, BUTTON_X, 10, 60);
    home_manager.scroll_filter_menu(env, context, "labels");
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "compatible-games");

    int temp = game.index;
    env.console.log(std::to_string(temp));
    do{
        pbf_press_button(context, BUTTON_DOWN, 10, 60);
        env.console.log(std::to_string(temp));
    }while(--temp==0);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "markings");
    pbf_press_button(context, BUTTON_A, 10, 60);
    pbf_press_button(context, BUTTON_UP, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 75);
    pbf_press_button(context, BUTTON_B, 10, 60);

    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
    screen = env.console.video().snapshot();
    std::string filter_text = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, home_filter_reader)));
    env.console.log(filter_text);

    if(filter_text=="No matches found!")return {};

    std::vector<std::pair<int,std::vector<int>>> level_list = {{1,{}},{2,{}},{4,{}},{5,{}},{7,{}},{8,{}},{10,{}},{11,{}},{13,{}},{14,{}},{16,{}},{17,{}},{19,{0}},{21,{}},{23,{}},{27,{0}},{29,{}},{32,{}},{41,{}},{43,{}},{46,{}},{48,{}},{50,{0,1}},{52,{0,2}},{54,{}},{56,{}},{60,{}},{63,{}},{66,{}},{69,{}},{72,{}},{74,{0,1}},{77,{0,1}},{79,{0}},{81,{}},{84,{}},{86,{}},{88,{0,1}},{92,{}},{96,{}},{98,{}},{100,{0}},{104,{}},{109,{}},{111,{}},{116,{}},{118,{}},{122,{1}},{129,{}},{138,{}},{140,{}},{147,{}},{148,{}},{152,{}},{153,{}},{155,{}},{156,{}},{158,{}},{159,{}},{161,{}},{163,{}},{165,{}},{167,{}},{170,{}},{177,{}},{179,{}},{180,{}},{183,{}},{187,{}},{188,{}},{194,{0,1}},{204,{}},{209,{}},{216,{}},{218,{}},{220,{}},{222,{1}},{223,{}},{228,{}},{231,{}},{236,{}},{238,{}},{239,{}},{240,{}},{246,{}},{247,{}},{252,{}},{253,{}},{255,{}},{256,{}},{258,{}},{259,{}},{261,{}},{263,{0,1}},{265,{}},{266,{}},{268,{}},{270,{}},{273,{}},{276,{}},{278,{}},{283,{}},{285,{}},{287,{}},{288,{}},{290,{}},{293,{}},{294,{}},{296,{}},{304,{}},{305,{}},{307,{}},{309,{}},{316,{}},{318,{}},{320,{}},{322,{}},{325,{}},{328,{}},{329,{}},{331,{}},{333,{}},{339,{}},{341,{}},{343,{}},{345,{}},{347,{}},{353,{}},{355,{}},{360,{}},{363,{}},{364,{}},{371,{}},{372,{}},{374,{}},{375,{}},{387,{}},{388,{}},{390,{}},{391,{}},{393,{}},{394,{}},{396,{}},{397,{}},{399,{}},{401,{}},{403,{}},{404,{}},{408,{}},{410,{}},{412,{0,1,2}},{415,{}},{418,{}},{420,{}},{422,{0,1}},{425,{}},{431,{}},{434,{}},{436,{}},{443,{}},{444,{}},{449,{}},{451,{}},{453,{}},{456,{}},{459,{}},{495,{}},{496,{}},{498,{}},{499,{}},{501,{}},{502,{}},{504,{}},{506,{}},{507,{}},{509,{}},{519,{}},{520,{}},{522,{}},{524,{}},{529,{}},{532,{}},{535,{}},{536,{}},{540,{}},{543,{}},{544,{}},{551,{}},{552,{}},{554,{}},{557,{}},{559,{}},{562,{}},{564,{}},{566,{}},{568,{}},{570,{0,1}},{574,{}},{575,{}},{577,{}},{578,{}},{580,{}},{582,{}},{583,{}},{585,{0,1,2,3}},{590,{}},{592,{}},{595,{}},{597,{}},{599,{}},{600,{}},{602,{}},{605,{}},{607,{}},{610,{}},{611,{}},{613,{}},{619,{}},{622,{}},{624,{}},{627,{}},{629,{}},{633,{}},{634,{}},{636,{}},{650,{}},{651,{}},{653,{}},{654,{}},{656,{}},{657,{}},{659,{}},{661,{}},{662,{}},{664,{}},{665,{}},{667,{}},{669,{}},{672,{}},{674,{}},{677,{}},{679,{}},{686,{}},{688,{}},{690,{}},{692,{}},{696,{}},{698,{}},{704,{}},{705,{}},{712,{}},{714,{}},{722,{}},{723,{}},{725,{}},{726,{}},{728,{}},{729,{}},{731,{}},{732,{}},{734,{}},{736,{}},{742,{}},{744,{0}},{747,{}},{749,{}},{751,{}},{753,{}},{755,{}},{757,{}},{759,{}},{761,{}},{767,{}},{769,{}},{782,{}},{783,{}},{789,{}},{790,{}},{810,{}},{811,{}},{813,{}},{814,{}},{816,{}},{817,{}},{819,{}},{821,{}},{822,{}},{824,{}},{825,{}},{827,{}},{829,{}},{831,{}},{833,{}},{835,{}},{837,{}},{838,{}},{843,{}},{846,{}},{848,{}},{850,{}},{856,{}},{857,{}},{859,{}},{860,{}},{878,{}},{885,{}},{886,{}},{906,{}},{907,{}},{909,{}},{910,{}},{912,{}},{913,{}},{915,{}},{917,{}},{919,{}},{921,{}},{924,{}},{926,{}},{928,{}},{929,{}},{932,{}},{933,{}},{940,{}},{942,{}},{944,{}},{948,{}},{955,{}},{957,{}},{958,{}},{960,{}},{963,{}},{965,{}},{969,{}},{971,{}},{996,{}},{997,{}}};
    // removed: ralts, kirlia, snorunt, nighttime evolutions
    std::vector<std::pair<int,StatsHuntGenderFilter>> genders_list = {{415, StatsHuntGenderFilter::Female},{757, StatsHuntGenderFilter::Female}};

    std::vector<std::pair<int,std::vector<std::string>>> moves_list = {{108,{"Rollout"}},{190,{"Double Hit"}},{193,{"Ancient Power"}},{203,{"Twin Beam"}},{206,{"Hyper Drill"}},{221,{"Ancient Power"}},{438,{"Mimic"}},{439,{"Mimic"}},{762,{"Stomp"}},{852,{"Taunt"}},{1011,{"Dragon Cheer"}}};

    //TODO: Implement move learning for move-enabled evolution

    pbf_press_button(context, BUTTON_A, 10, 80);
    pbf_press_button(context, BUTTON_DOWN, 10, 80);
    pbf_press_button(context, BUTTON_A, 10, 200); // Navigate into first pokemon to start finding something that evolves by leveling up


    PokemonData first_pokemon = home_manager.scan_pokemon(env, context);

    PokemonData curr_pokemon = first_pokemon;


    auto matches_pokemon = [&](const PokemonData& poke) -> bool {
        for (const auto& id_level : level_list) {
            bool level_match = id_level.first==curr_pokemon.id && (id_level.second.empty()||find(id_level.second.begin(),id_level.second.end(),curr_pokemon.form_id)!=id_level.second.end());
            if (!level_match) continue;

            for (const auto& id_gender : genders_list) {
                if (id_gender.first == poke.id && id_gender.second != poke.gender) {
                    return false;
                }
            }

            return true; // Was found in level match, but was not in genders_list. Can evolve by leveling up.
        }
        return false;
    };

    std::vector<PokemonData> out;

    do {
        env.console.log("Running search");
        if (matches_pokemon(curr_pokemon)) {
            //special case for shedinja
            if(curr_pokemon.id==290){
                if(slots_full%5<5){ // safe to add shedinja
                    out.push_back(curr_pokemon);
                    slots_full++;
                }else{
                    if(slots_full<29){ // we are able to push the last pokemon added to the next column
                        out.push_back(curr_pokemon);
                        out.push_back(PokemonData(292, 0, "Shedinja", StatsHuntGenderFilter::Any, PokemonType::BUG, PokemonType::GHOST, Region::HOENN, 0, -1, false, false, "Wonder Guard", PokemonType::NONE, false, true));
                        // Swap last and third to last element
                        std::swap(out[out.size() - 1], out[out.size() - 3]);
                        slots_full+=2;
                    }
                }
            }else{
                out.push_back(curr_pokemon);
                slots_full++;
            }
        }

        pbf_press_button(context, BUTTON_R, 10, 80);
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
        curr_pokemon = home_manager.scan_pokemon(env, context);
    } while (curr_pokemon.id != first_pokemon.id
             || curr_pokemon.level != first_pokemon.level
             || curr_pokemon.ot_id != first_pokemon.ot_id || slots_full == 30);

    return out;
}

std::vector<PokemonData> home_request_next_for_fun(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, Game& game, int slots_full){
    ImageFloatBox home_filter_reader(0.4, 0.41, 0.2, 0.045);
    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.049, 0.04); //pokemon national dex number pos
    ImageFloatBox nature_box(0.157, 0.783, 0.212, 0.042); // Nature box
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046); // OT ID box
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041); // Level box


    VideoSnapshot screen;

    VideoOverlaySet box_render(env.console);

    home_manager.navigate_menus_to(env, context, PageID::LIST_VIEW);

    context.wait_for_all_requests();
    pbf_press_button(context, BUTTON_X, 10, 60);
    home_manager.scroll_filter_menu(env, context, "labels");
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "compatible-games");

    int temp = game.index;
    env.console.log(std::to_string(temp));
    do{
        pbf_press_button(context, BUTTON_DOWN, 10, 60);
        env.console.log(std::to_string(temp));
    }while(--temp==0);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "markings");
    pbf_press_button(context, BUTTON_A, 10, 60);
    pbf_press_button(context, BUTTON_UP, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 75);
    pbf_press_button(context, BUTTON_B, 10, 60);

    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
    screen = env.console.video().snapshot();
    std::string filter_text = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, home_filter_reader)));
    env.console.log(filter_text);

    if(filter_text=="No matches found!")return {};

    std::vector<int> no_evo_list = {3,6,9,12,15,18,20,22,24,26,28,31,34,36,38,40,42,45,47,49,51,53,55,59,62,65,68,71,73,76,78,80,85,87,89,91,94,97,99,101,103,105,106,107,110,113,115,119,121,122,124,127,128,130,131,132,134,135,136,139,141,142,143,144,145,146,149,150,151,154,157,160,162,164,166,168,169,171,178,181,182,184,185,186,189,192,195,196,197,199,201,202,205,208,210,212,213,214,219,224,225,226,227,229,230,232,235,237,241,242,243,244,245,248,249,250,251,254,257,260,262,267,269,272,275,277,279,282,284,286,289,291,292,295,297,301,302,303,306,308,310,311,312,313,314,317,319,321,323,324,326,327,330,332,334,335,336,337,338,340,342,344,346,348,350,351,352,354,357,358,359,362,365,367,368,369,370,373,376,377,378,379,380,381,382,383,384,385,386,389,392,395,398,400,402,405,406,407,409,411,413,414,416,417,419,421,423,424,426,427,428,429,430,432,433,435,437,439,441,442,445,446,447,448,450,452,454,455,457,460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,491,492,493,494,497,500,503,505,508,510,512,514,516,518,521,523,526,527,528,530,531,534,537,538,539,541,542,545,547,549,550,553,555,556,558,560,561,563,565,567,569,571,573,576,579,581,584,586,587,589,591,593,594,596,598,601,604,606,609,612,614,615,617,618,620,621,623,626,628,630,631,632,635,637,638,639,640,641,642,643,644,645,646,647,648,649,652,655,658,660,663,666,668,671,673,675,676,678,681,683,685,687,689,691,693,695,697,699,700,701,702,703,706,707,709,710,711,713,715,716,717,718,719,720,721,724,727,730,733,735,738,740,741,743,745,746,748,750,752,754,756,758,760,763,764,765,766,768,770,771,772,773,774,775,776,777,778,779,780,781,784,785,786,787,788,791,792,793,794,795,796,797,798,799,800,801,802,804,805,806,807,809,812,815,818,820,823,826,828,830,832,834,836,839,841,842,844,845,847,849,851,853,855,858,861,862,863,864,865,866,867,869,870,871,872,873,874,875,876,877,879,880,881,882,883,887,888,889,890,892,893,894,895,896,897,898,899,900,901,902,903,904,905,908,911,914,916,918,920,923,925,927,930,931,934,936,937,939,941,943,945,947,949,950,952,954,956,959,961,962,964,966,967,968,970,972,973,975,976,977,978,979,980,981,982,983,984,985,986,987,988,989,990,991,992,993,994,995,998,999,1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1013,1014,1015,1016,1017,1018,1019,1020,1021,1022,1023,1024,1025};


    pbf_press_button(context, BUTTON_A, 10, 80);
    pbf_press_button(context, BUTTON_DOWN, 10, 80);
    pbf_press_button(context, BUTTON_A, 10, 200); // Navigate into first pokemon to start finding something that evolves by leveling up

    PokemonData first_pokemon = home_manager.scan_pokemon(env, context);

    PokemonData curr_pokemon = first_pokemon;


    auto matches_pokemon = [&](const PokemonData& poke) -> bool {
        for (const auto& id_level : no_evo_list) {
            bool level_match = id_level==curr_pokemon.id;
            if (!level_match) continue;
            return true;
        }
        return false;
    };

    std::vector<PokemonData> out;


    do {
        env.console.log("Running search");
        if (matches_pokemon(curr_pokemon)) {
            out.push_back(curr_pokemon);
            slots_full++;
        }

        pbf_press_button(context, BUTTON_R, 10, 80);
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
        curr_pokemon = home_manager.scan_pokemon(env, context);
    } while (curr_pokemon.id != first_pokemon.id
             || curr_pokemon.level != first_pokemon.level
             || curr_pokemon.ot_id != first_pokemon.ot_id || slots_full == 30);

    return out;

}


// This function is run in Pokémon Home, and should be used when wanting to transfer a Pokémon from the Home box into the Game box on the right.
// Requires env, context, and two coordinates sent as an std::pair<int,int>
void home_move_pokemon_to_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, const HomeCursor& game){

    home_manager.pick_up_pokemon(env, context);

    home_manager.navigate_to(env, context, game);

    home_manager.put_down_pokemon(env, context);

    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_UP, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);

    context.wait_for_all_requests();

}

// This function is run in Pokémon Home, and should be used when wanting to transfer a Pokémon to the Home box from the Game box on the right.
// Assumes the cursor is in Home position {0,5}
// Requires env, context, and two coordinates sent as an std::pair<int,int>
void home_move_pokemon_from_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, const HomeCursor& home, const HomeCursor& game, bool emergency = false){
    home_manager.navigate_to(env, context, game);

    home_manager.pick_up_pokemon(env, context);

    home_manager.navigate_to(env, context, home);

    home_manager.put_down_pokemon(env, context);

    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_UP, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);

    context.wait_for_all_requests();

}

std::vector<PokemonData> home_request_next_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, Game& game, mode mode, std::unordered_map<std::string, int>& item_counts, int slots_full){
    env.console.log("finding next pokemon");
    switch(mode){
    case mode::Level:
        return home_request_next_level_evo(env, context, home_manager, game, slots_full);
    case mode::Simple_Item:
        return {};
        // return home_request_next_simple_item_evo(env, context, home_manager, game, item_counts, slots_full);
    case mode::Fun:
        return home_request_next_for_fun(env, context, home_manager, game, slots_full);
    default:
        return {};
    }
}

// This function is run in Pokémon Home, and should be used when wanting to locate all Pokémon eligible for powering up
// Requires env, context, and a game name to identify what game to navigate to
// Returns the amount of Pokémon successfully retrieved
int home_fill_boxes_to_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, Game& game, std::string box_name, mode mode, std::unordered_map<std::string, int>& item_counts){

    int found = 0;

    while(found<30){
        std::vector<PokemonData> temp_mon = home_request_next_pokemon(env, context, home_manager, game, mode, item_counts, found);

        pbf_mash_button(context, BUTTON_B, 5s);

        home_manager.detect_home(env, context);

        if(temp_mon.size()==0||found+temp_mon.size()>30)return found;

        for(PokemonData next: temp_mon){
            std::optional<HomeCursor> dest = home_manager.locate_pokemon(next);
            if(dest.has_value()){
                home_manager.swap_pokemon(env, context, dest.value(), {++found%5, found/5 + 6});
            }else{
                found++;
            }
        }
    }


    // for(int i =0; i<6; i++){
    //     for(int j = 0; j < 5; j++){
    //         try{
    //             Pokemon temp_mon = home_request_next_pokemon(env, context, home_manager, game, mode, item_counts);

    //             pbf_wait(context, 500ms);
    //             context.wait_for_all_requests();

    //             home_move_pokemon_to_game(env, context, home_manager, {static_cast<int>(j),static_cast<int>(i+6)});
    //             found++;

    //             if(temp_mon.national_dex_number==290){j++;} // Special case to allow for Shedinja. Unlucky if already on bottom row.

    //         }catch(NoPokemonAvailableException){
    //             pbf_press_button(context, BUTTON_B, 10, 150);
    //             home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
    //             return found;
    //         }
    //     }
    // }

    // home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
    return found;
}

void sv_run_ace2(SingleSwitchProgramEnvironment& env, ProControllerContext& context, double b_probability = 0){
    PokemonSV::AdvanceDialogWatcher evo_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_BLACK);
    PokemonSV::AdvanceDialogWatcher next_battle_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_WHITE);
    PokemonSV::PromptDialogWatcher learn_move_message(COLOR_CYAN);
    PokemonSV::NormalBattleMenuWatcher battle_menu(COLOR_RED);

    ImageFloatBox evolve_message(0.28, 0.76, 0.065, 0.055);

    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();

    for(int i = 0; i < 4; i++){
        env.console.log("Running battle "+std::to_string(i+1), COLOR_CYAN);

        int ret = -1;
        while(ret==-1){
            ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_press_button(context, BUTTON_B, 20, 40);
                },
                {
                    battle_menu,
                }
            );
        }


        bool terrastalized = true;
        bool win = PokemonSV::run_pokemon(env.console, context, {}, true, terrastalized);

        if(!win){
            throw BattleFailedException{};
        }

        pbf_press_button(context, BUTTON_A, 10, 120);

        // Account for evolutions


        ret = wait_until(
            env.console, context,
            60s,
            {
                evo_message,
                next_battle_message,
                learn_move_message
            }
        );

        env.console.log("ret is "+std::to_string(ret));

        while(ret!=-1){
            switch(ret){
            case 0:
                screen = env.console.video().snapshot();
                pbf_press_button(context, BUTTON_A, 10, 20);
                if(sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, evolve_message))) == "What?"){
                    float prob_pick = rand();
                    float prob_pick_clamped = prob_pick/RAND_MAX;
                    float prob_manipped = (1-(std::powf(1-(b_probability),1.0f/4.0f)));

                    env.console.log(std::to_string(prob_pick)+" "+std::to_string(prob_pick_clamped)+" "+std::to_string(prob_manipped));
                    if(prob_pick_clamped > prob_manipped){   // TODO: Fix Probability Check
                        pbf_mash_button(context, BUTTON_B, 500ms);
                    }  else{
                        pbf_wait(context, 10000ms);
                    }
                }
            case 1:
                pbf_mash_button(context, BUTTON_B, 500ms);
            case 2:
                pbf_press_button(context, BUTTON_B, 10, 20);
                break;
            default:
                throw;
            }

            context.wait_for_all_requests();

            ret = wait_until(
                env.console, context,
                Milliseconds(60*TICKS_PER_SECOND),
                {
                    evo_message,
                    next_battle_message,
                    learn_move_message
                }
            );
        }
    }
}

bool switch_close_game_and_open(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const std::string& target_game, int retries = 0){
    if(retries > MAX_RETRIES)return false;

    HomeMenuWatcher home_menu(env.console);
    ImageFloatBox switch_game_checker(0, 0.2, 1, 0.06);
    VideoOverlaySet overlays(env.console);

    int ret = wait_until(env.console, context, 2s, {home_menu});

    context.wait_for_all_requests();
    while (ret!=0){
        pbf_press_button(context, BUTTON_HOME, 10, 240);
        ret = wait_until(env.console, context, 30s, {home_menu});
    }

    pbf_press_button(context, BUTTON_X, 10, 80);
    pbf_press_button(context, BUTTON_A, 10, 240);

    overlays.add(COLOR_GREEN, switch_game_checker);
    bool found = false;

    for (int attempt = 0; attempt < 12; attempt++){
        context.wait_for_all_requests();
        std::string game_name = OCR::ocr_read(
            Language::English,
            extract_box_reference(env.console.video().snapshot(), switch_game_checker)
            );
        game_name = sanitize_OCR(game_name);

        if (!game_name.empty() && game_name[0] != 'P'){
            size_t first_space = game_name.find(' ');
            if (first_space != std::string::npos && first_space + 1 < game_name.size()){
                game_name = game_name.substr(first_space + 1);
            }
        }

        env.console.log("Detected game: " + game_name);

        if (game_name == target_game){
            pbf_press_button(context, BUTTON_A, 10, 10);
            found = true;
            break;
        }

        pbf_press_button(context, BUTTON_RIGHT, 10, 30);
    }

    overlays.clear();

    if (!found){
        env.console.log("ERROR: Could not locate \"" + target_game + "\".");
        return switch_close_game_and_open(env, context, target_game, retries+1);
    }

    if (target_game == "Pokémon HOME"){
        HomeEnvironment home_manager_temp(env, context);
        if(home_manager_temp.get_view()=="Title Screen"){
            return true;
        }else{
            return switch_close_game_and_open(env, context, target_game, retries+1);
        }
    }

    if (target_game == "Pokémon Violet"){
        PokemonSV::OverworldWatcher overworld(env.console, COLOR_RED);
        load_into_sv(env, context);
        ret = wait_until(env.console, context, 30s, {overworld});
        switch(ret){
            case 0: return true;
            default: return switch_close_game_and_open(env, context, target_game, retries+1);
        }
    }

    env.console.log("WARNING: No special handling for \"" + target_game + "\".");
    return true;
}


void sv_run_enrichment(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string sv_box_name, int max_runs, mode mode){
    PokemonSV::OverworldWatcher overworld(env.console, COLOR_RED);
    HomeMenuWatcher home_menu(env.console);

    ImageFloatBox sv_box_name_read(0.35, 0.115, 0.2, 0.05);
    ImageFloatBox title_screen_read(0.6, 0.245, 0.25, 0.235);

    VideoOverlaySet box_render(env.console);
    VideoSnapshot screen = env.console.video().snapshot();

    auto nav_to_boxes = [&]() -> std::string {
        // First, check if we are in the overworld
        int ret = wait_until(
            env.console, context,
            Milliseconds(5*TICKS_PER_SECOND),
            {
                overworld
            }
        );      // TODO: Account for if the loading icon is on the screen

        // If not, mash B until we are
        while (ret == -1) {
            ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_mash_button(context, BUTTON_B, 30*TICKS_PER_SECOND);    // Allows for 30 second intervals and failing if there are consecutive issues later
                },
                {
                    overworld,
                }
            );
        }

        // We are in the overworld, nav to boxes
        PokemonSV::enter_box_system_from_overworld(env.program_info(),env.console, context); // Go to boxes to fill party

        context.wait_for_all_requests();

        return sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), sv_box_name_read)));
    };

    auto grab_column = [&]() -> void {
        pbf_press_button(context, BUTTON_MINUS, 10, 50);    // multi-select
        for(int i =0; i < 5; i++){
            pbf_press_button(context, BUTTON_DOWN, 10, 30);
        }
        pbf_press_button(context, BUTTON_A, 10, 50);
    };

    auto nav_to_column = [&](int i) -> void {
        for( ; i < 0; i++){
            pbf_press_button(context, BUTTON_LEFT, 10, 50);
        }
        for( ; i > 0; i--){
            pbf_press_button(context, BUTTON_RIGHT, 10, 50);
        }
    };

    auto move_pokemon_column = [&](int runs, bool to_party) -> void {
        std::string box_name = nav_to_boxes();

        env.console.log(box_name);
        while(box_name!=sv_box_name){   // navigate to correct box
            pbf_press_button(context, BUTTON_L, 10, 80);
            context.wait_for_all_requests();
            box_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), sv_box_name_read)));
            env.console.log(box_name);
        }

        if(to_party){
            nav_to_column(runs);
        }else{
            pbf_press_button(context, BUTTON_DOWN, 10, 50); // Move down 1 for party leader
            nav_to_column(-1);
        }

        grab_column();

        if(to_party){
            pbf_press_button(context, BUTTON_DOWN, 10, 50); // Move down 1 for party leader
            nav_to_column(-1-runs);
        }else{
            pbf_press_button(context, BUTTON_UP, 10, 50);   // Move back up 1
            nav_to_column(runs+1);
        }

        pbf_press_button(context, BUTTON_A, 10, 50);        // Put pokemon down
        pbf_press_button(context, BUTTON_B, 10, 300);       // Close out of boxes
    };

    // Loaded into game

    for(int runs = 0; runs < max_runs; runs++){

        move_pokemon_column(runs, true);            // Retrieve pokemon for this iteration

        // Close out of menu and start tournament
        pbf_press_button(context, BUTTON_B, 10, 300);
        pbf_mash_button(context, BUTTON_A, 25000ms);

        try{
            switch(mode){
                case mode::Level:
                    sv_run_ace2(env, context, 0.5);        // Run the tournament once
                    break;
                case mode::Simple_Item:
                    // TODO: Do simple evo item stuff
                    break;
                case mode::Fun:
                    // TODO: Do simple evo item stuff
                    sv_run_ace2(env, context, 0);        // Run the tournament once
                    break;
                default:
                    break;
            }


            move_pokemon_column(runs, false);       // Put away the pokemon from this iteration

            //save
            pbf_press_button(context, BUTTON_R, 10, 300);
            pbf_press_button(context, BUTTON_A, 10, 150);
            pbf_wait(context, 6000ms);
            pbf_press_button(context, BUTTON_B, 10, 300);
        }
        catch(const BattleFailedException& e){
            env.console.log(e.what());
            // go home, reboot, and retry
            runs--;
            switch_close_game_and_open(env, context, "Pokémon Violet");
        }

        catch(...){
            throw;
        }



    }
}

// This function is run in PLA. It is used to scroll left or right boxes to get to a target box
void pla_navigate_to_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int target){

}

void plza_run_ralts(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ssf_press_left_joystick(context, 128, 0, 0ms, 600ms, 00ms);
}

void Enrichment::initialize_home(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, std::vector<Game>& game_list){
    ImageFloatBox game_checker(0.0455, 0.244, 0.442, 0.057);
    VideoSnapshot screen = env.console.video().snapshot();

    VideoOverlaySet box_render(env.console);

    std::ostringstream ss;


    if(DISPOSE_GOS){
        home_dispose_of_go(env, context, home_manager);
        home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
    }

    if(WIPE_MARKINGS)wipe_markings(env, context, home_manager);
}

void Enrichment::enrich_with_games(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, std::vector<Game>& game_list){
    std::unordered_map<GameStatus, std::vector<mode>> mode_list = {
        {GameStatus::POKEMON_VIOLET, {mode::Level, /*mode::Simple_Item, */mode::Fun, mode::None}}
    };

    bool item_counts_init = false;

    for(auto game: game_list){
        enrichment_mode = mode_list[game.game][0];

        int pokemon = 30;
        do{
            switch(game.game){
                case GameStatus::POKEMON_VIOLET:
                    if(!item_counts_init){
                        switch_close_game_and_open(env, context, "Pokémon Violet");
                        // TODO: Make sure we are at the desk, use STARTING_AT_DESK
                        sv_get_evo_items(env, context, item_counts);
                        item_counts_init = true;
                        switch_close_game_and_open(env, context, "Pokémon HOME");
                    }
                    if(!SKIP_SETUP){
                        home_manager.detect_home(env, context);
                        home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_VIOLET);
                        context.wait_for_all_requests();
                        pokemon = home_fill_boxes_to_game(env, context, home_manager, game, SV_BOX_NAME, enrichment_mode, item_counts);
                        env.console.log("Pokemon: "+std::to_string(pokemon));
                        if(pokemon==0){
                            env.console.log("Pokemon is now 0.");
                            mode_list[game.game].erase(mode_list[game.game].begin());
                            enrichment_mode = mode_list[game.game][0];
                            env.console.log("enrichment_mode:"+to_string(enrichment_mode));
                            break;
                        }
                    }
                    switch_close_game_and_open(env, context, "Pokémon Violet");
                    sv_run_enrichment(env,context, SV_BOX_NAME, std::ceil(pokemon/5), enrichment_mode);
                    send_program_notification(
                        env, NOTIFICATION_ERROR_FATAL,
                        COLOR_GREEN,
                        "Ran SV Enrichment on up to 30 Pokemon",
                        {}, "",
                        {}
                        );
                default:
                    // switch_close_game_and_open(env, context, game.name);
                    pokemon=30;
            }
            switch_close_game_and_open(env, context, "Pokémon HOME");
            home_manager.detect_home(env, context);
            home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, game.game);
            home_put_away_pokemon(env, context, home_manager, game, EMERGENCY_DELOAD);
        }while(!(pokemon==0&&enrichment_mode==mode::None));
    }
}

// This function is run in Pokémon Home, and should be used when wanting to transfer all Pokémon from a game back to home.
// Requires env, context
void Enrichment::home_put_away_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager, Game& game, bool emergency = false){

    home_manager.navigate_to(env, context, {1, 6});

    switch(game.game){
        case GameStatus::POKEMON_VIOLET:
            home_navigate_to_box_secondary(env, context, SV_BOX_NAME);
        default:
            home_navigate_to_box_secondary(env, context, SV_BOX_NAME);
    }
    // pbf_press_button(context, BUTTON_UP, 10, 30);
    // move back to the Home side of the boxes
    // pbf_press_button(context, BUTTON_LEFT, 10, 30);
    home_manager.navigate_to(env, context, {0,0, HOME_FIRST_BOX});
    int current_box = HOME_FIRST_BOX;
    // For each Pokémon in the box, increment it's circle marking and find it a place to be put into
    try{
        for(int i = 0; i < 6; i++){
            for(int j = 0; j < 5; j++){
                home_manager.navigate_to(env, context, {0,0});
                ImageFloatBox slot_box(0.55 + (0.072 * i), 0.2 + (0.1035 * j), 0.03, 0.057);
                //checking color to know if a pokemon is on the slot or not
                if((int)image_stddev(extract_box_reference(env.console.video().snapshot(), slot_box)).sum() < 5)continue;

                HomeCursor target_pos = home_locate_empty_position(env, context, &current_box, HOME_LAST_BOX);
                home_move_pokemon_from_game(env, context, home_manager, target_pos, {static_cast<int>(j),static_cast<int>(i+6)}, emergency);
            }
        }
        pbf_press_button(context, BUTTON_DOWN, 10, 30);
        pbf_press_button(context, BUTTON_DOWN, 10, 30);
        pbf_press_button(context, BUTTON_DOWN, 10, 30);
        pbf_press_button(context, BUTTON_RIGHT, 10, 30);

        home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
    }catch(...){
        env.console.log("Error, ran out of space.");
    }

}

void Enrichment::home_dispose_of_go(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeEnvironment& home_manager){
    std::vector<int> blacklist = {144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251, 377, 378, 379, 380, 381, 382, 383, 384, 385, 386, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489,490,491,492,493,494,638,639,640,641,642,643,644,645,646,647,648,649,666,676,716,717,718,772,773,785,786,787,788,789,790,791,792,793,888,889,890,891,892,893,894,895,896,897,898,905,999,1000,1001,1002,1003,1004,1007,1008,1009,1010,1014,1015,1016,1017,1021,1022,1023,1024,1025}; // Take out legendaries, etc. that need to be preserved

    std::vector<int> lv10_blacklist = {2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18, 20, 22, 24, 28, 30, 31, 33, 34, 42, 44, 45, 47, 49, 51, 53, 55, 57, 61, 62, 64, 65, 65, 67, 68, 68, 70, 71, 73, 75, 76, 76, 76, 78, 80, 82, 85, 87, 89, 93, 94, 94, 97, 99, 101, 105, 106, 107, 110, 112, 117, 119, 124, 125, 126, 130, 139, 141, 148, 149, 153, 154, 156, 157, 157, 159, 160, 162, 164, 166, 168, 171, 178, 180, 181, 182, 184, 186, 186, 188, 189, 195, 202, 205, 210, 217, 219, 221, 224, 229, 230, 230, 232, 237, 247, 248, 253, 254, 256, 257, 259, 260, 262, 264, 266, 267, 268, 269, 271, 272, 274, 275, 277, 279, 281, 282, 284, 286, 288, 289, 291, 292, 294, 295, 297, 305, 306, 308, 310, 317, 319, 321, 323, 326, 329, 330, 332, 334, 340, 342, 344, 346, 348, 354, 356, 362, 364, 365, 372, 373, 375, 376, 388, 389, 391, 392, 394, 395, 397, 398, 400, 402, 404, 405, 409, 411, 414, 415, 416, 419, 421, 423, 426, 432, 435, 437, 444, 445, 450, 452, 454, 457, 460, 462, 464, 464, 466, 466, 467, 468, 473, 475, 477, 496, 497, 499, 500, 502, 503, 503, 505, 507, 508, 510, 520, 521, 523, 525, 526, 526, 530, 533, 534, 534, 536, 537, 541, 544, 545, 552, 553, 555, 558, 560, 563, 565, 567, 569, 571, 575, 576, 578, 579, 581, 583, 584, 586, 591, 593, 596, 598, 600, 601, 603, 604, 606, 608, 609, 611, 612, 614, 620, 623, 625, 628, 630, 634, 635, 637, 651, 652, 654, 655, 657, 658, 660, 662, 663, 665, 666, 668, 670, 671, 673, 675, 678, 680, 687, 689, 691, 693, 697, 699, 705, 706, 706, 713, 715, 723, 724, 724, 726, 727, 729, 730, 732, 733, 735, 737, 738, 743, 745, 748, 750, 752, 754, 756, 758, 760, 762, 763, 768, 770, 783, 784, 790, 791, 791, 792, 792, 811, 812, 814, 815, 817, 818, 820, 822, 823, 825, 826, 828, 830, 832, 834, 836, 838, 839, 844, 847, 849, 851, 857, 858, 860, 861, 862, 862, 863, 864, 866, 879, 886, 887, 901, 907, 908, 910, 911, 913, 914, 916, 918, 920, 922, 923, 925, 927, 929};

    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.049, 0.04); //pokemon national dex number pos
    ImageFloatBox nature_box(0.157, 0.783, 0.212, 0.042); // Nature box
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046); // OT ID box
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041); // Level box
    home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_PLA);

    home_manager.navigate_to(env, context, {1,11});

    int temp_box = PLA_FIRST_BOX;
    bool more_go = false;
    home_navigate_to_box_secondary(env, context, temp_box);
    try{
        do{
            HomeCursor next_spot = home_locate_empty_position_secondary(env, context, &temp_box, PLA_LAST_BOX, false);

            pbf_press_button(context, BUTTON_X, 10, 100);
            pbf_press_button(context, BUTTON_X, 10, 50);
            home_manager.scroll_filter_menu(env, context, "markings");
            pbf_press_button(context, BUTTON_A, 10, 60);
            pbf_press_button(context, BUTTON_UP, 10, 60);
            pbf_press_button(context, BUTTON_A, 10, 75);
            home_manager.scroll_filter_menu(env, context, "origin-mark");
            pbf_press_button(context, BUTTON_UP, 10, 30);
            pbf_press_button(context, BUTTON_UP, 10, 30);
            pbf_press_button(context, BUTTON_A, 10, 50);
            home_manager.scroll_filter_menu(env, context, "shiny");
            pbf_press_button(context, BUTTON_UP, 10, 30);
            pbf_press_button(context, BUTTON_A, 10, 50);
            home_manager.scroll_filter_menu(env, context, "compatible-games");
            context.wait_for_all_requests();
            while(home_manager.get_filter_menu_read(env, context)!="compatible-games"){
                pbf_wait(context, 250ms);
            }
            pbf_press_button(context, BUTTON_DOWN, 10, 50);
            pbf_press_button(context, BUTTON_DOWN, 10, 50);

            context.wait_for_all_requests();

            // block for checking that we have successfully navigated to PLA
            ImageFloatBox PLA_check(0.83, 0.4, 0.1, 0.06);
            VideoSnapshot screen = env.console.video().snapshot();
            FloatPixel PLA_button = image_stats(extract_box_reference(screen, PLA_check)).average;
            while(euclidean_distance(PLA_button, FloatPixel(255, 187, 0))>5){
                pbf_press_button(context, BUTTON_DOWN, 10, 60);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                PLA_button = image_stats(extract_box_reference(screen, PLA_check)).average;
            }

            pbf_press_button(context, BUTTON_A, 10, 50);
            pbf_press_button(context, BUTTON_B, 10, 240);


            // Inspect for blacklisted
            pbf_press_button(context, BUTTON_A, 10, 80);
            pbf_press_button(context, BUTTON_DOWN, 10, 80);
            pbf_press_button(context, BUTTON_A, 10, 200);

            // Take note of first ID No, ot id, and nature for looping
            context.wait_for_all_requests();
            screen = env.console.video().snapshot();
            int first_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
            int first_level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
            int first_nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);

            int id = first_id;
            int level = first_level;
            int nat_id = first_nat_id;
            do{
                more_go = false;
                env.console.log("Running search");
                bool blacklisted = false;
                for(auto id_p: blacklist){
                    blacklisted = blacklisted || id_p==nat_id;
                }
                for(auto id_p2: lv10_blacklist){
                    blacklisted = blacklisted || (id_p2==nat_id&&level<=10);
                }
                if(!blacklisted){
                    more_go = true;
                    env.console.log("Found Nonblacklisted");

                    // Move this into the boxes
                    pbf_press_button(context, BUTTON_B, 10, 270);
                    pbf_press_button(context, BUTTON_A, 10, 80);
                    pbf_press_button(context, BUTTON_A, 10, 150);

                    context.wait_for_all_requests();

                    home_manager.detect_home(env, context, true);

                    home_move_pokemon_to_game(env, context, home_manager, next_spot);
                    break;
                }
                pbf_press_button(context, BUTTON_R, 10, 80);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
                level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
                nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
            }while(id!=first_id||level!=first_level||nat_id!=first_nat_id);
        }while(more_go);

        pbf_mash_button(context, BUTTON_B, 5s);
        while(home_manager.get_view()!="Box View"){
            home_manager.detect_home(env, context);
            pbf_press_button(context, BUTTON_B, 10, 60);
        }


    }catch(...){
        env.console.log("Ran out of spaces");
    }

    home_manager.navigate_menus_to(env, context, PageID::LIST_VIEW, GameStatus::POKEMON_HOME);

    more_go = false;
    try{
        pbf_press_button(context, BUTTON_X, 10, 100);
        pbf_press_button(context, BUTTON_X, 10, 50);
        home_manager.scroll_filter_menu(env, context, "markings");
        pbf_press_button(context, BUTTON_A, 10, 60);
        pbf_press_button(context, BUTTON_UP, 10, 60);
        pbf_press_button(context, BUTTON_A, 10, 75);
        home_manager.scroll_filter_menu(env, context, "origin-mark");
        pbf_press_button(context, BUTTON_UP, 10, 30);
        pbf_press_button(context, BUTTON_UP, 10, 30);
        pbf_press_button(context, BUTTON_A, 10, 50);
        home_manager.scroll_filter_menu(env, context, "shiny");
        pbf_press_button(context, BUTTON_UP, 10, 30);
        pbf_press_button(context, BUTTON_A, 10, 80);
        pbf_press_button(context, BUTTON_B, 10, 270);

        do{

            // Inspect for blacklisted
            pbf_press_button(context, BUTTON_A, 10, 90);
            pbf_press_button(context, BUTTON_DOWN, 10, 90);
            pbf_press_button(context, BUTTON_A, 10, 210);

            // Take note of first ID No, ot id, and nature for looping
            context.wait_for_all_requests();
            VideoSnapshot screen = env.console.video().snapshot();
            int first_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
            int first_level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
            int first_nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);

            int id = first_id;
            int level = first_level;
            int nat_id = first_nat_id;
            do{
                more_go = false;
                env.console.log("Running search");
                bool blacklisted = false;
                for(auto id_p: blacklist){
                    blacklisted = blacklisted || id_p==nat_id;
                }
                for(auto id_p2: lv10_blacklist){
                    blacklisted = blacklisted || (id_p2==nat_id&&level<=10);
                }
                if(!blacklisted){
                    more_go = true;
                    env.console.log("Found Nonblacklisted");

                    // Release
                    pbf_press_button(context, BUTTON_B, 10, 280);
                    pbf_press_button(context, BUTTON_A, 10, 90);
                    pbf_press_button(context, BUTTON_UP, 10, 60);
                    pbf_press_button(context, BUTTON_UP, 10, 60);
                    pbf_press_button(context, BUTTON_A, 10, 160);
                    pbf_press_button(context, BUTTON_A, 10, 160);
                    pbf_press_button(context, BUTTON_UP, 10, 60);
                    pbf_press_button(context, BUTTON_A, 10, 160);
                    pbf_press_button(context, BUTTON_A, 10, 210);

                    context.wait_for_all_requests();

                    break;
                }
                pbf_press_button(context, BUTTON_R, 10, 90);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
                level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
                nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
            }while(id!=first_id||level!=first_level||nat_id!=first_nat_id);
        }while(more_go);

        pbf_mash_button(context, BUTTON_B, 5s);
        while(home_manager.get_view()!="Box View"){
            home_manager.detect_home(env, context);
            pbf_press_button(context, BUTTON_B, 10, 60);
        }


    }catch(...){
        env.console.log("Ran out of spaces");
    }
}

void Enrichment::wipe_markings(SingleSwitchProgramEnvironment& env, ProControllerContext& context,HomeEnvironment& home_manager){

    auto is_marked = [&](VideoSnapshot& screen) -> bool{
        ImageFloatBox home_circle_marking(0.8075, 0.817, 0.0025, 0.005);
        ImageFloatBox home_triangle_marking(0.8375, 0.817, 0.0025, 0.005);
        ImageFloatBox home_square_marking(0.8725, 0.817, 0.0025, 0.005);
        ImageFloatBox home_heart_marking(0.9025, 0.817, 0.0025, 0.005);
        ImageFloatBox home_star_marking(0.935, 0.817, 0.0025, 0.005);
        ImageFloatBox home_diamond_marking(0.9675, 0.817, 0.0025, 0.005);

        std::vector<ImageFloatBox> marking_list = {home_circle_marking,home_triangle_marking,home_square_marking,home_heart_marking,home_star_marking,home_diamond_marking};
        bool marked = false;
        for(auto m:marking_list){
            FloatPixel marking = image_stats(extract_box_reference(screen, m)).average;

            marked = marked|(marking.r==255)|(marking.b==255);
        }

        return marked;
    };

    VideoSnapshot screen;

    home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW);

    home_manager.navigate_to(env, context, {4,5});

    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    pbf_press_button(context, BUTTON_A, 10, 180);


    for(int i = 0; i<5;i++){
        for(int j = (i%2==0?0:5); (i%2==0?j<5:j>0); (i%2==0?j++:j--)){
            context.wait_for_all_requests();
            screen = env.console.video().snapshot();
            if(is_marked(screen)){
                home_clear_marking(env, context, home_manager);
                home_manager.navigate_to(env, context, {4,5});

                pbf_press_button(context, BUTTON_DOWN, 10, 30);
                pbf_press_button(context, BUTTON_A, 10, 180);
                i=0;
                j=0;
            }else{
                pbf_press_dpad(context, (i%2==0?DPAD_RIGHT:DPAD_LEFT), 10, 27);
            }
        }
        pbf_press_dpad(context, DPAD_DOWN, 10, 27);
        context.wait_for_all_requests();
    }

    pbf_press_button(context, BUTTON_A, 10, 27);
    pbf_press_dpad(context, DPAD_UP, 10, 27);

    home_manager.detect_home(env, context);
}

void Enrichment::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // FloatPixel pokemon_color = image_stats(extract_box_reference(env.console.video().snapshot(), ImageFloatBox(0.76, 0.295, 0.14, 0.23))).average;
    // env.console.log(std::to_string(pokemon_color.r)+", "+std::to_string(pokemon_color.g)+", "+std::to_string(pokemon_color.b));

    // plza_run_ralts(env, context);

    std::vector<Game> game_list = {Game(GameStatus::POKEMON_VIOLET,0,false)/*,Game(GameStatus::POKEMON_SWORD,3,false),Game(GameStatus::POKEMON_PLA,1,false),Game("GameStatus::POKEMON_EEVEE",4,false)*/};

    VideoSnapshot screen = env.console.video().snapshot();

    VideoOverlaySet box_render(env.console);
    HomeMenuWatcher home_menu(env.console);
    std::ostringstream ss;



    HomeEnvironment home_manager(env, context);

    if(EMERGENCY_DELOAD){
        home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_VIOLET);
        home_put_away_pokemon(env, context, home_manager, game_list[0], true);
    }
    if(!SKIP_SORT){
        initialize_home(env, context, home_manager, game_list);
        home_manager.sort_all_boxes(env, context, HOME_FIRST_BOX, HOME_LAST_BOX);
        send_program_notification(
                env, NOTIFICATION_ERROR_FATAL,
                COLOR_GREEN,
                "Finished Sorting Pokemon",
                {}, "",
                screen
            );
    }
    enrich_with_games(env, context, home_manager, game_list);
    send_program_notification(
        env, NOTIFICATION_ERROR_FATAL,
        COLOR_GREEN,
        "Finished Running Enhancements",
        {}, "",
        screen
        );

    home_manager.sort_all_boxes(env, context, HOME_FIRST_BOX, HOME_LAST_BOX);

    int ret = -1;
    while(ret!=0){
        pbf_press_button(context, BUTTON_HOME, 10, 240);
        ret = wait_until(
            env.console, context,
            Milliseconds(30*TICKS_PER_SECOND),
            {
                home_menu
            }
            );
    }    pbf_press_button(context, BUTTON_X, 10, 20);
    pbf_press_button(context, BUTTON_A, 10, 320);
    pbf_press_button(context, BUTTON_A, 10, 3150);
    pbf_press_button(context, BUTTON_A, 10, 3150);

    home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);


    // PokemonSV::AdvanceDialogWatcher evo_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_BLACK);
    // PokemonSV::AdvanceDialogWatcher next_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_WHITE);
    // PokemonSV::PromptDialogWatcher learn_move_message(COLOR_CYAN);
    // PokemonSV::NormalBattleMenuWatcher battle_menu(COLOR_RED);

    // int ret = wait_until(
    //     env.console, context,
    //     Milliseconds(60*TICKS_PER_SECOND),
    //     {
    //         evo_message,
    //         next_message,
    //         learn_move_message
    //     }
    //     );

    // env.console.log(std::to_string(ret));

    // ImageFloatBox evolve_message(0.28, 0.76, 0.065, 0.055);

    // int ret = wait_until(
    //     env.console, context,
    //     Milliseconds(60*TICKS_PER_SECOND),
    //     {
    //         evo_message,
    //         next_message,
    //         learn_move_message
    //     }
    //     );

    // switch(ret){
    // case 0:
    //     screen = env.console.video().snapshot();
    //     pbf_press_button(context, BUTTON_A, 10, 20);
    //     if(sanitize_OCR(OCR::(Language::English, extract_box_reference(screen, evolve_message))) == "What?"){
    //         if(rand() > (1-(std::pow(1-(0.5),1/4)))){
    //             pbf_mash_button(context, BUTTON_B, 500ms);
    //         }  else{
    //             pbf_wait(context, 10000ms);
    //         }
    //     }
    // case 1:
        // pbf_mash_button(context, BUTTON_B, 500ms);
    // case 2:
    //     pbf_press_button(context, BUTTON_B, 10, 20);
    //     break;
    // default:
    //     throw;
    // }


    // ImageFloatBox hand_region = {0.03, 0.15, 0.45, 0.5};
    // HomeCursorWatcher handWatcher(HomeCursorType::GRABBING, hand_region, COLOR_WHITE);

    // int ret = wait_until(env.console, context, 2000ms, {handWatcher});
    // if (ret == 0){
    //     auto [x, y] = handWatcher.location();
    //     env.console.log("HERE at ("+std::to_string(x)+", "+std::to_string(y)+")");
    // }

    // home_manager.scroll_filter_menu(env, context, "labels");

    // ImageFloatBox dialog_box(0.7, 0.22, 0.2, 0.05);
    // ImageViewRGB32 dialog_image = extract_box_reference(screen, dialog_box);
    // const auto result = FilterMenuReader::instance().read_substring(
    //     env.console, Language::English, dialog_image,
    //     OCR::WHITE_TEXT_FILTERS()
    //     );

    // env.console.log(std::to_string(FilterMenuReader::instance().distance_to(result.results.cbegin()->second.token, "labels")));

    // ImageFloatBox game_icon_box_prim(0.9255, 0.715, 0.035, 0.057);
    // FloatPixel game_icon_prim = image_stats(extract_box_reference(screen, game_icon_box_prim)).average;
    // env.console.log(std::to_string(game_icon_prim.r) + " " + std::to_string(game_icon_prim.g) + " " + std::to_string(game_icon_prim.b));

    // ImageFloatBox page_r_button(0.1, 0.13, 0.1, 0.1);
    // // FloatPixel pixel = image_stats(extract_box_reference(screen, page_r_button)).average;

    // box_render.add(COLOR_RED, page_r_button);
    // // env.console.log(std::to_string(pixel.r) + " " + std::to_string(pixel.g) + " " + std::to_string(pixel.b));

    // pbf_wait(context, 2000ms);

    // context.wait_for_all_requests();
    // box_render.clear();

}

}
}
}
