/*  Pokemon HOME — Dex Layout framework
 *
 *  Describes the living-dex region as a set of dexes, each with a *static* box footprint.
 *  The living-dex region is the sum of the enabled dexes' footprints, starting at the first
 *  Home box; everything after it is the "bulk" region that the sort planner is allowed to
 *  rearrange. This lets the living-dex/bulk boundary be *derived* from configuration instead
 *  of hand-set, and lets multiple dexes be added later without code changes.
 *
 *  Configuration source (optional): <RESOURCE_PATH>/PokemonHome/DexLayout.json
 *      {
 *        "dexes": [
 *          { "name": "National Dex", "boxes": 51 }
 *          // add more dexes here later; each contributes its static box count
 *        ]
 *      }
 *  If the file is absent or empty, a built-in default is used (National Dex = 51 boxes),
 *  matching the Enrichment living-dex layout (species + forms + generation spacers).
 */

#ifndef PokemonAutomation_PokemonHome_DexLayout_H
#define PokemonAutomation_PokemonHome_DexLayout_H

#include <string>
#include <vector>

namespace PokemonAutomation{
    class Logger;
namespace NintendoSwitch{
namespace PokemonHome{

// One dex the living-dex region reserves space for, with its static box footprint.
struct DexSpec{
    std::string name;
    int boxes = 0;
};

// Load the enabled dexes (and their static box counts) from DexLayout.json if present,
// otherwise the built-in default. Never returns empty.
std::vector<DexSpec> load_enabled_dexes(Logger& logger);

// Total boxes occupied by the living-dex region (sum of enabled dex footprints).
int total_dex_boxes(const std::vector<DexSpec>& dexes);

// Result of checking whether [home_first_box, home_last_box] can hold the living dex and
// leave room for a bulk region.
struct DexSpaceCheck{
    bool dex_fits = false;         // living dex fits within the Home range
    int  living_dex_boxes = 0;     // total living-dex footprint
    int  living_dex_last_box = 0;  // last box of the living dex (home_first + boxes - 1)
    int  bulk_first_box = 0;       // first bulk box (living_dex_last_box + 1)
    int  bulk_box_count = 0;       // boxes available for bulk sorting (0 if none)
    bool has_bulk_room = false;    // bulk_box_count > 0
    std::string message;           // human-readable summary / warning for the user
};

// Compute the living-dex / bulk split for the given Home box range and report space issues.
DexSpaceCheck check_dex_space(Logger& logger, int home_first_box, int home_last_box);

}
}
}
#endif
