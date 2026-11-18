#include "src/storyland/storyland_stats_menu.h"
#include "src/re/reLCS2/frontend/Frontend.h"

// Liberty City Stories uses native FONT1/FONT2/FONT3 atlases in GAME.DTZ.
// Its PS2 release does not supply VCS-style FRONTEND/DEFAULT/*.psp pages.
static StorylandLcsFrontendNavigation gStorylandLcsFrontend;
static std::array<bool, 11> gStorylandLcsHeld{};
static std::array<ULONGLONG, 11> gStorylandLcsRepeat{};
static ULONGLONG gStorylandLcsLastNativeKey=0;

static bool storylandLcsInputReady() {
    if(!gStorylandLcsWaitForInputRelease)return true;
    if(GetTickCount64()<gStorylandLcsInputEnabledAt)return false;
    // Wait for BOTH the keyboard and the controller to release the sting-skip
    // button. A long keypress must not queue an implicit New Game request.
    const std::array<int,9> keys={
        VK_RETURN,VK_SPACE,VK_ESCAPE,VK_BACK,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_PRIOR
    };
    for(int key:keys)if((GetAsyncKeyState(key)&0x8000)!=0)return false;
    if((GetAsyncKeyState(VK_NEXT)&0x8000)!=0)return false;
    if(gStorylandController.state.down(12)||gStorylandController.state.down(16)||
        gStorylandController.state.down(17))return false;
    gStorylandLcsWaitForInputRelease=false;
    gStorylandLcsHeld.fill(false);
    gStorylandLcsRepeat.fill(0);
    gStorylandLcsLastNativeKey=GetTickCount64();
    return false;
}

static std::filesystem::path storylandLcsRetailSoundBankPath() {
    // Prefer the actual game's AUDIO bank. The original retail bank is also
    // included in the installed assets/ directory for separately supplied DTZs.
    for (const auto& asset : gPlayTestRetailAssets.files) {
        if (asset.group != "Audio") continue;
        auto filename=std::filesystem::path(asset.path).filename().wstring();
        std::transform(filename.begin(),filename.end(),filename.begin(),
            [](wchar_t character){return wchar_t(std::towlower(character));});
        if (filename != L"sfx3_ps.sdt") continue;
        std::error_code error;
        if (std::filesystem::is_regular_file(asset.path,error))
            return std::filesystem::path(asset.path);
    }

    auto folder=std::filesystem::path(executableDirectory());
    for(unsigned level=0;level<8&&!folder.empty();++level) {
        const auto bank=folder/L"assets"/L"lcs_frontend"/L"SFX3_PS.SDT";
        std::error_code error;
        if (std::filesystem::is_regular_file(bank,error)) {
            auto raw=bank;
            raw.replace_extension(L".RAW");
            if(std::filesystem::is_regular_file(raw,error))return bank;
        }
        const auto parent=folder.parent_path();
        if(parent==folder)break;
        folder=parent;
    }
    return {};
}

static void storylandLcsMenuSound(uint16_t event) {
    if(!gPlayTestRuntime.sounds || gStorylandSfxVolume<=0.0f)return;
    if(gPlayTestActive&&!gPlayTestMissionEnvironment.frontendAudioEnabled)return;
    // Original LCS PS2 SET0/SFX3_PS.SDT bank: 0=FE_BACK,
    // 3=FE_HIGHLIGHT, 7=FE_SELECT. No generated WAV substitutes.
    const size_t sample=event==196?0u:event==198?7u:3u;
    const auto bank=storylandLcsRetailSoundBankPath();
    if(bank.empty()) {
        gPlayTestSoundStatus="Original LCS menu audio SET0/SFX3_PS.SDT/RAW is missing.";
        return;
    }
    auto ticket=gPlayTestGameAudio.submit(bank.wstring(),sample,true,
        gStorylandSfxVolume,true,false,true,StorylandFrontendAmbience::Menu);
    if(ticket&&ticket->failed)gPlayTestSoundStatus=ticket->diagnostic;
}

static std::string storylandLcsMenuText(const char* key, const char* fallback) {
    std::u16string units;
    if (gPlayTestMissionGxt.lookup(key, "", units) && !units.empty()) {
        std::wstring wide;
        wide.reserve(units.size());
        for (char16_t unit : units) wide.push_back(wchar_t(unit));
        return narrow(wide);
    }
    return fallback;
}

static const char* storylandLcsMenuTabKey(StorylandLcsFrontendTab tab) {
    switch (tab) {
    case StorylandLcsFrontendTab::Map: return "FEH_MAP";
    case StorylandLcsFrontendTab::Brief: return "FEH_BRI";
    case StorylandLcsFrontendTab::Game: return "FEH_LOA";
    case StorylandLcsFrontendTab::Stats: return "FEH_STA";
    case StorylandLcsFrontendTab::Controls: return "FEH_CON";
    case StorylandLcsFrontendTab::Audio: return "FEH_AUD";
    case StorylandLcsFrontendTab::Display: return "FEH_DIS";
    }
    return "FEH_LOA";
}

