#include "PokemonHome_PokemonData.h"
#include "CommonFramework/Globals.h"
#include <fstream>
#include <filesystem>
#include <format>
#include <Qstring>
#include <iostream>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

StatsHuntGenderFilter string_to_gender(const std::string& gender_str){
    if (gender_str == "male") return StatsHuntGenderFilter::Male;
    if (gender_str == "female") return StatsHuntGenderFilter::Female;
    if (gender_str == "genderless") return StatsHuntGenderFilter::Genderless;
    return StatsHuntGenderFilter::Any; // fallback / default
}

inline PokemonType string_to_type(const std::string& s){
    if (s == "none")     return PokemonType::NONE;
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
}


static std::string region_to_string(Region r){
    switch(r){
    case Region::KANTO: return "Kanto";
    case Region::JOHTO: return "Johto";
    case Region::HOENN: return "Hoenn";
    case Region::SINNOH: return "Sinnoh";
    case Region::UNOVA: return "Unova";
    case Region::KALOS: return "Kalos";
    case Region::ALOLA: return "Alola";
    case Region::GALAR: return "Galar";
    case Region::UNKNOWN: return "Unknown";
    case Region::HISUI: return "Hisui";
    case Region::PALDEA: return "Paldea";
    }
    return "Unknown";
}

static std::string type_to_string(PokemonType t){
    switch(t){
    case PokemonType::NONE: return "None";
    case PokemonType::NORMAL: return "Normal";
    case PokemonType::FIRE: return "Fire";
    case PokemonType::FIGHTING: return "Fighting";
    case PokemonType::WATER: return "Water";
    case PokemonType::FLYING: return "Flying";
    case PokemonType::GRASS: return "Grass";
    case PokemonType::POISON: return "Poison";
    case PokemonType::ELECTRIC: return "Electric";
    case PokemonType::GROUND: return "Ground";
    case PokemonType::PSYCHIC: return "Psychic";
    case PokemonType::ROCK: return "Rock";
    case PokemonType::ICE: return "Ice";
    case PokemonType::BUG: return "Bug";
    case PokemonType::DRAGON: return "Dragon";
    case PokemonType::GHOST: return "Ghost";
    case PokemonType::DARK: return "Dark";
    case PokemonType::STEEL: return "Steel";
    case PokemonType::FAIRY: return "Fairy";
    }
    return "Unknown";
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

// Compare ability arrays, treating "any" as a wildcard
bool PokemonInformation::ability_match(const std::vector<std::string>& other) const {
    if (ability.empty() || other.empty()) return true;

    // If either list contains "any", always a match
    if (std::find(ability.begin(), ability.end(), "any") != ability.end()) return true;
    if (std::find(other.begin(), other.end(), "any") != other.end()) return true;

    // Otherwise, require at least one overlap
    for (const auto& a : ability) {
        if (std::find(other.begin(), other.end(), a) != other.end()) {
            return true;
        }
    }
    return false;
}

std::vector<PokemonInformation> PokemonInformation::match(const std::vector<std::vector<PokemonInformation>>& candidates) const {
    std::vector<PokemonInformation> results;

    if (!id.has_value() || id.value() < 0 || id.value() >= (int)candidates.size()) {
        return results;
    }

    const auto& forms = candidates[id.value()]; // get inner vector for this ID

    for (const auto& info : forms) {
        if (info.id.has_value() && id.has_value() && info.id.value() != id.value()) continue;
        if (info.form_id.has_value() && form_id.has_value() && info.form_id.value() != form_id.value()) continue;
        if (info.form.has_value() && form.has_value() && info.form.value() != form.value()) continue;
        if (info.gender.has_value() && gender.has_value() && info.gender.value() != gender.value()) continue;
        if (info.type1.has_value() && type1.has_value() && info.type1.value() != type1.value()) continue;
        if (info.type2.has_value() && type2.has_value() && info.type2.value() != type2.value()) continue;
        if (info.region.has_value() && region.has_value() && info.region.value() != region.value()) continue;
        if (!ability.empty() && !info.ability.empty() && !ability_match(info.ability)) continue;

        results.push_back(info);
    }

    return results;
}


PokedexReader::PokedexReader():m_loaded(false){}

std::vector<std::vector<PokemonInformation>>& PokedexReader::get_pokedex(){
    if(!m_loaded){
        load_pokedex();
        m_loaded = true;
    }
    return m_pokemon;
}

void PokedexReader::load_pokedex(){
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

            pokemon.Type1(string_to_type(typesArray[0].to_string_throw()));
            if (typesArray.size() > 1){
                pokemon.Type2(string_to_type(typesArray[1].to_string_throw()));
            } else {
                pokemon.Type2(PokemonType::NONE);
            }

            // Abilities
            const JsonArray& abilitiesArray = obj->get_array_throw("ability");
            std::vector<std::string> abilities;
            for (size_t k = 0; k < abilitiesArray.size(); k++){
                abilities.push_back(abilitiesArray[k].to_string_throw());
            }
            pokemon.Ability(abilities);

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
                         bool shiny, bool gmax, std::string ability, PokemonType tera, bool prime_example)
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
    , ability(std::move(ability))
    , tera(tera)                 // enum: copy
    , prime_example(prime_example)
{
    placeholder = false;
}

PokemonData::PokemonData(int id, int form_id, std::string form,
                         StatsHuntGenderFilter gender,
                         PokemonType type1, PokemonType type2,
                         Region region, int ot_id, int level,
                         bool shiny, bool gmax, std::string ability, PokemonType tera, bool prime_example, bool placeholder)
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
    , ability(std::move(ability))
    , tera(tera)                 // enum: copy
    , prime_example(prime_example) // bool: copy
    , placeholder(placeholder)
{}

PokemonData::PokemonData(const JsonObject& obj) {
    id            = obj.get_integer_throw("id");
    form_id       = obj.get_integer_throw("form_id");
    form          = obj.get_string_throw("form");
    gender        = string_to_gender(obj.get_string_throw("gender"));
    type1         = string_to_type(obj.get_string_throw("type1"));
    type2         = string_to_type(obj.get_string_throw("type2"));
    region        = string_to_region(obj.get_string_throw("region"));
    ot_id         = obj.get_integer_throw("ot_id");
    level         = obj.get_integer_throw("level");
    shiny         = obj.get_boolean_throw("shiny");
    gmax          = obj.get_boolean_throw("gmax");
    ability       = obj.get_string_throw("ability");
    tera          = string_to_type(obj.get_string_throw("tera"));
    prime_example = obj.get_boolean_throw("prime_example");
    placeholder   = obj.get_boolean_throw("placeholder");
}




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
        if (!info.ability.empty() && std::find(info.ability.begin(), info.ability.end(), "any") == info.ability.end() && std::find(info.ability.begin(), info.ability.end(), ability) == info.ability.end()) continue;
        // Can add more fields if needed

        results.push_back(info);
    }

    return results;
}


