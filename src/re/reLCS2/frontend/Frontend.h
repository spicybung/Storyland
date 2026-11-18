#pragma once

#include <array>
#include <algorithm>
#include <string>

enum class StorylandLcsFrontendTab : int {
    Map = 0,
    Brief = 1,
    Game = 2,
    Stats = 3,
    Controls = 4,
    Audio = 5,
    Display = 6
};

enum class StorylandLcsFrontendPage {
    Main,
    LoadSlots,
    SaveSlots,
    DeleteSlots,
    ConfirmLoad,
    ConfirmSave,
    ConfirmDelete,
    ConfirmNew,
    Notice
};

struct StorylandLcsFrontendNavigation {
    StorylandLcsFrontendTab tab = StorylandLcsFrontendTab::Game;
    StorylandLcsFrontendPage page = StorylandLcsFrontendPage::Main;
    std::array<int, 7> selections{{0, 0, 1, 0, 0, 0, 0}};
    int selectedSlot = 1;
    int subpageSelection = 0;
    bool opened = false;
    bool gameplay = false;
    bool scriptSave = false;
    std::string notice;

    void open(bool activeGame) {
        if (opened && gameplay == activeGame) return;
        opened = true;
        gameplay = activeGame;
        scriptSave = false;
        tab = activeGame ? StorylandLcsFrontendTab::Map : StorylandLcsFrontendTab::Game;
        page = StorylandLcsFrontendPage::Main;
        subpageSelection = 0;
        selections = {{0, 0, 1, 0, 0, 0, 0}};
        notice.clear();
    }

    void close() {
        opened = false;
        scriptSave = false;
        page = StorylandLcsFrontendPage::Main;
        notice.clear();
    }

    int tabIndex() const { return int(tab); }

    int selected() const {
        return page == StorylandLcsFrontendPage::Main ?
            selections[size_t(tabIndex())] : subpageSelection;
    }

    void setSelected(int index, int itemCount) {
        if (itemCount <= 0) return;
        const int valid = std::clamp(index, 0, itemCount - 1);
        if (page == StorylandLcsFrontendPage::Main)
            selections[size_t(tabIndex())] = valid;
        else
            subpageSelection = valid;
    }

    void move(int step, int itemCount) {
        if (itemCount <= 0) return;
        const int current = selected();
        setSelected((current + step % itemCount + itemCount) % itemCount, itemCount);
    }

    void changeTab(int step) {
        if (page != StorylandLcsFrontendPage::Main) return;
        int index = tabIndex();
        index = (index + step % 7 + 7) % 7;
        tab = StorylandLcsFrontendTab(index);
        notice.clear();
    }

    void show(StorylandLcsFrontendPage next, int initial = 0) {
        page = next;
        subpageSelection = initial;
        notice.clear();
    }

    void back() {
        page = StorylandLcsFrontendPage::Main;
        subpageSelection = 0;
        notice.clear();
    }
};

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Literal menu entries extracted from the user's SLUS_214.23 PS2 ELF.
// A screen is 8 bytes heading plus 15 18-byte menu records.
struct StorylandLcsRetailMenuRow {
    uint16_t command;
    const char* gxtKey;
    uint16_t operation;
    uint16_t x;
    uint16_t y;
    uint16_t style;
};
struct StorylandLcsRetailMenuScreen {
    const char* title;
    const StorylandLcsRetailMenuRow* rows;
    size_t count;
};
inline constexpr std::array<StorylandLcsRetailMenuRow, 0> feh_sta_rows = {{
}};
inline constexpr std::array<StorylandLcsRetailMenuRow, 3> feh_loa_rows = {{
    {30u, "FES_LOA", 0x3A00u, 240u, 70u, 3u},
    {4u, "FES_NGA", 0x0900u, 0u, 0u, 3u},
    {31u, "FES_DGM", 0x3B00u, 0u, 0u, 3u},
}};
inline constexpr std::array<StorylandLcsRetailMenuRow, 0> feh_bri_rows = {{
}};
inline constexpr std::array<StorylandLcsRetailMenuRow, 4> feh_con_rows = {{
    {7u, "FEC_CCF", 0x0300u, 240u, 30u, 2u},
    {8u, "FEC_CDP", 0x0300u, 240u, 45u, 2u},
    {6u, "FEC_ILU", 0x0300u, 240u, 60u, 2u},
    {5u, "FEC_VIB", 0x0300u, 240u, 75u, 2u},
}};
inline constexpr std::array<StorylandLcsRetailMenuRow, 3> feh_aud_rows = {{
    {14u, "FEA_MUS", 0x0400u, 240u, 40u, 2u},
    {15u, "FEA_SFX", 0x0400u, 240u, 60u, 2u},
    {17u, "FEA_RSS", 0x0400u, 240u, 100u, 2u},
}};
inline constexpr std::array<StorylandLcsRetailMenuRow, 6> feh_dis_rows = {{
    {13u, "FED_BRI", 0x0500u, 240u, 40u, 2u},
    {11u, "FED_SUB", 0x0500u, 240u, 60u, 2u},
    {12u, "FED_WIS", 0x0500u, 240u, 80u, 2u},
    {40u, "FED_RDR", 0x0500u, 240u, 100u, 2u},
    {41u, "FED_HUD", 0x0500u, 240u, 120u, 2u},
    {29u, "FED_POS", 0x0500u, 240u, 140u, 3u},
}};
inline constexpr std::array<StorylandLcsRetailMenuRow, 5> feh_lan_rows = {{
    {19u, "FEL_ENG", 0x0600u, 240u, 60u, 3u},
    {20u, "FEL_FRE", 0x0600u, 240u, 80u, 3u},
    {21u, "FEL_GER", 0x0600u, 240u, 100u, 3u},
    {22u, "FEL_ITA", 0x0600u, 240u, 120u, 3u},
    {23u, "FEL_SPA", 0x0600u, 240u, 140u, 3u},
}};
inline constexpr std::array<StorylandLcsRetailMenuRow, 0> feh_map_rows = {{
}};

inline constexpr std::array<StorylandLcsRetailMenuScreen, 8> storylandLcsRetailScreens = {{
    {"FEH_STA", feh_sta_rows.data(), feh_sta_rows.size()},
    {"FEH_LOA", feh_loa_rows.data(), feh_loa_rows.size()},
    {"FEH_BRI", feh_bri_rows.data(), feh_bri_rows.size()},
    {"FEH_CON", feh_con_rows.data(), feh_con_rows.size()},
    {"FEH_AUD", feh_aud_rows.data(), feh_aud_rows.size()},
    {"FEH_DIS", feh_dis_rows.data(), feh_dis_rows.size()},
    {"FEH_LAN", feh_lan_rows.data(), feh_lan_rows.size()},
    {"FEH_MAP", feh_map_rows.data(), feh_map_rows.size()},
}};

inline const StorylandLcsRetailMenuScreen* storylandLcsRetailFindScreen(const char* key) {
    for (const auto& screen : storylandLcsRetailScreens)
        if (std::string_view(screen.title) == key) return &screen;
    return nullptr;
}