static const char* storylandLcsMenuTabName(StorylandLcsFrontendTab tab) {
    switch (tab) {
    case StorylandLcsFrontendTab::Map: return "Map";
    case StorylandLcsFrontendTab::Brief: return "Brief";
    case StorylandLcsFrontendTab::Game: return "Game";
    case StorylandLcsFrontendTab::Stats: return "Stats";
    case StorylandLcsFrontendTab::Controls: return "Controls";
    case StorylandLcsFrontendTab::Audio: return "Audio";
    case StorylandLcsFrontendTab::Display: return "Display";
    }
    return "Game";
}

static int storylandLcsMenuItemCount() {
    const auto& menu = gStorylandLcsFrontend;
    if (menu.page == StorylandLcsFrontendPage::LoadSlots ||
        menu.page == StorylandLcsFrontendPage::SaveSlots ||
        menu.page == StorylandLcsFrontendPage::DeleteSlots) return 8;
    if (menu.page == StorylandLcsFrontendPage::ConfirmLoad ||
        menu.page == StorylandLcsFrontendPage::ConfirmSave ||
        menu.page == StorylandLcsFrontendPage::ConfirmDelete ||
        menu.page == StorylandLcsFrontendPage::ConfirmNew) return 2;
    if (menu.page == StorylandLcsFrontendPage::Notice) return 1;
    switch (menu.tab) {
    case StorylandLcsFrontendTab::Game: {
        const auto* screen=storylandLcsRetailFindScreen("FEH_LOA");
        return (screen?int(screen->count):3)+(menu.gameplay?2:0);
    }
    case StorylandLcsFrontendTab::Audio: return 3;
    case StorylandLcsFrontendTab::Display: return 6;
    case StorylandLcsFrontendTab::Controls: return 4;
    case StorylandLcsFrontendTab::Stats:
        return std::max(1, int(storylandStatsMenuRows(gPlayTestMissionEnvironment, false).size()));
    case StorylandLcsFrontendTab::Brief:
        return std::max(1, int(gPlayTestMissionEnvironment.briefMessages.size()));
    default: return 1;
    }
}

static void storylandLcsMenuAdjust(int direction) {
    auto& menu=gStorylandLcsFrontend;
    if(menu.page!=StorylandLcsFrontendPage::Main)return;
    const int selected=menu.selected();
    if(menu.tab==StorylandLcsFrontendTab::Audio) {
        if(selected==0)gStorylandMusicVolume=std::clamp(
            gStorylandMusicVolume+direction*.05f,0.0f,1.0f);
        else if(selected==1)gStorylandSfxVolume=std::clamp(
            gStorylandSfxVolume+direction*.05f,0.0f,1.0f);
        else if(selected==2&&!gPlayTestRadioStations.empty()) {
            const int count=int(gPlayTestRadioStations.size());
            gPlayTestRadioStation=size_t((int(gPlayTestRadioStation)+direction+count)%count);
            if(gPlayTestActive)startStorylandPlayTestRadio();
        }
        storylandRetailFrontendPreferences(true);
        return;
    }
    if(menu.tab==StorylandLcsFrontendTab::Display) {
        if(selected==1)gPlayTestMissionEnvironment.missionAudioSubtitleEnabled=direction>0;
        else if(selected==2)gStorylandDisplaySettings.aspectRatio=
            StorylandAspectRatio(direction>0?2:1);
        else if(selected==3)gPlayTestMissionEnvironment.radarDisplayMode=
            std::clamp(gPlayTestMissionEnvironment.radarDisplayMode+direction,0,2);
        else if(selected==4)gPlayTestRuntime.hud=direction>0;
        gStorylandDisplaySettings.save(storylandUserFilesDirectory(false),gStorylandDisplaySettingsError);
        storylandRetailFrontendPreferences(true);
        return;
    }
    if(menu.tab==StorylandLcsFrontendTab::Controls) {
        if(selected==2)gStorylandInvertLook=direction>0;
        else if(selected==3)gStorylandController.setVibration(direction>0);
        storylandRetailFrontendPreferences(true);
    }
}

static void storylandLcsMenuBack();