bool PokemonData::operator==(const PokemonData& other) const {
    return id == other.id &&
           form_id == other.form_id &&
           form == other.form &&
           gender == other.gender &&
           type1 == other.type1 &&
           type2 == other.type2 &&
           region == other.region &&
           ot_id == other.ot_id &&
           level == other.level &&
           shiny == other.shiny &&
           gmax == other.gmax &&
           ability == other.ability &&
           tera == other.tera &&
           prime_example == other.prime_example;
}


bool PokemonData::operator<(const PokemonData& other){
    if (prime_example != other.prime_example) {
        return prime_example;
    }else{
        if(prime_example){
            if (id != other.id) {
                return id < other.id;
            }
            if (form_id != other.form_id){
                return form_id<other.form_id;
            }
        }else{
            if (shiny != other.shiny) {
                return shiny;
            }
            if (id != other.id) {
                return id < other.id;
            }
            if (form_id != other.form_id){
                return form_id<other.form_id;
            }
            if (level != other.level) {
                return level > other.level;
            }
        }
    }

    return false;
}

bool PokemonData::operator<(const PokemonData& other) const{
    if (prime_example != other.prime_example) {
        return prime_example;
    }else{
        if(prime_example){
            if (id != other.id) {
                return id < other.id;
            }
            if (form_id != other.form_id){
                return form_id<other.form_id;
            }
        }else{
            if (shiny != other.shiny) {
                return shiny;
            }
            if (id != other.id) {
                return id < other.id;
            }
            if (form_id != other.form_id){
                return form_id<other.form_id;
            }
            if (level != other.level) {
                return level > other.level;
            }
        }
    }

    return false;
}

