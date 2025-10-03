/*  Pokemon Moves Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Language.h"
#include "PokemonHome_AbilityReader.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{



AbilityOCR& AbilityOCR::instance(){
    static AbilityOCR reader;
    return reader;
}

AbilityOCR::AbilityOCR()
    : SmallDictionaryMatcher("PokemonHome/AbilityList.json")
{}

OCR::StringMatchResult AbilityOCR::read_substring(
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

AbilityReader::AbilityReader(Language language)
    : m_language(language)
{}

std::string AbilityReader::read_ability(Logger& logger, const ImageViewRGB32& box) const{
    const auto ocr_result = AbilityOCR::instance().read_substring(
        logger, m_language,
        box, OCR::BLACK_OR_WHITE_TEXT_FILTERS()
        );

    std::multimap<double, OCR::StringMatchData> results;
    if (!ocr_result.results.empty()){
        for (const auto& result : ocr_result.results){
            results.emplace(result.first, result.second);
        }
    }

    if (results.empty()){
        return "";
    }

    if (results.size() > 1){
        throw_and_log<OperationFailedException>(
            logger, ErrorReport::SEND_ERROR_REPORT,
            "MenuOption::read_option(): Unable to read item. Ambiguous or multiple results.\n"
            );
    }

    return results.begin()->second.token;
}



}
}
}
