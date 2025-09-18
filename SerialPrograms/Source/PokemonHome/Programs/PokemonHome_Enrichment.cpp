#include "PokemonHome_Enrichment.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonTools/OCR/OCR_RawOCR.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/Pokemon_Types.h"
#include "PokemonHome/Inference/PokemonHome_HomeApplicationDetector.h"
#include "PokemonHome/Inference/PokemonHome_SVItemReader.h"
#include "PokemonHome/Programs/HomeEnvironment/PokemonHome_HomeEnvironment.h"
#include "PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_DirectionDetector.h"
#include "PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "PokemonSV/Inference/Dialogs/PokemonSV_DialogArrowDetector.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV/Programs/Battles/PokemonSV_SinglesBattler.h"
#include "PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include <iostream>
#include <qdir.h>
#include <qobject.h>
#include <cmath>
#include <stdexcept>
#include <ctime>
#include <windows.h>
#include <string>
#include <sstream>


struct BattleFailedException : public std::exception {
    const char* what() const noexcept override {
        return "Calyrex Fainted.";
    }
};


namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;

const size_t MAX_COLUMNS = 6;
const size_t MAX_ROWS = 5;


Enrichment_Descriptor::Enrichment_Descriptor()
    : SingleSwitchProgramDescriptor(
          "PokemonHome:Enrichment",
          STRING_POKEMON + " Home", "Enrichment",
          "ComputerControl/blob/master/Wiki/Programs/PokemonHome/Enrichment.md",
          "Order boxes of " + STRING_POKEMON + ", run various XP grinding events across games, and deal with " + STRING_POKEMON + " from " + STRING_POKEMON + " Go.",
          FeedbackType::REQUIRED,
          AllowCommandsWhenRunning::DISABLE_COMMANDS,
          {SerialPABotBase::OLD_NINTENDO_SWITCH_DEFAULT_REQUIREMENTS}
          )
{}
struct Enrichment_Descriptor::Stats : public StatsTracker{
    Stats()

    {

    }

};
std::unique_ptr<StatsTracker> Enrichment_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