static void storylandLcsMenuAccept() {
    auto& menu = gStorylandLcsFrontend;
    const int selected = menu.selected();
    const auto page = menu.page;
    if (page == StorylandLcsFrontendPage::Notice) {
        menu.back();
        return;
    }
    if (page == StorylandLcsFrontendPage::ConfirmNew ||
        page == StorylandLcsFrontendPage::ConfirmLoad ||
        page == StorylandLcsFrontendPage::ConfirmSave ||
        page == StorylandLcsFrontendPage::ConfirmDelete) {
        if (selected == 0) { storylandLcsMenuBack(); return; }
        if (page == StorylandLcsFrontendPage::ConfirmNew) {
            PostMessageW(gMainWindow, WM_COMMAND, ID_VIEW_PLAY_TEST, 4);
            return;
        }
        if (page == StorylandLcsFrontendPage::ConfirmLoad) {
            if (menu.gameplay && !gStorylandFrontendBootPending && !gStorylandFrontend) {
                if (!storylandGameSaveSlot(menu.selectedSlot, true)) menu.notice = gStorylandSaveStatus;
                else storylandResumeFromPauseMenu();
            } else {
                PostMessageW(gMainWindow, WM_COMMAND, ID_VIEW_PLAY_TEST, 4 + menu.selectedSlot);
            }
            return;
        }
        if (page == StorylandLcsFrontendPage::ConfirmSave) {
            if (storylandCanSaveGame() && storylandGameSaveSlot(menu.selectedSlot, false)) {
                menu.notice = gStorylandSaveStatus;
                gPlayTestMissionEnvironment.saveGameFinished = true;
            } else menu.notice = gStorylandSaveStatus.empty() ?
                "Saving is not available here." : gStorylandSaveStatus;
            menu.show(StorylandLcsFrontendPage::Notice);
            return;
        }
        std::error_code error;
        const auto remove = [&](const std::filesystem::path& path) {
            if (path.empty() || error) return;
            std::filesystem::remove(path, error);
        };
        remove(storylandSaveSlotPath(menu.selectedSlot));
        remove(storylandPreviousSaveSlotPath(menu.selectedSlot));
        remove(storylandLegacySaveSlotPath(menu.selectedSlot));
        const std::string result = error ? "Failed to delete the selected save." :
            "Selected save deleted.";
        menu.show(StorylandLcsFrontendPage::Notice);
        menu.notice = result;
        return;
    }
    if (page == StorylandLcsFrontendPage::LoadSlots ||
        page == StorylandLcsFrontendPage::SaveSlots ||
        page == StorylandLcsFrontendPage::DeleteSlots) {
        menu.selectedSlot = selected + 1;
        const bool exists = storylandRetailFrontendSlotExists(menu.selectedSlot);
        if (page != StorylandLcsFrontendPage::SaveSlots && !exists) {
            menu.notice = "This save slot is empty.";
            return;
        }
        if (page == StorylandLcsFrontendPage::SaveSlots && !storylandCanSaveGame()) {
            menu.notice = "Saving is unavailable during a mission or restricted player state.";
            return;
        }
        if (page == StorylandLcsFrontendPage::LoadSlots)
            menu.show(StorylandLcsFrontendPage::ConfirmLoad);
        else if (page == StorylandLcsFrontendPage::DeleteSlots)
            menu.show(StorylandLcsFrontendPage::ConfirmDelete);
        else if (exists)
            menu.show(StorylandLcsFrontendPage::ConfirmSave);
        else {
            if (storylandGameSaveSlot(menu.selectedSlot, false)) {
                menu.show(StorylandLcsFrontendPage::Notice);
                menu.notice = gStorylandSaveStatus;
                gPlayTestMissionEnvironment.saveGameFinished = true;
            } else menu.notice = gStorylandSaveStatus;
        }
        return;
    }
    if (menu.tab == StorylandLcsFrontendTab::Game) {
        if (selected == 0) menu.show(StorylandLcsFrontendPage::LoadSlots);
        else if (selected == 1) {
            if (storylandShouldConfirmNewGame(menu.gameplay,
                storylandHasAnyGameSave(storylandRetailFrontendSlotExists)))
                menu.show(StorylandLcsFrontendPage::ConfirmNew);
            else PostMessageW(gMainWindow, WM_COMMAND, ID_VIEW_PLAY_TEST, 4);
        } else if (selected == 2) menu.show(StorylandLcsFrontendPage::DeleteSlots);
        else if (selected == 3 && menu.gameplay) {
            if (storylandCanSaveGame()) menu.show(StorylandLcsFrontendPage::SaveSlots);
            else menu.notice = "Saving is unavailable in the current player state.";
        } else if (selected == 4 && menu.gameplay) {
            storylandResumeFromPauseMenu();
        }
    } else if (menu.tab == StorylandLcsFrontendTab::Audio ||
        menu.tab == StorylandLcsFrontendTab::Display ||
        menu.tab == StorylandLcsFrontendTab::Controls) {
        if (menu.tab == StorylandLcsFrontendTab::Controls && selected < 2)
            gStorylandRemapOpen = true;
        else storylandLcsMenuAdjust(1);
    }
}

