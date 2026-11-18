#pragma once

#include "imgui.h"
#include "src/graphics/storyland_new_game_policy.h"
#include "src/graphics/storyland_frontend_widgets.h"
#include "src/storyland/storyland_ui_glow.h"
#include <algorithm>
#include <array>
#include <functional>
#include <cstring>
#include <cstdint>
#include <string>

struct StorylandMenuFooterLayout {
    std::array<float, 3> offsets{};
    float fontSize = 0.0f;
    float height = 0.0f;
};

inline StorylandMenuFooterLayout storylandMenuFooterLayout(float availableWidth, float fontSize,
        const std::array<float, 3>& textWidths) {
    constexpr float spacing = 8.0f;
    const float contentWidth = textWidths[0] + textWidths[1] + textWidths[2] + spacing * 2.0f;
    const float scale = contentWidth > 0.0f ? std::clamp(availableWidth / contentWidth, 0.0f, 1.0f) : 1.0f;
    StorylandMenuFooterLayout layout;
    const float gap = spacing * scale + std::max(0.0f, availableWidth - contentWidth) * 0.5f;
    layout.offsets[1] = textWidths[0] * scale + gap;
    layout.offsets[2] = (textWidths[0] + textWidths[1]) * scale + gap * 2.0f;
    layout.fontSize = fontSize * scale;
    layout.height = layout.fontSize;
    return layout;
}

struct StorylandGameMenuServices {
    bool loaded = false;
    bool active = false;
    bool canSave = false;
    bool vcsVisualGlow = false;
    const StorylandRetailHudAssets* artwork = nullptr;
    std::string title = "Storyland";
    std::string saveRestriction;
    std::function<bool(std::string&)> newGame;
    std::function<bool(int, std::string&)> loadGame, saveGame, deleteGame;
    std::function<std::string(int)> slotLabel;
    std::function<bool(int)> slotExists;
    std::function<void()> resume, returnToMain, quit, gallery, debug;
    std::function<void()> finishSaveMenu;
    std::function<void()> drawSettings, drawGameSource, drawStats;
    std::function<void(uint16_t)> frontendSound;
};

class StorylandGameMenu {
public:
    enum class Page { Closed, Main, Pause, Settings, Stats, Load, Save, Delete, ConfirmNew, ConfirmLoad, ConfirmSave, ConfirmDelete, ConfirmMain, ConfirmQuit };

    Page page() const { return currentPage; }
    bool open() const { return currentPage != Page::Closed; }
    bool scriptedSave() const { return savingFromScript; }
    const std::string& status() const { return message; }

    void showMain() { savingFromScript = false; homePage = Page::Main; show(Page::Main); }
    void showPause() { homePage = Page::Pause; show(Page::Pause); }
    void showSave(bool fromScript) {
        savingFromScript = fromScript;
        homePage = Page::Pause;
        show(Page::Save);
    }
    void close() { currentPage = Page::Closed; focusFirst = false; }

    void back(const StorylandGameMenuServices& services) {
        if(open()&&services.frontendSound)services.frontendSound(196);
        message.clear();
        switch (currentPage) {
        case Page::Closed: break;
        case Page::Main: show(Page::ConfirmQuit); break;
        case Page::Pause: close(); if (services.resume) services.resume(); break;
        case Page::ConfirmLoad: show(Page::Load); break;
        case Page::ConfirmSave: show(Page::Save); break;
        case Page::ConfirmDelete: show(Page::Delete); break;
        case Page::Save:
            if (savingFromScript) {
                savingFromScript = false;
                if (services.finishSaveMenu) services.finishSaveMenu();
                close(); if (services.resume) services.resume();
            } else show(homePage);
            break;
        default: show(homePage); break;
        }
    }

    void requestNewGame(const StorylandGameMenuServices& services) {
        if (storylandShouldConfirmNewGame(services.active, storylandHasAnyGameSave(services.slotExists)))
            show(Page::ConfirmNew);
        else
            startNew(services);
    }

