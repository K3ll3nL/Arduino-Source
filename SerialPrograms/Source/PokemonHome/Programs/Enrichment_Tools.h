#include "NintendoSwitch/Controllers/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

#ifndef ENRICHMENT_TOOLS_H
#define ENRICHMENT_TOOLS_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

void load_into_sv(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

std::string sanitize_OCR(std::string str);

}
}
}

#endif // ENRICHMENT_TOOLS_H
