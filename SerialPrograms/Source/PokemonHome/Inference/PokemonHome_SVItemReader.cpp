/*  Filter Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonTools/OCR/OCR_Routines.h"
#include <iostream>
#include "PokemonHome_SVItemReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


SVItemReader& SVItemReader::instance(){
    static SVItemReader reader;
    return reader;
}


SVItemReader::SVItemReader()
    : SmallDictionaryMatcher("PokemonHome/SVItemOCR.json")
{}



OCR::StringMatchResult SVItemReader::read_substring(
    Logger& logger,
    Language language,
    const ImageViewRGB32& image,
    const std::vector<OCR::TextColorRange>& text_color_ranges,
    double min_text_ratio, double max_text_ratio
    ) const{
    return match_substring_from_image_multifiltered(
        &logger, language, image, text_color_ranges,
        MAX_LOG10P, MAX_LOG10P_SPREAD, min_text_ratio, max_text_ratio
        );
}

int SVItemReader::distance_to(
    std::string at,
    std::string dest
    ){
    const std::vector<std::string> filters_list = {
        "hp-up",
        "protein",
        "iron",
        "carbos",
        "calcium",
        "rare-candy",
        "pp-up",
        "zinc",
        "pp-max",
        "sun-stone",
        "moon-stone",
        "fire-stone",
        "thunder-stone",
        "water-stone",
        "leaf-stone",
        "shiny-stone",
        "dusk-stone",
        "dawn-stone",
        "oval-stone",
        "odd-keystone",
        "griseous-orb",
        "adamant-orb",
        "lustrous-orb",
        "bright-powder",
        "white-herb",
        "quick-claw",
        "soothe-bell",
        "mental-herb",
        "choice-band",
        "kings-rock",
        "silver-powder",
        "amulet-coin",
        "cleanse-tag",
        "soul-dew",
        "smoke-ball",
        "everstone",
        "focus-band",
        "lucky-egg",
        "scope-lens",
        "metal-coat",
        "leftovers",
        "dragon-scale",
        "light-ball",
        "soft-sand",
        "hard-stone",
        "miracle-seed",
        "black-glasses",
        "black-belt",
        "magnet",
        "mystic-water",
        "sharp-beak",
        "poison-barb",
        "never-melt-ice",
        "spell-tag",
        "twisted-spoon",
        "charcoal",
        "dragon-fang",
        "silk-scarf",
        "upgrade",
        "shell-bell",
        "wide-lens",
        "muscle-band",
        "wise-glasses",
        "expert-belt",
        "light-clay",
        "life-orb",
        "power-herb",
        "toxic-orb",
        "flame-orb",
        "focus-sash",
        "zoom-lens",
        "metronome",
        "iron-ball",
        "lagging-tail",
        "destiny-knot",
        "black-sludge",
        "icy-rock",
        "smooth-rock",
        "heat-rock",
        "damp-rock",
        "grip-claw",
        "choice-scarf",
        "sticky-barb",
        "power-bracer",
        "power-belt",
        "power-lens",
        "power-band",
        "power-anklet",
        "power-weight",
        "shed-shell",
        "big-root",
        "choice-specs",
        "flame-plate",
        "splash-plate",
        "zap-plate",
        "meadow-plate",
        "icicle-plate",
        "fist-plate",
        "toxic-plate",
        "earth-plate",
        "sky-plate",
        "mind-plate",
        "insect-plate",
        "stone-plate",
        "spooky-plate",
        "draco-plate",
        "dread-plate",
        "iron-plate",
        "protector",
        "electirizer",
        "magmarizer",
        "dubious-disc",
        "reaper-cloth",
        "razor-claw",
        "razor-fang",
        "red-apricorn",
        "blue-apricorn",
        "yellow-apricorn",
        "green-apricorn",
        "pink-apricorn",
        "white-apricorn",
        "black-apricorn",
        "prism-scale",
        "eviolite",
        "float-stone",
        "rocky-helmet",
        "air-balloon",
        "red-card",
        "ring-target",
        "binding-band",
        "absorb-bulb",
        "cell-battery",
        "eject-button",
        "normal-gem",
        "health-feather",
        "muscle-feather",
        "resist-feather",
        "genius-feather",
        "clever-feather",
        "swift-feather",
        "dna-splicers",
        "dna-splicers",
        "weakness-policy",
        "assault-vest",
        "pixie-plate",
        "ability-capsule",
        "luminous-moss",
        "snowball",
        "safety-goggles",
        "bottle-cap",
        "gold-bottle-cap",
        "adrenaline-orb",
        "ice-stone",
        "red-nectar",
        "yellow-nectar",
        "pink-nectar",
        "purple-nectar",
        "terrain-extender",
        "protective-pads",
        "electric-seed",
        "psychic-seed",
        "misty-seed",
        "grassy-seed",
        "n-solarizer",
        "n-lunarizer",
        "n-solarizer",
        "n-lunarizer",
        "rusted-sword",
        "rusted-shield",
        "strawberry-sweet",
        "love-sweet",
        "berry-sweet",
        "clover-sweet",
        "flower-sweet",
        "star-sweet",
        "ribbon-sweet",
        "sweet-apple",
        "tart-apple",
        "throat-spray",
        "eject-pack",
        "heavy-duty-boots",
        "blunder-policy",
        "room-service",
        "utility-umbrella",
        "exp-candy-xs",
        "exp-candy-s",
        "exp-candy-m",
        "exp-candy-l",
        "exp-candy-xl",
        "lonely-mint",
        "adamant-mint",
        "naughty-mint",
        "brave-mint",
        "bold-mint",
        "impish-mint",
        "lax-mint",
        "relaxed-mint",
        "modest-mint",
        "mild-mint",
        "rash-mint",
        "quiet-mint",
        "calm-mint",
        "gentle-mint",
        "careful-mint",
        "sassy-mint",
        "timid-mint",
        "hasty-mint",
        "jolly-mint",
        "naive-mint",
        "serious-mint",
        "cracked-pot",
        "chipped-pot",
        "galarica-twig",
        "galarica-cuff",
        "galarica-wreath",
        "ability-patch",
        "adamant-crystal",
        "lustrous-globe",
        "griseous-core",
        "malicious-armor",
        "normal-tera-shard",
        "fire-tera-shard",
        "water-tera-shard",
        "electric-tera-shard",
        "grass-tera-shard",
        "ice-tera-shard",
        "fighting-tera-shard",
        "poison-tera-shard",
        "ground-tera-shard",
        "flying-tera-shard",
        "psychic-tera-shard",
        "bug-tera-shard",
        "rock-tera-shard",
        "ghost-tera-shard",
        "dragon-tera-shard",
        "dark-tera-shard",
        "steel-tera-shard",
        "fairy-tera-shard",
        "booster-energy",
        "ability-shield",
        "clear-amulet",
        "mirror-herb",
        "punching-glove",
        "covert-cloak",
        "loaded-dice",
        "auspicious-armor",
        "leaders-crest",
        "fairy-feather",
        "syrupy-apple",
        "unremarkable-teacup",
        "masterpiece-teacup",
        "cornerstone-mask",
        "wellspring-mask",
        "hearthflame-mask",
        "health-mochi",
        "muscle-mochi",
        "resist-mochi",
        "genius-mochi",
        "clever-mochi",
        "swift-mochi",
        "fresh-start-mochi",
        "metal-alloy",
        "stellar-tera-shard"
    };

    auto it_at = std::find(filters_list.begin(), filters_list.end(), at);
    auto it_dest = std::find(filters_list.begin(), filters_list.end(), dest);

    if (it_at == filters_list.end() || it_dest == filters_list.end()){
        return 0;
    }

    int index_at = static_cast<int>(std::distance(filters_list.begin(), it_at));
    int index_dest = static_cast<int>(std::distance(filters_list.begin(), it_dest));
    int size = static_cast<int>(filters_list.size());

    int forward = (index_dest - index_at + size) % size;
    int backward = (index_at - index_dest + size) % size;

    // Pick the shorter path — use negative if going backward is faster
    return (forward <= backward) ? forward : -backward;


}



}
}
}
