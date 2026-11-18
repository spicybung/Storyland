#pragma once

#include "src/platform/linux/storyland_linux_session.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <string>

struct StorylandPortableDtzEditor {
    bool open = false;
    int category = 0;
    uint32_t selectedRecord = UINT32_MAX;
    uint32_t selectedField = UINT32_MAX;
    std::array<char, 128> value{};
    std::array<char, 4096> outputPath{};
    std::string status;
    uint64_t revision = UINT64_MAX;

    void reset() {
        selectedRecord = UINT32_MAX;
        selectedField = UINT32_MAX;
        revision = UINT64_MAX;
        outputPath.fill(0);
        value.fill(0);
        status.clear();
    }

    void draw(StorylandLinuxSession& session) {
        if (!open) return;
        ImGui::SetNextWindowSize(ImVec2(540, 530), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Handling and pedestrian stats", &open)) {
            ImGui::End();
            return;
        }
        ImGui::TextUnformatted(session.vcs() ? "Vice City Stories - GAME.DTZ" : "Liberty City Stories - GAME.DTZ");
        ImGui::TextWrapped("The numbers below are the original game handling and pedestrian stat values. Changes are saved into a new GAME.DTZ file.");
        if (session.active()) ImGui::TextColored(ImVec4(1.0f, .7f, .3f, 1.0f),
            "Stop Play Game / Play Test before editing or saving GAME.DTZ.");
        const char* categories[] = {"Car handling", "Pedestrian stats"};
        if (ImGui::Combo("Category", &category, categories, 2)) {
            selectedRecord = UINT32_MAX;
            selectedField = UINT32_MAX;
            value.fill(0);
        }
        const auto& fields = session.archive.dataFields();
        const std::string expected = category == 0 ? "Vehicle handling" : "Ped stats";
        std::map<uint32_t, std::string> records;
        for (const auto& field : fields)
            if (field.blockName == expected) records.emplace(field.rowIndex, field.rowLabel);
        if (records.empty()) {
            ImGui::TextWrapped("This GAME.DTZ has no supported %s records.", expected.c_str());
            ImGui::End();
            return;
        }
        if (!records.count(selectedRecord)) {
            selectedRecord = records.begin()->first;
            selectedField = UINT32_MAX;
        }
        if (ImGui::BeginCombo("Model / stat profile", records.at(selectedRecord).c_str())) {
            for (const auto& record : records) {
                if (ImGui::Selectable(record.second.c_str(), selectedRecord == record.first)) {
                    selectedRecord = record.first;
                    selectedField = UINT32_MAX;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextUnformatted("Field                                    Native value");
        ImGui::BeginChild("##game-data-fields", ImVec2(0, 240), true);
        for (const auto& field : fields) {
            if (field.blockName != expected || field.rowIndex != selectedRecord) continue;
            ImGui::PushID(int(field.absoluteOffset));
            const bool selected = selectedField == field.absoluteOffset;
            const std::string label = field.name + " | " + field.valueText;
            if (ImGui::Selectable(label.c_str(), selected)) {
                selectedField = field.absoluteOffset;
                const auto units = field.valueText.find(" /");
                const auto text = field.valueText.substr(0, units);
                std::snprintf(value.data(), value.size(), "%s", text.c_str());
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        size_t editIndex = fields.size();
        for (size_t index = 0; index < fields.size(); ++index) {
            if (fields[index].absoluteOffset == selectedField &&
                fields[index].rowIndex == selectedRecord && fields[index].blockName == expected) {
                editIndex = index;
                break;
            }
        }
        if (editIndex < fields.size()) {
            const auto& field = fields[editIndex];
            ImGui::Text("%s (stored %s)", field.name.c_str(), field.type.c_str());
            if (field.hasLimits)
                ImGui::TextDisabled("Allowed: %.6g to %.6g", field.minimum, field.maximum);
            ImGui::BeginDisabled(session.active() || !field.editable);
            ImGui::InputText("Value", value.data(), value.size());
            if (ImGui::Button("Apply field")) {
                std::string report, error;
                if (session.archive.patchDataField(editIndex, value.data(), report, error)) {
                    status = "Applied to memory. Save GAME.DTZ to keep the change.";
                    selectedField = UINT32_MAX;
                    revision = session.archive.contentRevision();
                } else status = error;
            }
            ImGui::EndDisabled();
        }
        if (outputPath[0] == 0) {
            const std::filesystem::path filename = session.gameRoot() / "GAME-edited.DTZ";
            std::snprintf(outputPath.data(), outputPath.size(), "%s", filename.u8string().c_str());
        }
        ImGui::Separator();
        ImGui::InputText("Output DTZ", outputPath.data(), outputPath.size());
        ImGui::BeginDisabled(session.active() || outputPath[0] == 0);
        if (ImGui::Button("Save GAME.DTZ As...")) {
            std::string error;
            const auto target = std::filesystem::u8path(outputPath.data());
            if (session.archive.saveToFile(target.wstring(), true, error))
                status = "Saved: " + target.u8string();
            else status = "GAME.DTZ save failed: " + error;
        }
        ImGui::EndDisabled();
        if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
        ImGui::End();
    }
};
