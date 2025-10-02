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
    JsonObject* root = json_value.to_object();
    if (!root) return;

    // Resize outer vector so that index matches Pokémon ID (1-based)
    m_pokemon.resize(1025); // ID 1..1024, index 0 unused

    for(int i = 1; i <= 1024; i++){
        std::string key = std::format("{:04}", i);

        JsonValue* form = root->get_value(key);
        if (!form || !form->is_array()) continue;

        JsonArray& info = *form->to_array();

        for (size_t j = 0; j < info.size(); j++){
            JsonValue& element = info[j];
            JsonObject* obj = element.to_object();
            if (!obj) continue; // skip if not an object

            PokemonInformation pokemon;

            // Easy mappings
            pokemon.Id(i);
            pokemon.Form_Id(static_cast<int>(obj->get_integer_throw("form_id")));
            pokemon.Form(obj->get_string_throw("form"));
            pokemon.Gender(string_to_gender(obj->get_string_throw("gender")));
            pokemon.Region(string_to_region(obj->get_string_throw("region")));

            // Types
            const JsonArray& typesArray = obj->get_array_throw("types");

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
                return PokemonType::NONE;
            };

            pokemon.Type1(string_to_type(typesArray[0].to_string_throw()));
            if (typesArray.size() > 1){
                pokemon.Type2(string_to_type(typesArray[1].to_string_throw()));
            } else {
                pokemon.Type2(PokemonType::NONE);
            }

            // Push into the inner vector for this Pokémon ID
            m_pokemon[i].push_back(std::move(pokemon));
        }
    }
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




std::vector<PokemonInformation> PokemonData::match(const std::vector<std::vector<PokemonInformation>>& candidates) const {
    std::vector<PokemonInformation> results;

    const auto& forms = candidates[id]; // get inner vector for this ID

    for (const auto& info : forms) {
        if (info.id.has_value() && info.id.value() != id) continue;
        if (info.form_id.has_value() && info.form_id.value() != form_id) continue;
        if (info.form.has_value() && info.form.value() != form) continue;
        if (info.gender.has_value() && info.gender != gender) continue;
        if (info.type1.has_value() && info.type1.value() != type1) continue;
        if (info.type2.has_value() && info.type2.value() != type2) continue;
        if (info.region.has_value() && info.region.value() != region) continue;
        // Can add more fields if needed

        results.push_back(info);
    }

    return results;
}

HomeSlot::HomeSlot()
    : m_row(0), m_col(0), m_quick_color(), m_pokemon(std::nullopt) {}


HomeSlot::HomeSlot(int row, int col, FloatPixel quick_color)
    : m_row(row), m_col(col), m_quick_color(quick_color), m_pokemon(std::nullopt)
{}

HomeSlot::HomeSlot(int row, int col, FloatPixel quick_color, std::optional<PokemonData> pokemon)
    : m_row(row), m_col(col), m_quick_color(quick_color), m_pokemon(pokemon)
{}

void HomeSlot::setPokemon(const PokemonData& pokemon) {
    m_pokemon = pokemon;
}

void HomeSlot::clear() {
    m_pokemon.reset();
}


// Default constructor: empty slots
HomeBox::HomeBox()
    : m_slots(MAX_ROWS, std::vector<HomeSlot>(MAX_COLS))
{
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            m_slots[r][c] = HomeSlot(r, c, FloatPixel(), std::nullopt);
        }
    }
}

// Constructor: fill with Pokémon up to 30
HomeBox::HomeBox(const std::vector<PokemonData>& pokemon_list)
    : m_slots(MAX_ROWS, std::vector<HomeSlot>(MAX_COLS))
{
    int index = 0;
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            if (index < static_cast<int>(pokemon_list.size())) {
                m_slots[r][c] = HomeSlot(r, c, FloatPixel(), pokemon_list[index]);
                index++;
            } else {
                m_slots[r][c] = HomeSlot(r, c, FloatPixel(), std::nullopt);
            }
        }
    }
}

// Copy constructor
HomeBox::HomeBox(const HomeBox& other)
    : m_slots(MAX_ROWS, std::vector<HomeSlot>(MAX_COLS))
{
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            m_slots[r][c] = other.m_slots[r][c];
        }
    }
}

// Accessors
HomeSlot& HomeBox::at(int row, int col) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        throw std::out_of_range("HomeBox::at: row/col out of range.");
    }
    return m_slots[row][col];
}

const HomeSlot& HomeBox::at(int row, int col) const {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS) {
        throw std::out_of_range("HomeBox::at: row/col out of range.");
    }
    return m_slots[row][col];
}

// Swap Pokémon between two slots
void HomeBox::swap(int row1, int col1, int row2, int col2) {
    if (row1 < 0 || row1 >= MAX_ROWS || col1 < 0 || col1 >= MAX_COLS ||
        row2 < 0 || row2 >= MAX_ROWS || col2 < 0 || col2 >= MAX_COLS) {
        throw std::out_of_range("HomeBox::swapSlots: row/col out of range.");
    }

    auto& slot1 = m_slots[row1][col1];
    auto& slot2 = m_slots[row2][col2];

    std::swap(slot1.getPokemon(), slot2.getPokemon());
}

// Flatten into vector (skip blanks)
std::vector<PokemonData> HomeBox::flatten() const {
    std::vector<PokemonData> result;
    result.reserve(MAX_ROWS * MAX_COLS);

    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            const auto& slot = m_slots[r][c];
            if (slot.getPokemon().has_value()) {
                result.push_back(*slot.getPokemon());
            }
        }
    }
    return result;
}


// Default constructor: create 200 empty boxes
HomeStorage::HomeStorage()
    : m_boxes(BOX_COUNT)
{
}

// Accessors
HomeBox& HomeStorage::at(int box_index) {
    if (box_index < 0 || box_index >= BOX_COUNT) {
        throw std::out_of_range("HomeStorage::at: box_index out of range.");
    }
    return m_boxes[box_index];
}

const HomeBox& HomeStorage::at(int box_index) const {
    if (box_index < 0 || box_index >= BOX_COUNT) {
        throw std::out_of_range("HomeStorage::at: box_index out of range.");
    }
    return m_boxes[box_index];
}

// Swap Pokémon between two slots (possibly across boxes)
void HomeStorage::swapSlots(
    int box1, int row1, int col1,
    int box2, int row2, int col2
    ) {
    if (box1 < 0 || box1 >= BOX_COUNT || box2 < 0 || box2 >= BOX_COUNT) {
        throw std::out_of_range("HomeStorage::swapSlots: box index out of range.");
    }

    auto& slot1 = m_boxes[box1].at(row1, col1).getPokemon();
    auto& slot2 = m_boxes[box2].at(row2, col2).getPokemon();

    std::swap(slot1, slot2);
}

// Flatten all boxes into a single vector (skip empty slots)
std::vector<PokemonData> HomeStorage::flatten() const {
    std::vector<PokemonData> result;
    result.reserve(BOX_COUNT * HomeBox::MAX_ROWS * HomeBox::MAX_COLS);

    for (const auto& box : m_boxes) {
        auto box_flat = box.flatten();
        result.insert(result.end(), box_flat.begin(), box_flat.end());
    }

    return result;
}

}
}
}
