#include "PokemonHome_PokemonData.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/Globals.h"
#include <format>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

StatsHuntGenderFilter string_to_gender(const std::string& gender_str){
    if (gender_str == "male") return StatsHuntGenderFilter::Male;
    if (gender_str == "female") return StatsHuntGenderFilter::Female;
    if (gender_str == "genderless") return StatsHuntGenderFilter::Genderless;
    return StatsHuntGenderFilter::Any; // fallback / default
}

inline bool operator==(StatsHuntGenderFilter lhs, StatsHuntGenderFilter rhs) {
    // Cast to underlying type to avoid recursive calls
    int lhs_val = static_cast<int>(lhs);
    int rhs_val = static_cast<int>(rhs);

    // If either side is Any, treat as match
    if (lhs_val == static_cast<int>(StatsHuntGenderFilter::Any)) return true;
    if (rhs_val == static_cast<int>(StatsHuntGenderFilter::Any)) return true;

    return lhs_val == rhs_val;
}

inline bool operator!=(StatsHuntGenderFilter lhs, StatsHuntGenderFilter rhs) {
    return !(lhs == rhs);
}

std::vector<PokemonInformation> PokemonInformation::match(const std::vector<PokemonInformation>& candidates) const {
    std::vector<PokemonInformation> results;

    for (const auto& candidate : candidates) {
        if (id.has_value() && candidate.id != id) continue;
        if (form.has_value() && candidate.form != form) continue;
        if (form_id.has_value() && candidate.form_id != form_id) continue;
        if (evolution_chain.has_value() && candidate.evolution_chain != evolution_chain) continue;
        if (gender.has_value() && candidate.gender.value() != gender.value())continue;
        if (type1.has_value() && candidate.type1 != type1) continue;
        if (type2.has_value() && candidate.type2 != type2) continue;
        if (region.has_value() && candidate.region != region) continue;

        results.push_back(candidate);
    }

    return results;
}


PokedexReader::PokedexReader(){
    JsonValue json_value = load_json_file(RESOURCE_PATH() + "PokemonHome/pokedex.json");
    JsonObject* test = json_value.to_object();

    for(int i = 1; i <= 1024; i++){
        std::string key = std::format("{:04}", i);

        JsonValue* form = test->get_value(key);

        JsonArray* info = form->to_array();

        for (size_t j = 0; j < info->size(); j++){
            JsonValue& element = (*info)[j];
            JsonObject* obj = element.to_object();
            if (!obj) continue; // skip if not an object

            PokemonInformation pokemon;

            // Easy mappings
            pokemon.Id(i);
            pokemon.Form_Id(static_cast<int>(obj->get_integer_throw("form_id")));
            pokemon.Form(obj->get_string_throw("form"));
            pokemon.Gender(string_to_gender(obj->get_string_throw("gender")));
            pokemon.Region(string_to_region(obj->get_string_throw("region")));

            // pokemon.availability = ...
            // pokemon.evolution_chain = ...
            // pokemon.evolutions = ...
            // Assume `obj` is a JsonObject* for the current Element

            const JsonArray& typesArray = obj->get_array_throw("types");

            // Helper lambda to convert string to PokemonType enum
            auto string_to_type = [](const std::string& s) -> PokemonType{
                if (s == "normal")   return PokemonType::NORMAL;
                if (s == "fire")     return PokemonType::FIRE;
                if (s == "fighting") return PokemonType::FIGHTING;
                if (s == "water")    return PokemonType::WATER;
                if (s == "flying")   return PokemonType::FLYING;
                if (s == "grass")    return PokemonType::GRASS;
                if (s == "poison")   return PokemonType::POISON;
                if (s == "electric") return PokemonType::ELECTRIC;
                if (s == "ground")   return PokemonType::GROUND;
                if (s == "psychic")  return PokemonType::PSYCHIC;
                if (s == "rock")     return PokemonType::ROCK;
                if (s == "ice")      return PokemonType::ICE;
                if (s == "bug")      return PokemonType::BUG;
                if (s == "dragon")   return PokemonType::DRAGON;
                if (s == "ghost")    return PokemonType::GHOST;
                if (s == "dark")     return PokemonType::DARK;
                if (s == "steel")    return PokemonType::STEEL;
                if (s == "fairy")    return PokemonType::FAIRY;
                return PokemonType::NONE; // fallback
            };

            // First type
            pokemon.Type1(string_to_type(typesArray[0].to_string_throw()));

            // Second type if it exists
            if (typesArray.size() > 1){
                pokemon.Type2(string_to_type(typesArray[1].to_string_throw()));
            } else {
                pokemon.Type2(PokemonType::NONE);
            }

            // push into your collection
            m_pokemon.push_back(std::move(pokemon));
        }
    }

    json_value.clear();
}



PokemonData::PokemonData() = default;


PokemonData::PokemonData(int id, int form_id, std::string form,
                         StatsHuntGenderFilter gender,
                         PokemonType type1, PokemonType type2,
                         Region region, int ot_id, int level,
                         bool shiny, bool gmax, PokemonType tera, bool prime_example)
    : id(id)                     // int: just copy, no move needed
    , form_id(form_id)           // int: copy
    , form(std::move(form))      // string: move avoids copy
    , gender(gender)             // enum: copy
    , type1(type1)               // enum: copy
    , type2(type2)               // enum: copy
    , region(region)             // enum: copy
    , ot_id(ot_id)               // int: copy
    , level(level)               // int: copy
    , shiny(shiny)               // bool: copy
    , gmax(gmax)                 // bool: copy
    , tera(tera)                 // enum: copy
    , prime_example(prime_example) // bool: copy
{}




std::vector<PokemonInformation> PokemonData::match(const std::vector<PokemonInformation>& candidates) const {
    std::vector<PokemonInformation> results;

    for (const auto& info : candidates) {
        if (info.id.has_value() && info.id.value() != id) continue;
        if (info.form_id.has_value() && info.form_id.value() != form_id) continue;
        if (info.form.has_value() && info.form.value() != form) continue;
        if (info.gender.has_value()){
            auto lhsgender = info.gender.value();
            auto rhsgender = gender;
            if(lhsgender!=rhsgender){
                continue;
            }
        }
        if (info.type1.has_value() && info.type1.value() != type1) continue;
        if (info.type2.has_value() && info.type2.value() != type2) continue;
        if (info.region.has_value() && info.region.value() != region) continue;
        // Can add more fields if needed

        results.push_back(info);
    }

    return results;
}

}
}
}
