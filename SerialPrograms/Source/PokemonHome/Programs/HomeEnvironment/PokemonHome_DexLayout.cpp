/*  Pokemon HOME — Dex Layout framework. See PokemonHome_DexLayout.h. */

#include "PokemonHome_DexLayout.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/Globals.h"
#include <filesystem>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

static const char* DEX_LAYOUT_FILE = "PokemonHome/DexLayout.json";

std::vector<DexSpec> load_enabled_dexes(Logger& logger){
    std::vector<DexSpec> dexes;

    std::string path = RESOURCE_PATH() + DEX_LAYOUT_FILE;
    if (std::filesystem::exists(path)){
        try{
            JsonValue json = load_json_file(path);
            const JsonObject* root = json.to_object();
            const JsonArray*  arr  = root ? root->get_array("dexes") : nullptr;
            if (arr){
                for (size_t i = 0; i < arr->size(); i++){
                    const JsonObject* o = (*arr)[i].to_object();
                    if (o == nullptr) continue;
                    DexSpec spec;
                    spec.name  = o->get_string_default("name", "Dex");
                    spec.boxes = (int)o->get_integer_default("boxes", 0);
                    if (spec.boxes > 0) dexes.push_back(std::move(spec));
                }
            }
            if (!dexes.empty()){
                logger.log("DexLayout: loaded " + std::to_string(dexes.size()) + " dex(es) from " + path);
                return dexes;
            }
            logger.log("DexLayout: " + path + " contained no valid dexes; using built-in default.");
        }catch (...){
            logger.log("DexLayout: failed to read " + path + "; using built-in default.");
        }
    }

    // Built-in default: the National Dex Enrichment layout (species + forms + generation
    // spacers) occupies 51 boxes.
    dexes.push_back({"National Dex", 51});
    return dexes;
}

int total_dex_boxes(const std::vector<DexSpec>& dexes){
    int sum = 0;
    for (const DexSpec& d : dexes) sum += d.boxes;
    return sum;
}

DexSpaceCheck check_dex_space(Logger& logger, int home_first_box, int home_last_box){
    DexSpaceCheck result;

    std::vector<DexSpec> dexes = load_enabled_dexes(logger);
    result.living_dex_boxes    = total_dex_boxes(dexes);
    result.living_dex_last_box = home_first_box + result.living_dex_boxes - 1;
    result.bulk_first_box      = result.living_dex_last_box + 1;

    int available = home_last_box - home_first_box + 1;

    // Build a per-dex breakdown for the message.
    std::string breakdown;
    for (size_t i = 0; i < dexes.size(); i++){
        breakdown += (i ? ", " : "") + dexes[i].name + "=" + std::to_string(dexes[i].boxes);
    }

    if (result.living_dex_boxes > available){
        // The living dex alone does not fit in the configured Home range.
        result.dex_fits       = false;
        result.has_bulk_room  = false;
        result.bulk_box_count = 0;
        result.message =
            "Living dex needs " + std::to_string(result.living_dex_boxes) + " boxes (" + breakdown +
            ") but only " + std::to_string(available) + " are available (Home boxes " +
            std::to_string(home_first_box) + "-" + std::to_string(home_last_box) +
            "). Increase the Home box range or reduce enabled dexes.";
        return result;
    }

    result.dex_fits       = true;
    result.bulk_box_count  = home_last_box - result.bulk_first_box + 1;
    if (result.bulk_box_count < 0) result.bulk_box_count = 0;
    result.has_bulk_room   = result.bulk_box_count > 0;

    result.message =
        "Living dex uses " + std::to_string(result.living_dex_boxes) + " boxes (" + breakdown +
        "); bulk region = boxes " + std::to_string(result.bulk_first_box) + "-" +
        std::to_string(home_last_box) + " (" + std::to_string(result.bulk_box_count) + " boxes).";
    if (!result.has_bulk_room){
        result.message += " WARNING: no boxes remain for bulk sorting.";
    }
    return result;
}

}
}
}
