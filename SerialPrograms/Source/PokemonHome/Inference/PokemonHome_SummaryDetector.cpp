#include "PokemonHome_SummaryDetector.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "PokemonHome/Inference/PokemonHome_AbilityReader.h"
#include "PokemonHome/Inference/PokemonHome_BoxGenderDetector.h"
#include <iostream>
#include <unordered_map>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

struct FormVisualData {
    std::vector<FloatPixel> normal;
    std::vector<FloatPixel> shiny;
};

struct PokemonVisualData {
    std::vector<std::string> boxes;
    std::vector<FormVisualData> forms;
};

SummaryDetector::~SummaryDetector() = default;

SummaryDetector::SummaryDetector(Color color)
    : m_color(color)
{}

void SummaryDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool SummaryDetector::detect(const ImageViewRGB32& screen){
    return true;
}

PokemonData SummaryDetector::identify_pokemon(Logger& logger, const ImageViewRGB32& screen, PokedexReader& information){
    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.042, 0.04);
    ImageFloatBox shiny_symbol_box(0.702, 0.09, 0.04, 0.06);
    ImageFloatBox gmax_symbol_box(0.463, 0.09, 0.04, 0.06);
    ImageFloatBox pokemon_box_small(0.76, 0.295, 0.14, 0.23);
    ImageFloatBox origin_symbol_box(0.623, 0.095, 0.033, 0.05);
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041);
    ImageFloatBox type_1(0.622, 0.245, 0.029, 0.053);
    ImageFloatBox type_2(0.654, 0.245, 0.029, 0.053);
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046);
    ImageFloatBox ability_box(0.17, 0.84, 0.215, 0.044);

    auto closest_type = [&](const FloatPixel& color_box) -> PokemonType{
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
    };

    AbilityReader ability_reader(Language::English);


    FloatPixel type_1_color = image_stats(extract_box_reference(screen, type_1)).average;
    FloatPixel type_2_color = image_stats(extract_box_reference(screen, type_2)).average;
    PokemonType type1 = closest_type(type_1_color);
    PokemonType type2 = closest_type(type_2_color);
    // FloatPixel pokemon_color = image_stats(extract_box_reference(screen, pokemon_box_small)).average;    // TODO: Use this for multiple matches
    int shiny_stddev_value = static_cast<int>(image_stddev(extract_box_reference(screen, shiny_symbol_box)).sum());
    int gmax_stddev_value = static_cast<int>(image_stddev(extract_box_reference(screen, gmax_symbol_box)).sum());
    int ot_id_value = OCR::read_number_waterfill(logger, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
    int national_dex_number = OCR::read_number_waterfill(logger, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
    StatsHuntGenderFilter gender = BoxGenderDetector::detect(screen);
    int level = OCR::read_number_waterfill(logger, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
    std::string ability = ability_reader.read_ability(logger, extract_box_reference(screen, ability_box));

    std::vector<PokemonInformation> matches = PokemonInformation().Id(national_dex_number).Gender(gender).Type1(type1).Type2(type2).Ability({ability}).match(information.get_pokedex());

    logger.log(
        "identify_pokemon: dex=" + std::to_string(national_dex_number) +
        " gender=" + std::to_string((int)gender) +
        " type1=" + std::to_string((int)type1) +
        " type2=" + std::to_string((int)type2) +
        " ability=" + ability +
        " matches=" + std::to_string(matches.size())
    );

    if(matches.size()==1){
        return PokemonData(
            national_dex_number,
            matches[0].form_id.value(),
            matches[0].form.value(),
            gender,
            type1,
            type2,
            matches[0].region.value(),
            ot_id_value,
            level,
            shiny_stddev_value > 30,
            gmax_stddev_value > 30,
            ability,
            PokemonType::NONE, // TODO: Add tera typing
            false
            );
    }else{
        static const std::unordered_map<std::string, ImageFloatBox> box_registry = {
            {"main",  ImageFloatBox(0.760,  0.295,  0.14,   0.23  )},
            {"hat",   ImageFloatBox(0.72,   0.25,   0.15,   0.2)},
            {"sweet", ImageFloatBox(0.765,  0.383,  0.01,   0.02)},
            {"low",   ImageFloatBox(0.8,    0.55,   0.001,  0.001)},
            {"u1",    ImageFloatBox(0.760,  0.295,  0.0467, 0.0767)},
            {"u2",    ImageFloatBox(0.8067, 0.295,  0.0467, 0.0767)},
            {"u3",    ImageFloatBox(0.8533, 0.295,  0.0467, 0.0767)},
            {"u4",    ImageFloatBox(0.760,  0.3717, 0.0467, 0.0767)},
            {"u5",    ImageFloatBox(0.8067, 0.3717, 0.0467, 0.0767)},
            {"u6",    ImageFloatBox(0.8533, 0.3717, 0.0467, 0.0767)},
            {"u7",    ImageFloatBox(0.760,  0.4483, 0.0467, 0.0767)},
            {"u8",    ImageFloatBox(0.8067, 0.4483, 0.0467, 0.0767)},
            {"u9",    ImageFloatBox(0.8533, 0.4483, 0.0467, 0.0767)},
        };

        std::unordered_map<int, PokemonVisualData> visual_forms;
        {
            JsonValue json = load_json_file(RESOURCE_PATH() + "PokemonHome/visual_forms.json");
            JsonObject* root = json.to_object();
            if (root){
                for (auto it = root->begin(); it != root->end(); ++it){
                    int dex_id = std::stoi(it->first);
                    JsonObject* poke_obj = it->second.to_object();
                    if (!poke_obj) continue;

                    const JsonArray* boxes_json = poke_obj->get_array("boxes_used");
                    JsonValue* forms_val = poke_obj->get_value("forms");
                    if (!boxes_json || !forms_val || !forms_val->is_array()) continue;
                    JsonArray& forms_json = *forms_val->to_array();

                    PokemonVisualData poke_data;
                    for (size_t b = 0; b < boxes_json->size(); b++){
                        const std::string* box_name = (*boxes_json)[b].to_string();
                        if (box_name) poke_data.boxes.push_back(*box_name);
                    }

                    for (size_t i = 0; i < forms_json.size(); i++){
                        JsonObject* form = forms_json[i].to_object();
                        FormVisualData data;
                        if (form){
                            JsonValue* values_val = form->get_value("values");
                            if (values_val && values_val->is_array()){
                                JsonArray& values = *values_val->to_array();
                                JsonArray* normal_boxes = values.size() > 0 ? values[0].to_array() : nullptr;
                                JsonArray* shiny_boxes  = values.size() > 1 ? values[1].to_array() : nullptr;
                                for (size_t b = 0; b < poke_data.boxes.size(); b++){
                                    FloatPixel n, s;
                                    if (normal_boxes && b < normal_boxes->size()){
                                        JsonArray* rgb = (*normal_boxes)[b].to_array();
                                        if (rgb && rgb->size() == 3)
                                            n = FloatPixel((*rgb)[0].to_double_default(), (*rgb)[1].to_double_default(), (*rgb)[2].to_double_default());
                                    }
                                    if (shiny_boxes && b < shiny_boxes->size()){
                                        JsonArray* rgb = (*shiny_boxes)[b].to_array();
                                        if (rgb && rgb->size() == 3)
                                            s = FloatPixel((*rgb)[0].to_double_default(), (*rgb)[1].to_double_default(), (*rgb)[2].to_double_default());
                                    }
                                    data.normal.push_back(n);
                                    data.shiny.push_back(s);
                                }
                            }
                        }
                        poke_data.forms.push_back(std::move(data));
                    }
                    visual_forms[dex_id] = std::move(poke_data);
                }
                logger.log("visual_forms loaded: " + std::to_string(visual_forms.size()) + " entries");
            }else{
                logger.log("visual_forms failed to load from: " + RESOURCE_PATH() + "PokemonHome/visual_forms.json");
            }
        }

        std::unordered_map<int, std::pair<bool,bool>> issue_list = { // TODO: FIX THIS
            {774, {false,true}},
            {854,{true,true}},
            {855,{true,true}},
            {869, {false,true}},
            {1012,{true,true}},
            {1013,{true,true}}
        };


        // The only forms left are visual forms, although shiny alcremie throws a huge wrench in things. Not sure how we'll check that.
        // I'm thinking set the form to -1, we can use a system that encodes the last 4 symbols (square, heart, star, diamond) to store the form
        // This will only be allowed for a subset of pokemon, which we can store in a player-specific txt file. if the ot and date met match, we can allow it as marked, otherwise, nothing should be marked.
        int form_id = -2;

        if(!matches.empty() && ((shiny_stddev_value > 30)?issue_list[national_dex_number].second:issue_list[national_dex_number].first)){ // Shiny alcremie, all sinistea, etc.
            return PokemonData(
                national_dex_number,
                -1,
                matches[0].form.value(),
                gender,
                type1,
                type2,
                matches[0].region.value(),
                ot_id_value,
                level,
                shiny_stddev_value > 30,
                gmax_stddev_value > 30,
                ability,
                PokemonType::NONE, // TODO: Add tera typing
                false
                );
            // throw std::logic_error("Unidentifiable form encountered. Unknown solution.");
        }else{
            const PokemonInformation* best_match = nullptr;

            // Cache box measurements so Unown's 9 boxes aren't re-extracted per candidate.
            std::unordered_map<std::string, FloatPixel> box_cache;
            auto get_box_color = [&](const std::string& name) -> FloatPixel {
                auto cache_it = box_cache.find(name);
                if (cache_it != box_cache.end()) return cache_it->second;
                auto reg_it = box_registry.find(name);
                if (reg_it == box_registry.end()) return FloatPixel(0, 0, 0);
                FloatPixel color = image_stats(extract_box_reference(screen, reg_it->second)).average;
                box_cache[name] = color;
                return color;
            };

            if(national_dex_number==201){
                int temp = 1;
                (void) temp;
            }

            auto vit = visual_forms.find(national_dex_number);
            if (vit != visual_forms.end()){
                const PokemonVisualData& poke_data = vit->second;
                bool is_shiny = shiny_stddev_value > 30;

                logger.log("visual_form check: dex=" + std::to_string(national_dex_number) +
                    " is_shiny=" + std::to_string(is_shiny) +
                    " shiny_stddev=" + std::to_string(shiny_stddev_value) +
                    " forms_in_json=" + std::to_string(poke_data.forms.size()) +
                    " candidates=" + std::to_string(matches.size()));

                for (const auto& candidate : matches){
                    int fid = candidate.form_id.value_or(-1);
                    if (fid < 0 || fid >= (int)poke_data.forms.size()){
                        logger.log("  fid=" + std::to_string(fid) + " (" + candidate.form.value_or("?") + ") -> out of range, skipping");
                        continue;
                    }
                    const FormVisualData& form_data = poke_data.forms[fid];
                    const std::vector<FloatPixel>& colors = is_shiny ? form_data.shiny : form_data.normal;

                    bool has_data  = false;
                    bool all_match = true;
                    for (size_t b = 0; b < poke_data.boxes.size() && all_match; b++){
                        if (b >= colors.size()){ all_match = false; break; }
                        const FloatPixel& ref = colors[b];
                        if (ref.r == 0 && ref.g == 0 && ref.b == 0){ all_match = false; break; }
                        has_data = true;
                        auto dist = euclidean_distance(get_box_color(poke_data.boxes[b]), ref);
                        if (dist > 5.0)
                            all_match = false;
                    }
                    if (!has_data) all_match = false;
                    logger.log("  fid=" + std::to_string(fid) + " (" + candidate.form.value_or("?") + ") -> has_data=" + std::to_string(has_data) + " all_match=" + std::to_string(all_match));
                    if (all_match){
                        form_id    = fid;
                        best_match = &candidate;
                        break;
                    }
                }
            }

            if (form_id == -2 && !matches.empty() && visual_forms.find(national_dex_number) == visual_forms.end()){
                logger.log("No visual_forms entry for dex #" + std::to_string(national_dex_number) + ", using first match: " + matches[0].form.value_or("?"));
                return PokemonData(
                    national_dex_number,
                    matches[0].form_id.value(),
                    matches[0].form.value(),
                    gender, type1, type2,
                    matches[0].region.value(),
                    ot_id_value, level,
                    shiny_stddev_value > 30,
                    gmax_stddev_value > 30,
                    ability,
                    PokemonType::NONE,
                    false
                );
            }

            if (form_id == -2) {
                std::string msg = "Could not identify visual form for dex #" + std::to_string(national_dex_number) + ". Box readings:\n                    [";
                auto vit2 = visual_forms.find(national_dex_number);
                if (vit2 != visual_forms.end()){
                    const auto& boxes = vit2->second.boxes;
                    for (size_t bi = 0; bi < boxes.size(); bi++){
                        FloatPixel c = get_box_color(boxes[bi]);
                        bool is_last = (bi + 1 == boxes.size());
                        msg += "[" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + (is_last ? "]], " : "],");
                    }
                } else {
                    FloatPixel c = get_box_color("main");
                    msg += "\n  main: (" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
                }
                logger.log(msg);
                throw ShinyFormNotFoundError(msg);
            }

            return PokemonData(
                national_dex_number,
                best_match->form_id.value(),
                best_match->form.value(),
                gender,
                type1,
                type2,
                best_match->region.value(),
                ot_id_value,
                level,
                shiny_stddev_value > 30,
                gmax_stddev_value > 30,
                ability,
                PokemonType::NONE, // TODO: Add tera typing
                false
            );
        }





    }

}



SummaryWatcher::~SummaryWatcher() = default;

SummaryWatcher::SummaryWatcher(Color color)
    : VisualInferenceCallback("SummaryWatcher")
    , m_detector(color)
    , m_frozen_screen(
          COLOR_CYAN,                      // overlay color for frozen detection
          ImageFloatBox(0.0, 0.0, 1.0, 1.0), // monitor full screen; adjust if needed
          std::chrono::milliseconds(250),         // wait 0.25 seconds of inactivity
          5.0                              // RMSD threshold
          )
{}

// Draw both overlays for debugging
void SummaryWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);      // summary detection boxes
    m_frozen_screen.make_overlays(items); // frozen screen box
}

bool SummaryWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){
    bool frozen = m_frozen_screen.process_frame(screen, timestamp);
    if (!frozen) return false;
    return m_detector.detect(screen);
}

// Get the Pokémon info using the SummaryDetector
PokemonData SummaryWatcher::get_pokemon(Logger& logger, const ImageViewRGB32& screen, PokedexReader& information){
    return m_detector.identify_pokemon(logger, screen, information);
}



}
}
}