static void storylandLcsMenuBack() {
    auto& menu = gStorylandLcsFrontend;
    if (menu.page != StorylandLcsFrontendPage::Main) {
        const auto current = menu.page;
        if (current == StorylandLcsFrontendPage::ConfirmLoad) {
            menu.show(StorylandLcsFrontendPage::LoadSlots, menu.selectedSlot - 1);
        } else if (current == StorylandLcsFrontendPage::ConfirmSave) {
            menu.show(StorylandLcsFrontendPage::SaveSlots, menu.selectedSlot - 1);
        } else if (current == StorylandLcsFrontendPage::ConfirmDelete) {
            menu.show(StorylandLcsFrontendPage::DeleteSlots, menu.selectedSlot - 1);
        } else {
            if (current == StorylandLcsFrontendPage::SaveSlots && menu.scriptSave) {
                gPlayTestMissionEnvironment.saveGameFinished = true;
                menu.scriptSave = false;
            }
            menu.back();
        }
        return;
    }
    if (menu.gameplay) {
        storylandResumeFromPauseMenu();
    } else if (menu.tab != StorylandLcsFrontendTab::Game) {
        menu.tab = StorylandLcsFrontendTab::Game;
    }
}

static void storylandLcsMenuInput(int action) {
    auto& menu = gStorylandLcsFrontend;
    const int originalTab = menu.tabIndex();
    const int originalSelection = menu.selected();
    const auto originalPage = menu.page;
    if (action == 0 || action == 1) {
        menu.move(action == 0 ? -1 : 1, storylandLcsMenuItemCount());
        if (menu.selected() != originalSelection) storylandLcsMenuSound(197);
    } else if (action == 2 || action == 3) {
        if (menu.page == StorylandLcsFrontendPage::Main &&
            (menu.tab == StorylandLcsFrontendTab::Audio ||
             menu.tab == StorylandLcsFrontendTab::Display ||
             menu.tab == StorylandLcsFrontendTab::Controls)) {
            storylandLcsMenuAdjust(action == 2 ? -1 : 1);
            storylandLcsMenuSound(197);
        } else {
            menu.changeTab(action == 2 ? -1 : 1);
            if (menu.tabIndex() != originalTab) storylandLcsMenuSound(197);
        }
    } else if (action == 4) {
        storylandLcsMenuSound(198);
        storylandLcsMenuAccept();
    } else if (action == 5) {
        storylandLcsMenuSound(196);
        storylandLcsMenuBack();
    } else if (action == 6 || action == 7) {
        menu.changeTab(action == 6 ? -1 : 1);
        if (menu.tabIndex() != originalTab) storylandLcsMenuSound(197);
    }
    if (menu.page != originalPage) menu.subpageSelection = 0;
}

// Win32 sends the active menu its keyboard input before child controls and
// ImGui can consume it. Asynchronous keyboard sampling remains a fallback
// when focus is on another child HWND; both paths share the same selection.
static bool storylandLcsHandleNativeKey(HWND hwnd,UINT message,WPARAM key,LPARAM metadata) {
    if(message!=WM_KEYDOWN || gStorylandRemapOpen || gPlayTestLoading ||
        gPlayTestOpeningMovies.active() || gPlayTestMissionVm.vcs ||
        !(gStorylandFrontendBootPending || gPlayTestActive) ||
        !gPlayTestRuntime.pauseMenu) return false;
    if(gDearImGuiReady&&(ImGui::GetIO().WantTextInput||
        ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)))return false;
    int action=-1;
    switch(key) {
    case VK_UP:action=0;break;
    case VK_DOWN:action=1;break;
    case VK_LEFT:action=2;break;
    case VK_RIGHT:action=3;break;
    case VK_RETURN:action=4;break;
    case VK_ESCAPE:case VK_BACK:action=5;break;
    case VK_PRIOR:case 'Q':action=6;break;
    case VK_NEXT:case 'E':action=7;break;
    default:return false;
    }
    if(!storylandLcsInputReady())return true;
    if(!(metadata & (1LL<<30)) || action<=3) {
        storylandLcsMenuInput(action);
        if(gPreview)InvalidateRect(gPreview,nullptr,FALSE);
    }
    gStorylandLcsLastNativeKey=GetTickCount64();
    return true;
}

