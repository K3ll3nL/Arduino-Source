#ifndef PokemonAutomation_PokemonHome_HomeEnvironment_H
#define PokemonAutomation_PokemonHome_HomeEnvironment_H

#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonHome/Inference/PokemonHome_PokemonData.h"
#include <functional>
#include <unordered_map>

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonHome {

enum class PageID {
    CURRENT,
    TITLE_SCREEN,
    MAIN_MENU,
    GAME_SELECTION,
    BOX_VIEW,
    SUMMARY_VIEW,
    MARKINGS_VIEW,
    LIST_VIEW,
    UNKNOWN
};

}
}
}

namespace std {
template <>
struct hash<PokemonAutomation::NintendoSwitch::PokemonHome::PageID> {
    std::size_t operator()(const PokemonAutomation::NintendoSwitch::PokemonHome::PageID& id) const noexcept {
        return std::hash<int>()(static_cast<int>(id));
    }
};
}

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);

        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};


namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonHome {
using namespace Pokemon;


enum class GameStatus {
    NONE,
    POKEMON_HOME,
    POKEMON_PLA,
    POKEMON_PIKACHU,
    POKEMON_EEVEE,
    POKEMON_DIAMOND,
    POKEMON_PEARL,
    POKEMON_SWORD,
    POKEMON_SHIELD,
    POKEMON_SCARLET,
    POKEMON_VIOLET,
    CURRENT,
    UNKNOWN
};

// Continue with other enums, classes, and declarations
using NavigationFunction = ::std::function<void(SingleSwitchProgramEnvironment&, ProControllerContext&)>;

enum class SecondaryBoxStatus {
    FALSE,
    TRUE,
    UNKNOWN
};


enum class CursorActionResult{
    SUCCESS,
    FAILURE,
    ERROR_RECOVERABLE,
    ERROR_FATAL,
};

struct CursorActionResponse{
    CursorActionResult result;
    std::string message;
};

class HomeCursor {
    static constexpr int MAX_ROWS = 5;
    static constexpr int MAX_COLUMNS = 6;

public:
    HomeCursor(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false, bool = false);
    // HomeCursor(Pokemon&);
    HomeCursor(int, int);
    HomeCursor(HomeCursor, int);
    HomeCursor(int, int, int);
    HomeCursor(std::pair<int,int>&);
    // HomeCursor(HomePokemon);
    HomeCursor();

    bool operator==(const HomeCursor& other) const {
        return row == other.row &&
               col == other.col &&
               box == other.box &&
               secondary_open == other.secondary_open;
    }

    bool operator!=(const HomeCursor& other) const {
        return !(*this == other);
    }

    CursorActionResponse move_cursor_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor&);
    CursorActionResponse pick_up_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    CursorActionResponse put_down_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    CursorActionResponse identify_page(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false, int = UINT_MAX);
    int get_page();
    int get_page() const ;
    int get_row();   // TODO: Delete after implementing box system
    int get_row() const ;
    int get_col();   // TODO: Delete after implementing box system
    int get_col() const ;
    int distance_to(const HomeCursor&) const;
    bool secondary_open;

private:
    void align_col(SingleSwitchProgramEnvironment&, ProControllerContext&, const int&);
    void align_row(SingleSwitchProgramEnvironment&, ProControllerContext&, const int&);
    CursorActionResponse position_cursor(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor&, int = 0);
    CursorActionResponse navigate_to_page(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor&);
    CursorActionResponse locate_position(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false);

    int row;
    int col;
    int box;

    // size_t secondary_box;
    bool holding_pokemon;
    // bool InSecondaryBoxes;
};

class HomeEnvironment : public SingleSwitchProgramInstance {

public:
    HomeEnvironment(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    void navigate_menus_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const PageID, const GameStatus = GameStatus::CURRENT);
    void navigate_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor &);
    void detect_home(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false);
    void scroll_filter_menu(SingleSwitchProgramEnvironment&, ProControllerContext&, std::string, int = 0);
    void pick_up_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    void put_down_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    void swap_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor& slot1, const HomeCursor& slot2);
    void bail_out(SingleSwitchProgramEnvironment&env, ProControllerContext&context);
    std::string get_filter_menu_read(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    std::string get_view();
    bool sort_into_correct_boxes(SingleSwitchProgramEnvironment&, ProControllerContext&, int, int);
    void build_box(SingleSwitchProgramEnvironment&, ProControllerContext&, int);
    void sort_box(SingleSwitchProgramEnvironment&, ProControllerContext&, int);
    void sort_all_boxes(SingleSwitchProgramEnvironment&, ProControllerContext&, int, int);
    size_t get_box();

    HomeCursor get_cursor();

    void scan_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context, HomeCursor location);
    PokemonData scan_pokemon(SingleSwitchProgramEnvironment& env, ProControllerContext& context);


    HomeStorage boxes;  // TODO: Make Private


private:
    CursorActionResponse handle_errors(SingleSwitchProgramEnvironment&, ProControllerContext&, const CursorActionResponse&);

    void initialize_navigation_map(SingleSwitchProgramEnvironment&, ProControllerContext&);
    std::vector<PageID> find_navigation_path(SingleSwitchProgramEnvironment&, ProControllerContext&, PageID, PageID);
    void perform_navigation_steps(SingleSwitchProgramEnvironment&, ProControllerContext&, std::vector<PageID>&);
    void identify_game_icon(SingleSwitchProgramEnvironment&, ProControllerContext&);


    std::optional<HomeCursor> cursor;
    GameStatus game_open;
    PageID current_view;

    PokedexReader pokedex_information;

    std::unordered_map<PageID, std::vector<std::pair<PageID, NavigationFunction>>> navigation_map;
    std::unordered_map<std::pair<PageID, PageID>, std::vector<PageID>, pair_hash> navigation_cache;
    std::unordered_map<std::pair<int, int>, PokemonData*, pair_hash> best_map;
};

}
}
}
#endif