Enrichment::Enrichment()
    : HOME_FIRST_BOX(
          "<b>First dedicated box in Home:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          1, 1, 200
          )
    , HOME_LAST_BOX(
          "<b>Last dedicated box in Home:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          1, 1, 200
          )
    , PLA_FIRST_BOX(
          "<b>First Dedicated box in Legends Arceus:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          16, 1, 32
          )
    , PLA_LAST_BOX(
          "<b>Last Dedicated box in Legends Arceus:</b>",
          LockMode::LOCK_WHILE_RUNNING,
          28, 1, 32
          )
    , SV_BOX_NAME(
          false,
          "<b>S/V Box Name:</b><br>Name of the box in Scarlet/Violet to use for storage",
          LockMode::LOCK_WHILE_RUNNING,
          "To Home",
          "box_order"
          )
    , WIPE_MARKINGS(
          "<b>Wipe Markings:</b><br>Clear markings from home or pick up where the program last left off.",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , DISPOSE_GOS(
          "<b>Go Disposal:</b><br>Get rid of Pokémon with a Go origin mark (compatible with PLA). Will not include shinies",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , STARTING_AT_DESK(
          "<b>Starting at Desk:</b><br>Check this if you are absolutely sure the save is starting inside the<br>Uva Academy at the Academy Ace Tournament desk.",
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , EMERGENCY_DELOAD(
          "<b>DEBUG Emergency Deload:</b><br>Dump Pokemon into Home",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , NORMAL_DELOAD(
          "<b>DEBUG Normal Deload:</b><br>Avoid Second marking on home movements",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , SKIP_SETUP(
          "<b>DEBUG Skip Setup:</b><br>Skip loading pokemon into home boxes",
          LockMode::UNLOCK_WHILE_RUNNING,
          false
          )
    , NOTIFICATIONS({
          &NOTIFICATION_PROGRAM_FINISH
      })
{
    PA_ADD_OPTION(HOME_FIRST_BOX);            //number of first box to check and sort in HOME
    PA_ADD_OPTION(HOME_LAST_BOX);             //number of last box to check and sort in HOME
    PA_ADD_OPTION(PLA_FIRST_BOX);            //number of first box to check and sort in PLA
    PA_ADD_OPTION(PLA_LAST_BOX);             //number of last box to check and sort in PLA
    PA_ADD_OPTION(SV_BOX_NAME);         //box to use for s/v
    PA_ADD_OPTION(WIPE_MARKINGS);        //should wipe markings on home
    PA_ADD_OPTION(DISPOSE_GOS);         //Dispose of Go marked Pokémon
    PA_ADD_OPTION(STARTING_AT_DESK);     //pokemon sv start at desk
    PA_ADD_OPTION(EMERGENCY_DELOAD);     //pokemon sv start at desk
    PA_ADD_OPTION(NORMAL_DELOAD);     //pokemon sv start at desk
    PA_ADD_OPTION(SKIP_SETUP);          //pokemon sv start at desk
    PA_ADD_OPTION(NOTIFICATIONS);
    QDir().mkpath("Home Storage");

}

#include <string>
#include <sstream>
#include <stdexcept>
#include <memory>


std::string type_to_string(PokemonType type){
    const char * types[] = {
        "None",
        "Normal",
        "Fire",
        "Fighting",
        "Water",
        "Flying",
        "Grass",
        "Poison",
        "Electric",
        "Ground",
        "Psychic",
        "Rock",
        "Ice",
        "Bug",
        "Dragon",
        "Ghost",
        "Dark",
        "Steel",
        "Fairy",
    };
    return types[int(type)];
}



class PokemonMinimal {
public:
    float national_dex_number;
    std::string type1;
    std::string type2;

    PokemonMinimal()
        : national_dex_number(0), type1("NONE"), type2("NONE") {}
};




class PokemonBox {
public:

    static constexpr size_t MAX_ROWS = 5;
    static constexpr size_t MAX_COLUMNS = 6;

    // Constructor to initialize a 6x5 grid.
    PokemonBox()
        : pokemon_count(0),
        blanks(MAX_ROWS * MAX_COLUMNS),
        consecutive_blanks(MAX_ROWS * MAX_COLUMNS),
        box_num(0),
        grid(MAX_ROWS, std::vector<std::optional<Pokemon>>(MAX_COLUMNS)) {}

    // Function to add a Pokemon at a specific row and column.
    void add_pokemon(const std::optional<Pokemon> pokemon, size_t row, size_t col) {
        if (row >= MAX_ROWS || col >= MAX_COLUMNS) {
            throw std::out_of_range("Row or column out of bounds.");
        }
        if (grid[row][col].has_value()) {
            throw std::runtime_error("Position already occupied.");
        }
        grid[row][col] = pokemon;
        grid[row][col]->current_row = row;
        grid[row][col]->current_col = col;

        update_stats();
    }

    bool is_empty() const {
        return grid.empty();
    }

    std::optional<Pokemon>& get_pokemon(const size_t row, const size_t col){
        return grid[row][col];
    }

    // Overloaded function to add a Pokemon based on its attributes.
    void populate_pokemon(const Pokemon& pokemon) {
        size_t row = pokemon.current_row;
        size_t col = pokemon.current_col;

        if (row >= MAX_ROWS || col >= MAX_COLUMNS) {
            throw std::out_of_range("Pokemon's current position is invalid.");
        }
        if (grid[row][col].has_value()) {
            throw std::runtime_error("Position already occupied.");
        }
        grid[row][col] = pokemon;

        update_stats();
    }

    // Function to swap two Pokémon in the grid based on row and column positions.
    void swap_pokemon(size_t row1, size_t col1, size_t row2, size_t col2) {
        if (row1 >= MAX_ROWS || col1 >= MAX_COLUMNS || row2 >= MAX_ROWS || col2 >= MAX_COLUMNS) {
            throw std::out_of_range("Row or column out of bounds.");
        }
        std::swap(grid[row1][col1], grid[row2][col2]);

        // Update current positions.
        if (grid[row1][col1].has_value()) {
            grid[row1][col1]->current_row = row1;
            grid[row1][col1]->current_col = col1;
        }
        if (grid[row2][col2].has_value()) {
            grid[row2][col2]->current_row = row2;
            grid[row2][col2]->current_col = col2;
        }

        update_stats();
    }
    
    void update_stats() {
        pokemon_count = 0;
        blanks = 0;
        consecutive_blanks = 0;
        bool found_first = false;

        for (size_t row = 0; row < MAX_ROWS; ++row) {
            for (size_t col = 0; col < MAX_COLUMNS; ++col) {
                if (!grid[row][col].has_value()) {
                    ++blanks;
                } else {
                    ++pokemon_count;
                    if (!found_first) {
                        first_poke_slot = {row, col};
                        found_first = true;
                    }
                }
            }
        }

        // Calculate consecutive blanks from the end.
        for (int row = MAX_ROWS - 1; row >= 0; --row) {
            for (int col = MAX_COLUMNS - 1; col >= 0; --col) {
                if (!grid[row][col].has_value()) {
                    ++consecutive_blanks;
                } else {
                    return;
                }
            }
        }
    }


    bool is_sorted() const {
        // Placeholder to compare the last encountered Pokémon.
        std::optional<Pokemon> last_pokemon;

        // Iterate through the rows.
        for (const auto& row : grid) {
            // Iterate through the columns in the row.
            for (const auto& pokemon_opt : row) {
                if (pokemon_opt.has_value()) {
                    // If there is a previous Pokémon, compare their sorting property.
                    if (last_pokemon.has_value() &&
                        last_pokemon->national_dex_number > pokemon_opt->national_dex_number) {
                        return false; // Not sorted.
                    }
                    // Update the last encountered Pokémon.
                    last_pokemon = pokemon_opt;
                }
            }
        }
        return true; // The grid is sorted.
    }


    // Print the current state of the box for debugging purposes.
    std::string print_box(SingleSwitchProgramEnvironment* env = nullptr, bool log_to_env = false) const {
        std::ostringstream ss;

        for (size_t row = 0; row < MAX_ROWS; ++row) {
            for (size_t col = 0; col < MAX_COLUMNS; ++col) {
                if (grid[row][col].has_value()) {
                    ss << "Pokémon at {" << row << ", " << col << "}\n"
                       << grid[row][col]->log_details() << "\n";
                } else {
                    ss << "{" << row << ", " << col << "} Empty\n\n";
                }
            }
            ss << '\n';
        }

        if (log_to_env && env) {
            env->console.log(ss.str());
        }
        return ss.str();
    }

    void dump_each_pokemon_attribute(const JsonObject& pokemon_json) {
        try {
            // Iterate through each key-value pair in the "Pokemon" JsonObject
            for (const auto& [key, value] : pokemon_json) {
                try {
                    value.dump("Home Storage\\pokemon_attribute.json");
                } catch (const std::exception& e) {
                    std::cerr << "Error dumping value for key '" << key << "': " << e.what() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error iterating through 'Pokemon' attributes: " << e.what() << std::endl;
            throw;
        }
    }


    void dump_each_attribute(const JsonObject& json_object) {
        for (const auto& [key, value] : json_object) {
            try {
                value.dump("Home Storage\\attribute.json");
            } catch (const std::exception& e) {
                std::cerr << "Error dumping attribute " << key << ": " << e.what() << std::endl;
                if(key=="Pokemon"){
                    dump_each_pokemon_attribute(value.to_object_throw());
                }
                throw;
            }
        }
    }


    void dump_each_pokemon(){
        for (size_t poke_nb = 0; poke_nb < 30; poke_nb++) {
            JsonObject pokemon;
            size_t row = poke_nb / 6;
            size_t col = poke_nb % 6;

            pokemon["row"] = row;
            pokemon["column"] = col;

            if (grid[row][col].has_value()) {
                try {
                    pokemon["Pokemon"] = grid[row][col]->to_json();
                } catch (const std::exception& e) {
                    std::cerr << "Error converting Pokemon to JSON at row " << row
                              << ", column " << col << ": " << e.what() << std::endl;
                    pokemon["Pokemon"] = "error";
                }
            } else {
                pokemon["Pokemon"] = "blank";
            }

            try {
                pokemon.dump("Home Storage\\pokemon.json");
            } catch (const std::exception& e) {
                std::cerr << "Error dumping Pokemon at {" << row << ", " << col << "}: " << e.what() << std::endl;
                dump_each_attribute(pokemon);
            }
        }
    }


    void output_boxes_data_json() {
        JsonArray pokemon_data;

        for (size_t poke_nb = 0; poke_nb < 30; poke_nb++) {
            JsonObject pokemon;
            size_t row = poke_nb / 6;
            size_t col = poke_nb % 6;

            pokemon["row"] = row;
            pokemon["column"] = col;

            if (grid[row][col].has_value()) {
                try {
                    pokemon["Pokemon"] = grid[row][col]->to_json();
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
                dump_each_pokemon();
            }
        }

        try {
            pokemon_data.dump("Home Storage\\" + std::to_string(box_num) + ".json");
            QFile file(QString::fromStdString("Home Storage\\" + std::to_string(box_num) +".json"));
            file.close();
            // std::cout << "successfully dumped box " << std::to_string(box_num) << std::endl;
            pokemon_data.dump("Home Storage\\temp.json");
            QFile file2(QString::fromStdString("Home Storage\\temp.json"));
            file2.close();

        } catch (const std::exception& e) {
            std::cerr << "Error dumping JSON array to file: " << e.what() << std::endl;

            dump_each_pokemon();
        }
    }


    void parse_pokemon_box(const JsonValue& json) {
        const JsonArray* pokemon_array = json.to_array();
        if (!pokemon_array) {
            throw std::runtime_error("Provided JSON is not an array.");
        }

        for (const JsonValue& entry : *pokemon_array) {
            const JsonObject* obj = entry.to_object();
            if (!obj) {
                throw std::runtime_error("Invalid entry in array: Expected a JSON object.");
            }

            // Retrieve row and column
            int row = obj->get_integer_throw("row");
            int column = obj->get_integer_throw("column");

            // Retrieve the Pokemon object
            const JsonValue* pokemon_obj = obj->get_value("Pokemon");
            if (!pokemon_obj) {
                throw std::runtime_error("Missing 'Pokemon' object in entry.");
            }

            // Parse the Pokemon
            std::optional<Pokemon> pokemon(Pokemon::from_json(*pokemon_obj));


            // Add to the box
            this->add_pokemon(pokemon, row, column);
            // std::cout << "Read pokemon at {" << std::to_string(row) << ", " << std::to_string(column) << "}." << grid[row][column]->log_details() << std::endl;
        }

    }



    size_t pokemon_count;
    size_t blanks;
    size_t consecutive_blanks;
    size_t box_num;
    std::vector<std::vector<std::optional<Pokemon>>> grid;
    std::pair<size_t, size_t> first_poke_slot;
};

class BoxLayout {


public:
    BoxLayout() : layout(200), sorted(200, false), exists(200, false) {
    }


    void add_box(size_t index, PokemonBox box) {
        if (index >= layout.size()) {
            throw std::out_of_range("Index out of bounds.");
        }
        layout[index] = std::move(box); // Transfer ownership
        exists[index] = true;
    }

    void update_box(size_t index, PokemonBox new_box) {
        if (index >= layout.size()) {
            throw std::out_of_range("Index out of bounds.");
        }
        layout[index] = std::move(new_box);
        exists[index] = true;
    }


    PokemonBox& get_box(size_t index) {
        if (index >= layout.size()) {
            throw std::out_of_range("Index out of bounds.");
        }
        return layout[index];
    }

    void set_sorted(size_t index, bool is_sorted) {
        if (index >= sorted.size()) {
            throw std::out_of_range("Index out of bounds.");
        }
        sorted[index] = is_sorted;
    }

    bool is_sorted(size_t index) const {
        if (index >= sorted.size()) {
            throw std::out_of_range("Index out of bounds.");
        }
        return sorted[index];
    }

    bool box_exists(size_t index) const {
        if (index >= sorted.size()) {
            throw std::out_of_range("Index out of bounds.");
        }
        return exists[index];
    }

    bool important_sorted(size_t start, size_t end) const {
        if (start > end || end >= sorted.size()) {
            throw std::out_of_range("Invalid range.");
        }
        for (size_t i = start; i <= end; ++i) {
            if (layout[i].is_sorted()) {
                return false;
            }
        }
        return true;
    }

private:

    std::vector<PokemonBox> layout;
    std::vector<bool> sorted;
    std::vector<bool> exists;
};

Pokemon::Pokemon()
    : type1(PokemonType::NONE), type2(PokemonType::NONE), national_dex_number(0), shiny(false), gmax(false), gender(StatsHuntGenderFilter::Genderless),
    level(0), form_id(0), ot_id(0), color(), current_box(0), current_row(0), current_col(0) {}

bool Pokemon::operator<(const Pokemon& other) const {
    if (shiny != other.shiny) {
        return shiny;
    }
    if (national_dex_number != other.national_dex_number) {
        return national_dex_number < other.national_dex_number;
    }
    if (gender_specific_ids().count(national_dex_number) > 0) {
        if (gender != other.gender) {
            return this->gender== StatsHuntGenderFilter::Female;
        }
    }
    if (level != other.level) {
        return level > other.level;
    }
    return false;
}

bool Pokemon::operator==(const Pokemon& other) const {
    if (gender_specific_ids().count(national_dex_number) > 0) {
        return shiny == other.shiny &&
               national_dex_number == other.national_dex_number &&
               level == other.level &&
               gender == other.gender;
    }else{
        return shiny == other.shiny &&
               national_dex_number == other.national_dex_number &&
               level == other.level;
    }
}

bool Pokemon::operator<=(const Pokemon& other) const {
    return *this < other || *this == other;
}

void Pokemon::update_national_id(){
    static std::unordered_map<size_t, std::vector<std::pair<PokemonType, PokemonType>>> regional_codes = {
        {19, {{PokemonType::NORMAL, PokemonType::NONE}, {PokemonType::DARK, PokemonType::NORMAL}}},
        {20, {{PokemonType::NORMAL, PokemonType::NONE}, {PokemonType::DARK, PokemonType::NORMAL}}},
        {26, {{PokemonType::ELECTRIC, PokemonType::NONE}, {PokemonType::ELECTRIC, PokemonType::PSYCHIC}}},
        {27, {{PokemonType::GROUND, PokemonType::NONE}, {PokemonType::ICE, PokemonType::STEEL}}},
        {28, {{PokemonType::GROUND, PokemonType::NONE}, {PokemonType::ICE, PokemonType::STEEL}}},
        {37, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::ICE, PokemonType::NONE}}},
        {38, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::ICE, PokemonType::FAIRY}}},
        {50, {{PokemonType::GROUND, PokemonType::NONE}, {PokemonType::GROUND, PokemonType::STEEL}}},
        {51, {{PokemonType::GROUND, PokemonType::NONE}, {PokemonType::GROUND, PokemonType::STEEL}}},
        {52, {{PokemonType::NORMAL, PokemonType::NONE}, {PokemonType::DARK, PokemonType::NONE}, {PokemonType::STEEL, PokemonType::NONE}}},
        {53, {{PokemonType::NORMAL, PokemonType::NONE}, {PokemonType::DARK, PokemonType::NONE}}},
        {58, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::FIRE, PokemonType::ROCK}}},
        {59, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::FIRE, PokemonType::ROCK}}},
        {74, {{PokemonType::ROCK, PokemonType::GROUND}, {PokemonType::ROCK, PokemonType::ELECTRIC}}},
        {75, {{PokemonType::ROCK, PokemonType::GROUND}, {PokemonType::ROCK, PokemonType::ELECTRIC}}},
        {76, {{PokemonType::ROCK, PokemonType::GROUND}, {PokemonType::ROCK, PokemonType::ELECTRIC}}},
        {77, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::PSYCHIC, PokemonType::NONE}}},
        {78, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::PSYCHIC, PokemonType::FAIRY}}},
        {79, {{PokemonType::WATER, PokemonType::PSYCHIC}, {PokemonType::PSYCHIC, PokemonType::NONE}}},
        {80, {{PokemonType::WATER, PokemonType::PSYCHIC}, {PokemonType::POISON, PokemonType::PSYCHIC}}},
        {83, {{PokemonType::NORMAL, PokemonType::FLYING}, {PokemonType::FIGHTING, PokemonType::NONE}}},
        {88, {{PokemonType::POISON, PokemonType::NONE}, {PokemonType::POISON, PokemonType::DARK}}},
        {89, {{PokemonType::POISON, PokemonType::NONE}, {PokemonType::POISON, PokemonType::DARK}}},
        {100, {{PokemonType::ELECTRIC, PokemonType::NONE}, {PokemonType::ELECTRIC, PokemonType::GRASS}}},
        {101, {{PokemonType::ELECTRIC, PokemonType::NONE}, {PokemonType::ELECTRIC, PokemonType::GRASS}}},
        {103, {{PokemonType::GRASS, PokemonType::PSYCHIC}, {PokemonType::GRASS, PokemonType::DRAGON}}},
        {105, {{PokemonType::GROUND, PokemonType::NONE}, {PokemonType::FIRE, PokemonType::GHOST}}},
        {110, {{PokemonType::POISON, PokemonType::NONE}, {PokemonType::POISON, PokemonType::FAIRY}}},
        {122, {{PokemonType::PSYCHIC, PokemonType::FAIRY}, {PokemonType::ICE, PokemonType::PSYCHIC}}},
        {128, {{PokemonType::NORMAL, PokemonType::NONE}, {PokemonType::FIGHTING, PokemonType::NONE}, {PokemonType::FIGHTING, PokemonType::FIRE}, {PokemonType::FIGHTING, PokemonType::WATER}}},
        {144, {{PokemonType::ICE, PokemonType::FLYING}, {PokemonType::PSYCHIC, PokemonType::FLYING}}},
        {145, {{PokemonType::ELECTRIC, PokemonType::FLYING}, {PokemonType::FIGHTING, PokemonType::FLYING}}},
        {146, {{PokemonType::FIRE, PokemonType::FLYING}, {PokemonType::DARK, PokemonType::FLYING}}},
        {157, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::FIRE, PokemonType::GHOST}}},
        {194, {{PokemonType::WATER, PokemonType::GROUND}, {PokemonType::POISON, PokemonType::GROUND}}},
        {199, {{PokemonType::WATER, PokemonType::PSYCHIC}, {PokemonType::POISON, PokemonType::PSYCHIC}}},
        {211, {{PokemonType::WATER, PokemonType::POISON}, {PokemonType::DARK, PokemonType::POISON}}},
        {215, {{PokemonType::DARK, PokemonType::ICE}, {PokemonType::FIGHTING, PokemonType::POISON}}},
        {222, {{PokemonType::WATER, PokemonType::ROCK}, {PokemonType::GHOST, PokemonType::NONE}}},
        {263, {{PokemonType::NORMAL, PokemonType::NONE}, {PokemonType::DARK, PokemonType::NORMAL}}},
        {264, {{PokemonType::NORMAL, PokemonType::NONE}, {PokemonType::DARK, PokemonType::NORMAL}}},
        {479, {{PokemonType::ELECTRIC, PokemonType::GHOST}, {PokemonType::ELECTRIC, PokemonType::FIRE}, {PokemonType::ELECTRIC, PokemonType::WATER}, {PokemonType::ELECTRIC, PokemonType::ICE}, {PokemonType::ELECTRIC, PokemonType::FLYING}, {PokemonType::ELECTRIC, PokemonType::GRASS}}},
        {492, {{PokemonType::GRASS, PokemonType::NONE}, {PokemonType::GRASS, PokemonType::FLYING}}},
        {503, {{PokemonType::WATER, PokemonType::NONE}, {PokemonType::WATER, PokemonType::DARK}}},
        {549, {{PokemonType::GRASS, PokemonType::NONE}, {PokemonType::GRASS, PokemonType::FIGHTING}}},
        {554, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::ICE, PokemonType::NONE}}},
        {555, {{PokemonType::FIRE, PokemonType::NONE}, {PokemonType::ICE, PokemonType::NONE}}},
        {556, {{PokemonType::FIRE, PokemonType::PSYCHIC}, {PokemonType::ICE, PokemonType::FIRE}}},
        {562, {{PokemonType::GHOST, PokemonType::NONE}, {PokemonType::GROUND, PokemonType::GHOST}}},
        {570, {{PokemonType::DARK, PokemonType::NONE}, {PokemonType::NORMAL, PokemonType::GHOST}}},
        {571, {{PokemonType::DARK, PokemonType::NONE}, {PokemonType::NORMAL, PokemonType::GHOST}}},
        {618, {{PokemonType::GROUND, PokemonType::ELECTRIC}, {PokemonType::GROUND, PokemonType::STEEL}}},
        {628, {{PokemonType::NORMAL, PokemonType::FLYING}, {PokemonType::PSYCHIC, PokemonType::FLYING}}},
        {648, {{PokemonType::NORMAL, PokemonType::PSYCHIC}, {PokemonType::NORMAL, PokemonType::FIGHTING}}},
        {705, {{PokemonType::DRAGON, PokemonType::NONE}, {PokemonType::STEEL, PokemonType::DRAGON}}},
        {706, {{PokemonType::DRAGON, PokemonType::NONE}, {PokemonType::STEEL, PokemonType::DRAGON}}},
        {713, {{PokemonType::ICE, PokemonType::NONE}, {PokemonType::ICE, PokemonType::ROCK}}},
        {724, {{PokemonType::GRASS, PokemonType::GHOST}, {PokemonType::GRASS, PokemonType::FIGHTING}}},
        {741, {{PokemonType::FIRE, PokemonType::FLYING}, {PokemonType::ELECTRIC, PokemonType::FLYING}, {PokemonType::PSYCHIC, PokemonType::FLYING}, {PokemonType::GHOST, PokemonType::FLYING}}}
        // Urshifu

    };

    static std::unordered_map<size_t, std::vector<std::pair<FloatPixel, FloatPixel>>> visual_forms = {
        // {25, {{}}} // Pikachu (Base+Shiny, Cap, Hoenn, Sinnoh, Unova, Kalos, Alola, Partner, World)
        {201, {{FloatPixel(170.934285, 172.982582, 171.41171), /*Need shiny A form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(171.051985, 172.886542, 171.652027), /*Need shiny B form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(176.412055, 178.22759, 176.8081), /*Need shiny C form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(159.567979, 161.690026, 160.454821), FloatPixel(157.188212, 198.651457, 236.569283)},
               {FloatPixel(160.319388, 162.789918, 161.06639), /*Need shiny E form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(153.205915, 155.198765, 154.417376), FloatPixel(150.810034, 193.6098, 235.118375)},
               {FloatPixel(182.024254, 183.959737, 182.321307), FloatPixel(180.19163, 211.312942, 238.684495)},
               {FloatPixel(162.561158, 164.912954, 163.358196), /*Need shiny H form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(173.638146, 175.842502, 174.158502), /*Need shiny I form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(174.78719, 176.783337, 175.370398), FloatPixel(173.128448, 205.976991, 236.628388)},
               {FloatPixel(155.181497, 157.789063, 156.005996), FloatPixel(152.902236, 195.905834, 234.782663)},
               {FloatPixel(182.587585, 184.501769, 183.053828), FloatPixel(180.942259, 210.843551, 238.327138)},
               {FloatPixel(165.695827, 167.787549, 166.355483), /*Need shiny M form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(156.977935, 159.123696, 157.581949), /*Need shiny N form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(168.583553, 170.585142, 169.121477), FloatPixel(166.553304, 203.350642, 236.466633)},
               {FloatPixel(165.417406, 167.340598, 166.266998), /*Need shiny P form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(157.113683, 158.939366, 157.916402), FloatPixel(154.486959, 193.69863, 232.002593)},
               {FloatPixel(150.893273, 152.667826, 151.728475), FloatPixel(148.442904, 191.789708, 233.815925)},
               {FloatPixel(171.734725, 173.602096, 172.299721), FloatPixel(169.83889, 206.225567, 238.884264)},
               {FloatPixel(183.944942, 185.595455, 184.330615), FloatPixel(182.191525, 213.09559, 240.793006)},
               {FloatPixel(149.057921, 150.999191, 149.797233), /*Need shiny U form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(174.859411, 176.782708, 175.724967), FloatPixel(173.018827, 209.631461, 241.697251)},
               {FloatPixel(174.45659, 176.309105, 174.934824), FloatPixel(172.27166, 207.564951, 239.244499)},
               {FloatPixel(153.545989, 155.090164, 154.541672), /*Need shiny X form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(178.924631, 180.80897, 179.51523), /*Need shiny Y form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(161.410736, 163.732717, 162.055058), FloatPixel(159.08811, 198.56639, 234.025063)},
               {FloatPixel(166.253478, 167.907453, 166.422323), /*Need shiny ! form Unown*/ FloatPixel(255, 255, 255)},
               {FloatPixel(162.93385, 164.838815, 163.50664), FloatPixel(160.774164, 205.105798, 242.707804)}}}, // Unown (A-Z?!+Shiny)
        // {351, {{}}} // Castform (Normal+Shiny, Sunny+Shiny, Rainy+Shiny, Snowy+Shiny)
        // {386, {{}}} // Deoxys (Normal+Shiny, Attack+Shiny, Defense+Shiny, Speed+Shiny)
        {412, {{FloatPixel(187.917961, 224.88263, 150.888176), FloatPixel(186.449005, 226.713005, 155.512037)},
               {FloatPixel(234.216738, 234.237543, 209.198765), FloatPixel(232.057786, 236.224607, 215.107357)},
               {FloatPixel(238.852021, 218.341228, 220.157693), FloatPixel(236.174496, 219.359965, 224.627593)}}}, // Burmy (Plant+Shiny, Sandy+Shiny, Trash+Shiny)
        {413, {{FloatPixel(167.571861, 212.879677, 158.623366), FloatPixel(167.632945, 215.726481, 164.212975)},
               {FloatPixel(227.790158, 209.376304, 177.705915), FloatPixel(227.686578, 212.199439, 183.602096)},
               {/*Need nonshiny Trash Cloak form Wormadam*/ FloatPixel(255, 255, 255), FloatPixel(240.343821, 199.945587, 213.737379)}}}, // Wormadam (Plant+Shiny, Sandy+Shiny, Trash+Shiny)
        {422, {{FloatPixel(123.741276, 214.299886, 216.025723), /*Need shiny East sea form Shellos*/ FloatPixel(255, 255, 255)},
               {FloatPixel(249.065206, 203.930327, 199.138116), /*Need shiny West sea form Shellos*/ FloatPixel(255, 255, 255)}}}, // Shellos (East Sea+Shiny, West Sea+Shiny)
        {423, {{FloatPixel(115.69842, 205.94094, 167.599727), /*Need shiny East sea form Gastrodon*/ FloatPixel(255, 255, 255)},
               {FloatPixel(223.847374, 184.596384, 159.035166), FloatPixel(221.540532, 199.802884, 136.430507)}}}, // Gastrodon (East Sea+Shiny, West Sea+Shiny)
        {550, {{FloatPixel(154.554668, 184.862948, 128.165682), FloatPixel(176.424991, 220.055567, 139.549646)},
               {FloatPixel(155.520356, 196.498486, 152.874385), FloatPixel(173.566929, 223.443818, 153.306796)},
               {FloatPixel(155.77202, 182.861839, 141.182531), FloatPixel(176.417361, 215.793141, 147.944463)}}}, // Basculin (Red Striped+Shiny, Blue Striped+Shiny, White Striped+Shiny)
        {585, {{FloatPixel(248.905624, 231.960547, 202.304008), /*Need shiny Spring Form form Deerling*/ FloatPixel(255, 255, 255)},
               {FloatPixel(200.765784, 221.649718, 157.010388), /*Need shiny Summer Form form Deerling*/ FloatPixel(255, 255, 255)},
               {FloatPixel(249.129347, 223.596864, 153.540712), /*Need shiny Autumn Form form Deerling*/ FloatPixel(255, 255, 255)},
               {/*Need nonshiny Winter Form form Deerling*/ FloatPixel(255, 255, 255), /*Need shiny Winter Form form Deerling*/ FloatPixel(255, 255, 255)}}}, // Deerling (Spring+Shiny, Summer+Shiny, Autumn+Shiny, Winter+Shiny)
        {586, {{FloatPixel(226.703307, 210.355288, 183.036095), /*Need shiny Spring Form form Sawsbuck*/ FloatPixel(255, 255, 255)},
               {/*Need nonshiny Summer Form form Sawsbuck*/ FloatPixel(255, 255, 255), /*Need shiny Summer Form form Sawsbuck*/ FloatPixel(255, 255, 255)},
               {FloatPixel(225.556512, 191.422413, 157.115032), /*Need shiny Autumn Form form Sawsbuck*/ FloatPixel(255, 255, 255)},
               {FloatPixel(230.642283, 221.722029, 206.291687), /*Need shiny Winter Form form Sawsbuck*/ FloatPixel(255, 255, 255)}}}, // Sawsbuck (Spring+Shiny, Summer+Shiny, Autumn+Shiny, Winter+Shiny)
        {641, {{FloatPixel(180.638956, 187.422533, 160.762681), /*Need shiny Incarnate form Tornadus*/ FloatPixel(255, 255, 255)},
               {/*Need nonshiny Therian form Tornadus*/ FloatPixel(255, 255, 255), /*Need shiny Therian form Tornadus*/ FloatPixel(255, 255, 255)}}}, // Tornadus (Incarnate+Shiny, Therian+Shiny)
        {642, {{FloatPixel(181.380142, 198.698165, 205.539588), /*Need shiny Incarnate form Thundurus*/ FloatPixel(255, 255, 255)},
               {/*Need nonshiny Therian form Thundurus*/ FloatPixel(255, 255, 255), FloatPixel(217.579161, 205.614537, 235.753148)}}}, // Thundurus (Incarnate+Shiny, Therian+Shiny)
        {645, {{FloatPixel(216.922398, 184.366681, 141.085397), /*Need shiny Incarnate form Landorus*/ FloatPixel(255, 255, 255)},
               {/*Need nonshiny Therian form Landorus*/ FloatPixel(255, 255, 255), /*Need shiny Therian form Landorus*/ FloatPixel(255, 255, 255)}}} // Landorus (Incarnate+Shiny, Therian+Shiny)
        // {647, {{}}} // Keldeo (Normal+Shiny, Resolute+Shiny)
        // {658, {{}}} // Greninja (Normal+Shiny, Ash Greninja+Shiny)
        // {666, {{}}} // Vivillon (Archipelago+Shiny, Continental+Shiny, Elegant+Shiny, Fancy+Shiny, Garden+Shiny, High Plains+Shiny, Icy Snow+Shiny, Jungle+Shiny, Marine+Shiny, Meadow+Shiny, Modern+Shiny, Monsoon+Shiny, Ocean+Shiny, Pokeball+Shiny, Polar+Shiny, River+Shiny, Sandstorm+Shiny, Savanna+Shiny, Sun+Shiny, Tundra+Shiny)
        // {669, {{}}} // Flabebe (Red+Shiny, Yellow+Shiny, Orange+Shiny, Blue+Shiny, White+Shiny)
        // {670, {{}}} // Floette (Red+Shiny, Yellow+Shiny, Orange+Shiny, Blue+Shiny, White+Shiny)
        // {671, {{}}} // Florges (Red+Shiny, Yellow+Shiny, Orange+Shiny, Blue+Shiny, White+Shiny)
        // {676, {{}}} // Furfrou (Normal+Shiny, Heart+Shiny, Star+Shiny, Diamond+Shiny, Debutante+Shiny, Matron+Shiny, Dandy+Shiny, La Reine+Shiny, Kabuki+Shiny, Pharoah+Shiny)
        // {710, {{}}} // Pumpkaboo (Average+Shiny, Small+Shiny, Large+Shiny, Super+Shiny)
        // {711, {{}}} // Gourgeist (Average+Shiny, Small+Shiny, Large+Shiny, Super+Shiny)
        // {720, {{}}} // Hoopa (Confined+Shiny, Unbound+Shiny)
        // {745, {{}}} // Lycanroc (Midday+Shiny, Midnight+Shiny, Dusk+Shiny)
        // {774, {{}}} // Minior
        // {801, {{}}} // Magearna
        // {849, {{}}} // Toxtricity
        // {893, {{}}} // Zarude
        // {901, {{}}} // Ursaluna
        // {905, {{}}} // Enamorus (Incarnate+Shiny, Therian+Shiny)
        // {925, {{}}} // Maushold
        // {931, {{}}} // Squawkabilly
        // {978, {{}}} // Tatsugiri
        // {982, {{}}} // Dudunsparce
        // {999, {{}}} // Gimmighoul
    };


    double temp = (size_t)this->national_dex_number;

    auto it = regional_codes.find(this->national_dex_number);
    if (!(it == regional_codes.end() || it->second.empty())) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (this->type1 == it->second[i].first && this->type2 == it->second[i].second) {
                temp += i * 0.01; // Encode form as a suffix
            }
        }
    }

    auto it2 = visual_forms.find(this->national_dex_number);
    if (!(it2 == visual_forms.end() || it2->second.empty())) {
        double min_distance = std::numeric_limits<double>::max();
        size_t form_id=0;

        size_t i =0;

        for (const auto& [nonshiny, shiny] : it2->second) {
            double nonshiny_distance = euclidean_distance(nonshiny, this->color);
            double shiny_distance = euclidean_distance(shiny, this->color);
            if (nonshiny_distance < min_distance) {
                min_distance = nonshiny_distance;
                form_id = i;
            }
            if (shiny_distance < min_distance) {
                min_distance = shiny_distance;
                form_id = i;
            }


            i++;
        }

        temp += form_id * 0.0001;
    }


    this->national_dex_number = temp;

}

