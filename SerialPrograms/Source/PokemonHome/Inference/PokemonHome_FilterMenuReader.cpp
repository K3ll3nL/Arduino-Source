/*  Filter Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonTools/OCR/OCR_Routines.h"
#include <iostream>
#include "PokemonHome_FilterMenuReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{


FilterMenuReader& FilterMenuReader::instance(){
    static FilterMenuReader reader;
    return reader;
}


FilterMenuReader::FilterMenuReader()
    : SmallDictionaryMatcher("PokemonHome/FilterMenuOCR.json")
{}



OCR::StringMatchResult FilterMenuReader::read_substring(
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

int FilterMenuReader::distance_to(
    std::string at,
    std::string dest
){
    const std::vector<std::string> filters_list = {
        "pokemon-species",
        "gender",
        "type-1",
        "type-2",
        "nature",
        "markings",
        "origin-mark",
        "move-1",
        "move-2",
        "move-3",
        "move-4",
        "ability",
        "original-trainer",
        "shiny",
        "pokerus",
        "compatible-games",
        "labels"
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
