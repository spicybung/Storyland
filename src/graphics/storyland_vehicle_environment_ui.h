#pragma once

#include "src/graphics/storyland_vehicle_environment.h"
#include "imgui.h"

inline void storylandDrawVehicleEnvironmentViewer(
    StorylandVehicleEnvironmentState& state, bool textureOriginTop=false) {
    if (!state.viewerOpen) return;
    ImGui::SetNextWindowSize(ImVec2(640,520),ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Vehicle cube-map viewer",&state.viewerOpen)) {
        ImGui::Checkbox("Scene reflections",&state.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Freeze capture",&state.frozen);
        constexpr int sizes[]{32,64,128,256};
        int selected=1;
        for (int index=0;index<4;++index) if(state.faceSize==sizes[index]) selected=index;
        if (ImGui::Combo("Face resolution",&selected,"32\0" "64\0" "128\0" "256\0")) {
            state.faceSize=sizes[selected];
            state.frozen=false;
            state.ready=false;
            state.nextCaptureFace=0;
            state.completedFaces=0;
        }
        ImGui::Text("Probe %.2f, %.2f, %.2f | %d x %d per face",state.position[0],
            state.position[1],state.position[2],state.faceSize,state.faceSize);
        ImGui::Text("%llu captures | %zu submitted triangles",static_cast<unsigned long long>(state.captures),
            state.renderedTriangles);
        ImGui::TextWrapped("One live probe renders the loaded scenery and sky around the view. "
            "Vehicle materials retain their original reflection coefficient. Named material rasters "
            "use their authored environment texture.");
        if (!state.diagnostic.empty()) ImGui::TextWrapped("%s",state.diagnostic.c_str());
        if (state.ready && state.texture) {
            constexpr const char* labels[]{"+X","-X","+Y","-Y","+Z","-Z"};
            const float side=std::max(48.0f,(ImGui::GetContentRegionAvail().x-24.0f)/3.0f);
            for (unsigned face=0;face<6;++face) {
                if (face%3) ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextUnformatted(labels[face]);
                const float left=float(face%3)/3.0f, right=float(face%3+1)/3.0f;
                const float bottom=float(face/3)/2.0f, top=float(face/3+1)/2.0f;
                ImGui::Image(static_cast<ImTextureID>(state.texture),ImVec2(side,side),
                    ImVec2(left,textureOriginTop?1-top:top),
                    ImVec2(right,textureOriginTop?1-bottom:bottom));
                ImGui::EndGroup();
            }
        } else ImGui::TextUnformatted("A scene capture becomes available while a game scene is loaded.");
    }
    ImGui::End();
}