    void draw(const StorylandGameMenuServices& services) {
        if (!open()) return;
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        auto* background = ImGui::GetBackgroundDrawList();
        const bool artwork=storylandDrawFrontendBackground(background,services.artwork,ImVec2(0,0),screen,
            IM_COL32(255,255,255,currentPage==Page::Main?255:150));
        background->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(7, 10, 18, artwork?75:currentPage == Page::Main ? 245 : 170));
        const float margin = std::min(20.0f, std::min(screen.x, screen.y) * .04f);
        const float width = std::max(1.0f, std::min(460.0f, screen.x - margin * 2));
        const float height = std::max(1.0f, std::min(650.0f, screen.y - margin * 2));
        ImGui::SetNextWindowPos(ImVec2(screen.x * .5f, screen.y * .5f), ImGuiCond_Always, ImVec2(.5f, .5f));
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(artwork?.82f:.97f);
        const auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 18));
        if (ImGui::Begin("##StorylandGameMenu", nullptr, flags)) {
            const float titleHeight=storylandFrontendFontHeight(services.artwork)*1.5f;
            ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.96f, .44f, .72f, 1));
            if(services.vcsVisualGlow) {
                auto* draw=ImGui::GetWindowDrawList();
                const auto position=ImGui::GetCursorScreenPos();
                const float radius=std::clamp(ImGui::GetFontSize()*.10f,1.0f,2.0f);
                const float wrap=ImGui::GetContentRegionAvail().x;
                storylandDrawUiGlow(position,ImVec2(radius,radius),IM_COL32(245,112,184,255),
                    [&](ImVec2 point,ImU32 tint) {
                        storylandDrawPlayTestText(draw,services.artwork,point,tint,services.title.c_str(),
                            titleHeight,wrap,storylandFrontendFont(services.artwork,"fancytext"));
                    });
            }
            storylandFrontendText(services.artwork,services.title.c_str(),titleHeight,"fancytext");
            ImGui::PopStyleColor();
            storylandFrontendText(services.artwork,title());
            ImGui::Separator();
            const std::array<const char*, 3> help = {
                "Arrows/D-pad: Move", "Enter/A: Select", "Esc/B: Back"
            };
            const char* helpFont = storylandFrontendFont(services.artwork);
            const float helpFontSize = storylandFrontendFontHeight(services.artwork);
            std::array<float, 3> helpWidths{};
            for (size_t index = 0; index < help.size(); ++index)
                helpWidths[index] = storylandMeasurePlayTestText(services.artwork,help[index],helpFontSize,0,helpFont).x;
            const auto footer = storylandMenuFooterLayout(ImGui::GetContentRegionAvail().x, helpFontSize, helpWidths);
            const float footerHeight = footer.height + ImGui::GetStyle().ItemSpacing.y * 3;
            ImGui::BeginChild("##MenuBody", ImVec2(0, -footerHeight), false);
            bool firstButton = true;
            const auto button = [&](const char* text, bool enabled = true) {
                ImGui::BeginDisabled(!enabled);
                const bool clicked = storylandFrontendButton(services.artwork,text,ImVec2(-1,36));
                if(enabled&&ImGui::IsItemFocused()) {
                    const ImGuiID focused=ImGui::GetItemID();
                    if(lastFocused&&focused!=lastFocused&&!focusFirst&&services.frontendSound)
                        services.frontendSound(197);
                    lastFocused=focused;
                }
                if(clicked&&std::strcmp(text,"Back")!=0&&std::strcmp(text,"Cancel")!=0&&services.frontendSound)
                    services.frontendSound(198);
                if(services.vcsVisualGlow&&enabled&&(ImGui::IsItemFocused()||ImGui::IsItemHovered())) {
                    const auto first=ImGui::GetItemRectMin(),last=ImGui::GetItemRectMax();
                    auto* draw=ImGui::GetWindowDrawList();
                    draw->AddRect(ImVec2(first.x-1,first.y-1),ImVec2(last.x+1,last.y+1),
                        IM_COL32(245,112,184,34),0,0,1.0f);
                    draw->AddRect(ImVec2(first.x-2,first.y-2),ImVec2(last.x+2,last.y+2),
                        IM_COL32(245,112,184,16),0,0,1.0f);
                }
                if (firstButton && enabled) {
                    if (focusFirst) {
                        ImGui::SetKeyboardFocusHere(-1);
                        ImGui::SetNavCursorVisible(true);
                    }
                    firstButton = false;
                }
                ImGui::EndDisabled();
                return enabled && clicked;
            };
            const Page drawnPage = currentPage;
            if (currentPage == Page::Main) {
                if (services.drawGameSource) services.drawGameSource();
                if (button("New Game", services.loaded)) {
                    requestNewGame(services);
                }
                if (button("Load Game", services.loaded)) show(Page::Load);
                if (button("Delete Game", services.loaded && bool(services.deleteGame))) show(Page::Delete);
                if (button("Settings")) show(Page::Settings);
                if (services.gallery && button("Gallery", services.loaded)) services.gallery();
                if (services.debug && button("Tools")) services.debug();
                if (button("Quit Storyland")) show(Page::ConfirmQuit);
            } else if (currentPage == Page::Pause) {
                if (button("Resume")) { close(); if (services.resume) services.resume(); }
                if (button("Save Game", services.canSave)) show(Page::Save);
                if (!services.canSave && !services.saveRestriction.empty())
                    storylandFrontendText(services.artwork,services.saveRestriction.c_str());
                if (button("Load Game")) show(Page::Load);
                if (services.drawStats && button("Stats")) show(Page::Stats);
                if (button("Settings")) show(Page::Settings);
                if (services.gallery && button("Gallery")) services.gallery();
                if (services.debug && button("Debug tools")) services.debug();
                if (button("Return to main menu")) show(Page::ConfirmMain);
                if (button("Quit Storyland")) show(Page::ConfirmQuit);
            } else if (currentPage == Page::Stats) {
                const float backHeight=36.0f+ImGui::GetStyle().ItemSpacing.y;
                const float rowsHeight=std::max(1.0f,ImGui::GetContentRegionAvail().y-backHeight);
                if(ImGui::BeginChild("##StatsRows",ImVec2(0,rowsHeight),false,ImGuiWindowFlags_NoNav)) {
                    if(focusFirst)ImGui::SetScrollY(0);
                    if(services.drawStats)services.drawStats();
                    const bool up=ImGui::IsKeyPressed(ImGuiKey_UpArrow,true)||
                        ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp,true);
                    const bool down=ImGui::IsKeyPressed(ImGuiKey_DownArrow,true)||
                        ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown,true);
                    if(up!=down)ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()+(down?40.0f:-40.0f),
                        0.0f,ImGui::GetScrollMaxY()));
                }
                ImGui::EndChild();
                if (button("Back")) back(services);
            } else if (currentPage == Page::Settings) {
                if (services.drawSettings) {
                    if(focusFirst)ImGui::SetKeyboardFocusHere();
                    services.drawSettings();
                    firstButton=false;
                }
                ImGui::Spacing();
                if (button("Back")) back(services);
            } else if (currentPage == Page::Load || currentPage == Page::Save || currentPage == Page::Delete) {
                const bool saving = currentPage == Page::Save;
                const bool deleting = currentPage == Page::Delete;
                for (int slot = 1; slot <= 8; ++slot) {
                    const bool exists = services.slotExists && services.slotExists(slot);
                    std::string caption = "Slot " + std::to_string(slot) + " - ";
                    caption += exists ? (services.slotLabel ? services.slotLabel(slot) : "Saved game") : "Empty";
                    ImGui::PushID(slot);
                    if (button(caption.c_str(), saving ? services.canSave : exists)) {
                        selectedSlot = slot;
                        if (saving && !exists) save(services);
                        else show(saving ? Page::ConfirmSave : deleting ? Page::ConfirmDelete : Page::ConfirmLoad);
                    }
                    ImGui::PopID();
                }
                if (saving && !services.canSave) storylandFrontendText(services.artwork,services.saveRestriction.c_str());
                if (button("Back")) back(services);
            } else {
                const char* question = currentPage == Page::ConfirmNew ? "Start a new game? Unsaved progress will be lost." :
                    currentPage == Page::ConfirmLoad ? "Load this save? Unsaved progress will be lost." :
                    currentPage == Page::ConfirmSave ? "Replace the saved game in this slot?" :
                    currentPage == Page::ConfirmDelete ? "Delete the saved game in this slot? This cannot be undone." :
                    currentPage == Page::ConfirmMain ? "Return to the main menu? Unsaved progress will be lost." :
                    "Quit Storyland? Unsaved progress will be lost.";
                storylandFrontendText(services.artwork,question);
                ImGui::Spacing();
                if (button("Cancel")) back(services);
                if (button("Confirm")) {
                    if (currentPage == Page::ConfirmNew) startNew(services);
                    else if (currentPage == Page::ConfirmLoad) load(services);
                    else if (currentPage == Page::ConfirmSave) save(services);
                    else if (currentPage == Page::ConfirmDelete) removeSave(services);
                    else if (currentPage == Page::ConfirmMain) {
                        if (services.returnToMain) services.returnToMain();
                        showMain();
                    } else if (currentPage == Page::ConfirmQuit && services.quit) services.quit();
                }
            }
            if (!message.empty()) {
                ImGui::Spacing();
                storylandFrontendText(services.artwork,message.c_str());
            }
            ImGui::EndChild();
            ImGui::Separator();
            const ImVec2 helpPosition = ImGui::GetCursorScreenPos();
            const ImU32 helpColor = ImGui::GetColorU32(ImGuiCol_Text);
            if (footer.fontSize > 0.0f) {
                for (size_t index = 0; index < help.size(); ++index)
                    storylandDrawPlayTestText(ImGui::GetWindowDrawList(),services.artwork,
                        ImVec2(helpPosition.x + footer.offsets[index],helpPosition.y),helpColor,
                        help[index],footer.fontSize,0,helpFont);
            }
            ImGui::Dummy(ImVec2(0.0f, footer.height));
            if (drawnPage == currentPage) focusFirst = false;
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    Page currentPage = Page::Closed, homePage = Page::Main;
    int selectedSlot = 1;
    bool focusFirst = true, savingFromScript = false;
    ImGuiID lastFocused=0;
    std::string message;

    void show(Page page) { currentPage = page; focusFirst = true; lastFocused=0; message.clear(); }
    const char* title() const {
        switch (currentPage) {
        case Page::Main: return "MAIN MENU";
        case Page::Pause: return "PAUSED";
        case Page::Settings: return "SETTINGS";
        case Page::Stats: return "STATS";
        case Page::Load: return "LOAD GAME";
        case Page::Save: return "SAVE GAME";
        case Page::Delete: return "DELETE GAME";
        default: return "CONFIRM";
        }
    }
    void startNew(const StorylandGameMenuServices& services) {
        message.clear();
        if (services.newGame && services.newGame(message)) close();
    }
    void load(const StorylandGameMenuServices& services) {
        message.clear();
        if (services.loadGame && services.loadGame(selectedSlot, message)) {
            savingFromScript = false;
            close(); if (services.resume) services.resume();
        }
    }
    void save(const StorylandGameMenuServices& services) {
        message.clear();
        if (services.saveGame && services.saveGame(selectedSlot, message)) {
            if (savingFromScript && services.finishSaveMenu) services.finishSaveMenu();
            savingFromScript = false;
            show(Page::Pause);
            message = "Game saved in slot " + std::to_string(selectedSlot) + ".";
        }
    }
    void removeSave(const StorylandGameMenuServices& services) {
        message.clear();
        if (services.deleteGame && services.deleteGame(selectedSlot, message)) {
            show(Page::Delete);
            message = "Deleted saved game in slot " + std::to_string(selectedSlot) + ".";
        }
    }
};