std::string Pokemon::log_details(SingleSwitchProgramEnvironment* env, bool log_to_env) const {
    std::ostringstream ss;
    ss << "Shiny: " << (shiny ? "Yes" : "No") << ", "
       << "Dex: " << national_dex_number << ", "
       << "Level: " << level << ", "
       << "Quick color" << quick_color << ", "
       << "Position: Box " << current_box << ", "
       << ", (" << current_row << ", " << current_col << ")\n";

    if (log_to_env && env) {
        env->console.log(ss.str());
    }
    return ss.str();
}

JsonValue Pokemon::to_json(){
    JsonObject pokemon;
    pokemon["national_dex_number"] = national_dex_number;
    pokemon["shiny"] = shiny;
    pokemon["gmax"] = gmax;
    pokemon["gender"] = gender_to_string(gender);
    pokemon["level"] = level;
    pokemon["form_id"] = form_id;
    pokemon["type1"] = type_to_string(type1);
    pokemon["type2"] = type_to_string(type2);

    JsonObject pokemon_color;
    pokemon_color["r"] = color.r;
    pokemon_color["g"] = color.g;
    pokemon_color["b"] = color.b;
    pokemon["color"] = JsonValue(std::move(pokemon_color));


    JsonObject quick_color_obj;
    quick_color_obj["r"] = quick_color.r;
    quick_color_obj["g"] = quick_color.g;
    quick_color_obj["b"] = quick_color.b;
    pokemon["quick_color"] = JsonValue(std::move(quick_color_obj));

    pokemon["current_box"] = current_box; // Box number the Pokémon is currently in
    pokemon["current_row"] = current_row; // Row position in the current box
    pokemon["current_col"] = current_col; // Column position in the current box


    return pokemon;
}

Pokemon Pokemon::from_json(const JsonValue& value) {
    const JsonObject* pokemon_obj = value.to_object();
    if (!pokemon_obj) {
        throw std::runtime_error("Invalid JSON: Expected a JSON object for Pokemon.");
    }

    FloatPixel color(
        pokemon_obj->get_object("color")->get_double_throw("r"),
        pokemon_obj->get_object("color")->get_double_throw("g"),
        pokemon_obj->get_object("color")->get_double_throw("b")
        );

    FloatPixel quick_color(
        pokemon_obj->get_object("quick_color")->get_double_throw("r"),
        pokemon_obj->get_object("quick_color")->get_double_throw("g"),
        pokemon_obj->get_object("quick_color")->get_double_throw("b")
        );

    StatsHuntGenderFilter gender;
    std::string gender_str = pokemon_obj->get_string_throw("gender");

    if (gender_str == "Any") {
        gender = StatsHuntGenderFilter::Any;
    } else if (gender_str == "Male") {
        gender = StatsHuntGenderFilter::Male;
    } else if (gender_str == "Female") {
        gender = StatsHuntGenderFilter::Female;
    } else if (gender_str == "Genderless") {
        gender = StatsHuntGenderFilter::Genderless;
    } else {
        // Handle unknown or invalid gender strings (optional)
        throw std::runtime_error("Invalid gender value: " + gender_str);
    }

    PokemonType type1;
    std::string type1_str = pokemon_obj->get_string_throw("type1");

    if (type1_str == "None") {
        type1 = PokemonType::NONE;
    } else if (type1_str == "Normal") {
        type1 = PokemonType::NORMAL;
    } else if (type1_str == "Fire") {
        type1 = PokemonType::FIRE;
    } else if (type1_str == "Fighting") {
        type1 = PokemonType::FIGHTING;
    } else if (type1_str == "Water") {
        type1 = PokemonType::WATER;
    } else if (type1_str == "Flying") {
        type1 = PokemonType::FLYING;
    } else if (type1_str == "Grass") {
        type1 = PokemonType::GRASS;
    } else if (type1_str == "Poison") {
        type1 = PokemonType::POISON;
    } else if (type1_str == "Electric") {
        type1 = PokemonType::ELECTRIC;
    } else if (type1_str == "Ground") {
        type1 = PokemonType::GROUND;
    } else if (type1_str == "Psychic") {
        type1 = PokemonType::PSYCHIC;
    } else if (type1_str == "Rock") {
        type1 = PokemonType::ROCK;
    } else if (type1_str == "Ice") {
        type1 = PokemonType::ICE;
    } else if (type1_str == "Bug") {
        type1 = PokemonType::BUG;
    } else if (type1_str == "Dragon") {
        type1 = PokemonType::DRAGON;
    } else if (type1_str == "Ghost") {
        type1 = PokemonType::GHOST;
    } else if (type1_str == "Dark") {
        type1 = PokemonType::DARK;
    } else if (type1_str == "Steel") {
        type1 = PokemonType::STEEL;
    } else if (type1_str == "Fairy") {
        type1 = PokemonType::FAIRY;
    } else {
        // Handle unknown or invalid gender strings (optional)
        throw std::runtime_error("Invalid type value: " + type1_str);
    }

    PokemonType type2;
    std::string type2_str = pokemon_obj->get_string_throw("type2");

    if (type2_str == "None") {
        type2 = PokemonType::NONE;
    } else if (type2_str == "Normal") {
        type2 = PokemonType::NORMAL;
    } else if (type2_str == "Fire") {
        type2 = PokemonType::FIRE;
    } else if (type2_str == "Fighting") {
        type2 = PokemonType::FIGHTING;
    } else if (type2_str == "Water") {
        type2 = PokemonType::WATER;
    } else if (type2_str == "Flying") {
        type2 = PokemonType::FLYING;
    } else if (type2_str == "Grass") {
        type2 = PokemonType::GRASS;
    } else if (type2_str == "Poison") {
        type2 = PokemonType::POISON;
    } else if (type2_str == "Electric") {
        type2 = PokemonType::ELECTRIC;
    } else if (type2_str == "Ground") {
        type2 = PokemonType::GROUND;
    } else if (type2_str == "Psychic") {
        type2 = PokemonType::PSYCHIC;
    } else if (type2_str == "Rock") {
        type2 = PokemonType::ROCK;
    } else if (type2_str == "Ice") {
        type2 = PokemonType::ICE;
    } else if (type2_str == "Bug") {
        type2 = PokemonType::BUG;
    } else if (type2_str == "Dragon") {
        type2 = PokemonType::DRAGON;
    } else if (type2_str == "Ghost") {
        type2 = PokemonType::GHOST;
    } else if (type2_str == "Dark") {
        type2 = PokemonType::DARK;
    } else if (type2_str == "Steel") {
        type2 = PokemonType::STEEL;
    } else if (type2_str == "Fairy") {
        type2 = PokemonType::FAIRY;
    } else {
        // Handle unknown or invalid gender strings (optional)
        throw std::runtime_error("Invalid type value: " + type2_str);
    }

    Pokemon pokemon;
    pokemon.national_dex_number = static_cast<float>(pokemon_obj->get_double_throw("national_dex_number"));
    pokemon.shiny = pokemon_obj->get_boolean_throw("shiny");
    pokemon.gmax = pokemon_obj->get_boolean_throw("gmax");
    pokemon.gender = gender;
    pokemon.level = static_cast<uint16_t>(pokemon_obj->get_integer_throw("level"));  // Ensure correct type
    pokemon.form_id = static_cast<size_t>(pokemon_obj->get_integer_throw("form_id"));  // Ensure correct type
    pokemon.current_box = static_cast<size_t>(pokemon_obj->get_integer_throw("current_box"));  // Ensure correct type
    pokemon.current_row = static_cast<size_t>(pokemon_obj->get_integer_throw("current_row"));  // Ensure correct type
    pokemon.current_col = static_cast<size_t>(pokemon_obj->get_integer_throw("current_col"));  // Ensure correct type
    pokemon.type1 = type1;
    pokemon.type2 = type2;
    pokemon.color = color;
    pokemon.quick_color = quick_color;

    return pokemon;
}

PokemonType closest_type(const FloatPixel& color_box){
    // Use string literals directly as keys
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
}

Pokemon home_read_pokemon_summary(SingleSwitchProgramEnvironment& env, ProControllerContext& context, size_t box, size_t row, size_t col) {
    Pokemon pokemon;

    while (true) {
        // pokemon = Pokemon();
        pokemon.current_box = box;
        pokemon.current_row = row;
        pokemon.current_col = col;

        // Populate Pokémon attributes (same as before)
        ImageFloatBox national_dex_number_box(0.448, 0.245, 0.042, 0.04);
        ImageFloatBox shiny_symbol_box(0.702, 0.09, 0.04, 0.06);
        ImageFloatBox gmax_symbol_box(0.463, 0.09, 0.04, 0.06);
        ImageFloatBox pokemon_box_small(0.76, 0.295, 0.14, 0.23);
        ImageFloatBox origin_symbol_box(0.623, 0.095, 0.033, 0.05);
        ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041);
        ImageFloatBox type_1(0.622, 0.245, 0.029, 0.053);
        ImageFloatBox type_2(0.654, 0.245, 0.029, 0.053);
        ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046);


        VideoSnapshot screen = env.console.video().snapshot();
        VideoOverlaySet box_render(env.console);
        std::ostringstream ss;

        ImageFloatBox r_button(0.87, 0.11, 0.001, 0.001);
        FloatPixel pixel = image_stats(extract_box_reference(screen, r_button)).average;

        box_render.add(COLOR_RED, r_button);
        // env.console.log(std::to_string(pixel.r) + " " + std::to_string(pixel.g) + " " + std::to_string(pixel.b));

        // Get out ahead if the page hasn't finished scrolling
        if (pixel.r == 255 && pixel.g == 210 && pixel.b == 107) {
            pbf_wait(context, 500ms);
            context.wait_for_all_requests();
            continue; // Retry without returning
        } else {
        }

        FloatPixel type_1_color = image_stats(extract_box_reference(screen, type_1)).average;
        FloatPixel type_2_color = image_stats(extract_box_reference(screen, type_2)).average;

        pokemon.type1 = closest_type(type_1_color);
        pokemon.type2 = closest_type(type_2_color);

        FloatPixel pokemon_color = image_stats(extract_box_reference(screen, pokemon_box_small)).average;
        pokemon.color = pokemon_color;

        int shiny_stddev_value = static_cast<int>(image_stddev(extract_box_reference(screen, shiny_symbol_box)).sum());
        pokemon.shiny = shiny_stddev_value > 30;

        int gmax_stddev_value = static_cast<int>(image_stddev(extract_box_reference(screen, gmax_symbol_box)).sum());
        pokemon.gmax = gmax_stddev_value > 30;

        int ot_id_value = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
        pokemon.ot_id = ot_id_value;

        int national_dex_number = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
        if (national_dex_number < 0 || national_dex_number > 1025) {
            // env.console.log("FOUND ISSUE!!!!");
            pbf_wait(context, 500ms);
            context.wait_for_all_requests();
            continue; // Retry without returning
        }
        pokemon.national_dex_number = static_cast<uint16_t>(national_dex_number);
        pokemon.update_national_id();
        // env.console.log(std::to_string(pokemon.national_dex_number));

        BoxGenderDetector::make_overlays(box_render);
        StatsHuntGenderFilter gender = BoxGenderDetector::detect(screen);
        pokemon.gender = gender;

        int level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
        if (level < 0 || level > 100) {
            // env.console.log("FOUND ISSUE!!!!");
            pbf_wait(context, 500ms);
            context.wait_for_all_requests();
            continue; // Retry without returning
        }
        pokemon.level = static_cast<uint16_t>(level);

        if (pokemon.national_dex_number <= 1025) {
            break; // Valid Pokémon, exit the loop
        }
    }

    return pokemon;
}

std::pair<size_t,size_t> home_locate_home_position(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    int col;
    int row;
    FloatPixel temp[5][6];

    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();
    VideoOverlaySet box_render(env.console);

    for(int i = 0; i<5; i++){
        for(int j = 0; j<6; j++){
            ImageFloatBox pointer_box(0.0735 + (0.072 * j), 0.165 + (0.1035 * i), 0.0055, 0.004);
            box_render.add(COLOR_RED, pointer_box);
            FloatPixel current_box_value = image_stats(extract_box_reference(screen, pointer_box)).average;
            // env.console.log(std::to_string(current_box_value.r)+" "+std::to_string(current_box_value.g)+" "+std::to_string(current_box_value.b));
            temp[i][j] = current_box_value;
        }
    }
    pbf_press_button(context, BUTTON_ZL, 10, 30);
    context.wait_for_all_requests();
    screen = env.console.video().snapshot();
    for(int i = 0; i<5; i++){
        for(int j = 0; j<6; j++){
            ImageFloatBox pointer_box(0.0735 + (0.072 * j), 0.165 + (0.1035 * i), 0.0055, 0.004);
            box_render.add(COLOR_RED, pointer_box);
            FloatPixel current_box_value = image_stats(extract_box_reference(screen, pointer_box)).average;
            if(temp[i][j].r != current_box_value.r &&temp[i][j].g != current_box_value.g && temp[i][j].b != current_box_value.b){
                row = i;
                col = j;
            }
        }
    }
    pbf_press_button(context, BUTTON_ZR, 10, 30);
    context.wait_for_all_requests();
    box_render.clear();

    return {row,col};
}

// Helps pick up the pokemon at the expected spot. Corrects for already holding a pokemon, not being in red selection mode, and cursor not being in the correct place.
void home_pick_up_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager){


    pbf_press_button(context, BUTTON_Y, 10, 70);    // press y button
    home_manager.cursor->pick_up_pokemon(env, context);


}

void home_check_markings(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int col, int row){
    ImageFloatBox home_circle_marking(0.8075, 0.817, 0.0025, 0.005);
    ImageFloatBox home_triangle_marking(0.8375, 0.817, 0.0025, 0.005);
    ImageFloatBox home_square_marking(0.8725, 0.817, 0.0025, 0.005);
    ImageFloatBox home_heart_marking(0.9025, 0.817, 0.0025, 0.005);
    ImageFloatBox home_star_marking(0.935, 0.817, 0.0025, 0.005);
    ImageFloatBox home_diamond_marking(0.9675, 0.817, 0.0025, 0.005);

    ImageFloatBox home_circle_marking_big(0.75, 0.5, 0.0025, 0.005);
    ImageFloatBox home_triangle_marking_big(0.87, 0.5, 0.0025, 0.005);
    ImageFloatBox home_square_marking_big(0.75, 0.62, 0.0025, 0.005);
    ImageFloatBox home_heart_marking_big(0.87, 0.62, 0.0025, 0.005);
    ImageFloatBox home_star_marking_big(0.75, 0.75, 0.0025, 0.005);
    ImageFloatBox home_diamond_marking_big(0.87, 0.75, 0.0025, 0.005);

    context.wait_for_all_requests();
    VideoSnapshot screen = env.console.video().snapshot();

    VideoOverlaySet box_render(env.console);

    box_render.add(COLOR_GREEN, home_circle_marking);
    box_render.add(COLOR_GREEN, home_triangle_marking);
    box_render.add(COLOR_GREEN, home_square_marking);
    box_render.add(COLOR_GREEN, home_heart_marking);
    box_render.add(COLOR_GREEN, home_star_marking);
    box_render.add(COLOR_GREEN, home_diamond_marking);



    std::vector<ImageFloatBox> marking_list = {home_circle_marking,home_triangle_marking,home_square_marking,home_heart_marking,home_star_marking,home_diamond_marking};
    bool marked = false;
    for(auto m:marking_list){
        FloatPixel marking = image_stats(extract_box_reference(screen, m)).average;

        marked = marked|(marking.r==255)|(marking.b==255);
    }

    if(marked){
        pbf_press_button(context, BUTTON_A, 10, 18);
        pbf_press_dpad(context, DPAD_DOWN,10, 40);
        pbf_press_dpad(context, DPAD_DOWN,10, 40);

        // Check can be marked in the first place
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
        ImageFloatBox can_mark(0.16 + (col * .0705), std::min(0.585, row*0.11+0.395), 0.0075, 0.01);
        box_render.add(COLOR_BLACK,can_mark);
        FloatPixel scan_val = image_stats(extract_box_reference(screen, can_mark)).average;
        env.console.log(std::to_string(scan_val.r)+" "+std::to_string(scan_val.g)+" "+std::to_string(scan_val.b));
        if(!(scan_val.r==255&&scan_val.b==0)){
            pbf_press_button(context, BUTTON_B, 10, 27);
            context.wait_for_all_requests();
            box_render.clear();
            return;
        }

        pbf_press_button(context, BUTTON_A, 10, 27);

        box_render.add(COLOR_GREEN, home_circle_marking_big);
        box_render.add(COLOR_GREEN, home_triangle_marking_big);
        box_render.add(COLOR_GREEN, home_square_marking_big);
        box_render.add(COLOR_GREEN, home_heart_marking_big);
        box_render.add(COLOR_GREEN, home_star_marking_big);
        box_render.add(COLOR_GREEN, home_diamond_marking_big);

        std::vector<ImageFloatBox> marking_list_big = {home_circle_marking_big,home_triangle_marking_big,home_square_marking_big,home_heart_marking_big,home_star_marking_big,home_diamond_marking_big};

        for(int i = 0; i<3; i++){
            for(int j=0;j<2;j++){
                while(image_stats(extract_box_reference(screen, marking_list_big[2*i+j])).average.r==255||image_stats(extract_box_reference(screen, marking_list_big[2*i+j])).average.b==255){
                    pbf_press_button(context, BUTTON_A, 10, 27);
                    context.wait_for_all_requests();
                    screen = env.console.video().snapshot();
                }
                pbf_press_dpad(context, DPAD_RIGHT, 10, 18);
            }
            pbf_press_dpad(context, DPAD_DOWN,10 ,18);
        }
        pbf_press_button(context, BUTTON_A, 10,27);

        box_render.clear();
    }else{
        env.console.log("No Markings Found");
    }

    context.wait_for_all_requests();

}

