#pragma once
#include "src/storyland/storyland_script_report.h"
#include <imgui.h>

class StorylandScriptLogPopup {
public:
    void request() {manual=true;}
    bool isOpen() const {return showing;}
    void draw(const StorylandMissionVm& mission,const StorylandMissionProgram& program,
        const StorylandMissionEnvironment& world,const std::filesystem::path& directory) {
        const auto failure=storylandScriptFailureKey(mission,program);
        const bool newFailure=!failure.empty() && failure!=lastFailure;
        lastFailure=failure;
        if(newFailure||manual) {
            failedSnapshot=newFailure;
            snapshot=storylandScriptReport(mission,program,world);
            saved.clear();error.clear();
            ImGui::OpenPopup("Script stopped##StorylandScriptLog");
            showing=true;
            manual=false;
        }
        if(ImGui::BeginPopupModal("Script stopped##StorylandScriptLog",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s",failedSnapshot?"A script stopped. Save a log for debugging?":
                "Save a log of the current script state for debugging?");
            if(!failure.empty()&&!mission.diagnostic.empty())ImGui::TextWrapped("%s",mission.diagnostic.c_str());
            if(ImGui::Button("Save log"))storylandWriteScriptReport(directory,snapshot,saved,error);
            ImGui::SameLine();
            if(ImGui::Button("Close")) {showing=false;ImGui::CloseCurrentPopup();}
            if(!saved.empty())ImGui::TextWrapped("Saved: %s",saved.u8string().c_str());
            if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
            ImGui::EndPopup();
        }
    }
private:
    bool manual=false,showing=false,failedSnapshot=false;
    std::string lastFailure,snapshot,error;
    std::filesystem::path saved;
};
