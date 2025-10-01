#ifndef PokemonAutomation_PokemonHome_HomeEnvironment_H
#define PokemonAutomation_PokemonHome_HomeEnvironment_H

#include "CommonFramework/ImageTools/FloatPixel.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Options/Pokemon_StatsHuntFilter.h"
#include "Pokemon/Pokemon_Types.h"
#include "PokemonHome/Options/PokemonHome_BoxSortingTable.h"
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
    const size_t MAX_ROWS = 5;
    const size_t MAX_COLUMNS = 6;

public:
    HomeCursor(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false, bool = false);
    // HomeCursor(Pokemon&);
    HomeCursor(size_t, size_t);
    HomeCursor(HomeCursor, size_t);
    HomeCursor(size_t, size_t, size_t);
    HomeCursor(std::pair<size_t,size_t>&);
    // HomeCursor(HomePokemon);
    HomeCursor();

    CursorActionResponse move_cursor_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor&);
    CursorActionResponse pick_up_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    CursorActionResponse put_down_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    CursorActionResponse identify_page(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false, size_t = UINT_MAX);
    size_t get_page();
    size_t get_row();   // TODO: Delete after implementing box system
    size_t get_col();   // TODO: Delete after implementing box system
    int distance_to(const HomeCursor&);
    bool secondary_open;

private:
    void align_col(SingleSwitchProgramEnvironment&, ProControllerContext&, const size_t&);
    void align_row(SingleSwitchProgramEnvironment&, ProControllerContext&, const size_t&);
    CursorActionResponse position_cursor(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor&, size_t = 0);
    CursorActionResponse navigate_to_page(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor&);
    CursorActionResponse locate_position(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false);

    size_t row;
    size_t col;
    size_t box;

    // size_t secondary_box;
    bool holding_pokemon;
    // bool InSecondaryBoxes;
};

class PokemonHome_HomeEnvironment : public SingleSwitchProgramInstance {

public:
    PokemonHome_HomeEnvironment(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    void navigate_menus_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const PageID, const GameStatus = GameStatus::CURRENT);
    void navigate_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor &);
    void detect_home(SingleSwitchProgramEnvironment&, ProControllerContext&, bool = false);
    void scroll_filter_menu(SingleSwitchProgramEnvironment&, ProControllerContext&, std::string, size_t = 0);
    void pick_up_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    void put_down_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    void swap_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&, const HomeCursor& slot1, const HomeCursor& slot2);
    void bail_out(SingleSwitchProgramEnvironment&env, ProControllerContext&context);
    std::string get_filter_menu_read(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    std::string get_view();

    size_t get_box();

    HomeCursor get_cursor();


private:
    CursorActionResponse handle_errors(SingleSwitchProgramEnvironment&, ProControllerContext&, const CursorActionResponse&);

    void initialize_navigation_map(SingleSwitchProgramEnvironment&, ProControllerContext&);
    std::vector<PageID> find_navigation_path(SingleSwitchProgramEnvironment&, ProControllerContext&, PageID, PageID);
    void perform_navigation_steps(SingleSwitchProgramEnvironment&, ProControllerContext&, std::vector<PageID>&);
    void identify_game_icon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    std::optional<HomeCursor> cursor;
    GameStatus game_open;
    PageID current_view;
    // std::vector<HomePokemonBox> boxes;

    std::unordered_map<PageID, std::vector<std::pair<PageID, NavigationFunction>>> navigation_map;
    std::unordered_map<std::pair<PageID, PageID>, std::vector<PageID>, pair_hash> navigation_cache;
};

}
}
}
#endif