void home_navigate_to_box_secondary(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string target) {
    ImageFloatBox home_box_checker_secondary(0.62, 0.105, 0.255, 0.04);
    VideoOverlaySet box_render(env.console);

    // Add overlays for debugging
    box_render.add(COLOR_RED, home_box_checker_secondary);
    box_render.add(COLOR_GREEN, home_box_checker_secondary);

    context.wait_for_all_requests();

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };

    std::string box_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary)));

    // Store the last 5 reads
    std::deque<std::string> prev_reads;

    while (target != box_name) {
        // Add current box_name to prev_reads
        prev_reads.push_back(box_name);
        if (prev_reads.size() > 5) {
            prev_reads.pop_front();
        }

        // Check if the last 5 reads are all the same
        if (prev_reads.size() == 5 && std::all_of(prev_reads.begin(), prev_reads.end(), [&](const std::string& val) {
                return val == prev_reads.front();
            })) {
            // All 5 reads are the same, press DPAD_LEFT
            pbf_press_button(context, BUTTON_LEFT, 10, 80);
        }

        // Navigate to the next box
        pbf_press_button(context, BUTTON_L, 10, 80);
        context.wait_for_all_requests();
        box_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary)));
    }
}

void home_navigate_to_box_secondary(SingleSwitchProgramEnvironment& env, ProControllerContext& context, size_t target){
    ImageFloatBox home_box_checker_secondary(0.85, 0.725, 0.03, 0.03);
    VideoOverlaySet box_render(env.console);

    // Go to the first box in the program
    box_render.add(COLOR_RED, home_box_checker_secondary);

    size_t home_box = OCR::read_number_waterfill(env.console, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary), 0xff000000, 0xff7f7f7f);

    while(home_box!=target){
        while((home_box>200)){
            pbf_wait(context, 300);
            context.wait_for_all_requests();
            home_box = OCR::read_number_waterfill(env.console, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary), 0xff000000, 0xff7f7f7f);
        }
        env.console.log("Home_box is " + std::to_string(home_box));
        context.wait_for_all_requests();
        box_render.clear();
        while(home_box<target){
            pbf_press_button(context, BUTTON_R, 10, 47);
            home_box++;
        }
        while(home_box>target){
            pbf_press_button(context, BUTTON_L, 10, 47);
            home_box--;
        }
        context.wait_for_all_requests();
        box_render.add(COLOR_RED, home_box_checker_secondary);
        home_box = OCR::read_number_waterfill(env.console, extract_box_reference(env.console.video().snapshot(), home_box_checker_secondary), 0xff000000, 0xff7f7f7f);
    }

    pbf_wait(context, 1000ms);
    context.wait_for_all_requests();
    box_render.clear();

}

void home_scan_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, VideoSnapshot screen, PokemonHome_HomeEnvironment& home_manager, bool clear_markings=false, bool release_go=false){
    home_manager.navigate_to(env, context, {0,0});

    for(int i = 0; i<5;i++){
        for(int j = 0; j<6; j++){
            screen = env.console.video().snapshot();
            if(clear_markings)home_check_markings(env, context, j, i);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 27);
        }
        pbf_press_dpad(context, DPAD_DOWN, 10, 27);
        context.wait_for_all_requests();
    }
    pbf_press_dpad(context, DPAD_DOWN, 10, 27);
    pbf_press_dpad(context, DPAD_DOWN, 10, 27);

    context.wait_for_all_requests();
}

std::pair<size_t,size_t> home_locate_empty_position(SingleSwitchProgramEnvironment& env, ProControllerContext& context, size_t* current_box, size_t last_box){
    VideoOverlaySet box_render(env.console);

    VideoSnapshot screen = env.console.video().snapshot();

    while(*current_box<last_box){
        for (size_t row = 0; row < 5; row++){
            for (size_t column = 0; column < 6; column++){
                ImageFloatBox slot_box(0.06 + (0.072 * column), 0.2 + (0.1035 * row), 0.03, 0.057);
                int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();

                //checking color to know if a pokemon is on the slot or not
                if(current_box_value < 5){
                    return {row, column};
                }
            }
        }
        pbf_press_button(context, BUTTON_R, 10, 80);
        (*current_box)++;
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
    }
    throw;
}

std::pair<size_t,size_t> home_locate_empty_position_secondary(SingleSwitchProgramEnvironment& env, ProControllerContext& context, size_t* current_box, size_t last_box, bool transpose=false){
    VideoOverlaySet box_render(env.console);

    VideoSnapshot screen = env.console.video().snapshot();

    if(transpose){
        while(*current_box<last_box){
            for (size_t column = 0; column < 6; column++){
                for (size_t row = 0; row < 5; row++){
                    ImageFloatBox slot_box(0.55 + (0.072 * column), 0.2 + (0.1035 * row), 0.03, 0.057);
                    int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();

                    //checking color to know if a pokemon is on the slot or not
                    if(current_box_value < 5){
                        return {row, column+6};
                    }
                }
            }
            pbf_press_button(context, BUTTON_R, 10, 80);
            (*current_box)++;
            context.wait_for_all_requests();
            screen = env.console.video().snapshot();
        }
    }else{
        while(*current_box<last_box){
            for (size_t row = 0; row < 5; row++){
                for (size_t column = 0; column < 6; column++){
                    ImageFloatBox slot_box(0.55 + (0.072 * column), 0.2 + (0.1035 * row), 0.03, 0.057);
                    int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();

                    //checking color to know if a pokemon is on the slot or not
                    if(current_box_value < 5){
                        return {row, column};
                    }
                }
            }
            pbf_press_button(context, BUTTON_R, 10, 80);
            (*current_box)++;
            context.wait_for_all_requests();
            screen = env.console.video().snapshot();
        }
    }
    throw;
}

void home_swap_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, const std::pair<size_t, size_t>& slot1, const std::pair<size_t, size_t>& slot2){
    home_manager.navigate_to(env, context, slot1);

    home_pick_up_pokemon(env, context, home_manager);

    home_manager.navigate_to(env, context, slot2);

    pbf_press_button(context, BUTTON_Y, 10, 80);
    home_manager.cursor->put_down_pokemon(env, context);

    home_manager.navigate_to(env, context, slot2); // TEMPORARY, DOUBLE CHECK POSITION


    context.wait_for_all_requests();
}

void home_swap_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, const std::pair<size_t, size_t>& slot1, PokemonBox& box1 , const std::pair<size_t, size_t>& slot2, PokemonBox& box2){
    // Check if both slots are empty

    std::optional<Pokemon>& pokemon1 = box1.get_pokemon(slot1.first, slot1.second);
    std::optional<Pokemon>& pokemon2 = box2.get_pokemon(slot2.first, slot2.second);
    if(!pokemon1.has_value() && !pokemon2.has_value()){
        return;
    }

    // Check if just one slot is empty
    if(!pokemon1.has_value()){
        // Go to pokemon 2
        home_manager.navigate_to(env, context, slot2, box2.box_num);
        // Pick it up
        home_pick_up_pokemon(env, context, home_manager);
        // go to pokemon 1
        home_manager.navigate_to(env, context, slot1, box1.box_num);
        // Press y
        pbf_press_button(context, BUTTON_Y, 10, 70);
        home_manager.cursor->put_down_pokemon(env, context);
        // Swap the pokemon pointers and row, col, box
        pokemon1 = std::move(pokemon2.value());  // Move the real value into pokemon1
        pokemon2.reset();                       // Clear pokemon2 to make it blank
        pokemon1->current_box = box1.box_num;
        pokemon1->current_row = slot1.first;
        pokemon1->current_col = slot1.second;
    }else if (!pokemon2.has_value()){
        // Go to pokemon 1
        home_manager.navigate_to(env, context, slot1, box1.box_num);
        // Pick it up
        home_pick_up_pokemon(env, context, home_manager);
        // go to pokemon 2
        home_manager.navigate_to(env, context, slot2, box2.box_num);
        // Press y
        pbf_press_button(context, BUTTON_Y, 20, 70);
        home_manager.cursor->put_down_pokemon(env, context);
        // Swap the pokemon pointers and row, col, box
        pokemon2 = std::move(pokemon1.value());  // Move the real value into pokemon2
        pokemon1.reset();                        // Clear pokemon1 to make it blank
        pokemon2->current_box = box2.box_num;
        pokemon2->current_row = slot2.first;
        pokemon2->current_col = slot2.second;
    }else{      // Both exist, move to the closest one and swap them
        if(std::fabs(box1.box_num-home_manager.get_box())<std::fabs(box2.box_num-home_manager.get_box())){      // Left is closest
            // Go to pokemon 1
            home_manager.navigate_to(env, context, slot1, box1.box_num);
            // Pick it up
            home_pick_up_pokemon(env, context, home_manager);
            // go to pokemon 2
            home_manager.navigate_to(env, context, slot2, box2.box_num);
            // Press y
            pbf_press_button(context, BUTTON_Y, 20, 70);
            home_manager.cursor->put_down_pokemon(env, context);
            // Swap the pokemon pointers and row, col, box
            std::swap(*pokemon2, *pokemon1);
            std::swap(pokemon2->current_box, pokemon1->current_box);
            std::swap(pokemon2->current_col, pokemon1->current_col);
            std::swap(pokemon2->current_row, pokemon1->current_row);
        }else if(std::fabs(box2.box_num-home_manager.get_box())<std::fabs(box1.box_num-home_manager.get_box())){                                                          // Right is closest
            // Go to pokemon 2
            home_manager.navigate_to(env, context, slot2, box2.box_num);
            // Pick it up
            home_pick_up_pokemon(env, context, home_manager);
            // go to pokemon 1
            home_manager.navigate_to(env, context, slot1, box1.box_num);
            // Press y
            pbf_press_button(context, BUTTON_Y, 10, 70);
            home_manager.cursor->put_down_pokemon(env, context);
            // Swap the pokemon pointers and row, col, box
            std::swap(*pokemon1, *pokemon2);
            std::swap(pokemon1->current_box, pokemon2->current_box);
            std::swap(pokemon1->current_col, pokemon2->current_col);
            std::swap(pokemon1->current_row, pokemon2->current_row);
        }else{ // Same box, calculate movement distances (just go to slot 1 for now)
            // Go to pokemon 1
            home_manager.navigate_to(env, context, slot1, box1.box_num);
            // Pick it up
            home_pick_up_pokemon(env, context, home_manager);
            // go to pokemon 2
            home_manager.navigate_to(env, context, slot2, box2.box_num);
            // Press y
            pbf_press_button(context, BUTTON_Y, 20, 15);
            home_manager.cursor->put_down_pokemon(env, context);
            // Swap the pokemon pointers and row, col, box
            std::swap(*pokemon2, *pokemon1);
            std::swap(pokemon2->current_box, pokemon1->current_box);
            std::swap(pokemon2->current_col, pokemon1->current_col);
            std::swap(pokemon2->current_row, pokemon1->current_row);
        }
    }



}

void sv_get_evo_items(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    std::unordered_map<std::string, int> item_counts = {
        {"auspicious-armor",0},
        {"berry-sweet",0},
        {"chipped-pot",0},
        {"clover-sweet",0},
        {"cracked-pot",0},
        {"dawn-stone",0},
        {"dragon-scale",0},
        {"dubious-disc",0},
        {"dusk-stone",0},
        {"electirizer",0},
        {"fire-stone",0},
        {"flower-sweet",0},
        {"galarica-cuff",0},
        {"galarica-wreath",0},
        {"ice-stone",0},
        {"kings-rock",0},
        {"leaders-crest",0},
        {"leaf-stone",0},
        {"love-sweet",0},
        {"magmarizer",0},
        {"malicious-armor",0},
        {"masterpiece-teacup",0},
        {"metal-alloy",0},
        {"metal-coat",0},
        {"moon-stone",0},
        {"oval-stone",0},
        {"prism-scale",0},
        {"protector",0},
        {"razor-claw",0},
        {"razor-fang",0},
        {"reaper-cloth",0},
        {"ribbon-sweet",0},
        {"scroll-of-darkness",0},
        {"scroll-of-waters",0},
        {"shiny-stone",0},
        {"star-sweet",0},
        {"strawberry-sweet",0},
        {"sun-stone",0},
        {"sweet-apple",0},
        {"syrupy-apple",0},
        {"tart-apple",0},
        {"thunderstone",0},
        {"unremarkable-teacup",0},
        {"upgrade",0},
        {"water-stone",0}
    };

    PokemonSV::enter_menu_from_overworld(env.program_info(), env.console, context, -1);

    PokemonSV::enter_bag_from_menu(env.program_info(), env.console, context);

    ImageFloatBox other_icon(0.485, 0.11, 0.005, 0.005);
    while(euclidean_distance(image_stats(extract_box_reference(env.console.video().snapshot(), other_icon)).average, FloatPixel(255, 215.5,0))>5){
        pbf_press_dpad(context, DPAD_RIGHT, 10, 16);
        context.wait_for_all_requests();
    }

    auto set_item_count = [&](std::unordered_map<std::string, int>& items, const std::string& key, int value) -> void{
        auto it = items.find(key);
        if (it != items.end()) {
            it->second = value;
        }
    };

    auto get_item_row_y = [&](VideoSnapshot& screen) -> float {
        PokemonSV::GradientArrowDetector arrow_detector(COLOR_RED, PokemonSV::GradientArrowType::RIGHT, {0.1, 0.17, 0.05, 0.75});
        ImageFloatBox item_box;

        arrow_detector.detect(item_box, screen);

        return item_box.y;
    };

    auto get_selected_bag_quantity = [&](VideoSnapshot& screen, float row_y = -1.0) -> int{
        ImageFloatBox bag_quant(0.49, row_y==-1.0?get_item_row_y(screen):row_y, 0.045, 0.069);

        return OCR::read_number_waterfill(env.logger(),extract_box_reference(screen, bag_quant),0xff000000, 0xff404050, true);
    };

    auto get_selected_bag_item = [&](VideoSnapshot& screen) -> std::pair<std::string, int> {
        float row_y = get_item_row_y(screen);

        ImageFloatBox bag_text(0.175, row_y, 0.2, 0.069);

        auto result = SVItemReader::instance().read_substring(
            env.console, Language::English, extract_box_reference(screen, bag_text),
            OCR::BLACK_TEXT_FILTERS()
            );

        return {result.results.cbegin()->second.token,get_selected_bag_quantity(screen, row_y)};
    };

    ImageFloatBox first_selected(0.46, 0.175, 0.001, 0.002);
    ImageFloatBox item_large(0.595, 0.5, 0.31, 0.051);
    bool failed_match = false;
    std::pair<std::string, int> item;

    VideoSnapshot screen = env.console.video().snapshot();

    do{
        try{
            item = get_selected_bag_item(screen);
            env.console.log(item.first+" "+std::to_string(item.second));

            // Double check we are on the right row, if not, don't move.
            failed_match = item.first != SVItemReader::instance().read_substring(env.console, Language::English, extract_box_reference(screen, item_large),OCR::WHITE_TEXT_FILTERS()).results.cbegin()->second.token;
        }catch(...){
            failed_match = true;
        }

        if(!failed_match){
            pbf_press_button(context, BUTTON_DOWN, 16, 35);
            set_item_count(item_counts, item.first, item.second);
        }else{
            pbf_wait(context, 20ms);
        }

        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
    }while(failed_match || euclidean_distance(image_stats(extract_box_reference(screen, first_selected)).average, FloatPixel(255, 215.5,0))>5);
}

Pokemon home_request_next_simple_item_evo(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, Game& game){
    return Pokemon();
}