static void storylandLcsPollKeyboard() {
    if(!storylandLcsInputReady())return;
    if (!gMainWindow || !IsWindow(gMainWindow)) return;
    const HWND foreground = GetForegroundWindow();
    const bool foregroundActive = foreground == gMainWindow ||
        (foreground && GetAncestor(foreground, GA_ROOT) == gMainWindow);
    if (!foregroundActive) {
        gStorylandLcsHeld.fill(false);
        gStorylandLcsRepeat.fill(0);
        return;
    }
    const std::array<int, 11> keys = {
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN,
        VK_ESCAPE, VK_PRIOR, VK_NEXT, 'Q', 'E', VK_BACK
    };
    const std::array<int, 11> actions = {0,1,2,3,4,5,6,7,6,7,5};
    const ULONGLONG now = GetTickCount64();
    // If the focused HWND delivers WM_KEYDOWN normally, do not fire a second
    // selection event when this frame subsequently polls GetAsyncKeyState.
    const bool nativeKeysActive = now >= gStorylandLcsLastNativeKey &&
        now - gStorylandLcsLastNativeKey < 400ull;
    for (size_t index = 0; index < keys.size(); ++index) {
        const bool down = (GetAsyncKeyState(keys[index]) & 0x8000) != 0;
        if (!down || nativeKeysActive) {
            gStorylandLcsHeld[index] = false;
            gStorylandLcsRepeat[index] = 0;
            continue;
        }
        const bool first = !gStorylandLcsHeld[index];
        const bool repeat = index < 4 && now >= gStorylandLcsRepeat[index];
        gStorylandLcsHeld[index] = true;
        if (!first && !repeat) continue;
        gStorylandLcsRepeat[index] = now + (first ? 270ull : 105ull);
        storylandLcsMenuInput(actions[index]);
        if(gPreview)InvalidateRect(gPreview,nullptr,FALSE);
        if(!gPlayTestRuntime.pauseMenu)return;
    }
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp)) storylandLcsMenuInput(0);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown)) storylandLcsMenuInput(1);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) storylandLcsMenuInput(2);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) storylandLcsMenuInput(3);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)) storylandLcsMenuInput(4);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) storylandLcsMenuInput(5);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1)) storylandLcsMenuInput(6);
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1)) storylandLcsMenuInput(7);
    }
}

