/*  Pokemon Moves Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonHome_AbilityReader_H
#define PokemonAutomation_PokemonHome_AbilityReader_H

#include "CommonFramework/Language.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

class AbilityOCR : public OCR::SmallDictionaryMatcher{
    static constexpr double MAX_LOG10P = -1.30;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

public:
    AbilityOCR();

    static AbilityOCR& instance();

    OCR::StringMatchResult read_substring(
        Logger& logger,
        Language language,
        const ImageViewRGB32& image,
        const std::vector<OCR::TextColorRange>& text_color_ranges,
        double min_text_ratio = 0.01, double max_text_ratio = 0.50
        ) const;


};

class AbilityReader{

public:
    AbilityReader(Language language);

    std::string read_ability(Logger& logger, const ImageViewRGB32& box) const;

private:
    Language m_language;
};



}
}
}
#endif