Pokemon home_request_next_level_evo(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, Game& game){
    ImageFloatBox home_filter_reader(0.4, 0.41, 0.2, 0.045);
    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.049, 0.04); //pokemon national dex number pos
    ImageFloatBox nature_box(0.157, 0.783, 0.212, 0.042); // Nature box
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046); // OT ID box
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041); // Level box

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };


    VideoSnapshot screen;

    VideoOverlaySet box_render(env.console);

    home_manager.navigate_menus_to(env, context, PageID::LIST_VIEW, game.game);

    pbf_press_button(context, BUTTON_Y, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 60);
    pbf_press_button(context, BUTTON_X, 10, 60);
    home_manager.scroll_filter_menu(env, context, "labels");
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_DOWN, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "compatible-games");

    size_t temp = game.index;
    env.console.log(std::to_string(temp));
    do{
        pbf_press_button(context, BUTTON_DOWN, 10, 60);
        env.console.log(std::to_string(temp));
    }while(--temp==0);
    pbf_press_button(context, BUTTON_A, 10, 60);
    home_manager.scroll_filter_menu(env, context, "markings");
    pbf_press_button(context, BUTTON_A, 10, 60);
    pbf_press_button(context, BUTTON_UP, 10, 60);
    pbf_press_button(context, BUTTON_A, 10, 60);
    pbf_press_button(context, BUTTON_B, 10, 60);

    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
    screen = env.console.video().snapshot();
    std::string filter_text = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, home_filter_reader)));
    env.console.log(filter_text);

    if(filter_text=="No matches found!")return Pokemon();

    std::vector<std::pair<int,std::vector<double>>> level_list = {{1,{}},{2,{}},{4,{}},{5,{}},{7,{}},{8,{}},{10,{}},{11,{}},{13,{}},{14,{}},{16,{}},{17,{}},{19,{0,0.01}},{21,{}},{23,{}},{27,{0}},{29,{}},{32,{}},{41,{}},{43,{}},{46,{}},{48,{}},{50,{0,.01}},{52,{0,0.02}},{54,{}},{56,{}},{60,{}},{63,{}},{66,{}},{69,{}},{72,{}},{74,{0,0.01}},{77,{0,0.01}},{79,{0}},{81,{}},{84,{}},{86,{}},{88,{0,0.01}},{92,{}},{96,{}},{98,{}},{100,{0}},{104,{}},{109,{}},{111,{}},{116,{}},{118,{}},{122,{0.01}},{129,{}},{138,{}},{140,{}},{147,{}},{148,{}},{152,{}},{153,{}},{155,{}},{156,{}},{158,{}},{159,{}},{161,{}},{163,{}},{165,{}},{167,{}},{170,{}},{177,{}},{179,{}},{180,{}},{183,{}},{187,{}},{188,{}},{194,{0,0.01}},{204,{}},{209,{}},{216,{}},{218,{}},{220,{}},{222,{0.01}},{223,{}},{228,{}},{231,{}},{236,{}},{238,{}},{239,{}},{240,{}},{246,{}},{247,{}},{252,{}},{253,{}},{255,{}},{256,{}},{258,{}},{259,{}},{261,{}},{263,{0,0.01}},{264,{0.01}},{265,{}},{266,{}},{268,{}},{270,{}},{273,{}},{276,{}},{278,{}},{280,{}},{281,{}},{283,{}},{285,{}},{287,{}},{288,{}},{290,{}},{293,{}},{294,{}},{296,{}},{304,{}},{305,{}},{307,{}},{309,{}},{316,{}},{318,{}},{320,{}},{322,{}},{325,{}},{328,{}},{329,{}},{331,{}},{333,{}},{339,{}},{341,{}},{343,{}},{345,{}},{347,{}},{353,{}},{355,{}},{360,{}},{361,{}},{363,{}},{364,{}},{371,{}},{372,{}},{374,{}},{375,{}},{387,{}},{388,{}},{390,{}},{391,{}},{393,{}},{394,{}},{396,{}},{397,{}},{399,{}},{401,{}},{403,{}},{404,{}},{408,{}},{410,{}},{412,{0.0001,0.0002,0.0003}},{415,{}},{418,{}},{420,{}},{423,{0.0001,0.0002}},{425,{}},{431,{}},{434,{}},{436,{}},{443,{}},{444,{}},{449,{}},{451,{}},{453,{}},{456,{}},{459,{}},{495,{}},{496,{}},{498,{}},{499,{}},{501,{}},{502,{}},{504,{}},{506,{}},{507,{}},{509,{}},{519,{}},{520,{}},{522,{}},{524,{}},{529,{}},{532,{}},{535,{}},{536,{}},{540,{}},{543,{}},{544,{}},{551,{}},{552,{}},{554,{}},{557,{}},{559,{}},{562,{}},{564,{}},{566,{}},{568,{}},{570,{0,0.01}},{574,{}},{575,{}},{577,{}},{578,{}},{580,{}},{582,{}},{583,{}},{585,{0.0001,0.0002,0.0003,0.0004}},{590,{}},{592,{}},{595,{}},{597,{}},{599,{}},{600,{}},{602,{}},{605,{}},{607,{}},{610,{}},{611,{}},{613,{}},{619,{}},{622,{}},{624,{}},{627,{}},{629,{}},{633,{}},{634,{}},{636,{}},{650,{}},{651,{}},{653,{}},{654,{}},{656,{}},{657,{}},{659,{}},{661,{}},{662,{}},{664,{}},{665,{}},{667,{}},{669,{}},{672,{}},{674,{}},{677,{}},{679,{}},{686,{}},{688,{}},{690,{}},{692,{}},{696,{}},{698,{}},{704,{}},{705,{}},{712,{}},{714,{}},{722,{}},{723,{}},{725,{}},{726,{}},{728,{}},{729,{}},{731,{}},{732,{}},{734,{}},{736,{}},{742,{}},{744,{}},{747,{}},{749,{}},{751,{}},{753,{}},{755,{}},{757,{}},{759,{}},{761,{}},{767,{}},{769,{}},{782,{}},{783,{}},{789,{}},{790,{}},{810,{}},{811,{}},{813,{}},{814,{}},{816,{}},{817,{}},{819,{}},{821,{}},{822,{}},{824,{}},{825,{}},{827,{}},{829,{}},{831,{}},{833,{}},{835,{}},{837,{}},{838,{}},{843,{}},{846,{}},{848,{}},{850,{}},{856,{}},{857,{}},{859,{}},{860,{}},{878,{}},{885,{}},{886,{}},{906,{}},{907,{}},{909,{}},{910,{}},{912,{}},{913,{}},{915,{}},{917,{}},{919,{}},{921,{}},{924,{}},{926,{}},{928,{}},{929,{}},{932,{}},{933,{}},{940,{}},{942,{}},{944,{}},{948,{}},{955,{}},{957,{}},{958,{}},{960,{}},{963,{}},{965,{}},{969,{}},{971,{}},{996,{}},{997,{}}};

    std::vector<std::pair<int,StatsHuntGenderFilter>> genders_list = {{415, StatsHuntGenderFilter::Female},{757, StatsHuntGenderFilter::Female}};

    std::vector<std::pair<int,std::vector<std::string>>> moves_list = {{108,{"Rollout"}},{190,{"Double Hit"}},{193,{"Ancient Power"}},{203,{"Twin Beam"}},{206,{"Hyper Drill"}},{221,{"Ancient Power"}},{438,{"Mimic"}},{439,{"Mimic"}},{762,{"Stomp"}},{852,{"Taunt"}},{1011,{"Dragon Cheer"}}};

    pbf_press_button(context, BUTTON_A, 10, 80);
    pbf_press_button(context, BUTTON_DOWN, 10, 80);
    pbf_press_button(context, BUTTON_A, 10, 200); // Navigate into first pokemon to start finding something that evolves by leveling up

    Pokemon first_pokemon = home_read_pokemon_summary(env, context, 0, 0, 0);

    Pokemon curr_pokemon = first_pokemon;


    auto matches_pokemon = [&](const Pokemon& poke) -> bool {
        for (const auto& id_level : level_list) {
            bool level_match = id_level.first==curr_pokemon.national_dex_number && (id_level.second.empty()||find(id_level.second.begin(),id_level.second.end(),curr_pokemon.form_id)!=id_level.second.end());
            if (!level_match) continue;

            for (const auto& id_gender : genders_list) {
                if (id_gender.first == poke.national_dex_number && id_gender.second != poke.gender) {
                    return false;
                }
            }

            return true; // Was found in level match, but was not in genders_list. Can evolve by leveling up.
        }
        return false;
    };


    do {
        env.console.log("Running search");
        if (matches_pokemon(curr_pokemon)) {
            pbf_press_button(context, BUTTON_B, 10, 270);
            break;  // Found the first pokemon that can evolve by level up, break out of the do-while loop.
        }

        pbf_press_button(context, BUTTON_R, 10, 80);
        context.wait_for_all_requests();
        screen = env.console.video().snapshot();
        curr_pokemon = home_read_pokemon_summary(env, context, 0, 0, 0);
    } while (curr_pokemon.national_dex_number != first_pokemon.national_dex_number
             || curr_pokemon.level != first_pokemon.level
             || curr_pokemon.ot_id != first_pokemon.ot_id);

    if (!matches_pokemon(curr_pokemon)) {
        return Pokemon();
    }

    pbf_press_button(context, BUTTON_A, 10, 40);
    pbf_press_button(context, BUTTON_A, 10, 40);

    HomeBoxViewWatcher boxWatcher(COLOR_BLUE);


    context.wait_for_all_requests();

    int ret = wait_until(
        env.console, context, 5000ms, {
            boxWatcher
        });




    while(ret!=0){
        pbf_press_button(context, BUTTON_A, 10, 40);

        context.wait_for_all_requests();

        ret = wait_until(
            env.console, context, 2000ms, {
                boxWatcher
            });
    }

    home_manager.detect_home(env, context, true);

    return curr_pokemon;

}

// This function is run in Pokémon Home, and should be used when wanting to transfer a Pokémon from the Home box into the Game box on the right.
// Requires env, context, and two coordinates sent as an std::pair<size_t,size_t>
void home_move_pokemon_to_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, std::pair<size_t,size_t> game){
    home_pick_up_pokemon(env, context, home_manager);

    home_manager.navigate_to(env, context, game);

    home_manager.cursor->put_down_pokemon(env, context);

    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_UP, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);

    context.wait_for_all_requests();

}

// This function is run in Pokémon Home, and should be used when wanting to transfer a Pokémon to the Home box from the Game box on the right.
// Assumes the cursor is in Home position {0,5}
// Requires env, context, and two coordinates sent as an std::pair<size_t,size_t>
void home_move_pokemon_from_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, std::pair<size_t,size_t> home, std::pair<size_t,size_t> game, bool emergency = false){
    home_manager.navigate_to(env, context, game);


    home_pick_up_pokemon(env, context, home_manager);

    home_manager.navigate_to(env, context, home);

    home_manager.cursor->put_down_pokemon(env, context);

    pbf_press_button(context, BUTTON_Y, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_DOWN, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);
    if(emergency)pbf_press_button(context, BUTTON_A, 10, 50);
    pbf_press_button(context, BUTTON_UP, 10, 50);
    pbf_press_button(context, BUTTON_A, 10, 50);

    context.wait_for_all_requests();

}


Pokemon home_request_next_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, Game& game, mode mode){
    switch(mode){
    case mode::Level:
        return home_request_next_level_evo(env, context, home_manager, game);
    case mode::Simple_Item:
        return home_request_next_simple_item_evo(env, context, home_manager, game);
    default:
        return Pokemon();
    }
}


// This function is run in Pokémon Home, and should be used when wanting to locate all Pokémon eligible for powering up
// Requires env, context, and a game name to identify what game to navigate to
// Returns the amount of Pokémon successfully retrieved
int home_fill_boxes_to_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, Game& game, std::string box_name, mode mode){


    pbf_press_button(context, BUTTON_LEFT, 10, 30);
    pbf_press_button(context, BUTTON_DOWN, 10, 30);
    home_navigate_to_box_secondary(env, context, box_name);
    int found = 0;
    for(int i =0; i<6; i++){
        for(int j = 0; j < 5; j++){
            Pokemon temp_mon = home_request_next_pokemon(env, context, home_manager, game, mode);
            if(temp_mon==Pokemon()){
                pbf_press_button(context, BUTTON_B, 10, 150);
                home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
                return found;
            }
            pbf_wait(context, 500ms);
            context.wait_for_all_requests();

            // env.console.log(std::to_string(home_pos.first)+" "+std::to_string(home_pos.second));
            // env.console.log(std::to_string(i)+" "+std::to_string(j));
            home_move_pokemon_to_game(env, context, home_manager, {j,i+6});
            found++;

            if(temp_mon.national_dex_number==290){j++;} // Special case to allow for Shedinja. Unlucky if already on bottom row.
        }
    }

    home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
    return found;
}

PokemonBox home_build_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, size_t box_num = 0) {
    pbf_wait(context, 250ms);
    context.wait_for_all_requests();

    VideoOverlaySet box_render(env.console);
    VideoSnapshot screen = env.console.video().snapshot();

    PokemonBox tempbox;

    std::vector<std::pair<size_t, size_t>> blank_list;
    std::pair<size_t, size_t> first_poke_slot = {0, 0};
    bool find_first_poke = false;
    size_t pokemon_count = 0;

    tempbox.box_num = box_num;
    tempbox.pokemon_count = pokemon_count; // Used this to silence a few lines before

    home_manager.navigate_to(env, context, {0, 0, box_num});

    FloatPixel pixel_data[5][6] = {};  // Initialize to default values

    // retake a screenshot now that cursor is at (0,0) and is not interfering with coloration
    context.wait_for_all_requests();
    screen = env.console.video().snapshot();

    // Scan the screen for Pokémon presence and colors
    for (size_t row = 0; row < 5; row++) {
        for (size_t column = 0; column < 6; column++) {
            ImageFloatBox slot_box(0.06 + (0.072 * column), 0.2 + (0.1035 * row), 0.03, 0.057);
            ImageFloatBox slot_box2(0.059400 + (0.071861 * column), 0.198700 + (0.105544 * row), 0.03, 0.057);
            int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();

            if (current_box_value < 5) {
                box_render.add(COLOR_RED, slot_box);
            } else {
                pixel_data[row][column] = image_stats(extract_box_reference(screen, slot_box2)).average;
                box_render.add(COLOR_GREEN, slot_box);
                pokemon_count++;
                if (!find_first_poke) {
                    first_poke_slot = {row, column};
                    find_first_poke = true;
                }
            }
        }
    }


    if (!find_first_poke) {
        tempbox.update_stats();
        return tempbox;  // Return empty box if no Pokémon are found
    }else if(first_poke_slot!=std::pair<size_t, size_t>{0, 0}){
        home_manager.navigate_to(env, context, first_poke_slot);
    }

    box_render.clear();

    home_manager.navigate_menus_to(env, context, PageID::SUMMARY_VIEW);

    // env.console.log("Pokemon in box: " + std::to_string(pokemon_count));

    for(size_t i = 0; i < 30; i++){
        size_t row = i / 6;
        size_t col = i % 6;

        if(euclidean_distance(pixel_data[row][col],FloatPixel(0,0,0))>0){
            context.wait_for_all_requests();
            screen = env.console.video().snapshot();
            std::optional<Pokemon> temp_pokemon(home_read_pokemon_summary(env, context, box_num, row, col));

            if (temp_pokemon) {
                // env.console.log("Adding quick color to {" + std::to_string(row) + ", " + std::to_string(col) + "}.");
                temp_pokemon->quick_color = pixel_data[row][col];
                tempbox.add_pokemon(temp_pokemon, row, col);
            } else {
                env.console.log("Failed to read Pokémon at {" + std::to_string(row) + ", " + std::to_string(col) + "}.");
            }

            pbf_press_button(context, BUTTON_R, 10, 80);  // Navigate to next Pokémon

            context.wait_for_all_requests();
        }
    }


    tempbox.update_stats();
    home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW);
    // tempbox.print_box(&env, true);

    return tempbox;
}

bool home_reconcile_spaces(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, BoxLayout& boxes, size_t box_num, bool recursed = false){
    home_manager.navigate_to(env, context, {0,0});

    pbf_wait(context, 125ms);

    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();

    size_t mismatches = 0;

    bool succeeded = true;

    PokemonBox& box = boxes.get_box(box_num);

    for (size_t row = 0; row < 5; row++){
        for (size_t column = 0; column < 6; column++){
            ImageFloatBox slot_box(0.06 + (0.072 * column), 0.2 + (0.1035 * row), 0.03, 0.057);
            ImageFloatBox slot_box2(0.059400 + (0.071861 * column), 0.1987 + (0.105544 * row), 0.03, 0.057);
            int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();
            FloatPixel color_value = image_stats(extract_box_reference(screen, slot_box2)).average;

            if(!box.grid[row][column]){ // blank pokemon space
                if(current_box_value>=5){
                    env.console.log("Box " + std::to_string(box.box_num)+" was not reconciled");
                    succeeded = succeeded && false;
                    mismatches++;
                }
            }else{
                double euc_dist = euclidean_distance(box.grid[row][column]->quick_color,color_value);
                if(euc_dist>=6.5f){
                    env.console.log("Box " + std::to_string(box.box_num)+" was not reconciled at {" + std::to_string(row) + ", " + std::to_string(column) + "}. Euclidian distance was "+std::to_string(euclidean_distance(box.grid[row][column]->quick_color,color_value)));
                    succeeded = succeeded && false;
                    mismatches++;
                }
            }
        }
    }
    if(succeeded)env.console.log("Box " + std::to_string(box.box_num)+" successfully reconciled");


    // If it didn't succeed, just double check that we aren't at the wrong box.
    if(!succeeded && mismatches >= 15 && ! recursed){
        env.console.log("Box " + std::to_string(box.box_num)+" was not reconciled. "+ std::to_string(mismatches)+" mismatches, triggering adjacency audit.");
        bool left_one = boxes.box_exists(box_num-1)?home_reconcile_spaces(env, context, home_manager, boxes, box_num-1, true):false;
        bool right_one = boxes.box_exists(box_num+1)?home_reconcile_spaces(env, context, home_manager, boxes, box_num+1, true):false;

        // If we somehow matched with the wrong box,
        if(left_one||right_one){
            env.console.log("Adjacency audit succeeded. Resetting Home Manager state.");
            home_manager.bail_out(env, context);
            home_manager.navigate_to(env, context, {0, 0, box_num});
            return home_reconcile_spaces(env, context, home_manager, boxes, box_num, false);
        }else{
            env.console.log("Adjacency audit failed. Continuing.");
        }

    }
    return succeeded;
}