static bool storylandDrawLcsFrontend(float gameTop) {
    const bool active=(gStorylandFrontendBootPending||gPlayTestActive)&&
        (gPlayTestRuntime.pauseMenu||gStorylandFrontend)&&
        !gPlayTestOpeningMovies.active()&&!gPlayTestMissionVm.vcs;
    if (!active) {
        gStorylandLcsFrontend.close();
        gStorylandLcsHeld.fill(false);
        gStorylandLcsRepeat.fill(0);
        return false;
    }
    const bool gameplay = gPlayTestActive && !gStorylandFrontend;
    gStorylandLcsFrontend.open(gameplay);
    if (gPlayTestMissionEnvironment.saveMenuRequested) {
        gPlayTestMissionEnvironment.saveMenuRequested = false;
        gStorylandLcsFrontend.show(StorylandLcsFrontendPage::SaveSlots);
        gStorylandLcsFrontend.scriptSave = true;
    }
    storylandLcsPollKeyboard();
    if(!gPlayTestRuntime.pauseMenu&&!gStorylandFrontend)return false;

    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    if (screen.x <= 1.0f || screen.y <= gameTop) return true;
    auto* draw = ImGui::GetBackgroundDrawList();
    draw->AddRectFilled(ImVec2(0, gameTop), screen, IM_COL32_BLACK);
    const float scale = std::max(.01f, std::min(screen.x / 480.0f,
        (screen.y - gameTop) / 272.0f));
    const float x = (screen.x - 480.0f * scale) * .5f;
    const float y = gameTop + (screen.y - gameTop - 272.0f * scale) * .5f;
    const ImVec2 first(x,y), last(x+480.0f*scale,y+272.0f*scale);
    draw->PushClipRect(first,last,true);

    const ImTextureID background=storylandRetailSpriteTexture(
        &gPlayTestHudAssets,"frontend_background");
    const bool hdBackground=storylandDrawFrontendBackground(draw,&gPlayTestHudAssets,first,last);
    if(!hdBackground&&background)draw->AddImage(background,first,last);
    else if(!hdBackground) {
        // LCS font/menu sprites are optional in incomplete GAME.DTZ dumps.
        // Never hide the actual menu just because its backdrop was not decoded.
        draw->AddRectFilled(first,last,IM_COL32(16,27,51,255));
        draw->AddRectFilled(ImVec2(x,y+209.0f*scale),last,
            IM_COL32(8,15,33,255));
        draw->AddLine(ImVec2(x+12*scale,y+37*scale),
            ImVec2(x+468*scale,y+37*scale),IM_COL32(80,101,137,255),scale);
    }

    bool nativeFontReady=false;
    for(const auto& entry:gPlayTestHudAssets.sprites) {
        const auto& name=entry.first;
        if(name.find("gametext")!=std::string::npos||
            name.find("fancytext")!=std::string::npos||
            name.find("font1")!=std::string::npos||
            name.find("font2")!=std::string::npos||
            name.find("font3")!=std::string::npos) {
            nativeFontReady=true;
            break;
        }
    }
    const auto text=[&](const std::string& label,float px,float py,float height,
        ImU32 color,const char* font,bool centered=false) {
        if(label.empty())return;
        font=storylandFrontendFont(&gPlayTestHudAssets,font);
        if(nativeFontReady) {
            storylandDrawPlayTestText(draw,&gPlayTestHudAssets,
                ImVec2(x+px*scale,y+py*scale),color,label.c_str(),
                height*scale,460*scale,font,centered);
            return;
        }
        ImFont* fallbackFont=ImGui::GetFont();
        const float size=std::max(11.0f,height*scale);
        const ImVec2 extent=fallbackFont->CalcTextSizeA(
            size,1000000.0f,0.0f,label.c_str());
        const float left=x+px*scale-(centered?extent.x*.5f:0.0f);
        const float top=y+py*scale;
        draw->AddText(fallbackFont,size,ImVec2(left+scale,top+scale),
            IM_COL32(0,0,0,180),label.c_str());
        draw->AddText(fallbackFont,size,ImVec2(left,top),color,label.c_str());
    };
    const auto& menu = gStorylandLcsFrontend;
    static constexpr std::array<const char*,7> names={"Map","Brief","Game","Stats","Controls","Audio","Display"};
    float tabLeft = 8;
    for (int index=0;index<7;++index) {
        const auto tab=StorylandLcsFrontendTab(index);
        const bool selected=tab==menu.tab;
        const float width=index==4?73.0f:64.0f;
        const std::string caption=storylandLcsMenuText(storylandLcsMenuTabKey(tab),names[size_t(index)]);
        text(caption,tabLeft+width*.5f,7,15,selected?IM_COL32(223,241,255,255):
            IM_COL32(150,168,191,255),"gametext",true);
        if (!gStorylandLcsWaitForInputRelease&&ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::IsMouseHoveringRect(ImVec2(x+tabLeft*scale,y+2*scale),
                ImVec2(x+(tabLeft+width)*scale,y+29*scale)) &&
            menu.page==StorylandLcsFrontendPage::Main) {
            gStorylandLcsFrontend.tab=tab;
            storylandLcsMenuSound(197);
        }
        tabLeft+=width+2;
    }
    text(storylandLcsMenuText(storylandLcsMenuTabKey(menu.tab),
        storylandLcsMenuTabName(menu.tab)),21,50,30,IM_COL32(239,243,255,255),"fancytext");

    std::vector<std::string> items;
    std::vector<StorylandStatsMenuRow> statsRows;
    switch(menu.page) {
    case StorylandLcsFrontendPage::LoadSlots:
    case StorylandLcsFrontendPage::SaveSlots:
    case StorylandLcsFrontendPage::DeleteSlots:
        for (int slot=1;slot<=8;++slot) {
            const bool exists=storylandRetailFrontendSlotExists(slot);
            items.push_back(exists ? "Save File " + std::to_string(slot) :
                storylandLcsMenuText(("FEM_SL"+std::to_string(slot)).c_str(),
                    ("Save File "+std::to_string(slot)+" Not Present").c_str()));
        }
        break;
    case StorylandLcsFrontendPage::ConfirmNew:
    case StorylandLcsFrontendPage::ConfirmLoad:
    case StorylandLcsFrontendPage::ConfirmSave:
    case StorylandLcsFrontendPage::ConfirmDelete:
        items={storylandLcsMenuText("FEM_NO","NO"),storylandLcsMenuText("FEM_YES","YES")};
        break;
    case StorylandLcsFrontendPage::Notice:
        items={storylandLcsMenuText("FEM_OK","OK")};
        break;
    case StorylandLcsFrontendPage::Main:
        switch(menu.tab) {
        case StorylandLcsFrontendTab::Game:
            if(const auto* retail=storylandLcsRetailFindScreen("FEH_LOA"))
                for(size_t i=0;i<retail->count;++i)
                    items.push_back(storylandLcsMenuText(retail->rows[i].gxtKey,retail->rows[i].gxtKey));
            if(menu.gameplay) {
                items.push_back(storylandLcsMenuText("FES_SAV","SAVE GAME"));
                items.push_back(storylandLcsMenuText("FEI_RES","RESUME"));
            }
            break;
        case StorylandLcsFrontendTab::Audio:
            if(const auto* retail=storylandLcsRetailFindScreen("FEH_AUD"))
                for(size_t i=0;i<retail->count;++i) {
                    std::string value=storylandLcsMenuText(retail->rows[i].gxtKey,retail->rows[i].gxtKey);
                    if(i==0)value+="  "+std::to_string(int(std::lround(gStorylandMusicVolume*100)))+"%";
                    if(i==1)value+="  "+std::to_string(int(std::lround(gStorylandSfxVolume*100)))+"%";
                    if(i==2)value+="  "+(gPlayTestRadioStation<gPlayTestRadioStations.size()? 
                        gPlayTestRadioStations[gPlayTestRadioStation].label:storylandLcsMenuText("FEA_NON","NONE"));
                    items.push_back(std::move(value));
                }
            break;
        case StorylandLcsFrontendTab::Display:
            if(const auto* retail=storylandLcsRetailFindScreen("FEH_DIS"))
                for(size_t i=0;i<retail->count;++i) {
                    std::string value=storylandLcsMenuText(retail->rows[i].gxtKey,retail->rows[i].gxtKey);
                    if(i==1)value+=std::string("  ")+(gPlayTestMissionEnvironment.missionAudioSubtitleEnabled?"ON":"OFF");
                    if(i==2)value+=std::string("  ")+(int(gStorylandDisplaySettings.aspectRatio)==2?"ON":"OFF");
                    if(i==3)value+="  "+std::to_string(gPlayTestMissionEnvironment.radarDisplayMode);
                    if(i==4)value+=std::string("  ")+(gPlayTestRuntime.hud?"ON":"OFF");
                    items.push_back(std::move(value));
                }
            break;
        case StorylandLcsFrontendTab::Controls:
            if(const auto* retail=storylandLcsRetailFindScreen("FEH_CON"))
                for(size_t i=0;i<retail->count;++i) {
                    std::string value=storylandLcsMenuText(retail->rows[i].gxtKey,retail->rows[i].gxtKey);
                    if(i==2)value+=std::string("  ")+(gStorylandInvertLook?"ON":"OFF");
                    if(i==3)value+=std::string("  ")+(gStorylandController.vibrationEnabled?"ON":"OFF");
                    items.push_back(std::move(value));
                }
            break;
        case StorylandLcsFrontendTab::Map:items={""};break;
        case StorylandLcsFrontendTab::Brief: {
            for(const auto& entry:gPlayTestMissionEnvironment.briefMessages) {
                const auto units=storylandMissionFormattedText(gPlayTestMissionEnvironment,
                    entry.key,entry.numbers,entry.stringKey);
                items.push_back(narrow(std::wstring(units.begin(),units.end())));
            }
            if(items.empty())items={"NO BRIEF MESSAGES"};
            break;
        }
        case StorylandLcsFrontendTab::Stats: {
            statsRows=storylandStatsMenuRows(gPlayTestMissionEnvironment,false);
            for(const auto& row:statsRows)
                items.push_back(storylandLcsMenuText(row.labelKey,row.fallback));
            break;
        }
        }
    }
    if(menu.page!=StorylandLcsFrontendPage::Main) {
        const char* heading="GAME";
        if(menu.page==StorylandLcsFrontendPage::LoadSlots)heading="LOAD GAME";
        else if(menu.page==StorylandLcsFrontendPage::SaveSlots)heading="SAVE GAME";
        else if(menu.page==StorylandLcsFrontendPage::DeleteSlots)heading="DELETE GAME";
        else if(menu.page==StorylandLcsFrontendPage::ConfirmNew)heading="NEW GAME";
        else if(menu.page==StorylandLcsFrontendPage::ConfirmLoad)heading="LOAD GAME";
        else if(menu.page==StorylandLcsFrontendPage::ConfirmSave)heading="SAVE GAME";
        else if(menu.page==StorylandLcsFrontendPage::ConfirmDelete)heading="DELETE GAME";
        text(heading,21,50,30,IM_COL32(245,247,255,255),"fancytext");
    }
    if(menu.page==StorylandLcsFrontendPage::Main && menu.tab==StorylandLcsFrontendTab::Map) {
        const int tiles=gPlayTestHudAssets.radarGrid;
        if(tiles>0&&tiles<=16&&!gPlayTestHudAssets.radarTiles.empty()) {
            const float side=190.0f/tiles;
            for(const auto& entry:gPlayTestHudAssets.radarTiles) {
                const int tile=entry.first;
                if(tile<0||tile>=tiles*tiles)continue;
                const ImTextureID texture=storylandRetailSpriteTexture(&gPlayTestHudAssets,entry.second);
                if(!texture)continue;
                const int tx=tile%tiles,ty=tile/tiles;
                draw->AddImage(texture,
                    ImVec2(x+(147+tx*side)*scale,y+(44+ty*side)*scale),
                    ImVec2(x+(147+(tx+1)*side)*scale,y+(44+(ty+1)*side)*scale));
            }
        } else {
            text(menu.gameplay?"LIBERTY CITY":"START A GAME TO VIEW THE MAP",240,145,20,
                IM_COL32(188,204,230,255),"gametext",true);
        }
    } else {
        float startY=menu.page==StorylandLcsFrontendPage::Main?93.0f:91.0f;
        float spacing=27.0f;
        float lineHeight=22.0f;
        if(items.size()>=6) {startY=83;spacing=19;lineHeight=17;}
        if(menu.page==StorylandLcsFrontendPage::ConfirmNew) {
            const auto question=storylandLcsMenuText("FESZ_QR","Start a new game?");
            text(question,240,70,15,IM_COL32(216,225,243,255),"gametext",true);
            startY=136;
        } else if(menu.page==StorylandLcsFrontendPage::ConfirmLoad ||
            menu.page==StorylandLcsFrontendPage::ConfirmSave ||
            menu.page==StorylandLcsFrontendPage::ConfirmDelete) {
            const char* key=menu.page==StorylandLcsFrontendPage::ConfirmLoad?"FESZ_QL":
                menu.page==StorylandLcsFrontendPage::ConfirmDelete?"FESZ_QD":"FESZ_QO";
            text(storylandLcsMenuText(key,"Proceed?"),240,70,15,
                IM_COL32(216,225,243,255),"gametext",true);
            startY=136;
        } else if(menu.page==StorylandLcsFrontendPage::Notice) {
            text(menu.notice,240,98,18,IM_COL32(212,232,255,255),"gametext",true);
            startY=148;
        }
        const int selected=menu.selected();
        const int firstItem=items.size()>7?std::clamp(selected-4,0,int(items.size())-7):0;
        const int shown=std::min<int>(7,int(items.size())-firstItem);
        for(int row=0;row<shown;++row) {
            const int index=firstItem+row;
            float centerY=startY+row*spacing;
            if(menu.page==StorylandLcsFrontendPage::Main&&!menu.gameplay) {
                const auto* retail=storylandLcsRetailFindScreen(storylandLcsMenuTabKey(menu.tab));
                if(retail&&size_t(index)<retail->count&&retail->rows[index].y>0&&
                   retail->rows[index].y<200)centerY=float(retail->rows[index].y)+23.0f;
            }
            const bool current=index==selected;
            const bool unavailable=(menu.page==StorylandLcsFrontendPage::LoadSlots ||
                menu.page==StorylandLcsFrontendPage::DeleteSlots) &&
                !storylandRetailFrontendSlotExists(index+1);
            const unsigned pulse=unsigned((GetTickCount64()/380u)%2u);
            const ImU32 color=unavailable?IM_COL32(112,123,143,255):
                current?(pulse?IM_COL32(255,255,255,255):IM_COL32(173,226,255,255)):
                    IM_COL32(141,155,177,255);
            const bool statsRow=menu.page==StorylandLcsFrontendPage::Main&&menu.tab==StorylandLcsFrontendTab::Stats;
            if(statsRow)draw->PushClipRect(ImVec2(x+130*scale,y+(centerY-2)*scale),
                ImVec2(x+354*scale,y+(centerY+lineHeight+2)*scale),true);
            text(items[size_t(index)],145,centerY,lineHeight,color,
                menu.page==StorylandLcsFrontendPage::Main&&
                menu.tab==StorylandLcsFrontendTab::Game?"fancytext":"gametext");
            if(statsRow) {
                draw->PopClipRect();
                if(size_t(index)<statsRows.size()) {
                    const auto& value=statsRows[size_t(index)].value;
                    const float valueWidth=nativeFontReady ?
                        storylandMeasurePlayTestText(&gPlayTestHudAssets,value.c_str(),lineHeight*scale,100*scale,
                            storylandFrontendFont(&gPlayTestHudAssets)).x :
                        ImGui::GetFont()->CalcTextSizeA(std::max(11.0f,lineHeight*scale),1000000.0f,0.0f,value.c_str()).x;
                    text(value,462-valueWidth/scale,centerY,lineHeight,color,"gametext");
                }
            }
            if(!gStorylandLcsWaitForInputRelease&&ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                ImGui::IsMouseHoveringRect(
                    ImVec2(x+127*scale,y+(centerY-6)*scale),
                    ImVec2(x+475*scale,y+(centerY+spacing-1)*scale))) {
                gStorylandLcsFrontend.setSelected(index,storylandLcsMenuItemCount());
                if(index==selected)storylandLcsMenuInput(4);
                else storylandLcsMenuSound(197);
            }
        }
    }
    if(!menu.notice.empty()&&menu.page!=StorylandLcsFrontendPage::Notice)
        text(menu.notice,240,219,15,IM_COL32(239,207,156,255),"gametext",true);
    text(storylandLcsMenuText("FEI_SEL","Select"),360,248,14,
        IM_COL32(227,237,255,255),"gametext");
    text(storylandLcsMenuText("FEI_BAC","Back"),420,248,14,
        IM_COL32(227,237,255,255),"gametext");
    draw->PopClipRect();
    return true;
}
