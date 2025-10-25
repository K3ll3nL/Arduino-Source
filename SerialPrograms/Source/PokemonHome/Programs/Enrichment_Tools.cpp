#include "Enrichment_Tools.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

void load_into_sv(SingleSwitchProgramEnvironment& env, ProControllerContext& context){

    WhiteScreenOverWatcher pre_title_screen(PokemonAutomation::COLOR_RED, {0.5, 0, 0.5, 0.3});

    int ret = wait_until(
        env.console, context,
        Milliseconds(240*TICKS_PER_SECOND),
        {
            pre_title_screen
        }
        );

    if(ret==-1)throw;
    else{
        pbf_mash_button(context, BUTTON_A, 4*TICKS_PER_SECOND);
    }
};

std::string sanitize_OCR(std::string str){
    static const char unwanted[] = "\n\r—.,";
    for (char c : unwanted){
        str.erase(std::remove(str.begin(), str.end(), c), str.end());
    }
    return str;
};

}
}
}