PokemonBox home_load_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, size_t box_num){
    env.console.log("running home_load_box on box "+std::to_string(box_num));

    home_manager.navigate_to(env, context, {0, 0, box_num});

    context.wait_for_all_requests();

    JsonValue json_value;
    PokemonBox box;

    box.box_num = box_num;
    try {

        json_value = load_json_file("Home Storage\\" + std::to_string(box_num) +".json");

        QFile file(QString::fromStdString("Home Storage\\" + std::to_string(box_num) +".json"));
        file.close();

        box.parse_pokemon_box(json_value);

        box.output_boxes_data_json();

        // if(!home_reconcile_spaces(env, context, home_manager, box))throw std::runtime_error("Space reconciliation failed for box " + std::to_string(box_num));
    } catch (...) {
        env.log("Failed to load JSON file", COLOR_RED);
        box = home_build_box(env, context, home_manager, box_num);
        box.output_boxes_data_json();    }

    box.update_stats();

    return box;
}

PokemonBox* home_load_box(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    PokemonHome_HomeEnvironment& home_manager,
    size_t box_num,
    BoxLayout& boxes
    ){
    env.console.log("running home_load_box on box " + std::to_string(box_num));

    home_manager.navigate_to(env, context, {0, 0, box_num});
    context.wait_for_all_requests();

    try {
        if (boxes.box_exists(box_num)) {
            env.console.log("Using cached box from BoxLayout for box " + std::to_string(box_num));
        }
        else {
            JsonValue json_value = load_json_file("Home Storage\\" + std::to_string(box_num) + ".json");

            PokemonBox new_box;
            new_box.box_num = box_num;
            new_box.parse_pokemon_box(json_value);
            new_box.output_boxes_data_json();

            boxes.add_box(box_num, new_box);
        }

        // Reconcile spaces
        if (!home_reconcile_spaces(env, context, home_manager, boxes, box_num)) {
            env.log("Space reconciliation failed; rebuilding box.", COLOR_RED);
            PokemonBox new_box = home_build_box(env, context, home_manager, box_num);
            new_box.output_boxes_data_json();
            boxes.add_box(box_num, new_box);
        }
    }
    catch (...) {
        env.log("Failed to load JSON file or parse box", COLOR_RED);
        PokemonBox new_box = home_build_box(env, context, home_manager, box_num);
        new_box.output_boxes_data_json();
        boxes.add_box(box_num, new_box);
    }

    // Always return a pointer to the stored box
    PokemonBox& box = boxes.get_box(box_num);
    box.update_stats();
    return &box;
}



bool home_sort_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, PokemonBox& box) {
    bool touched = false;
    size_t rows = MAX_ROWS;
    size_t cols = MAX_COLUMNS;

    bool reverse = false;


    // Perform a selection sort on the grid
    for (size_t i = 0; i < rows * cols - 1; ++i) {
        size_t min_idx = i;
        size_t min_row = i / cols;
        size_t min_col = i % cols;

        // Find the "smallest" Pokémon (by national dex, level, etc.)
        for (size_t j = i; j < rows * cols; ++j) {
            size_t row = j / cols;
            size_t col = j % cols;

            // Comparison logic:
            if (!box.grid[min_row][min_col] ||
                (box.grid[row][col] &&
                 (!box.grid[min_row][min_col] || *box.grid[row][col] < *box.grid[min_row][min_col]))) {
                min_idx = j;
                min_row = row;
                min_col = col;
            }

        }

        // If the min_idx is different from the current index, swap them
        if (min_idx != i) {

            while (min_idx + 1 < 30) {
                size_t next_idx = min_idx + 1;
                size_t cur_row = min_idx / cols;
                size_t cur_col = min_idx % cols;
                size_t next_row = next_idx / cols;
                size_t next_col = next_idx % cols;

                // Compare values for equality
                if (box.grid[cur_row][cur_col] && box.grid[next_row][next_col] &&
                    *box.grid[cur_row][cur_col] == *box.grid[next_row][next_col]) {
                    min_idx = next_idx;
                    min_row = next_row;
                    min_col = next_col;
                } else {
                    break;
                }
            }

            touched = true;
            size_t swap_row = i / cols;
            size_t swap_col = i % cols;



            // Define slots for swapping
            std::pair<size_t, size_t> slot1 = {swap_row, swap_col};
            std::pair<size_t, size_t> slot2 = {min_row, min_col};

            if(!box.grid[slot1.first][slot1.second] && !box.grid[slot2.first][slot2.second])continue;


            // set up easy swapping, given that there are no blank spaces
            if(reverse && !box.grid[slot2.first][slot2.second]){
                reverse = false;
            }else if (!reverse && !box.grid[slot1.first][slot1.second]){
                reverse = true;

            }

            // For time saving, if the swap spot is next, don't reverse (don't trigger if current spot is blank, uncommon)
            if(reverse && i + 1 == min_idx && box.grid[slot1.first][slot1.second].has_value()){
                reverse = false;
            }

            // I think this is where the consecutive blanks logic goes if at all (in an else block, separating out the nullptr check?)

            // Call home_swap_pokemon using last_position if available
            if (reverse) {
                home_swap_pokemon(env, context, home_manager, slot2, slot1);
            } else {
                home_swap_pokemon(env, context, home_manager, slot1, slot2);
            }

            reverse = !reverse;


            // Swap in the box
            box.swap_pokemon(swap_row, swap_col, min_row, min_col);
        }
    }

    box.update_stats();

    context.wait_for_all_requests();



    return touched;
}

bool home_make_easy_swaps(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, BoxLayout& boxes, size_t left_num, size_t right_num){

    env.console.log("Left box " +std::to_string(left_num));
    env.console.log("Right box " +std::to_string(right_num));

    if(boxes.get_box(left_num).blanks==30){
        pbf_press_button(context, BUTTON_R, 10, 35);
        home_manager.navigate_to(env, context, {0,0});
        pbf_press_button(context, BUTTON_ZR, 10, 35);
        pbf_press_button(context, BUTTON_A, 10, 35);
        pbf_press_button(context, BUTTON_DOWN, 10, 35);
        pbf_press_button(context, BUTTON_DOWN, 10, 35);
        pbf_press_button(context, BUTTON_DOWN, 10, 35);
        pbf_press_button(context, BUTTON_DOWN, 10, 35);
        pbf_press_button(context, BUTTON_RIGHT, 10, 35);
        pbf_press_button(context, BUTTON_RIGHT, 10, 35);
        pbf_press_button(context, BUTTON_RIGHT, 10, 35);
        pbf_press_button(context, BUTTON_RIGHT, 10, 35);
        pbf_press_button(context, BUTTON_RIGHT, 10, 35);
        pbf_press_button(context, BUTTON_A, 10, 35);
        pbf_press_button(context, BUTTON_L, 10, 35);
        pbf_press_button(context, BUTTON_A, 10, 35);
        pbf_press_button(context, BUTTON_ZL, 10, 35);
        pbf_press_button(context, BUTTON_R, 10, 35);

        std::swap(boxes.get_box(left_num).grid, boxes.get_box(right_num).grid);

        // Update Pokémon metadata after grid swap
        for (size_t i = 0; i < boxes.get_box(left_num).grid.size(); ++i) {
            for (size_t j = 0; j < boxes.get_box(left_num).grid[i].size(); ++j) {
                if (boxes.get_box(left_num).grid[i][j]) {
                    boxes.get_box(left_num).grid[i][j]->current_box = boxes.get_box(left_num).box_num;
                    boxes.get_box(left_num).grid[i][j]->current_row = i;
                    boxes.get_box(left_num).grid[i][j]->current_col = j;
                }
                if (boxes.get_box(right_num).grid[i][j]) {
                    boxes.get_box(right_num).grid[i][j]->current_box = boxes.get_box(right_num).box_num;
                    boxes.get_box(right_num).grid[i][j]->current_row = i;
                    boxes.get_box(right_num).grid[i][j]->current_col = j;
                }
            }
        }

        boxes.get_box(left_num).update_stats();
        boxes.get_box(right_num).update_stats();

        return true;
    }

    size_t runs = 0;
    for(; runs < 30; runs++){

        std::optional<Pokemon> highestLeft;
        size_t left_row = 0, left_col = 0;
        bool found_blank_on_left = false;

        std::optional<Pokemon> lowestRight;
        size_t right_row = 0, right_col = 0;

        for (size_t i = 0; i < 30; i++) {
            auto& leftPokemon = boxes.get_box(left_num).grid[i / 6][i % 6];
            if (!leftPokemon.has_value()) {
                // Keep track that we found a blank
                if(!found_blank_on_left){
                    highestLeft = leftPokemon;
                    left_row = i / 6;
                    left_col = i % 6;
                    found_blank_on_left = true;
                }
            } else if (!found_blank_on_left) {
                // Only update highestLeft if we haven't found any blank so far
                if (!highestLeft.has_value() || highestLeft.value() < leftPokemon.value()) {
                    highestLeft = leftPokemon;
                    left_row = i / 6;
                    left_col = i % 6;
                }
            }

            auto& rightPokemon = boxes.get_box(right_num).grid[i / 6][i % 6];
            if (rightPokemon.has_value()) {
                if (!lowestRight.has_value() || rightPokemon.value() < lowestRight.value()) {
                    lowestRight = rightPokemon;
                    right_row = i / 6;
                    right_col = i % 6;
                }
            }
        }

        // if(highestLeft.has_value())highestLeft->log_details(&env, true);
        // if(lowestRight.has_value())lowestRight->log_details(&env, true);

        if(!lowestRight.has_value() || (highestLeft.has_value() && highestLeft.value()<=lowestRight.value()))break;

        home_swap_pokemon(env, context, home_manager, {left_row, left_col}, boxes.get_box(left_num), {right_row, right_col}, boxes.get_box(right_num));

        boxes.get_box(left_num).update_stats();
        boxes.get_box(right_num).update_stats();


    }

    if(runs>0){
        boxes.get_box(left_num).output_boxes_data_json();
        boxes.get_box(right_num).output_boxes_data_json();
        return true;
    }else{
        return false;
    }
}

bool home_read_main_menu(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string target){
    ImageFloatBox menu_box(0.055, 0.015, 0.25, 0.06); // Header box

    VideoSnapshot screen = env.console.video().snapshot();

    VideoOverlaySet box_render(env.console);

    std::ostringstream ss;

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };

    context.wait_for_all_requests();
    box_render.add(COLOR_GREEN, menu_box);
    std::string menu_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), menu_box)));
    env.console.log(menu_name);
    while(menu_name==""){
        env.console.log("menu_name==\"\"");
        pbf_wait(context,500ms);
        context.wait_for_all_requests();
        box_render.add(COLOR_GREEN, menu_box);
        menu_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), menu_box)));
        env.console.log(menu_name);
    }
    box_render.clear();
    env.console.log(menu_name);
    env.console.log(std::to_string(menu_name==target));
    return menu_name==target;
}

bool home_read_filter_submenu(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string target){
    ImageFloatBox filter_menu(0.65, 0.095, 0.295, 0.06); // Header box
    ImageFloatBox filter_menu_markings(0.65, 0.325, 0.295, 0.06); // Header box

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };

    VideoSnapshot screen = env.console.video().snapshot();

    VideoOverlaySet box_render(env.console);

    std::ostringstream ss;

    context.wait_for_all_requests();
    box_render.add(COLOR_GREEN, filter_menu);
    std::string submenu_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), filter_menu)));
    size_t count = 0;
    while(submenu_name!=target){

        // This next block checks for the only menu that does not extend to the top of the screen, the markings menu. Sometimes the options can be greyed out,
        // which results in weird behavior. This next block only allows the menu to back out if a menu has been selected successfully.
        box_render.add(COLOR_GREEN, filter_menu_markings);
        context.wait_for_all_requests();
        std::string markings = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), filter_menu_markings)));
        if(markings=="Search by markings"){
            if(target=="Search by markings"){
                return true;
            }else{
                pbf_press_button(context, BUTTON_B, 10, 50);
            }
        }

        // This block also checks for unexpected behavior, might be unnecessary with the addition of the above block
        if(count==5){
            context.wait_for_all_requests();
            submenu_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), filter_menu)));
            if(submenu_name!="Filters"){
                pbf_press_button(context, BUTTON_X,10, 50);
            }
        }


        pbf_press_button(context, BUTTON_DOWN, 10, 50);
        pbf_press_button(context, BUTTON_A, 10, 50);
        context.wait_for_all_requests();
        submenu_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), filter_menu)));
        count++;
    }
    box_render.clear();
    return true;
}

void sv_run_ace(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ImageFloatBox sv_battle_button(0.82, 0.655, 0.1, 0.055);
    ImageFloatBox sv_keep_current_pokemon(0.645, 0.615, 0.2, 0.055);
    ImageFloatBox sv_move_1_pp(0.91, 0.615, 0.03, 0.0355);
    ImageFloatBox sv_b_back(0.95, 0.95, 0.04, 0.035);
    ImageFloatBox evolve_message(0.28, 0.76, 0.065, 0.055);
    ImageFloatBox evolve_message2(0.28, 0.76, 0.15, 0.055);
    ImageFloatBox evolve_message3(0.28, 0.76, 0.45, 0.115);
    ImageFloatBox white_dialog_checker(0.853, 0.825, 0.019, 0.05);
    ImageFloatBox fainted_checker(0.19, 0.22, 0.051, 0.03);

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };

    VideoOverlaySet box_render(env.console);
    VideoSnapshot screen = env.console.video().snapshot();
    PokemonSV::DialogBoxDetector dialog_detector;
    PokemonSV::DialogArrowDetector arrow_detector(Color(255,255,255), white_dialog_checker);

    int no_response_check = 0;
    int failsafe_checks = 0;
    bool secondary_move = false;
    while(true){
        screen = env.console.video().snapshot();
        box_render.add(COLOR_BLUE, sv_battle_button);
        std::string text1 = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, sv_battle_button)));
        env.console.log(text1);
        box_render.add(COLOR_BLUE, sv_keep_current_pokemon);
        std::string text2 = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, sv_keep_current_pokemon)));
        env.console.log(text2);
        box_render.add(COLOR_BLACK, sv_b_back);
        std::string text3 = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, sv_b_back)));
        env.console.log(text3);
        std::string text4 = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, evolve_message)));
        env.console.log(text4);
        std::string text5 = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, fainted_checker)));
        env.console.log(text5);
        std::string text_box = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, evolve_message3)));
        env.console.log(text_box);
        box_render.add(COLOR_WHITE, white_dialog_checker);
        if(text5 == "FAINTED"||failsafe_checks==5){
            throw BattleFailedException{};
        }
        if(text_box == "| hope to see you in the tournament again soon"){
            return;
        }
        if(text4 == "What?"){
            pbf_press_button(context, BUTTON_A, 10, 90);
            std::string text6 = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), evolve_message2)));
            env.console.log(text6);
            while(text6!="Congratulations!" && text6!="What? You gotta"){
                pbf_wait(context, 500ms);
                text6 = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), evolve_message2)));
                env.console.log(text6);
            }
            // ___ wants to learn the move ___
            pbf_press_button(context, BUTTON_A, 10, 90);
            std::string evo_text;
            do{
                context.wait_for_all_requests();
                box_render.add(COLOR_GREEN, evolve_message3);
                screen = env.console.video().snapshot();
                evo_text = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, evolve_message3)));
                env.console.log(evo_text.c_str() + evo_text.find(' ') + 1);
                if(std::strncmp(evo_text.c_str() + evo_text.find(' ') + 1, "wants to learn the move", 23) == 0 || std::strncmp(evo_text.c_str() + evo_text.find(' ') + 1, "learned", 7) == 0){
                    pbf_press_button(context, BUTTON_B, 10, 90);
                }
                pbf_wait(context, 1000ms);
            }while((std::strncmp(evo_text.c_str() + evo_text.find(' ') + 1, "wants to learn the move", 23) == 0 || std::strncmp(evo_text.c_str() + evo_text.find(' ') + 1, "learned", 7)== 0));

        }else if(text1 == "Battle"){
            pbf_press_button(context, BUTTON_A, 10, 90);
            box_render.add(COLOR_RED, sv_move_1_pp);
            context.wait_for_all_requests();
            screen = env.console.video().snapshot();
            FloatPixel image_value = image_stats(extract_box_reference(screen, sv_move_1_pp)).average;
            env.console.log(std::to_string(image_value.r));
            if(image_value.r>100&&!secondary_move){
                secondary_move=true;
                pbf_press_button(context, BUTTON_DOWN, 10, 50);
            }
            pbf_press_button(context, BUTTON_A, 10, 50);
            pbf_wait(context, 22000ms);
            failsafe_checks=0;
        }else if(text2 == "Keep current "+STRING_POKEMON){
            env.console.log("Keep current");
            pbf_press_button(context, BUTTON_B, 10, 50);
            pbf_wait(context, 8000ms);
            failsafe_checks=0;
        }else if (text3 == "Back"){
            pbf_mash_button(context, BUTTON_B, 600);
        }else if(dialog_detector.detect(screen)){
            pbf_press_button(context, BUTTON_A, 10, 250);
            failsafe_checks=0;
        }else if(arrow_detector.detect(env.console.video().snapshot())){
            pbf_press_button(context, BUTTON_A, 10, 250);
            failsafe_checks=0;
        }else if (no_response_check>25){
            no_response_check = 0;
            failsafe_checks++;
            pbf_press_button(context, BUTTON_B, 10, 250);
        }else{
            no_response_check++;
        }
        context.wait_for_all_requests();
        pbf_wait(context, 600ms);
    }
}

