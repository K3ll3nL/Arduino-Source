#ifndef POKEMONHOME_POKEMONDATA_H
#define POKEMONHOME_POKEMONDATA_H

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

    PokemonInformation& Id(int id) { this->id = id; return *this; }
    PokemonInformation& Form(std::string form) { this->form = std::move(form); return *this; }
    PokemonInformation& Form_Id(int form_id) { this->form_id = form_id; return *this; }
    PokemonInformation& Gender(StatsHuntGenderFilter gender) { this->gender = gender; return *this; }
    PokemonInformation& Type1(PokemonType type1) { this->type1 = type1; return *this; }
    PokemonInformation& Type2(PokemonType type2) { this->type2 = type2; return *this; }
    PokemonInformation& Region(Region region) { this->region = region; return *this; }

    std::vector<PokemonInformation> match(const std::vector<PokemonInformation>& candidates) const;
};


class PokedexReader {
public:
    PokedexReader();

    std::vector<PokemonInformation> m_pokemon;
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
        PokemonType tera,
        bool prime_example
        );

    int get_id() const;
    int get_form_id() const;
    std::string get_form() const;
    StatsHuntGenderFilter get_gender() const;
    PokemonType get_type1() const;
    PokemonType get_type2() const;
    Region get_region() const;
    int get_ot_id() const;
    int get_level() const;
    bool is_shiny() const;
    bool is_gmax() const;
    PokemonType get_tera() const;
    bool is_prime_example() const;

    std::vector<PokemonInformation> match(const std::vector<PokemonInformation>& candidates) const;
    bool operator==(const PokemonData& other) const;
    bool operator!=(const PokemonData& other) const { return !(*this == other); }

    std::string to_string() const;

private:
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

    PokemonType tera; // TODO: Implement tera type
    // std::string pokeball; // TODO: Implement pokeball types

    bool prime_example;
};


}
}
}
#endif // POKEMONHOME_POKEMONDATA_H
