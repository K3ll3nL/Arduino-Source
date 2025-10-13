#ifndef POKEMONHOME_POKEMONDATA_H
#define POKEMONHOME_POKEMONDATA_H

#include "CommonFramework/ImageTools/FloatPixel.h"
#include "Pokemon/Options/Pokemon_StatsHuntFilter.h"
#include "Pokemon/Pokemon_Types.h"
#include <optional>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

using namespace Pokemon;


enum class Region{
    KANTO,
    JOHTO,
    HOENN,
    SINNOH,
    UNOVA,
    KALOS,
    ALOLA,
    GALAR,
    UNKNOWN,
    HISUI,
    PALDEA
};


inline Region string_to_region(const std::string& region_str){
    if (region_str == "kanto")  return Region::KANTO;
    if (region_str == "johto")  return Region::JOHTO;
    if (region_str == "hoenn")  return Region::HOENN;
    if (region_str == "sinnoh") return Region::SINNOH;
    if (region_str == "unova")  return Region::UNOVA;
    if (region_str == "kalos")  return Region::KALOS;
    if (region_str == "alola")  return Region::ALOLA;
    if (region_str == "galar")  return Region::GALAR;
    if (region_str == "hisui")  return Region::HISUI;
    if (region_str == "paldea") return Region::PALDEA;
    return Region::UNKNOWN; // fallback
}


class PokemonInformation {
public:
    std::optional<int> id;
    std::optional<std::string> form;
    std::optional<int> form_id;
    std::optional<int> evolution_chain;
    std::optional<StatsHuntGenderFilter> gender;
    std::optional<PokemonType> type1;
    std::optional<PokemonType> type2;
    std::optional<Region> region;
    std::vector<std::string> ability;


    PokemonInformation& Id(int id) { this->id = id; return *this; }
    PokemonInformation& Form(std::string form) { this->form = std::move(form); return *this; }
    PokemonInformation& Form_Id(int form_id) { this->form_id = form_id; return *this; }
    PokemonInformation& Gender(StatsHuntGenderFilter gender) { this->gender = gender; return *this; }
    PokemonInformation& Type1(PokemonType type1) { this->type1 = type1; return *this; }
    PokemonInformation& Type2(PokemonType type2) { this->type2 = type2; return *this; }
    PokemonInformation& Region(Region region) { this->region = region; return *this; }
    PokemonInformation& Ability(std::vector<std::string> ability) { this->ability = std::move(ability); return *this; }

    std::vector<PokemonInformation> match(const std::vector<std::vector<PokemonInformation>>& candidates) const;

private:
    bool ability_match(const std::vector<std::string>& other) const;
};


class PokedexReader {
public:
    PokedexReader();

    std::vector<std::vector<PokemonInformation>>& get_pokedex();

private:

    void load_pokedex();

    std::vector<std::vector<PokemonInformation>> m_pokemon;  // TODO: Turn this into a vec<vec<PkmnInfo>>
    bool m_loaded;
};


class PokemonData
{
public:
    PokemonData();
    PokemonData(
        int id,
        int form_id,
        std::string form,
        StatsHuntGenderFilter gender,
        PokemonType type1,
        PokemonType type2,
        Region region,
        int ot_id,
        int level,
        bool shiny,
        bool gmax,
        std::string ability,
        PokemonType tera,
        bool prime_example
        );
    PokemonData(
        int id,
        int form_id,
        std::string form,
        StatsHuntGenderFilter gender,
        PokemonType type1,
        PokemonType type2,
        Region region,
        int ot_id,
        int level,
        bool shiny,
        bool gmax,
        std::string ability,
        PokemonType tera,
        bool prime_example,
        bool placeholder
        );

    std::vector<PokemonInformation> match(const std::vector<std::vector<PokemonInformation>>& candidates) const;
    bool operator==(const PokemonData& other) const;
    bool operator!=(const PokemonData& other) const { return !(*this == other); }
    bool operator<(const PokemonData& other);
    bool operator<(const PokemonData& other) const;

    std::string to_string() const;

    int id;
    int form_id;
    std::string form;
    StatsHuntGenderFilter gender;
    PokemonType type1;
    PokemonType type2;
    Region region;
    int ot_id;
    int level;
    bool shiny;
    bool gmax;
    std::string ability;

    PokemonType tera; // TODO: Implement tera type
    // std::string pokeball; // TODO: Implement pokeball types

    bool prime_example;
    bool placeholder;
};


class HomeSlot {
public:
    // Constructors
    HomeSlot();
    HomeSlot(int row, int col, FloatPixel quick_color);
    HomeSlot(int row, int col, FloatPixel quick_color, std::optional<PokemonData> pokemon);
    HomeSlot(int row, int col, const PokemonData& pokemon);

    // Accessors
    int row() const { return m_row; }
    int col() const { return m_col; }
    FloatPixel quick_color() const { return m_quick_color; }
    const std::optional<PokemonData>& getPokemon() const { return m_pokemon; }
    std::optional<PokemonData>& getPokemon() { return m_pokemon; }

    bool isOccupied() const { return m_pokemon.has_value() && !m_pokemon->placeholder; }
    bool isEmpty() const { return !m_pokemon.has_value();}
    /* isOccupied speaks to the state of the slot in Pokemon HOME. isEmpty speaks
     * to the state of the optional. If you want to safely check the pokemon object,
     * use isEmpty. If you want to konw that you can select the slot in home, use
     * isOccupied.
     */


    // Mutators
    void setPokemon(const PokemonData& pokemon);
    void clear();

    FloatPixel m_quick_color;
    int m_row;
    int m_col;


private:
    std::optional<PokemonData> m_pokemon;
};


class HomeBox {
public:
    static constexpr int MAX_ROWS = 5;
    static constexpr int MAX_COLS = 6;



    // Default constructor: initializes all blank slots
    HomeBox();

    // Constructor: fills with provided Pokémon up to 30 (row-major order)
    explicit HomeBox(const std::vector<PokemonData>& pokemon_list);

    // Copy constructor
    HomeBox(const HomeBox& other);
    HomeBox& operator=(const HomeBox&);
    HomeBox& operator=(HomeBox&&) noexcept;
    // Accessors
    HomeSlot& at(int row, int col);
    const HomeSlot& at(int row, int col) const;
    std::optional<std::pair<int, int>> find_pokemon(const PokemonData& target) const;

    // Swap Pokémon between two slots in this box
    void swap(int row1, int col1, int row2, int col2);

    // Flatten into a vector of existing Pokémon (ignores empty slots)
    std::vector<PokemonData> flatten() const;

    bool loaded;

private:
    std::vector<std::vector<HomeSlot>> m_slots;
};


class HomeStorage {
public:
    static constexpr int BOX_COUNT = 200;


    // Default constructor: initializes 200 empty boxes
    HomeStorage();

    // Accessors
    HomeBox& at(int box_index);
    const HomeBox& at(int box_index) const;
    std::optional<std::tuple<int, int, int>> find_pokemon(const PokemonData& target) const;
    // Swap Pokémon between any two slots, possibly across boxes
    void extracted(int &box1, int &box2);
    void swapSlots(int box1, int row1, int col1, int box2, int row2, int col2);

    // Flatten all boxes into a vector of existing Pokémon (ignores empty slots)
    std::vector<PokemonData> flatten() const;

private:
    std::vector<HomeBox> m_boxes;
};

}
}
}
#endif // POKEMONHOME_POKEMONDATA_H