std::string PokemonData::to_string() const {
    QString s;
    s += QString("PokemonData { id: %1, form_id: %2, form: %3, gender: %4, ")
             .arg(id)
             .arg(form_id)
             .arg(QString::fromStdString(form),
                  QString::fromStdString(gender_to_string(gender)));

    s += QString("type1: %1, type2: %2, region: %3, ability: %4, ")
             .arg(QString::fromStdString(type_to_string(type1)),
                  QString::fromStdString(type_to_string(type2)),
                  QString::fromStdString(region_to_string(region)),
                  QString::fromStdString(ability));

    s += QString("ot_id: %1, level: %2, shiny: %3, gmax: %4, tera: %5, prime_example: %6 }")
             .arg(ot_id)
             .arg(level)
             .arg(shiny ? "true" : "false",
                  gmax ? "true" : "false",
                  QString::fromStdString(type_to_string(tera)),
                  prime_example ? "true" : "false");

    return s.toStdString();
}

JsonValue PokemonData::to_json(){
    JsonObject pokemon;
    pokemon["id"] = id;
    pokemon["form_id"] = form_id;
    pokemon["form"] = form;
    pokemon["gender"] = gender_to_string(gender);
    pokemon["type1"] = type_to_string(type1);
    pokemon["type2"] = type_to_string(type2);
    pokemon["region"] = region_to_string(region);
    pokemon["ot_id"] = ot_id;
    pokemon["level"] = level;
    pokemon["shiny"] = shiny;
    pokemon["gmax"] = gmax;
    pokemon["ability"] = ability;
    pokemon["tera"] = type_to_string(tera);
    pokemon["prime_example"] = prime_example;
    pokemon["placeholder"] = placeholder;

    return pokemon;
}

HomeSlot::HomeSlot()
    : m_quick_color(), m_row(0), m_col(0), m_pokemon(std::nullopt) {}


HomeSlot::HomeSlot(int row, int col, FloatPixel quick_color)
    : m_quick_color(quick_color), m_row(row), m_col(col), m_pokemon(std::nullopt)
{}

HomeSlot::HomeSlot(int row, int col, FloatPixel quick_color, std::optional<PokemonData> pokemon)
    : m_quick_color(quick_color), m_row(row), m_col(col), m_pokemon(pokemon)
{}

HomeSlot::HomeSlot(int row, int col, const PokemonData& pokemon)
    : m_quick_color(), m_row(row), m_col(col), m_pokemon(pokemon)
{}

HomeSlot::HomeSlot(const JsonObject& obj) {
    m_row = obj.get_integer_throw("row");
    m_col = obj.get_integer_throw("column");

    auto temp = obj.get_object("quick_color");
    if(!temp){
        m_quick_color = FloatPixel();
    }else{
        m_quick_color = FloatPixel(temp->get_double_throw("r"), temp->get_double_throw("g"), temp->get_double_throw("b")); // assuming FloatPixel has from_json
    }

    if (!obj.get_object("Pokemon")) {
        m_pokemon = std::nullopt;
    } else {
        m_pokemon = PokemonData(*obj.get_object("Pokemon"));
    }
}


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
    loaded = false;
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
    loaded = true;
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

HomeBox::HomeBox(const JsonValue& json)
    : m_slots(MAX_ROWS, std::vector<HomeSlot>(MAX_COLS))
{
    const JsonArray* slots_arr = json.to_array();
    if (!slots_arr) {
        throw std::runtime_error("HomeBox JSON is not an array");
    }

    for (const JsonValue& entry : *slots_arr) {
        const JsonObject* obj = entry.to_object();
        if (!obj) {
            throw std::runtime_error("Invalid entry in array: Expected a JSON object.");
        }

        int row = obj->get_integer_throw("row");
        int col = obj->get_integer_throw("column");

        // Let HomeSlot parse itself
        m_slots[row][col] = HomeSlot(*obj);
    }

    loaded = true;
}





HomeBox& HomeBox::operator=(const HomeBox& other) {
    if (this != &other) {
        m_slots = other.m_slots;
    }
    return *this;
}