void sv_run_ace2(SingleSwitchProgramEnvironment& env, ProControllerContext& context, double b_probability = 0){
    PokemonSV::AdvanceDialogWatcher evo_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_BLACK);
    PokemonSV::AdvanceDialogWatcher next_battle_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_WHITE);
    PokemonSV::PromptDialogWatcher learn_move_message(COLOR_CYAN);
    PokemonSV::NormalBattleMenuWatcher battle_menu(COLOR_RED);

    ImageFloatBox evolve_message(0.28, 0.76, 0.065, 0.055);

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };

    context.wait_for_all_requests();

    VideoSnapshot screen = env.console.video().snapshot();

    for(int i = 0; i < 4; i++){
        env.console.log("Running battle "+std::to_string(i+1), COLOR_CYAN);

        int ret = -1;
        while(ret==-1){
            ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_press_button(context, BUTTON_B, 20, 40);
                },
                {
                    battle_menu,
                }
            );
        }


        bool terrastalized = false;
        bool win = PokemonSV::run_pokemon(env.console, context, {}, true, terrastalized);

        if(!win){
            throw BattleFailedException{};
        }

        pbf_press_button(context, BUTTON_A, 10, 120);

        // Account for evolutions


        ret = wait_until(
            env.console, context,
            Milliseconds(60*TICKS_PER_SECOND),
            {
                evo_message,
                next_battle_message,
                learn_move_message
            }
        );

        env.console.log("ret is "+std::to_string(ret));

        while(ret!=-1){
            switch(ret){
            case 0:
                screen = env.console.video().snapshot();
                pbf_press_button(context, BUTTON_A, 10, 20);
                if(sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, evolve_message))) == "What?"){
                    float prob_pick = rand();
                    float prob_pick_clamped = prob_pick/RAND_MAX;
                    float prob_manipped = (1-(std::powf(1-(b_probability),1.0f/4.0f)));

                    env.console.log(std::to_string(prob_pick)+" "+std::to_string(prob_pick_clamped)+" "+std::to_string(prob_manipped));
                    if(prob_pick_clamped > prob_manipped){   // TODO: Fix Probability Check
                        pbf_mash_button(context, BUTTON_B, 500ms);
                    }  else{
                        pbf_wait(context, 10000ms);
                    }
                }
            case 1:
                pbf_mash_button(context, BUTTON_B, 500ms);
            case 2:
                pbf_press_button(context, BUTTON_B, 10, 20);
                break;
            default:
                throw;
            }

            context.wait_for_all_requests();

            ret = wait_until(
                env.console, context,
                Milliseconds(60*TICKS_PER_SECOND),
                {
                    evo_message,
                    next_battle_message,
                    learn_move_message
                }
            );
        }
    }
}

void sv_run_enrichment(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string sv_box_name, int max_runs, mode mode){
    PokemonSV::OverworldWatcher overworld(env.console, COLOR_RED);
    WhiteScreenOverWatcher pre_title_screen(COLOR_RED, {0.5, 0, 0.5, 0.3});
    HomeMenuWatcher home_menu(env.console);

    ImageFloatBox sv_box_name_read(0.35, 0.115, 0.2, 0.05);
    ImageFloatBox title_screen_read(0.6, 0.245, 0.25, 0.235);

    VideoOverlaySet box_render(env.console);
    VideoSnapshot screen = env.console.video().snapshot();

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };

    auto nav_to_boxes = [&]() -> std::string {
        // First, check if we are in the overworld
        int ret = wait_until(
            env.console, context,
            Milliseconds(5*TICKS_PER_SECOND),
            {
                overworld
            }
        );      // TODO: Account for if the loading icon is on the screen

        // If not, mash B until we are
        while (ret == -1) {
            ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_mash_button(context, BUTTON_B, 30*TICKS_PER_SECOND);    // Allows for 30 second intervals and failing if there are consecutive issues later
                },
                {
                    overworld,
                }
            );
        }

        // We are in the overworld, nav to boxes
        PokemonSV::enter_box_system_from_overworld(env.program_info(),env.console, context); // Go to boxes to fill party

        context.wait_for_all_requests();

        return sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), sv_box_name_read)));
    };

    auto grab_column = [&]() -> void {
        pbf_press_button(context, BUTTON_MINUS, 10, 50);    // multi-select
        for(int i =0; i < 5; i++){
            pbf_press_button(context, BUTTON_DOWN, 10, 30);
        }
        pbf_press_button(context, BUTTON_A, 10, 50);
    };

    auto nav_to_column = [&](int i) -> void {
        for( ; i < 0; i++){
            pbf_press_button(context, BUTTON_LEFT, 10, 50);
        }
        for( ; i > 0; i--){
            pbf_press_button(context, BUTTON_RIGHT, 10, 50);
        }
    };

    auto move_pokemon_column = [&](int runs, bool to_party) -> void {
        std::string box_name = nav_to_boxes();

        env.console.log(box_name);
        while(box_name!=sv_box_name){   // navigate to correct box
            pbf_press_button(context, BUTTON_L, 10, 80);
            context.wait_for_all_requests();
            box_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), sv_box_name_read)));
            env.console.log(box_name);
        }

        if(to_party){
            nav_to_column(runs);
        }else{
            pbf_press_button(context, BUTTON_DOWN, 10, 50); // Move down 1 for party leader
            nav_to_column(-1);
        }

        grab_column();

        if(to_party){
            pbf_press_button(context, BUTTON_DOWN, 10, 50); // Move down 1 for party leader
            nav_to_column(-1-runs);
        }else{
            pbf_press_button(context, BUTTON_UP, 10, 50);   // Move back up 1
            nav_to_column(runs+1);
        }

        pbf_press_button(context, BUTTON_A, 10, 50);        // Put pokemon down
        pbf_press_button(context, BUTTON_B, 10, 300);       // Close out of boxes
    };

    int ret = wait_until(
        env.console, context,
        Milliseconds(240*TICKS_PER_SECOND),
        {
            pre_title_screen
        }
        );

    if(ret==-1)throw;
    else{
        pbf_mash_button(context, BUTTON_A, 4*TICKS_PER_SECOND);
    }

    // Loaded into game

    for(int runs = 0; runs < max_runs; runs++){

        move_pokemon_column(runs, true);            // Retrieve pokemon for this iteration

        // Close out of menu and start tournament
        pbf_press_button(context, BUTTON_B, 10, 300);
        pbf_mash_button(context, BUTTON_A, 25000ms);

        try{
            switch(mode){
                case mode::Level:
                    sv_run_ace2(env, context, 0.25);        // Run the tournament once
                case mode::Simple_Item:
                    // TODO: Do simple evo item stuff
                default:
                    break;
            }


            move_pokemon_column(runs, false);       // Put away the pokemon from this iteration

            //save
            pbf_press_button(context, BUTTON_R, 10, 300);
            pbf_press_button(context, BUTTON_A, 10, 150);
            pbf_wait(context, 6000ms);
            pbf_press_button(context, BUTTON_B, 10, 300);
        }
        catch(const BattleFailedException& e){
            env.console.log(e.what());
            // go home, reboot, and retry
            ret = -1;
            while(ret!=0){
                pbf_press_button(context, BUTTON_HOME, 10, 240);
                ret = wait_until(
                    env.console, context,
                    Milliseconds(30*TICKS_PER_SECOND),
                    {
                        home_menu
                    }
                    );
            }
            pbf_press_button(context, BUTTON_X, 10, 6*TICKS_PER_SECOND);
            pbf_press_button(context, BUTTON_A, 10, 320);
            pbf_press_button(context, BUTTON_A, 10, 3150);
            pbf_press_button(context, BUTTON_A, 10, 3150);

            runs--;
        }

        catch(...){
            throw;
        }



    }
}

// This function is run in PLA. It is used to scroll left or right boxes to get to a target box
void pla_navigate_to_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, size_t target){

}

void switch_close_game_and_open(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::string game, PokemonHome_HomeEnvironment& home_manager){
    HomeMenuWatcher home_menu(env.console);

    ImageFloatBox switch_game_checker(0, 0.2, 1, 0.06);

    auto sanitize_OCR = [&](std::string str) -> std::string {
        char chars[] = "\n\r—.,";
        for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
        return str;
    };

    VideoOverlaySet box_render(env.console);

    context.wait_for_all_requests();

    int ret = -1;
    while(ret!=0){
        pbf_press_button(context, BUTTON_HOME, 10, 240);
        ret = wait_until(
            env.console, context,
            Milliseconds(30*TICKS_PER_SECOND),
            {
                home_menu
            }
            );
    }    pbf_press_button(context, BUTTON_X, 10, 80);
    pbf_press_button(context, BUTTON_A, 10, 240);

    for(int i = 0; i < 12; i++){
        box_render.add(COLOR_GREEN, switch_game_checker);
        context.wait_for_all_requests();
        std::string game_name = sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(env.console.video().snapshot(), switch_game_checker)));
        if (game_name[0] != 'P'){
            size_t first_space = game_name.find(' ');
            if (first_space != std::string::npos && first_space + 1 < game_name.size()){
                game_name = game_name.substr(first_space + 1);
            }
        }

        env.console.log(game_name);

        if(game_name==game){
            pbf_press_button(context, BUTTON_A, 10, 10);

            if(game=="Pokémon HOME"){

                pbf_wait(context, 30*TICKS_PER_SECOND); // TODO: Replace with actual title screen detection

                home_manager.detect_home(env, context);
            }
            return;
        }else{
            pbf_press_button(context, BUTTON_RIGHT, 10, 30);

        }
    }
    box_render.clear();
    throw;
}

void Enrichment::initialize_home(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, std::vector<Game>& game_list){
    ImageFloatBox game_checker(0.0455, 0.244, 0.442, 0.057);
    VideoSnapshot screen = env.console.video().snapshot();

    VideoOverlaySet box_render(env.console);

    std::ostringstream ss;


    // TODO: Set up Go disposal
    if(DISPOSE_GOS)home_dispose_of_go(env, context, home_manager);
}

void Enrichment::sort_all_boxes(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, bool& started, bool& swaps_made){
    home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_HOME);

    BoxLayout Home;

    if(SKIP_SETUP){return;}

    do {
        swaps_made = false;

        env.console.log("Starting from box " + std::to_string(HOME_FIRST_BOX));

        home_manager.navigate_to(env, context, {0, 0, HOME_FIRST_BOX});
        PokemonBox* left = home_load_box(env, context, home_manager, HOME_FIRST_BOX, Home);
        PokemonBox* right = nullptr;



        for (size_t left_box = HOME_FIRST_BOX; left_box < HOME_LAST_BOX; ++left_box) {
            size_t right_box = left_box + 1;

            if (right_box > HOME_LAST_BOX) break; // Prevent out-of-bounds access

            // navigate to and load the right box
            // home_navigate_to_box(env, context, right_box);
            home_manager.navigate_to(env, context, {0, 0, right_box});
            right = home_load_box(env, context, home_manager, right_box, Home);

            // Run easy swaps
            bool swaps = home_make_easy_swaps(env, context, home_manager, Home, left_box, right_box);

            swaps_made = swaps_made || swaps ;

            if(swaps){
                // Before moving on, make sure the quick views are where they should be. If the next function returns false, we have a huge problem.
                env.console.log("Validating spaces");
                home_manager.navigate_to(env, context, {0,0, right->box_num});
                context.wait_for_all_requests();
                // TODO: Handle cases of 5 reconcile failures. Maybe close and restart?
                if(!home_reconcile_spaces(env, context, home_manager, Home, right_box)){
                    env.console.log("Validation failed. Rebuilding boxes.");
                    home_manager.navigate_to(env, context, {0,0, left_box});
                    PokemonBox temp = home_build_box(env, context, home_manager, left_box);
                    Home.update_box(left_box, temp);
                    pbf_wait(context, 1000ms);
                    home_manager.navigate_to(env, context, {0,0, right_box});
                    temp = home_build_box(env, context, home_manager, right_box);
                    Home.update_box(right_box, temp);
                    pbf_wait(context, 1000ms);
                    context.wait_for_all_requests();
                    home_manager.navigate_to(env, context, {0,0, left_box});
                    left_box--;
                    continue;
                }else{
                    env.console.log("Validation succeeded. Continuing.");
                }
            }
            left = right;
        }


        send_program_notification(
            env, NOTIFICATION_ERROR_FATAL,
            COLOR_GREEN,
            "Completed Left Scan",
            {}, "",
            {}
            );

        if(!swaps_made){
            break;
        }

        swaps_made = false;



        for (size_t right_box = HOME_LAST_BOX; right_box > HOME_FIRST_BOX; --right_box) {
            size_t left_box = right_box - 1;

            if (left_box < HOME_FIRST_BOX) break; // Prevent out-of-bounds access

            // navigate to and load the left box
            // home_navigate_to_box(env, context, left_box);
            home_manager.navigate_to(env, context, {0, 0, left_box});
            left = &Home.get_box(left_box);
            if (!left->is_empty()) {
                left = home_load_box(env, context, home_manager, left_box, Home);
                Home.add_box(left_box, *left);
            }

            // Run easy swaps
            bool swaps = home_make_easy_swaps(env, context, home_manager, Home, left_box, right_box);

            swaps_made = swaps_made || swaps ;

            if(swaps){
                env.console.log("Validating blanks");
                home_manager.navigate_to(env, context, {0,0, left->box_num});
                context.wait_for_all_requests();
                if(!home_reconcile_spaces(env, context, home_manager, Home, left_box)){
                    env.console.log("Validation failed. Rebuilding boxes.");
                    home_manager.navigate_to(env, context, {0,0, right_box});
                    PokemonBox temp = home_build_box(env, context, home_manager, right_box);
                    Home.update_box(right_box, temp);
                    pbf_wait(context, 1000ms);
                    home_manager.navigate_to(env, context, {0,0, left_box});
                    temp = home_build_box(env, context, home_manager, left_box);
                    Home.update_box(left_box, temp);
                    pbf_wait(context, 1000ms);
                    context.wait_for_all_requests();
                    home_manager.navigate_to(env, context, {0,0, right_box});
                    right_box++;
                    continue;
                }else{
                    env.console.log("Validation succeeded. Continuing.");
                }
            }
            right = left;

        }

        send_program_notification(
            env, NOTIFICATION_ERROR_FATAL,
            COLOR_GREEN,
            "Completed Right Scan",
            {}, "",
            {}
            );
    } while (!swaps_made);


    send_program_notification(
        env, NOTIFICATION_ERROR_FATAL,
        COLOR_GREEN,
        "Finished Swapping between boxes. Sorting within each box.",
        {}, "",
        env.console.video().snapshot()
        );

    // Now, we fine sort each box since every pokemon is in its respective box.
    for(size_t i = HOME_FIRST_BOX; i <= HOME_LAST_BOX; i++){

        home_manager.navigate_to(env, context, {0,0,i});

        home_sort_box(env, context, home_manager, Home.get_box(i));

        Home.get_box(i).output_boxes_data_json();

        if(!home_reconcile_spaces(env, context, home_manager, Home, i)){
            PokemonBox temp = home_build_box(env, context, home_manager, i);
            Home.update_box(i, temp);
            i--;
        }else{
            Home.set_sorted(i, true);
        }

    }

    home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
}

void Enrichment::enrich_with_games(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, std::vector<Game>& game_list){
    std::unordered_map<GameStatus, std::vector<mode>> mode_list = {
        {GameStatus::POKEMON_VIOLET, {mode::Level, mode::Simple_Item, mode::None}}
    };

    for(auto game: game_list){
        enrichment_mode = mode_list[game.game][0];

        int pokemon = 30;
        do{
            switch(game.game){
                case GameStatus::POKEMON_VIOLET:
                    if(!SKIP_SETUP){
                        home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_VIOLET);
                        context.wait_for_all_requests();
                        pokemon = home_fill_boxes_to_game(env, context, home_manager, game, SV_BOX_NAME, enrichment_mode);
                        if(pokemon==0){
                            mode_list.erase(mode_list.begin());
                            enrichment_mode = mode_list[game.game][0];
                            continue;
                        }
                    }
                    switch_close_game_and_open(env, context, "Pokémon Violet", home_manager);
                    sv_run_enrichment(env,context, SV_BOX_NAME, std::ceil(pokemon/6), enrichment_mode);
                    send_program_notification(
                        env, NOTIFICATION_ERROR_FATAL,
                        COLOR_GREEN,
                        "Ran SV Enrichment on up to 30 Pokemon",
                        {}, "",
                        {}
                        );
                default:
                    // switch_close_game_and_open(env, context, game.name);
                    pokemon=30;
            }
            switch_close_game_and_open(env, context, "Pokémon HOME", home_manager);
            home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, game.game);
            home_put_away_pokemon(env, context, home_manager, game, EMERGENCY_DELOAD);
        }while(pokemon!=0&&enrichment_mode!=mode::None);
    }
}

// This function is run in Pokémon Home, and should be used when wanting to transfer all Pokémon from a game back to home.
// Requires env, context
void Enrichment::home_put_away_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager, Game& game, bool emergency = false){

    home_manager.navigate_to(env, context, {1, 6, HOME_FIRST_BOX});

    switch(game.game){
        case GameStatus::POKEMON_VIOLET:
            home_navigate_to_box_secondary(env, context, SV_BOX_NAME);
        default:
            home_navigate_to_box_secondary(env, context, SV_BOX_NAME);
    }
    // pbf_press_button(context, BUTTON_UP, 10, 30);
    // move back to the Home side of the boxes
    // pbf_press_button(context, BUTTON_LEFT, 10, 30);
    home_manager.navigate_to(env, context, {0,0, HOME_FIRST_BOX});
    size_t current_box = HOME_FIRST_BOX;
    // For each Pokémon in the box, increment it's circle marking and find it a place to be put into
    try{
        for(int i = 0; i < 6; i++){
            for(int j = 0; j < 5; j++){
                home_manager.navigate_to(env, context, {0,0});
                ImageFloatBox slot_box(0.55 + (0.072 * i), 0.2 + (0.1035 * j), 0.03, 0.057);
                //checking color to know if a pokemon is on the slot or not
                if((int)image_stddev(extract_box_reference(env.console.video().snapshot(), slot_box)).sum() < 5)continue;

                std::pair<size_t,size_t> target_pos = home_locate_empty_position(env, context, &current_box, HOME_LAST_BOX);
                home_move_pokemon_from_game(env, context, home_manager, target_pos, {j,i+6}, emergency);
            }
        }
        pbf_press_button(context, BUTTON_DOWN, 10, 30);
        pbf_press_button(context, BUTTON_DOWN, 10, 30);
        pbf_press_button(context, BUTTON_DOWN, 10, 30);
        pbf_press_button(context, BUTTON_RIGHT, 10, 30);

        home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
    }catch(...){
        env.console.log("Error, ran out of space.");
    }

}

