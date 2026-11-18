#pragma once

#include "imgui.h"
#include "src/storyland/storyland_display_settings.h"
#include <array>

// Existing retail HUD routines use viewport-local coordinates. Scope only
// draw-list presentation, never ImGui windows or widgets.
class StorylandImGuiViewport {
public:
    explicit StorylandImGuiViewport(const StorylandViewRectangle& view)
        : origin(float(view.x), float(view.y)), previousSize(ImGui::GetIO().DisplaySize),
          previousMouse(ImGui::GetIO().MousePos) {
        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(float(view.width), float(view.height));
        io.MousePos = ImVec2(previousMouse.x - origin.x, previousMouse.y - origin.y);
        lists[0].draw = ImGui::GetBackgroundDrawList();
        lists[1].draw = ImGui::GetForegroundDrawList();
        for (auto& list : lists) {
            list.vertex = list.draw->VtxBuffer.Size;
            list.draw->AddDrawCmd();
            list.command = list.draw->CmdBuffer.Size - 1;
            list.draw->PushClipRect(ImVec2(0, 0), io.DisplaySize, false);
        }
    }

    ~StorylandImGuiViewport() {
        for (auto& list : lists) {
            const int lastCommand = list.draw->CmdBuffer.Size;
            for (int index = list.vertex; index < list.draw->VtxBuffer.Size; ++index) {
                auto& position = list.draw->VtxBuffer[index].pos;
                position.x += origin.x; position.y += origin.y;
            }
            for (int index = list.command; index < lastCommand; ++index) {
                auto& command = list.draw->CmdBuffer[index];
                if (!command.ElemCount && !command.UserCallback) continue;
                command.ClipRect.x += origin.x; command.ClipRect.z += origin.x;
                command.ClipRect.y += origin.y; command.ClipRect.w += origin.y;
            }
            list.draw->PopClipRect();
        }
        ImGui::GetIO().DisplaySize = previousSize;
        ImGui::GetIO().MousePos = previousMouse;
    }

    StorylandImGuiViewport(const StorylandImGuiViewport&) = delete;
    StorylandImGuiViewport& operator=(const StorylandImGuiViewport&) = delete;

private:
    struct DrawRange { ImDrawList* draw = nullptr; int vertex = 0, command = 0; };
    std::array<DrawRange, 2> lists{};
    ImVec2 origin, previousSize, previousMouse;
};
