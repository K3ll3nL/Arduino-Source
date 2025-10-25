/*  Program Name
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/OCR/OCR_RawOCR.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "PokemonHome/Programs/Enrichment_Tools.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV/Inference/PokemonSV_PokePortalDetector.h"
#include "PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "PokemonSwSh/Commands/PokemonSwSh_Commands_EggRoutines.h"
#include "PokemonSwSh/Programs/EggPrograms/PokemonSwSh_EggAutonomous.h"

#include "PokemonHome/Programs/PokemonHome_Enrichment2.h"

#include <fstream>
#include <iostream>

namespace PokemonAutomation{

class VideoOverlaySet;
class VideoOverlay;
class OverlayBoxScope;


namespace NintendoSwitch{
namespace PokemonHome{


class FileWatcher : public VisualInferenceCallback{
public:
    FileWatcher(std::string phrase):VisualInferenceCallback("FileWatcher"),m_phrase(phrase){}

    virtual void make_overlays(VideoOverlaySet& items) const override{}
    virtual bool process_frame(const VideoSnapshot& frame) override{
        std::ifstream file("Home Storage/lock.txt");
        if (!file.is_open()){
            std::cerr << "could not open file";
            return false;
        }

        std::string mode, game, quantity;
        file >> mode;
        if (mode == m_phrase){
            if (file >> game){
                m_game = game;
                if(file >> quantity){
                    m_quantity = quantity;
                }
            } else {
                m_game.clear();
            }
            return true;
        }
        return false;
    }


    std::string m_phrase;
    std::string m_game;
    std::string m_quantity;
};


Enrichment2_Descriptor::Enrichment2_Descriptor()
    : SingleSwitchProgramDescriptor(
        "GameName:Enrichment2",
        "Game Name", "Enrichment 2",
        "ComputerControl/blob/master/Wiki/Programs/GameName/Enrichment2.md",
        "Secondary Part of Home Enrichment, for second switch.",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        {ControllerFeature::NintendoSwitch_ProController},
        FasterIfTickPrecise::NOT_FASTER
    )
{}
struct Enrichment2_Descriptor::Stats : public StatsTracker{
    Stats()
        : m_attempts(m_stats["Attempts"])
        , m_errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Attempts");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }
    std::atomic<uint64_t>& m_attempts;
    std::atomic<uint64_t>& m_errors;
};
std::unique_ptr<StatsTracker> Enrichment2_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}



Enrichment2::Enrichment2()
    : GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

bool close_game_and_open(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string& target_game){

    if(target_game == "sv"){
        target_game = "Pokémon Scarlet";
    }

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
            pbf_mash_button(context, BUTTON_A, 2s);
            found = true;
            break;
        }

        pbf_press_button(context, BUTTON_RIGHT, 10, 30);
    }

    overlays.clear();

    if (!found){
        env.console.log("ERROR: Could not locate \"" + target_game + "\".");
        return false;
    }

    if (target_game == "Pokémon Scarlet"){
        PokemonSV::OverworldWatcher overworld(env.console, COLOR_RED);
        load_into_sv(env, context);
        return wait_until(env.console, context, 30s, {overworld})==0;
    }

    env.console.log("WARNING: No special handling for \"" + target_game + "\".");
    return true;
}


bool test(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    while(!context.cancelled()){
        pbf_wait(context, 3s);

        pbf_press_button(context, BUTTON_B, 10, 50);

        VideoSnapshot screen = env.console.video().snapshot();

        env.console.log("Testing");
    }

    return true;

}



void Enrichment2::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    Enrichment2_Descriptor::Stats& stats = env.current_stats<Enrichment2_Descriptor::Stats>();

    FileWatcher tradeWatcher("trading");
    int mode = run_until<ProControllerContext>(
        env.console, context,
        [&](ProControllerContext& scope){
            test(env, scope);
        },
        {tradeWatcher},
        20s
    );

    switch(mode){
    case 0:
        close_game_and_open(env, context, tradeWatcher.m_game);
        PokemonSV::enter_menu_from_overworld(env.program_info(), env.console, context, 3);

        PokemonSV::PokePortalWatcher pokePortal(COLOR_RED);
        int ret = wait_until(env.console, context, 20s,{pokePortal});

        if(ret==-1)throw;

        pbf_press_button(context, BUTTON_DOWN, 10, 40);
        pbf_press_button(context, BUTTON_DOWN, 10, 40);
        pbf_press_button(context, BUTTON_A, 10, 40);
        pbf_press_button(context, BUTTON_A, 10, 70);
        pbf_press_button(context, BUTTON_A, 10, 70);
    }



    try{

    } catch(OperationFailedException&){
        stats.m_errors++;
        env.update_stats();
        throw;
    }

    env.update_stats();
    GO_HOME_WHEN_DONE.run_end_of_program(context);
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}






}
}
}