void Enrichment::home_dispose_of_go(SingleSwitchProgramEnvironment& env, ProControllerContext& context, PokemonHome_HomeEnvironment& home_manager){
    std::vector<int> blacklist = {144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251, 377, 378, 379, 380, 381, 382, 383, 384, 385, 386, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489,490,491,492,493,494,638,639,640,641,642,643,644,645,646,647,648,649,666,676,716,717,718,772,773,785,786,787,788,789,790,791,792,793,888,889,890,891,892,893,894,895,896,897,898,905,999,1000,1001,1002,1003,1004,1007,1008,1009,1010,1014,1015,1016,1017,1021,1022,1023,1024,1025}; // Take out legendaries, etc. that need to be preserved

    std::vector<int> lv10_blacklist = {2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18, 20, 22, 24, 28, 30, 31, 33, 34, 42, 44, 45, 47, 49, 51, 53, 55, 57, 61, 62, 64, 65, 65, 67, 68, 68, 70, 71, 73, 75, 76, 76, 76, 78, 80, 82, 85, 87, 89, 93, 94, 94, 97, 99, 101, 105, 106, 107, 110, 112, 117, 119, 124, 125, 126, 130, 139, 141, 148, 149, 153, 154, 156, 157, 157, 159, 160, 162, 164, 166, 168, 171, 178, 180, 181, 182, 184, 186, 186, 188, 189, 195, 202, 205, 210, 217, 219, 221, 224, 229, 230, 230, 232, 237, 247, 248, 253, 254, 256, 257, 259, 260, 262, 264, 266, 267, 268, 269, 271, 272, 274, 275, 277, 279, 281, 282, 284, 286, 288, 289, 291, 292, 294, 295, 297, 305, 306, 308, 310, 317, 319, 321, 323, 326, 329, 330, 332, 334, 340, 342, 344, 346, 348, 354, 356, 362, 364, 365, 372, 373, 375, 376, 388, 389, 391, 392, 394, 395, 397, 398, 400, 402, 404, 405, 409, 411, 414, 415, 416, 419, 421, 423, 426, 432, 435, 437, 444, 445, 450, 452, 454, 457, 460, 462, 464, 464, 466, 466, 467, 468, 473, 475, 477, 496, 497, 499, 500, 502, 503, 503, 505, 507, 508, 510, 520, 521, 523, 525, 526, 526, 530, 533, 534, 534, 536, 537, 541, 544, 545, 552, 553, 555, 558, 560, 563, 565, 567, 569, 571, 575, 576, 578, 579, 581, 583, 584, 586, 591, 593, 596, 598, 600, 601, 603, 604, 606, 608, 609, 611, 612, 614, 620, 623, 625, 628, 630, 634, 635, 637, 651, 652, 654, 655, 657, 658, 660, 662, 663, 665, 666, 668, 670, 671, 673, 675, 678, 680, 687, 689, 691, 693, 697, 699, 705, 706, 706, 713, 715, 723, 724, 724, 726, 727, 729, 730, 732, 733, 735, 737, 738, 743, 745, 748, 750, 752, 754, 756, 758, 760, 762, 763, 768, 770, 783, 784, 790, 791, 791, 792, 792, 811, 812, 814, 815, 817, 818, 820, 822, 823, 825, 826, 828, 830, 832, 834, 836, 838, 839, 844, 847, 849, 851, 857, 858, 860, 861, 862, 862, 863, 864, 866, 879, 886, 887, 901, 907, 908, 910, 911, 913, 914, 916, 918, 920, 922, 923, 925, 927, 929};

    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.049, 0.04); //pokemon national dex number pos
    ImageFloatBox nature_box(0.157, 0.783, 0.212, 0.042); // Nature box
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046); // OT ID box
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041); // Level box
    home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_PLA);

    home_manager.navigate_to(env, context, {1,11});

    size_t temp_box = PLA_FIRST_BOX;
    bool more_go = false;
    home_navigate_to_box_secondary(env, context, temp_box);
    try{
        do{
            std::pair<size_t,size_t> next_spot = home_locate_empty_position_secondary(env, context, &temp_box, PLA_LAST_BOX, false);

            pbf_press_button(context, BUTTON_X, 10, 100);
            pbf_press_button(context, BUTTON_X, 10, 50);
            home_manager.scroll_filter_menu(env, context, "markings");
            context.wait_for_all_requests();
            pbf_press_button(context, BUTTON_A, 10, 50);
            pbf_press_button(context, BUTTON_UP, 10, 50);
            pbf_press_button(context, BUTTON_A, 10, 50);
            home_manager.scroll_filter_menu(env, context, "origin-mark");
            context.wait_for_all_requests();
            pbf_press_button(context, BUTTON_UP, 10, 30);
            pbf_press_button(context, BUTTON_UP, 10, 30);
            pbf_press_button(context, BUTTON_A, 10, 50);
            home_manager.scroll_filter_menu(env, context, "shiny");
            context.wait_for_all_requests();
            pbf_press_button(context, BUTTON_UP, 10, 30);
            pbf_press_button(context, BUTTON_A, 10, 50);
            home_manager.scroll_filter_menu(env, context, "compatible-games");
            context.wait_for_all_requests();
            while(home_manager.get_filter_menu_read(env, context)!="compatible-games"){
                pbf_wait(context, 250ms);
            }
            pbf_press_button(context, BUTTON_DOWN, 10, 50);
            pbf_press_button(context, BUTTON_DOWN, 10, 50);

            context.wait_for_all_requests();

            // block for checking that we have successfully navigated to PLA
            ImageFloatBox PLA_check(0.83, 0.4, 0.1, 0.06);
            VideoSnapshot screen = env.console.video().snapshot();
            FloatPixel PLA_button = image_stats(extract_box_reference(screen, PLA_check)).average;
            while(euclidean_distance(PLA_button, FloatPixel(255, 187, 0))>5){
                pbf_press_button(context, BUTTON_DOWN, 10, 60);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                PLA_button = image_stats(extract_box_reference(screen, PLA_check)).average;
            }

            pbf_press_button(context, BUTTON_A, 10, 50);
            pbf_press_button(context, BUTTON_B, 10, 240);


            // Inspect for blacklisted
            pbf_press_button(context, BUTTON_A, 10, 80);
            pbf_press_button(context, BUTTON_DOWN, 10, 80);
            pbf_press_button(context, BUTTON_A, 10, 200);

            // Take note of first ID No, ot id, and nature for looping
            context.wait_for_all_requests();
            screen = env.console.video().snapshot();
            int first_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
            int first_level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
            int first_nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);

            int id = first_id;
            int level = first_level;
            int nat_id = first_nat_id;
            do{
                more_go = false;
                env.console.log("Running search");
                bool blacklisted = false;
                for(auto id_p: blacklist){
                    blacklisted = blacklisted || id_p==nat_id;
                }
                for(auto id_p2: lv10_blacklist){
                    blacklisted = blacklisted || (id_p2==nat_id&&level<=10);
                }
                if(!blacklisted){
                    more_go = true;
                    env.console.log("Found Nonblacklisted");

                    // Move this into the boxes
                    pbf_press_button(context, BUTTON_B, 10, 270);
                    pbf_press_button(context, BUTTON_A, 10, 80);
                    pbf_press_button(context, BUTTON_A, 10, 150);

                    context.wait_for_all_requests();

                    home_manager.detect_home(env, context, true);

                    home_move_pokemon_to_game(env, context, home_manager, {next_spot.first, next_spot.second+6});
                    break;
                }
                pbf_press_button(context, BUTTON_R, 10, 80);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
                level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
                nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
            }while(id!=first_id||level!=first_level||nat_id!=first_nat_id);
        }while(more_go);

        pbf_press_button(context, BUTTON_B, 10, 240);
        pbf_press_button(context, BUTTON_B, 10, 240);
        size_t count = 0;
        while(!home_read_main_menu(env, context, "POKEMON")){
            if(++count==5){
                pbf_press_button(context, BUTTON_B, 10, 240);
            }
            pbf_wait(context, 500ms);
        }


    }catch(...){
        env.console.log("Ran out of spaces");
    }

    home_manager.navigate_menus_to(env, context, PageID::LIST_VIEW, GameStatus::POKEMON_HOME);

    more_go = false;
    try{
        pbf_press_button(context, BUTTON_X, 10, 100);
        pbf_press_button(context, BUTTON_X, 10, 50);
        home_manager.scroll_filter_menu(env, context, "markings");
        pbf_press_button(context, BUTTON_A, 10, 50);
        pbf_press_button(context, BUTTON_UP, 10, 50);
        pbf_press_button(context, BUTTON_A, 10, 50);
        home_manager.scroll_filter_menu(env, context, "origin-mark");
        pbf_press_button(context, BUTTON_UP, 10, 30);
        pbf_press_button(context, BUTTON_UP, 10, 30);
        pbf_press_button(context, BUTTON_A, 10, 50);
        home_manager.scroll_filter_menu(env, context, "shiny");
        pbf_press_button(context, BUTTON_UP, 10, 30);
        pbf_press_button(context, BUTTON_A, 10, 80);
        pbf_press_button(context, BUTTON_B, 10, 270);

        do{

            // Inspect for blacklisted
            pbf_press_button(context, BUTTON_A, 10, 90);
            pbf_press_button(context, BUTTON_DOWN, 10, 90);
            pbf_press_button(context, BUTTON_A, 10, 210);

            // Take note of first ID No, ot id, and nature for looping
            context.wait_for_all_requests();
            VideoSnapshot screen = env.console.video().snapshot();
            int first_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
            int first_level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
            int first_nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);

            int id = first_id;
            int level = first_level;
            int nat_id = first_nat_id;
            do{
                more_go = false;
                env.console.log("Running search");
                bool blacklisted = false;
                for(auto id_p: blacklist){
                    blacklisted = blacklisted || id_p==nat_id;
                }
                for(auto id_p2: lv10_blacklist){
                    blacklisted = blacklisted || (id_p2==nat_id&&level<=10);
                }
                if(!blacklisted){
                    more_go = true;
                    env.console.log("Found Nonblacklisted");

                    // Release
                    pbf_press_button(context, BUTTON_B, 10, 280);
                    pbf_press_button(context, BUTTON_A, 10, 90);
                    pbf_press_button(context, BUTTON_UP, 10, 60);
                    pbf_press_button(context, BUTTON_UP, 10, 60);
                    pbf_press_button(context, BUTTON_A, 10, 160);
                    pbf_press_button(context, BUTTON_A, 10, 160);
                    pbf_press_button(context, BUTTON_UP, 10, 60);
                    pbf_press_button(context, BUTTON_A, 10, 160);
                    pbf_press_button(context, BUTTON_A, 10, 210);

                    context.wait_for_all_requests();

                    break;
                }
                pbf_press_button(context, BUTTON_R, 10, 90);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
                level = OCR::read_number_waterfill(env.console, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
                nat_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
            }while(id!=first_id||level!=first_level||nat_id!=first_nat_id);
        }while(more_go);

        pbf_press_button(context, BUTTON_B, 10, 240);
        pbf_press_button(context, BUTTON_B, 10, 240);
        size_t count = 0;
        while(!home_read_main_menu(env, context, "POKEMON")){
            if(++count==5){
                pbf_press_button(context, BUTTON_B, 10, 240);
            }
            pbf_wait(context, 500ms);
        }


    }catch(...){
        env.console.log("Ran out of spaces");
    }
}

void Enrichment::wipe_markings(SingleSwitchProgramEnvironment& env, ProControllerContext& context,PokemonHome_HomeEnvironment& home_manager){

    VideoOverlaySet box_render(env.console);

    // Navigate to Pokémon Home for initial setup
    home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_HOME);

    home_manager.navigate_to(env, context, {0, 0, HOME_FIRST_BOX});

    box_render.clear();

    if(WIPE_MARKINGS){
        while(home_manager.get_box()<=HOME_LAST_BOX){
            home_scan_box(env, context, env.console.video().snapshot(), home_manager, WIPE_MARKINGS);
            context.wait_for_all_requests();
            home_manager.navigate_to(env, context, {0, 0, home_manager.get_box()+1});
        }
    }

    home_manager.navigate_menus_to(env, context, PageID::MAIN_MENU);
}

void Enrichment::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.console.log("Opened");

    std::vector<Game> game_list = {Game(GameStatus::POKEMON_VIOLET,0,false)/*,Game(GameStatus::POKEMON_SWORD,3,false),Game(GameStatus::POKEMON_PLA,1,false),Game("GameStatus::POKEMON_EEVEE",4,false)*/};

    VideoSnapshot screen = env.console.video().snapshot();

    VideoOverlaySet box_render(env.console);
    HomeMenuWatcher home_menu(env.console);
    std::ostringstream ss;

    PokemonHome_HomeEnvironment home_manager(env, context);

    bool started = false;
    bool swaps_made = true;

    if(EMERGENCY_DELOAD){
        home_manager.navigate_menus_to(env, context, PageID::BOX_VIEW, GameStatus::POKEMON_VIOLET);
        home_put_away_pokemon(env, context, home_manager, game_list[0], true);
    }
    if(!SKIP_SETUP){
        initialize_home(env, context, home_manager, game_list);
        sort_all_boxes(env, context, home_manager, started, swaps_made);
        send_program_notification(
                env, NOTIFICATION_ERROR_FATAL,
                COLOR_GREEN,
                "Finished Sorting Pokemon",
                {}, "",
                screen
            );
    }
    enrich_with_games(env, context, home_manager, game_list);
    send_program_notification(
        env, NOTIFICATION_ERROR_FATAL,
        COLOR_GREEN,
        "Finished Running Enhancements",
        {}, "",
        screen
        );

    sort_all_boxes(env, context, home_manager, started, swaps_made);

    int ret = -1;
    while(ret!=0){
        pbf_press_button(context, BUTTON_HOME, 10, 240);
        ret = wait_until(
            env.console, context,
            Milliseconds(30*TICKS_PER_SECOND),
            {
                home_menu
            }
            );
    }    pbf_press_button(context, BUTTON_X, 10, 20);
    pbf_press_button(context, BUTTON_A, 10, 320);
    pbf_press_button(context, BUTTON_A, 10, 3150);
    pbf_press_button(context, BUTTON_A, 10, 3150);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);


    // PokemonSV::AdvanceDialogWatcher evo_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_BLACK);
    // PokemonSV::AdvanceDialogWatcher next_message(COLOR_CYAN, PokemonSV::DialogType::DIALOG_WHITE);
    // PokemonSV::PromptDialogWatcher learn_move_message(COLOR_CYAN);
    // PokemonSV::NormalBattleMenuWatcher battle_menu(COLOR_RED);

    // int ret = wait_until(
    //     env.console, context,
    //     Milliseconds(60*TICKS_PER_SECOND),
    //     {
    //         evo_message,
    //         next_message,
    //         learn_move_message
    //     }
    //     );

    // env.console.log(std::to_string(ret));

    // ImageFloatBox evolve_message(0.28, 0.76, 0.065, 0.055);

    // auto sanitize_OCR = [&](std::string str) -> std::string {
    //     char chars[] = "\n\r—.,";
    //     for(auto a:chars){str.erase(std::remove(str.begin(),str.end(), a),str.end());}
    //     return str;
    // };

    // int ret = wait_until(
    //     env.console, context,
    //     Milliseconds(60*TICKS_PER_SECOND),
    //     {
    //         evo_message,
    //         next_message,
    //         learn_move_message
    //     }
    //     );

    // switch(ret){
    // case 0:
    //     screen = env.console.video().snapshot();
    //     pbf_press_button(context, BUTTON_A, 10, 20);
    //     if(sanitize_OCR(OCR::ocr_read(Language::English, extract_box_reference(screen, evolve_message))) == "What?"){
    //         if(rand() > (1-(std::pow(1-(0.5),1/4)))){
    //             pbf_mash_button(context, BUTTON_B, 500ms);
    //         }  else{
    //             pbf_wait(context, 10000ms);
    //         }
    //     }
    // case 1:
        // pbf_mash_button(context, BUTTON_B, 500ms);
    // case 2:
    //     pbf_press_button(context, BUTTON_B, 10, 20);
    //     break;
    // default:
    //     throw;
    // }


    // ImageFloatBox hand_region = {0.03, 0.15, 0.45, 0.5};
    // HomeCursorWatcher handWatcher(HomeCursorType::GRABBING, hand_region, COLOR_WHITE);

    // int ret = wait_until(env.console, context, 2000ms, {handWatcher});
    // if (ret == 0){
    //     auto [x, y] = handWatcher.location();
    //     env.console.log("HERE at ("+std::to_string(x)+", "+std::to_string(y)+")");
    // }

    // home_manager.scroll_filter_menu(env, context, "labels");

    // ImageFloatBox dialog_box(0.7, 0.22, 0.2, 0.05);
    // ImageViewRGB32 dialog_image = extract_box_reference(screen, dialog_box);
    // const auto result = FilterMenuReader::instance().read_substring(
    //     env.console, Language::English, dialog_image,
    //     OCR::WHITE_TEXT_FILTERS()
    //     );

    // env.console.log(std::to_string(FilterMenuReader::instance().distance_to(result.results.cbegin()->second.token, "labels")));

    // ImageFloatBox game_icon_box_prim(0.9255, 0.715, 0.035, 0.057);
    // FloatPixel game_icon_prim = image_stats(extract_box_reference(screen, game_icon_box_prim)).average;
    // env.console.log(std::to_string(game_icon_prim.r) + " " + std::to_string(game_icon_prim.g) + " " + std::to_string(game_icon_prim.b));

    // ImageFloatBox page_r_button(0.1, 0.13, 0.1, 0.1);
    // // FloatPixel pixel = image_stats(extract_box_reference(screen, page_r_button)).average;

    // box_render.add(COLOR_RED, page_r_button);
    // // env.console.log(std::to_string(pixel.r) + " " + std::to_string(pixel.g) + " " + std::to_string(pixel.b));

    // pbf_wait(context, 2000ms);

    // context.wait_for_all_requests();
    // box_render.clear();

}

}
}
}