HomeBox& HomeBox::operator=(HomeBox&& other) noexcept {
    if (this != &other) {
        m_slots = std::move(other.m_slots);
        loaded = other.loaded;
    }
    return *this;
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

std::optional<std::pair<int, int>> HomeBox::find_pokemon(const PokemonData& target) const {
    for (int row = 0; row < MAX_ROWS; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            const HomeSlot& slot = m_slots[row][col];

            if (!slot.isEmpty()) {
                const PokemonData& data = slot.getPokemon().value();
                if (data == target) {
                    return std::make_pair(row, col);
                }
            }
        }
    }
    return std::nullopt;
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
    std::swap(slot1.m_quick_color, slot2.m_quick_color);
}

// Flatten into vector (skip blanks)
std::vector<PokemonData*> HomeBox::flatten() {
    std::vector<PokemonData*> result;
    result.reserve(MAX_ROWS * MAX_COLS);

    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            auto& slot = m_slots[r][c];
            if (slot.getPokemon().has_value()) {
                result.push_back(&(*slot.getPokemon()));
            }
        }
    }
    return result;
}

JsonArray HomeBox::to_json() {
    JsonArray pokemon_data;

    for (int poke_nb = 0; poke_nb < 30; poke_nb++) {
        JsonObject pokemon;
        int row = poke_nb / 6;
        int col = poke_nb % 6;

        pokemon["row"] = row;
        pokemon["column"] = col;

        HomeSlot slot = at(row,col);

        if (!slot.isEmpty()) {
            try {
                pokemon["Pokemon"] = slot.getPokemon()->to_json();
                JsonObject quick_color;
                quick_color["r"] = slot.quick_color().r;
                quick_color["g"] = slot.quick_color().g;
                quick_color["b"] = slot.quick_color().b;
                pokemon["quick_color"] = JsonValue(std::move(quick_color));
            } catch (const std::exception& e) {
                std::cerr << "Error converting Pokemon to JSON at row " << row
                          << ", column " << col << ": " << e.what() << std::endl;
                pokemon["Pokemon"] = "error";
            }
        } else {
            pokemon["Pokemon"] = "blank";
        }

        try {
            pokemon_data.push_back(std::move(pokemon));
        } catch (const std::exception& e) {
            std::cerr << "Error adding Pokemon to JSON array: " << e.what() << std::endl;
        }
    }

    return pokemon_data;
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


std::optional<std::tuple<int, int, int>> HomeStorage::find_pokemon(const PokemonData& target) const {
    for (int box_index = 0; box_index < 200; box_index++) {
        const HomeBox& box = m_boxes[box_index];
        auto pos = box.find_pokemon(target);
        if (pos.has_value()) {
            auto [row, col] = *pos;
            return std::make_tuple(row, col, box_index);
        }
    }
    return std::nullopt;
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

void HomeStorage::store(int box){
    if (box < 0 || box >= BOX_COUNT) {
        throw std::out_of_range("HomeStorage::store: box index out of range.");
    }

    // Get the JSON representation from the HomeBox
    JsonArray j = m_boxes[box].to_json();

    std::filesystem::path folder = "Home Storage";
    std::filesystem::create_directories(folder); // ensure folder exists
    std::filesystem::path file_path = folder / (std::to_string(box) + ".json");

    // Write JSON to file (pretty-print with 4-space indent)
    std::ofstream out(file_path);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + file_path.string());
    }

    out << j.dump(4);  // pretty-printed JSON
    out.close();
}


bool HomeStorage::load(int box){
    std::filesystem::path path = std::filesystem::path("Home Storage") / (std::to_string(box) + ".json");
    if (!std::filesystem::exists(path)){
        return false; // file doesn't exist
    }

    std::ifstream file(path);
    if (!file.is_open()){
        return false; // failed to open
    }

    // Read entire file into a string
    std::string file_contents((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    try{
        JsonValue json = parse_json(file_contents);
        m_boxes.at(box) = HomeBox(json);
    }catch(const std::exception& e){
        std::cerr << "Failed to load box " << box << ": " << e.what() << std::endl;
        return false;
    }

    return true;
}




// Flatten all boxes into a single vector (skip empty slots)
std::vector<PokemonData*> HomeStorage::flatten() {
    std::vector<PokemonData*> result;
    result.reserve(BOX_COUNT * HomeBox::MAX_ROWS * HomeBox::MAX_COLS);

    for (auto& box : m_boxes) {
        auto box_flat = box.flatten();
        result.insert(result.end(), box_flat.begin(), box_flat.end());
    }

    return result;
}

}
}
}
