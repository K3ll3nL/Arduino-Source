#ifndef PokemonAutomation_PokemonHome_HomeEnvironment_H
#define PokemonAutomation_PokemonHome_HomeEnvironment_H

#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonHome/Options/PokemonHome_BoxSortingTable.h"
#include <functional>
#include <unordered_map>

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonHome {

enum class PageID {
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

class HomeCursor {
    const size_t MAX_ROWS = 5;
    const size_t MAX_COLUMNS = 6;

public:
    HomeCursor(SingleSwitchProgramEnvironment&, ProControllerContext&);

    void locate_position();
    void move_cursor_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const std::pair<size_t, size_t>);
    void pick_up_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);
    void put_down_pokemon(SingleSwitchProgramEnvironment&, ProControllerContext&);

private:
    size_t row;
    size_t column;
    size_t home_box;
    size_t secondary_box;

    bool InSecondaryBoxes;
};

class PokemonHome_HomeEnvironment : public SingleSwitchProgramInstance {

public:
    PokemonHome_HomeEnvironment(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    void navigate_to(SingleSwitchProgramEnvironment&, ProControllerContext&, const PageID, const GameStatus = GameStatus::CURRENT, const std::pair<size_t, size_t> = {0,0}, const size_t = 0);
    void detect_home(SingleSwitchProgramEnvironment&, ProControllerContext&);
    std::string get_view();

private:
    void initialize_navigation_map(SingleSwitchProgramEnvironment&, ProControllerContext&);
    std::vector<PageID> find_navigation_path(SingleSwitchProgramEnvironment&, ProControllerContext&, PageID, PageID);
    void perform_navigation_steps(SingleSwitchProgramEnvironment&, ProControllerContext&, std::vector<PageID>&);

    HomeCursor cursor;
    GameStatus game_open;
    PageID current_view;

    std::unordered_map<PageID, std::vector<std::pair<PageID, NavigationFunction>>> navigation_map;
    std::unordered_map<std::pair<PageID, PageID>, std::vector<PageID>, pair_hash> navigation_cache;
};

}
}
}
#endif
