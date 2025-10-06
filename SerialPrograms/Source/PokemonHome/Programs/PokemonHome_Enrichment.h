#ifndef ENRICHMENT_H
#define ENRICHMENT_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonHome/Inference/PokemonHome_BoxGenderDetector.h"
#include "PokemonHome/Programs/HomeEnvironment/PokemonHome_HomeEnvironment.h"

#include<unordered_set>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

class Pokemon {
public:
    PokemonType type1;
    PokemonType type2;
    float national_dex_number;
    bool shiny;
    bool gmax;
    StatsHuntGenderFilter gender;
    uint16_t level;
    int form_id;
    int ot_id;


    FloatPixel color;
    FloatPixel quick_color;

    int current_box; // Box number the Pokémon is currently in
    int current_row; // Row position in the current box
    int current_col; // Column position in the current box

    // Shared gender-specific IDs across all Pokemon
    static const std::unordered_set<float>& gender_specific_ids() {
        static const std::unordered_set<float> ids = {3.0f, 12.0f, 19.0f, 19.009995f, 20.0f, 25.0f, 26.0f, 41.0f, 42.0f, 44.0f, 45.0f, 64.0f, 65.0f, 84.0f, 85.0f, 97.0f, 111.0f, 112.0f, 118.0f, 119.0f, 123.0f, 129.0f, 130.0f, 133.0f, 154.0f, 165.0f, 166.0f, 178.0f, 185.0f, 186.0f, 190.0f, 194.0f, 195.0f, 198.0f, 202.0f, 203.0f, 207.0f, 208.0f, 212.0f, 214.0f, 215.0f, 215.009995f, 217.0f, 221.0f, 224.0f, 229.0f, 232.0f, 255.0f, 256.0f, 257.0f, 267.0f, 269.0f, 272.0f, 274.0f, 275.0f, 307.0f, 308.0f, 315.0f, 316.0f, 317.0f, 322.0f, 323.0f, 332.0f, 350.0f, 369.0f, 396.0f, 397.0f, 398.0f, 399.0f, 400.0f, 401.0f, 402.0f, 403.0f, 404.0f, 405.0f, 407.0f, 415.0f, 417.0f, 418.0f, 419.0f, 424.0f, 443.0f, 444.0f, 445.0f, 449.0f, 450.0f, 453.0f, 454.0f, 456.0f, 457.0f, 459.0f, 460.0f, 461.0f, 464.0f, 465.0f, 473.0f, 521.0f, 592.0f, 593.0f, 668.0f, 678.0f, 876.0f, 902.0f, 916.0f};
        return ids;
    }

    // Default constructor
    Pokemon();

    void update_national_id();


    // Log Pokemon details (to environment or as a string)
    std::string log_details(SingleSwitchProgramEnvironment* = nullptr, bool = false) const;
    JsonValue to_json();
    static Pokemon from_json(const JsonValue& value);

    // Operator< for sorting Pokémon
    bool operator<(const Pokemon& other) const;
    bool operator==(const Pokemon& other) const;
    bool operator<=(const Pokemon& other) const;
};

class Enrichment_Descriptor : public SingleSwitchProgramDescriptor{
public:
    Enrichment_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

struct Game{
    GameStatus game;
    int index;
    bool accessed;
    bool depleted;
};

enum class mode{
    Level,
    Simple_Item,
    Fun,
    None
};


inline std::string to_string(mode mode){
    switch(mode){
    case mode::Level: return "level";
    case mode::Simple_Item: return "simple item";
    case mode::None: return "none";
    default: return "invalid mode";
    }
};

class Enrichment : public SingleSwitchProgramInstance{
public:
    Enrichment();

    void home_put_away_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&, HomeEnvironment&, Game&, bool);
    void home_dispose_of_go(SingleSwitchProgramEnvironment&, ProControllerContext&, HomeEnvironment&);
    void wipe_markings(SingleSwitchProgramEnvironment&, ProControllerContext&, HomeEnvironment&);
    void initialize_home(SingleSwitchProgramEnvironment&, ProControllerContext&, HomeEnvironment&, std::vector<Game>&);
    void sort_all_boxes(SingleSwitchProgramEnvironment&, ProControllerContext&, HomeEnvironment&, bool&, bool&);
    void enrich_with_games(SingleSwitchProgramEnvironment&, ProControllerContext&, HomeEnvironment&, std::vector<Game>&);

    virtual void program(SingleSwitchProgramEnvironment&, ProControllerContext&) override;

private:
    SimpleIntegerOption<uint16_t> HOME_FIRST_BOX;
    SimpleIntegerOption<uint16_t> HOME_LAST_BOX;
    SimpleIntegerOption<uint16_t> PLA_FIRST_BOX;
    SimpleIntegerOption<uint16_t> PLA_LAST_BOX;
    StringOption SV_BOX_NAME;
    BooleanCheckBoxOption WIPE_MARKINGS;
    BooleanCheckBoxOption DISPOSE_GOS;
    BooleanCheckBoxOption STARTING_AT_DESK;
    BooleanCheckBoxOption EMERGENCY_DELOAD;
    BooleanCheckBoxOption NORMAL_DELOAD;
    BooleanCheckBoxOption SKIP_SETUP;
    BooleanCheckBoxOption SKIP_SORT;

    EventNotificationsOption NOTIFICATIONS;

    std::unordered_map<std::string, int> item_counts;
    mode enrichment_mode;
    Game current_game;
};
}
}
}

#endif // POKEMONHOME_ENRICHMENT_H
