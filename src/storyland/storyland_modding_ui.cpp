#include "src/storyland/storyland_modding_ui.h"
#include "src/storyland/storyland_scm_workbench.h"
#include "src/storyland/storyland_dyom.h"
#include "imgui.h"
#include "vendors/nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

namespace {

template<size_t Capacity>
bool textField(const char* label, std::string& value, bool multiline = false,
    const ImVec2& size = ImVec2(0, 0)) {
    std::array<char, Capacity> buffer{};
    const size_t length = std::min(value.size(), buffer.size() - 1);
    std::memcpy(buffer.data(), value.data(), length);
    bool changed = false;
    if (multiline) {
        ImGui::TextUnformatted(label);
        ImGui::PushID(label);
        changed = ImGui::InputTextMultiline("##Text", buffer.data(), buffer.size(), size);
        ImGui::PopID();
    } else {
        changed = ImGui::InputText(label, buffer.data(), buffer.size());
    }
    if (changed) value.assign(buffer.data());
    return changed;
}

std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().u8string();
    for (char& letter : extension)
        letter = char(std::tolower(static_cast<unsigned char>(letter)));
    return extension;
}

std::string hexOffset(size_t offset) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%08llX", static_cast<unsigned long long>(offset));
    return text;
}

std::string byteText(const std::vector<uint8_t>& bytes) {
    std::string result;
    for (uint8_t byte : bytes) {
        char text[4];
        std::snprintf(text, sizeof(text), "%02X", unsigned(byte));
        if (!result.empty()) result += ' ';
        result += text;
    }
    return result;
}

bool parseOffset(const std::string& text, size_t& offset) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        begin += 2;
        base = 16;
    }
    unsigned long long value = 0;
    const auto parsed = std::from_chars(begin, end, value, base);
    if (parsed.ec != std::errc{} || parsed.ptr != end || begin == end ||
        value > std::numeric_limits<size_t>::max()) return false;
    offset = size_t(value);
    return true;
}

const char* operandKindName(StorylandScmOperandKind kind) {
    switch (kind) {
    case StorylandScmOperandKind::Integer: return "Integer";
    case StorylandScmOperandKind::Float: return "Float";
    case StorylandScmOperandKind::Text: return "Text";
    case StorylandScmOperandKind::Local: return "Local variable";
    case StorylandScmOperandKind::Global: return "Global variable";
    case StorylandScmOperandKind::LocalArray: return "Local array";
    case StorylandScmOperandKind::GlobalArray: return "Global array";
    case StorylandScmOperandKind::FunctionHeader: return "Function header";
    }
    return "Operand";
}

const char* entityLabel(StorylandDyomEntityKind kind) {
    switch (kind) {
    case StorylandDyomEntityKind::Actor: return "Actor";
    case StorylandDyomEntityKind::Vehicle: return "Vehicle";
    case StorylandDyomEntityKind::Object: return "Object";
    case StorylandDyomEntityKind::Pickup: return "Pickup";
    }
    return "Entity";
}

const char* objectiveLabel(StorylandDyomObjectiveKind kind) {
    switch (kind) {
    case StorylandDyomObjectiveKind::ReachPoint: return "Reach checkpoint";
    case StorylandDyomObjectiveKind::ReachPointOnFoot: return "Reach checkpoint on foot";
    case StorylandDyomObjectiveKind::ReachPointInVehicle: return "Drive to checkpoint";
    case StorylandDyomObjectiveKind::EnterVehicle: return "Enter vehicle";
    case StorylandDyomObjectiveKind::DefeatActor: return "Defeat actor";
    case StorylandDyomObjectiveKind::DestroyVehicle: return "Destroy vehicle";
    case StorylandDyomObjectiveKind::CollectPickup: return "Collect pickup";
    case StorylandDyomObjectiveKind::Wait: return "Wait";
    case StorylandDyomObjectiveKind::Survive: return "Survive";
    }
    return "Objective";
}

bool positionFields(StorylandDyomPosition& position, const StorylandModdingServices& services,
    bool includeHeading = true) {
    bool changed = false;
    float coordinates[] = {position.x, position.y, position.z};
    if (ImGui::InputFloat3("Position", coordinates, "%.3f")) {
        position.x = coordinates[0];
        position.y = coordinates[1];
        position.z = coordinates[2];
        changed = true;
    }
    if (includeHeading)
        changed |= ImGui::InputFloat("Heading (degrees)", &position.heading, 1, 15, "%.2f");
    ImGui::BeginDisabled(!services.playerAvailable);
    if (ImGui::Button("Use player position")) {
        position.x = services.playerPlacement[0];
        position.y = services.playerPlacement[1];
        position.z = services.playerPlacement[2];
        if (includeHeading) position.heading = services.playerPlacement[3];
        changed = true;
    }
    ImGui::EndDisabled();
    if (!services.playerAvailable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Start Play Test to capture a position from the world.");
    return changed;
}

std::filesystem::path defaultProjectPath(const StorylandModdingServices& services) {
    if (!services.userDirectory.empty())
        return services.userDirectory / "DYOM" / "Untitled.dyom";
    if (!services.gameDirectory.empty()) return services.gameDirectory / "Untitled.dyom";
    return "Untitled.dyom";
}

}

struct StorylandModdingUi::Workspace {
    enum class Action { None, LoadScm, ImportPatch, RevertScm, NewDyom, LoadDyom };
    struct Revision {
        StorylandDyomProject project;
        uint64_t number = 0;
    };

    StorylandScmWorkbench scm;
    StorylandDyomProject dyom;
    bool scmOpen = false;
    bool dyomOpen = false;
    bool focusScm = false;
    bool focusDyom = false;
    bool dyomInitialized = false;
    std::string scmPath, scmOutputPath, scmPatchPath, scmTextPath;
    std::string dyomPath, dyomSaveAsPath, scmStatus, dyomStatus;
    bool scmError = false, dyomError = false;
    ImGuiTextFilter scopeFilter;
    std::string instructionFilter, offsetInput, operandInput;
    size_t selectedInstruction = SIZE_MAX;
    int selectedOperand = -1;
    int valueFormat = 0;
    bool scrollToInstruction = false;
    bool exportAllScopes = false;
    std::vector<StorylandScmReference> references;
    std::vector<std::string> scmIssues;
    std::vector<StorylandDyomIssue> dyomIssues;
    std::vector<Revision> undoDyom, redoDyom;
    uint64_t dyomRevision = 0, nextDyomRevision = 0, savedDyomRevision = 0;
    uint64_t scmRevision = 0, savedScmRevision = 0;
    int selectedEntity = -1, selectedObjective = -1, addEntityKind = 0;
    size_t selectedVoiceProfile = 0;
    int selectedVoiceLanguage = 0;
    std::string selectedVoiceLine, selectedVoiceEmotion, voiceStatus;
    bool voiceError = false;
    Action pending = Action::None;
    std::filesystem::path pendingPath;

    bool unsavedScm() const { return scm.loaded() && scm.dirty() && scmRevision != savedScmRevision; }
    bool unsavedDyom() const { return dyomInitialized && dyomRevision != savedDyomRevision; }

    void request(Action action, const std::filesystem::path& path = {}) {
        pending = action;
        pendingPath = path;
    }

    void scmResult(bool success, const std::string& error, const std::string& message) {
        scmError = !success;
        scmStatus = success ? message : error;
    }

    void dyomResult(bool success, const std::string& error, const std::string& message) {
        dyomError = !success;
        dyomStatus = success ? message : error;
    }

    void clearScmSelection() {
        selectedInstruction = SIZE_MAX;
        selectedOperand = -1;
        operandInput.clear();
        references.clear();
        scmIssues.clear();
    }

    void changedScm() {
        ++scmRevision;
        selectedOperand = -1;
        references.clear();
        scmIssues.clear();
    }

    void initializeDyom(const StorylandModdingServices& services) {
        dyom = {};
        if (services.playerAvailable) {
            dyom.player.position = {services.playerPlacement[0], services.playerPlacement[1],
                services.playerPlacement[2], services.playerPlacement[3]};
        }
        dyomPath = defaultProjectPath(services).u8string();
        dyomInitialized = true;
        dyomRevision = ++nextDyomRevision;
        savedDyomRevision = dyomRevision;
        undoDyom.clear();
        redoDyom.clear();
        dyomIssues.clear();
        selectedEntity = selectedObjective = -1;
        dyomStatus = "New mission. Set the player start and add an objective.";
        dyomError = false;
    }

    void perform(const StorylandModdingServices& services) {
        const Action action = pending;
        const auto path = pendingPath;
        pending = Action::None;
        pendingPath.clear();
        std::string error;
        if (action == Action::LoadScm) {
            StorylandScmWorkbench candidate;
            if (!candidate.load(path, error)) {
                scmResult(false, error, {});
                return;
            }
            scm = std::move(candidate);
            scmPath = path.u8string();
            const auto directory = path.parent_path();
            const std::string stem = path.stem().u8string();
            scmOutputPath = (directory / (stem + "-edited.scm")).u8string();
            scmPatchPath = (directory / (stem + ".scmpatch")).u8string();
            scmTextPath = (directory / (stem + "-disassembly.txt")).u8string();
            scmRevision = savedScmRevision = 0;
            clearScmSelection();
            scmResult(true, {}, "SCM opened. Select MAIN or a mission to inspect its instructions.");
        } else if (action == Action::ImportPatch) {
            const bool success = scm.importPatch(path, error);
            if (success) changedScm();
            scmResult(success, error, "Patch applied to this document. Save a copy to write an edited SCM.");
        } else if (action == Action::RevertScm) {
            scm.revert();
            changedScm();
            scmResult(true, {}, "Restored the opened SCM bytes.");
        } else if (action == Action::NewDyom) {
            initializeDyom(services);
        } else if (action == Action::LoadDyom) {
            StorylandDyomProject candidate;
            if (!storylandLoadDyomProject(path, candidate, error)) {
                dyomResult(false, error, {});
                return;
            }
            dyom = std::move(candidate);
            dyomPath = path.u8string();
            dyomInitialized = true;
            dyomRevision = ++nextDyomRevision;
            savedDyomRevision = dyomRevision;
            undoDyom.clear();
            redoDyom.clear();
            dyomIssues.clear();
            selectedEntity = selectedObjective = -1;
            dyomResult(true, {}, "Mission opened: " + dyom.title);
        }
    }

    void drawPending(const StorylandModdingServices& services) {
        if (pending == Action::None) return;
        const bool scmAction = pending == Action::LoadScm || pending == Action::RevertScm;
        const bool dyomAction = pending == Action::NewDyom || pending == Action::LoadDyom;
        const bool needsDecision = (scmAction && unsavedScm()) || (dyomAction && unsavedDyom());
        if (!needsDecision) {
            perform(services);
            return;
        }
        if (!ImGui::IsPopupOpen("Unsaved modding changes"))
            ImGui::OpenPopup("Unsaved modding changes");
        if (ImGui::BeginPopupModal("Unsaved modding changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(scmAction ? "The SCM document has unsaved edits." : "The mission project has unsaved edits.");
            ImGui::TextUnformatted("Keep editing to save them, or discard them to continue.");
            if (ImGui::Button("Keep editing", ImVec2(150, 0))) {
                pending = Action::None;
                pendingPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard and continue", ImVec2(180, 0))) {
                perform(services);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    bool choosePath(std::string& path, StorylandModdingFileKind kind, bool save,
        const StorylandModdingServices& services, const char* button) {
        if (!services.chooseFile) return false;
        ImGui::SameLine();
        if (!ImGui::Button(button)) return false;
        const auto selected = services.chooseFile(kind, save);
        if (selected.empty()) return false;
        path = selected.u8string();
        return true;
    }

    void navigateScm(size_t offset) {
        std::string error;
        if (!scm.selectOffset(offset, error)) {
            scmResult(false, error, {});
            return;
        }
        selectedInstruction = offset;
        selectedOperand = -1;
        instructionFilter.clear();
        scrollToInstruction = true;
    }

    void selectOperand(const StorylandScmOperandView& operand, int index) {
        selectedOperand = index;
        valueFormat = 0;
        operandInput = operand.value;
        if (operand.hasTarget) {
            valueFormat = int(StorylandScmValueFormat::Target);
            operandInput = hexOffset(operand.target);
        } else if (operand.kind == StorylandScmOperandKind::Text) {
            valueFormat = int(StorylandScmValueFormat::Text);
            operandInput = operand.text;
        }
    }

    void drawScmInstruction() {
        const auto& rows = scm.instructions();
        const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
            return row.offset == selectedInstruction;
        });
        if (found == rows.end()) {
            ImGui::TextUnformatted("Select an instruction to inspect and tune its operands.");
            return;
        }
        const auto& instruction = *found;
        ImGui::Text("%s | opcode %04X%s | %zu bytes", hexOffset(instruction.offset).c_str(),
            unsigned(instruction.opcode), instruction.inverted ? " (NOT)" : "", instruction.end - instruction.offset);
        ImGui::TextWrapped("%s", instruction.text.c_str());
        ImGui::TextWrapped("Bytes: %s", instruction.hex.c_str());
        if (!instruction.diagnostic.empty())
            ImGui::TextColored(ImVec4(1, .6f, .3f, 1), "%s", instruction.diagnostic.c_str());
        if (ImGui::Button("Copy instruction")) ImGui::SetClipboardText(instruction.text.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Find jumps here")) references = scm.referencesTo(instruction.offset);
        if (ImGui::BeginTable("Operands", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthFixed, 65);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 110);
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Stored at", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();
            for (size_t index = 0; index < instruction.operands.size(); ++index) {
                const auto& operand = instruction.operands[index];
                ImGui::PushID(int(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const std::string label = std::to_string(index + 1);
                if (ImGui::Selectable(label.c_str(), selectedOperand == int(index),
                    ImGuiSelectableFlags_SpanAllColumns)) selectOperand(operand, int(index));
                ImGui::TableNextColumn(); ImGui::TextUnformatted(operandKindName(operand.kind));
                ImGui::TableNextColumn(); ImGui::TextUnformatted(operand.value.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(hexOffset(operand.offset).c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (selectedOperand >= 0 && size_t(selectedOperand) < instruction.operands.size()) {
            const auto& operand = instruction.operands[size_t(selectedOperand)];
            ImGui::TextWrapped("%s", operand.description.c_str());
            ImGui::TextDisabled("Encoding 0x%02X | %zu stored bytes", unsigned(operand.encoding), operand.size);
            const char* formats[] = {"Native value", "Integer", "Float", "Text", "Variable reference", "Jump target (file offset)"};
            ImGui::BeginDisabled(!operand.editable);
            ImGui::Combo("Interpretation", &valueFormat, formats, 6);
            textField<4096>("New value", operandInput);
            const bool apply = ImGui::Button("Apply operand");
            ImGui::EndDisabled();
            if (!operand.editable) ImGui::TextDisabled("This structural operand is read-only.");
            if (operand.hasTarget) {
                ImGui::SameLine();
                if (ImGui::Button("Go to target")) {
                    const size_t target = operand.target;
                    navigateScm(target);
                    return;
                }
            }
            const bool global = operand.kind == StorylandScmOperandKind::Global ||
                operand.kind == StorylandScmOperandKind::GlobalArray;
            const bool local = operand.kind == StorylandScmOperandKind::Local ||
                operand.kind == StorylandScmOperandKind::LocalArray;
            if (global || local) {
                ImGui::SameLine();
                if (ImGui::Button("Find variable uses"))
                    references = scm.variableReferences(global, operand.variable, global);
            }
            if (apply) {
                std::string error;
                const bool success = scm.editOperand(instruction.offset, size_t(selectedOperand),
                    operandInput, StorylandScmValueFormat(valueFormat), error);
                if (success) changedScm();
                scmResult(success, error, "Operand updated. The instruction size and surrounding bytes are preserved.");
            }
        }
    }

    void drawScm(const StorylandModdingServices& services) {
        if (!scmOpen) return;
        if (focusScm && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
            ImGui::SetNextWindowFocus();
            focusScm = false;
        }
        ImGui::SetNextWindowSize(ImVec2(1050, 740), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("MAIN.SCM Workbench", &scmOpen, ImGuiWindowFlags_NoFocusOnAppearing)) { ImGui::End(); return; }
        if (scmPath.empty() && !services.scmPath.empty()) scmPath = services.scmPath.u8string();
        ImGui::SetNextItemWidth(std::max(200.0f, ImGui::GetContentRegionAvail().x - 180));
        textField<8192>("SCM file", scmPath);
        if (choosePath(scmPath, StorylandModdingFileKind::Scm, false, services, "Browse##SCM"))
            request(Action::LoadScm, std::filesystem::u8path(scmPath));
        if (ImGui::Button("Open SCM")) request(Action::LoadScm, std::filesystem::u8path(scmPath));
        ImGui::SameLine();
        ImGui::BeginDisabled(services.scmPath.empty());
        if (ImGui::Button("Use loaded game's MAIN.SCM")) request(Action::LoadScm, services.scmPath);
        ImGui::EndDisabled();
        if (!scm.loaded()) {
            ImGui::TextWrapped("Open MAIN.SCM to browse its native mission table, inspect instructions, find references and tune existing operands. Sanny Builder is optional for whole-source compilation.");
            if (!scmStatus.empty()) ImGui::TextWrapped("%s", scmStatus.c_str());
            ImGui::End();
            return;
        }
        ImGui::Separator();
        ImGui::Text("%s | %zu bytes | %zu script ranges%s",
            scm.dialect() == StorylandScmDialect::Vcs ? "VCS" : "LCS", scm.bytes().size(),
            scm.scopes().size(), scm.dirty() ? " | modified" : "");
        ImGui::TextWrapped("%s", scm.sourcePath().u8string().c_str());
        std::string error;
        ImGui::BeginDisabled(!scm.canUndo());
        if (ImGui::Button("Undo##SCM")) {
            const bool success = scm.undo(error);
            if (success) changedScm();
            scmResult(success, error, "Undid the last edit.");
        }
        ImGui::EndDisabled(); ImGui::SameLine();
        ImGui::BeginDisabled(!scm.canRedo());
        if (ImGui::Button("Redo##SCM")) {
            const bool success = scm.redo(error);
            if (success) changedScm();
            scmResult(success, error, "Reapplied the edit.");
        }
        ImGui::EndDisabled(); ImGui::SameLine();
        ImGui::BeginDisabled(!scm.dirty());
        if (ImGui::Button("Revert to opened file")) request(Action::RevertScm);
        ImGui::EndDisabled(); ImGui::SameLine();
        if (ImGui::Button("Validate SCM")) {
            const bool valid = scm.validate(scmIssues);
            scmResult(valid, "Validation found issues listed below.", "All decoded script ranges and edited operands validated.");
        }
        scopeFilter.Draw("Find script", 250);
        const auto& scopes = scm.scopes();
        const auto& selectedScope = scopes[scm.selectedScope()];
        if (ImGui::BeginCombo("Script", selectedScope.name.c_str())) {
            for (size_t index = 0; index < scopes.size(); ++index) {
                const auto& scope = scopes[index];
                const std::string label = (scope.mission < 0 ? "MAIN" : std::to_string(scope.mission)) +
                    " | " + scope.name + " | " + scope.scriptName;
                if (!scopeFilter.PassFilter(label.c_str())) continue;
                if (ImGui::Selectable(label.c_str(), scm.selectedScope() == index)) {
                    if (scm.selectScope(index, error)) clearScmSelection();
                    else scmResult(false, error, {});
                }
            }
            ImGui::EndCombo();
        }
        textField<256>("Find instruction / opcode / value", instructionFilter);
        ImGui::SetNextItemWidth(180);
        textField<64>("File offset", offsetInput);
        ImGui::SameLine();
        if (ImGui::Button("Go##Offset")) {
            size_t offset = 0;
            if (parseOffset(offsetInput, offset)) navigateScm(offset);
            else scmResult(false, "Enter a decimal file offset or hexadecimal value beginning with 0x.", {});
        }
        const auto& instructions = scm.instructions();
        const bool filtered = !instructionFilter.empty();
        const std::vector<size_t> visibleRows = filtered ? scm.find(instructionFilter) : std::vector<size_t>{};
        const size_t visibleCount = filtered ? visibleRows.size() : instructions.size();
        const auto instructionIndex = [&](size_t row) { return filtered ? visibleRows[row] : row; };
        if (ImGui::BeginTable("SCM instructions", 3, ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
            ImVec2(0, 235))) {
            ImGui::TableSetupColumn("File offset", ImGuiTableColumnFlags_WidthFixed, 105);
            ImGui::TableSetupColumn("Opcode", ImGuiTableColumnFlags_WidthFixed, 75);
            ImGui::TableSetupColumn("Instruction");
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin(int(visibleCount));
            if (scrollToInstruction) {
                for (size_t row = 0; row < visibleCount; ++row) {
                    if (instructions[instructionIndex(row)].offset == selectedInstruction) {
                        clipper.IncludeItemByIndex(int(row));
                        break;
                    }
                }
            }
            while (clipper.Step()) {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                    const size_t nativeIndex = instructionIndex(size_t(index));
                    const auto& instruction = instructions[nativeIndex];
                    ImGui::PushID(int(nativeIndex));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(hexOffset(instruction.offset).c_str(),
                        selectedInstruction == instruction.offset, ImGuiSelectableFlags_SpanAllColumns)) {
                        selectedInstruction = instruction.offset;
                        selectedOperand = -1;
                    }
                    if (scrollToInstruction && instruction.offset == selectedInstruction) {
                        ImGui::SetScrollHereY(.5f);
                        scrollToInstruction = false;
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%04X%s", unsigned(instruction.opcode), instruction.inverted ? " !" : "");
                    ImGui::TableNextColumn();
                    if (instruction.valid) ImGui::TextUnformatted(instruction.text.c_str());
                    else ImGui::TextColored(ImVec4(1, .6f, .3f, 1), "%s", instruction.text.c_str());
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("%zu matching instructions | native decoding does not imply engine support for every opcode", visibleCount);
        if (ImGui::BeginTabBar("SCM inspection")) {
            if (ImGui::BeginTabItem("Instruction")) {
                drawScmInstruction();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("References")) {
                ImGui::TextUnformatted("Use Find jumps here or Find variable uses in the instruction inspector.");
                size_t destination = SIZE_MAX;
                ImGui::BeginChild("Reference results", ImVec2(0, 190), ImGuiChildFlags_Borders);
                for (size_t index = 0; index < references.size(); ++index) {
                    const auto& reference = references[index];
                    ImGui::PushID(int(index));
                    const std::string label = hexOffset(reference.instruction) + " | " +
                        scm.scopes()[reference.scope].name + " | " + reference.text;
                    if (ImGui::Selectable(label.c_str())) destination = reference.instruction;
                    ImGui::PopID();
                }
                ImGui::EndChild();
                if (destination != SIZE_MAX) navigateScm(destination);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Changes")) {
                const auto changes = scm.changes();
                ImGui::Text("%zu changed byte ranges", changes.size());
                ImGui::BeginChild("Changed bytes", ImVec2(0, 170), ImGuiChildFlags_Borders);
                for (const auto& change : changes)
                    ImGui::TextWrapped("%s  %s  ->  %s", hexOffset(change.offset).c_str(),
                        byteText(change.before).c_str(), byteText(change.after).c_str());
                ImGui::EndChild();
                ImGui::TextWrapped("Original SHA-256: %s", scm.originalSha256().c_str());
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Save / export")) {
                ImGui::TextWrapped("Save Copy writes a separate SCM. A patch records the expected original bytes and can be applied to the matching original file.");
                textField<8192>("Edited SCM", scmOutputPath);
                choosePath(scmOutputPath, StorylandModdingFileKind::Scm, true, services, "Browse##ScmOutput");
                if (ImGui::Button("Save edited copy")) {
                    const bool success = scm.saveCopy(std::filesystem::u8path(scmOutputPath), error);
                    if (success) savedScmRevision = scmRevision;
                    scmResult(success, error, "Saved edited copy: " + scmOutputPath);
                }
                textField<8192>("Patch file", scmPatchPath);
                choosePath(scmPatchPath, StorylandModdingFileKind::ScmPatch, true, services, "Browse##PatchOutput");
                if (ImGui::Button("Export patch")) {
                    const bool success = scm.exportPatch(std::filesystem::u8path(scmPatchPath), error);
                    if (success) savedScmRevision = scmRevision;
                    scmResult(success, error, "Saved patch: " + scmPatchPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Apply patch file")) request(Action::ImportPatch, std::filesystem::u8path(scmPatchPath));
                if (services.chooseFile) {
                    ImGui::SameLine();
                    if (ImGui::Button("Open patch...")) {
                        const auto chosen = services.chooseFile(StorylandModdingFileKind::ScmPatch, false);
                        if (!chosen.empty()) { scmPatchPath = chosen.u8string(); request(Action::ImportPatch, chosen); }
                    }
                }
                textField<8192>("Disassembly", scmTextPath);
                choosePath(scmTextPath, StorylandModdingFileKind::Disassembly, true, services, "Browse##TextOutput");
                ImGui::Checkbox("All script ranges", &exportAllScopes);
                if (ImGui::Button("Export readable disassembly"))
                    scmResult(scm.exportDisassembly(std::filesystem::u8path(scmTextPath), exportAllScopes, error),
                        error, "Exported disassembly: " + scmTextPath);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Diagnostics")) {
                for (const auto& diagnostic : scm.diagnostics()) ImGui::TextWrapped("%s", diagnostic.c_str());
                for (const auto& issue : scmIssues) ImGui::TextWrapped("%s", issue.c_str());
                if (scm.diagnostics().empty() && scmIssues.empty())
                    ImGui::TextUnformatted("No diagnostics for the selected script.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (!scmStatus.empty())
            ImGui::TextColored(scmError ? ImVec4(1, .5f, .4f, 1) : ImVec4(.65f, .9f, .75f, 1),
                "%s", scmStatus.c_str());
        ImGui::End();
    }

    bool drawCharacterVoices(const StorylandModdingServices& services);
    void drawDyom(const StorylandModdingServices& services);
};

bool StorylandModdingUi::Workspace::drawCharacterVoices(const StorylandModdingServices& services) {
    ImGui::TextWrapped("Preview character voices using approved English and Russian dialogue.");
    ImGui::Text("%zu characters | %zu emotions | %zu approved lines",
        storylandMissionVoiceProfileCount(), storylandMissionVoiceEmotionCount(), storylandMissionVoiceLineCount());
    if (ImGui::Combo("Dialogue language", &selectedVoiceLanguage, "English\0Russian\0")) {
        if (services.missionVoiceBusy && services.stopMissionVoice) services.stopMissionVoice();
        selectedVoiceLine.clear();
        selectedVoiceEmotion.clear();
        voiceStatus.clear();
    }
    const std::string_view language = selectedVoiceLanguage == 1 ? "ru" : "en";
    const auto* profile = storylandMissionVoiceProfileAt(selectedVoiceProfile);
    if (!profile) return false;
    if (ImGui::BeginCombo("Character voice", profile->displayName.data())) {
        for (size_t index = 0; index < storylandMissionVoiceProfileCount(); ++index) {
            const auto* candidate = storylandMissionVoiceProfileAt(index);
            if (candidate && ImGui::Selectable(candidate->displayName.data(), index == selectedVoiceProfile)) {
                if (services.missionVoiceBusy && services.stopMissionVoice) services.stopMissionVoice();
                selectedVoiceProfile = index;
                profile = candidate;
                selectedVoiceLine.clear();
                selectedVoiceEmotion.clear();
                voiceStatus.clear();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextWrapped("Timbre: %s", profile->timbre.data());
    ImGui::TextWrapped("Accent direction: %s", profile->accentDirection.data());
    ImGui::TextWrapped("Performance: %s", profile->performanceDirection.data());

    const auto* line = storylandFindMissionVoiceLine(selectedVoiceLine);
    if (!line || line->profileId != profile->id || line->language != language) {
        line = nullptr;
        for (size_t index = 0; index < storylandMissionVoiceLineCount(); ++index) {
            const auto* candidate = storylandMissionVoiceLineAt(index);
            if (candidate && candidate->profileId == profile->id && candidate->language == language) {
                line = candidate;
                selectedVoiceLine = std::string(line->id);
                break;
            }
        }
    }
    if (!line) {
        ImGui::TextUnformatted("This character has no approved dialogue in the selected language.");
        return false;
    }
    if (ImGui::BeginCombo("Catalogue line", line->text.data())) {
        for (size_t index = 0; index < storylandMissionVoiceLineCount(); ++index) {
            const auto* candidate = storylandMissionVoiceLineAt(index);
            if (!candidate || candidate->profileId != profile->id || candidate->language != language) continue;
            if (ImGui::Selectable(candidate->text.data(), candidate == line)) {
                if (services.missionVoiceBusy && services.stopMissionVoice) services.stopMissionVoice();
                line = candidate;
                selectedVoiceLine = std::string(line->id);
                selectedVoiceEmotion.clear();
                voiceStatus.clear();
            }
        }
        ImGui::EndCombo();
    }
    if (selectedVoiceEmotion.empty()) selectedVoiceEmotion = std::string(line->emotion);
    if (ImGui::BeginCombo("Emotion", selectedVoiceEmotion.c_str())) {
        for (size_t index = 0; index < storylandMissionVoiceEmotionCount(); ++index) {
            const auto* emotion = storylandMissionVoiceEmotionAt(index);
            if (emotion && ImGui::Selectable(emotion->id.data(), emotion->id == selectedVoiceEmotion)) {
                if (services.missionVoiceBusy && services.stopMissionVoice) services.stopMissionVoice();
                selectedVoiceEmotion = std::string(emotion->id);
                voiceStatus.clear();
            }
        }
        ImGui::EndCombo();
    }
    const StorylandMissionVoiceRequest request{profile->id, line->id, {}, selectedVoiceEmotion};
    StorylandPreparedMissionVoice prepared;
    const auto check = storylandPrepareMissionVoice(request, prepared);
    if (!check.accepted()) {
        ImGui::TextColored(ImVec4(1, .5f, .4f, 1), "%s", check.message.c_str());
        return false;
    }
    ImGui::TextWrapped("Dialogue: %s", prepared.text.data());
    ImGui::Text("Voice directions: rate %.2fx | pitch %+.2f semitones | energy %.2f | roughness %.2f",
        prepared.speakingRateMultiplier, prepared.pitchOffsetSemitones, prepared.energy, prepared.roughness);
    ImGui::TextDisabled("Applied controls depend on the selected speech engine.");
    ImGui::TextWrapped("Only complete catalogue lines can be requested. Add new wording to the voice catalogue and rebuild before using it for speech.");

    if (ImGui::Button("Copy voice request")) {
        const nlohmann::json document = {
            {"format", "storyland_mission_voice_request"}, {"format_version", 1},
            {"policy_id", std::string(storylandMissionVoicePolicyId())},
            {"profile_id", std::string(profile->id)}, {"line_id", std::string(line->id)},
            {"language", std::string(line->language)},
            {"emotion", selectedVoiceEmotion}, {"text", std::string(prepared.text)},
            {"controls", {{"rate_multiplier", prepared.speakingRateMultiplier},
                {"pitch_offset_semitones", prepared.pitchOffsetSemitones},
                {"energy", prepared.energy}, {"roughness", prepared.roughness}}}
        };
        ImGui::SetClipboardText(document.dump(2).c_str());
        voiceStatus = "Copied the exact catalogue request and resolved voice controls.";
        voiceError = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy dialogue text")) ImGui::SetClipboardText(prepared.text.data());
    const bool hasObjective = selectedObjective >= 0 && size_t(selectedObjective) < dyom.objectives.size();
    ImGui::BeginDisabled(!hasObjective);
    const bool useText = ImGui::Button("Use text for selected objective");
    ImGui::EndDisabled();
    if (!hasObjective) ImGui::TextDisabled("Select an objective to use this line as its displayed text.");
    bool changed = false;
    if (useText && hasObjective) {
        auto& text = dyom.objectives[size_t(selectedObjective)].text;
        changed = text != prepared.text;
        text = std::string(prepared.text);
        voiceStatus = "The selected objective now displays this catalogue line.";
        voiceError = false;
    }
    ImGui::Separator();
    if (!services.previewMissionVoice)
        ImGui::TextWrapped("Voice playback is unavailable: no speech synthesis backend is connected.");
    else ImGui::TextWrapped("Speech backend: %s", services.missionVoiceBackendName.empty() ?
        "Connected" : services.missionVoiceBackendName.c_str());
    ImGui::BeginDisabled(!services.previewMissionVoice || services.missionVoiceBusy);
    if (ImGui::Button("Preview voice")) {
        const auto result = storylandDispatchMissionVoice(request, services.previewMissionVoice);
        voiceError = !result.accepted();
        voiceStatus = voiceError ? result.message : (services.missionVoicePlaybackStatus.empty() ?
            "The speech backend accepted this catalogue request." : "");
    }
    ImGui::EndDisabled();
    if (services.stopMissionVoice) {
        ImGui::SameLine();
        ImGui::BeginDisabled(!services.missionVoiceBusy);
        if (ImGui::Button("Stop voice")) {
            services.stopMissionVoice();
            voiceStatus.clear();
        }
        ImGui::EndDisabled();
    }
    if (!services.missionVoicePlaybackStatus.empty())
        ImGui::TextColored(services.missionVoiceFailed ? ImVec4(1, .5f, .4f, 1) : ImVec4(.65f, .9f, .75f, 1),
            "%s", services.missionVoicePlaybackStatus.c_str());
    if (!voiceStatus.empty())
        ImGui::TextColored(voiceError ? ImVec4(1, .5f, .4f, 1) : ImVec4(.65f, .9f, .75f, 1),
            "%s", voiceStatus.c_str());
    return changed;
}

void StorylandModdingUi::Workspace::drawDyom(const StorylandModdingServices& services) {
    if (!dyomOpen) return;
    if (!dyomInitialized) initializeDyom(services);
    if (focusDyom && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        ImGui::SetNextWindowFocus();
        focusDyom = false;
    }
    ImGui::SetNextWindowSize(ImVec2(940, 730), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("DYOM Mission Designer", &dyomOpen, ImGuiWindowFlags_NoFocusOnAppearing)) { ImGui::End(); return; }
    ImGui::TextUnformatted("Do Your Own Mission");
    ImGui::TextWrapped("Create a mission project, place its cast and vehicles, order the objectives, then play it through Storyland's native mission engine.");
    ImGui::SetNextItemWidth(std::max(200.0f, ImGui::GetContentRegionAvail().x - 170));
    textField<8192>("Project file", dyomPath);
    choosePath(dyomPath, StorylandModdingFileKind::Dyom, true, services, "Browse##DyomSave");
    if (ImGui::Button("New mission")) request(Action::NewDyom);
    ImGui::SameLine();
    if (ImGui::Button("Open project")) request(Action::LoadDyom, std::filesystem::u8path(dyomPath));
    if (services.chooseFile) {
        ImGui::SameLine();
        if (ImGui::Button("Open...##Dyom")) {
            const auto selected = services.chooseFile(StorylandModdingFileKind::Dyom, false);
            if (!selected.empty()) request(Action::LoadDyom, selected);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save project")) {
        std::string error;
        const bool success = storylandSaveDyomProject(std::filesystem::u8path(dyomPath), dyom, error);
        if (success) savedDyomRevision = dyomRevision;
        dyomResult(success, error, "Saved mission project: " + dyomPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save as...")) {
        const auto current = std::filesystem::u8path(dyomPath);
        dyomSaveAsPath = (current.parent_path() / (current.stem().u8string() + "-copy.dyom")).u8string();
        ImGui::OpenPopup("Save mission as");
    }
    if (ImGui::BeginPopupModal("Save mission as", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(540);
        textField<8192>("Destination", dyomSaveAsPath);
        choosePath(dyomSaveAsPath, StorylandModdingFileKind::Dyom, true, services, "Browse##DyomSaveAs");
        ImGui::TextUnformatted("Save the current draft as a .dyom project at this path.");
        if (ImGui::Button("Save copy", ImVec2(130, 0))) {
            std::string error;
            const bool success = storylandSaveDyomProject(std::filesystem::u8path(dyomSaveAsPath), dyom, error);
            if (success) {
                dyomPath = dyomSaveAsPath;
                savedDyomRevision = dyomRevision;
                ImGui::CloseCurrentPopup();
            }
            dyomResult(success, error, "Saved mission project: " + dyomSaveAsPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(130, 0))) ImGui::CloseCurrentPopup();
        if (dyomError && !dyomStatus.empty()) ImGui::TextWrapped("%s", dyomStatus.c_str());
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::TextDisabled(unsavedDyom() ? "Unsaved changes" : "Saved / unchanged");
    ImGui::BeginDisabled(undoDyom.empty());
    if (ImGui::Button("Undo##Dyom")) {
        redoDyom.push_back({std::move(dyom), dyomRevision});
        auto previous = std::move(undoDyom.back());
        undoDyom.pop_back();
        dyom = std::move(previous.project);
        dyomRevision = previous.number;
        dyomIssues.clear();
        selectedEntity = selectedObjective = -1;
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(redoDyom.empty());
    if (ImGui::Button("Redo##Dyom")) {
        undoDyom.push_back({std::move(dyom), dyomRevision});
        auto next = std::move(redoDyom.back());
        redoDyom.pop_back();
        dyom = std::move(next.project);
        dyomRevision = next.number;
        dyomIssues.clear();
        selectedEntity = selectedObjective = -1;
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    if (ImGui::Button("Validate mission")) {
        dyomIssues = storylandValidateDyomProject(dyom, true);
        const bool valid = std::none_of(dyomIssues.begin(), dyomIssues.end(), [](const auto& issue) {
            return !issue.warning;
        });
        dyomResult(valid, "Complete the fields listed in Validation before playing.",
            "The authored mission is valid. Models and weapons are checked against the loaded game when playing.");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!services.canStartDyom || !services.playDyom);
    if (ImGui::Button(services.dyomActive || services.dyomFinished || services.dyomFailed ?
        "Retry mission" : "Play mission")) {
        std::string error;
        dyomIssues = storylandValidateDyomProject(dyom, true);
        const bool valid = std::none_of(dyomIssues.begin(), dyomIssues.end(), [](const auto& issue) {
            return !issue.warning;
        });
        if (valid) {
            const bool started = services.playDyom(dyom, error);
            dyomResult(started, error, "Playing: " + dyom.title);
            if (started) {
                dyomOpen = false;
                scmOpen = false;
                ImGui::SetWindowFocus(nullptr);
            }
        } else dyomResult(false, "Complete the fields listed in Validation before playing.", {});
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !services.canStartDyom)
        ImGui::SetTooltip("%s", services.playRestriction.empty() ? "Load VCS game data to play this mission." : services.playRestriction.c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(!services.stopDyom ||
        !(services.dyomActive || services.dyomFinished || services.dyomFailed));
    if (ImGui::Button("Stop mission")) {
        services.stopDyom();
        dyomResult(true, {}, "Stopped mission preview. The project remains open.");
    }
    ImGui::EndDisabled();
    if (!services.canStartDyom && !services.playRestriction.empty())
        ImGui::TextDisabled("%s", services.playRestriction.c_str());
    if (!services.dyomStatus.empty()) ImGui::TextWrapped("%s", services.dyomStatus.c_str());
    if (services.dyomObjective >= 0 && size_t(services.dyomObjective) < services.dyomObjectiveCount) {
        ImGui::Text("Objective %d / %zu", services.dyomObjective + 1, services.dyomObjectiveCount);
        if (!services.dyomObjectiveText.empty()) ImGui::TextWrapped("%s", services.dyomObjectiveText.c_str());
    }
    ImGui::Separator();

    Revision before{dyom, dyomRevision};
    bool changed = false;
    const auto uniqueEntityId = [&](const std::string& prefix) {
        for (size_t number = 1;; ++number) {
            const auto candidate = prefix + std::to_string(number);
            if (std::none_of(dyom.entities.begin(), dyom.entities.end(), [&](const auto& entity) {
                return entity.id == candidate;
            })) return candidate;
        }
    };
    const auto uniqueObjectiveId = [&]() {
        for (size_t number = 1;; ++number) {
            const auto candidate = "objective" + std::to_string(number);
            if (std::none_of(dyom.objectives.begin(), dyom.objectives.end(), [&](const auto& objective) {
                return objective.id == candidate;
            })) return candidate;
        }
    };
    if (ImGui::BeginTabBar("DYOM editor pages")) {
        if (ImGui::BeginTabItem("Mission")) {
            changed |= textField<512>("Title", dyom.title);
            changed |= textField<256>("Author", dyom.author);
            changed |= textField<8192>("Description", dyom.description, true, ImVec2(-1, 100));
            changed |= ImGui::InputInt("Reward", &dyom.reward, 100, 1000);
            changed |= ImGui::InputFloat("Mission time limit (seconds)", &dyom.timeLimitSeconds, 10, 60, "%.1f");
            ImGui::TextDisabled("A zero time limit means no deadline.");
            int clock[] = {dyom.hour, dyom.minute};
            if (ImGui::InputInt2("Start time (hour, minute)", clock)) {
                dyom.hour = clock[0]; dyom.minute = clock[1]; changed = true;
            }
            changed |= ImGui::InputInt("VCS weather", &dyom.weather);
            changed |= textField<4096>("Success message", dyom.successText);
            changed |= textField<4096>("Failure message", dyom.failureText);
            ImGui::TextWrapped("Project files contain your mission definitions. They use the player's VCS data and leave MAIN.SCM and campaign progress unchanged.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Player start")) {
            changed |= positionFields(dyom.player.position, services);
            changed |= ImGui::InputInt("Health", &dyom.player.health);
            changed |= ImGui::InputInt("Armour", &dyom.player.armour);
            changed |= ImGui::InputInt("Starting weapon ID", &dyom.player.weapon);
            changed |= ImGui::InputInt("Starting ammunition", &dyom.player.ammunition, 10, 100);
            ImGui::TextWrapped("Use Play Test to find a grounded start position, then capture it with Use player position. Weapon 0 is unarmed.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Actors and scenery")) {
            const char* kinds[] = {"Actor", "Vehicle", "Object", "Pickup"};
            ImGui::SetNextItemWidth(150);
            ImGui::Combo("Add type", &addEntityKind, kinds, 4);
            ImGui::SameLine();
            ImGui::BeginDisabled(dyom.entities.size() >= 64);
            if (ImGui::Button("Add entity")) {
                StorylandDyomEntity entity;
                entity.kind = StorylandDyomEntityKind(addEntityKind);
                std::string prefix = kinds[addEntityKind];
                prefix[0] = char(std::tolower(static_cast<unsigned char>(prefix[0])));
                entity.id = uniqueEntityId(prefix);
                entity.name = kinds[addEntityKind];
                entity.position = dyom.player.position;
                if (services.playerAvailable)
                    entity.position = {services.playerPlacement[0], services.playerPlacement[1],
                        services.playerPlacement[2], services.playerPlacement[3]};
                if (entity.kind == StorylandDyomEntityKind::Vehicle) entity.health = 1000;
                dyom.entities.push_back(std::move(entity));
                selectedEntity = int(dyom.entities.size()) - 1;
                changed = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine(); ImGui::TextDisabled("%zu / 64 placed entities", dyom.entities.size());
            ImGui::BeginChild("DYOM entities", ImVec2(0, 150), ImGuiChildFlags_Borders);
            for (size_t index = 0; index < dyom.entities.size(); ++index) {
                const auto& entity = dyom.entities[index];
                const std::string label = std::to_string(index + 1) + ". " + entityLabel(entity.kind) +
                    " | " + (entity.name.empty() ? entity.id : entity.name) + " | model " + std::to_string(entity.model);
                ImGui::PushID(int(index));
                if (ImGui::Selectable(label.c_str(), selectedEntity == int(index))) selectedEntity = int(index);
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (selectedEntity >= 0 && size_t(selectedEntity) < dyom.entities.size()) {
                auto& entity = dyom.entities[size_t(selectedEntity)];
                const auto oldId = entity.id;
                ImGui::PushID(selectedEntity);
                changed |= textField<256>("Name", entity.name);
                int kind = int(entity.kind);
                if (ImGui::Combo("Type", &kind, kinds, 4)) {
                    entity.kind = StorylandDyomEntityKind(kind);
                    changed = true;
                }
                changed |= ImGui::InputInt("Model ID", &entity.model);
                if (services.modelName && services.gameLoaded) {
                    const auto model = services.modelName(entity.model);
                    ImGui::TextWrapped("%s", model.empty() ? "Model is not present in the loaded game." : model.c_str());
                }
                changed |= positionFields(entity.position, services);
                if (entity.kind == StorylandDyomEntityKind::Actor) {
                    changed |= ImGui::InputInt("Health", &entity.health);
                    changed |= ImGui::InputInt("Pedestrian type", &entity.pedestrianType);
                    const char* behaviors[] = {"Idle", "Attack player", "Follow player"};
                    int behavior = int(entity.behavior);
                    if (ImGui::Combo("Behavior", &behavior, behaviors, 3)) {
                        entity.behavior = StorylandDyomActorBehavior(behavior); changed = true;
                    }
                    changed |= ImGui::InputInt("Weapon ID", &entity.weapon);
                    changed |= ImGui::InputInt("Ammunition", &entity.ammunition, 10, 100);
                    changed |= ImGui::Checkbox("Mission fails if this actor dies", &entity.mustSurvive);
                } else if (entity.kind == StorylandDyomEntityKind::Vehicle) {
                    changed |= ImGui::InputInt("Vehicle health", &entity.health, 50, 100);
                    changed |= ImGui::InputInt("Primary colour (-1 = original)", &entity.primaryColor);
                    changed |= ImGui::InputInt("Secondary colour (-1 = original)", &entity.secondaryColor);
                    changed |= ImGui::Checkbox("Mission fails if this vehicle is destroyed", &entity.mustSurvive);
                } else if (entity.kind == StorylandDyomEntityKind::Pickup) {
                    changed |= ImGui::InputInt("Native pickup type", &entity.pickupType);
                }
                if (ImGui::TreeNode("Entity reference")) {
                    if (textField<128>("ID", entity.id)) {
                        for (auto& objective : dyom.objectives)
                            if (objective.target == oldId) objective.target = entity.id;
                        changed = true;
                    }
                    ImGui::TextWrapped("Objectives refer to this stable ID. Renaming it updates their references.");
                    ImGui::TreePop();
                }
                const bool duplicate = ImGui::Button("Duplicate entity");
                ImGui::SameLine();
                const bool remove = ImGui::Button("Delete entity");
                ImGui::PopID();
                if (duplicate && dyom.entities.size() < 64) {
                    StorylandDyomEntity copy = entity;
                    copy.id = uniqueEntityId("entity");
                    copy.name += " copy";
                    dyom.entities.push_back(std::move(copy));
                    selectedEntity = int(dyom.entities.size()) - 1;
                    changed = true;
                } else if (remove) {
                    const auto reference = std::find_if(dyom.objectives.begin(), dyom.objectives.end(), [&](const auto& objective) {
                        return objective.target == entity.id;
                    });
                    if (reference != dyom.objectives.end())
                        dyomResult(false, "This entity is used by objective " + reference->id +
                            ". Change or remove that objective reference before deleting it.", {});
                    else {
                        dyom.entities.erase(dyom.entities.begin() + selectedEntity);
                        selectedEntity = std::min(selectedEntity, int(dyom.entities.size()) - 1);
                        changed = true;
                    }
                }
            } else ImGui::TextWrapped("Add an actor, vehicle, object or pickup, then select it to edit its model and placement.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Objectives")) {
            ImGui::BeginDisabled(dyom.objectives.size() >= 256);
            if (ImGui::Button("Add objective")) {
                StorylandDyomObjective objective;
                objective.id = uniqueObjectiveId();
                objective.text = "Reach the checkpoint.";
                objective.position = dyom.player.position;
                if (services.playerAvailable)
                    objective.position = {services.playerPlacement[0], services.playerPlacement[1],
                        services.playerPlacement[2], services.playerPlacement[3]};
                dyom.objectives.push_back(std::move(objective));
                selectedObjective = int(dyom.objectives.size()) - 1;
                changed = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine(); ImGui::TextDisabled("%zu / 256 objectives, played in order", dyom.objectives.size());
            ImGui::BeginChild("DYOM objective order", ImVec2(0, 155), ImGuiChildFlags_Borders);
            for (size_t index = 0; index < dyom.objectives.size(); ++index) {
                const auto& objective = dyom.objectives[index];
                const auto label = std::to_string(index + 1) + ". " + objectiveLabel(objective.kind) + " | " + objective.text;
                ImGui::PushID(int(index));
                if (ImGui::Selectable(label.c_str(), selectedObjective == int(index))) selectedObjective = int(index);
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (selectedObjective >= 0 && size_t(selectedObjective) < dyom.objectives.size()) {
                auto& objective = dyom.objectives[size_t(selectedObjective)];
                const char* kinds[] = {"Reach checkpoint", "Reach checkpoint on foot", "Drive to checkpoint",
                    "Enter vehicle", "Defeat actor", "Destroy vehicle", "Collect pickup", "Wait", "Survive"};
                int kind = int(objective.kind);
                if (ImGui::Combo("Objective type", &kind, kinds, 9)) {
                    objective.kind = StorylandDyomObjectiveKind(kind);
                    objective.target.clear();
                    changed = true;
                }
                changed |= textField<4096>("Objective message", objective.text, true, ImVec2(-1, 62));
                const bool reach = objective.kind == StorylandDyomObjectiveKind::ReachPoint ||
                    objective.kind == StorylandDyomObjectiveKind::ReachPointOnFoot ||
                    objective.kind == StorylandDyomObjectiveKind::ReachPointInVehicle;
                if (reach) {
                    changed |= positionFields(objective.position, services, false);
                    changed |= ImGui::InputFloat("Checkpoint radius", &objective.radius, .5f, 2, "%.2f");
                }
                const bool actorTarget = objective.kind == StorylandDyomObjectiveKind::DefeatActor;
                const bool vehicleTarget = objective.kind == StorylandDyomObjectiveKind::EnterVehicle ||
                    objective.kind == StorylandDyomObjectiveKind::DestroyVehicle ||
                    objective.kind == StorylandDyomObjectiveKind::ReachPointInVehicle;
                const bool pickupTarget = objective.kind == StorylandDyomObjectiveKind::CollectPickup;
                if (actorTarget || vehicleTarget || pickupTarget) {
                    const auto target = std::find_if(dyom.entities.begin(), dyom.entities.end(), [&](const auto& entity) {
                        return entity.id == objective.target;
                    });
                    const std::string preview = target != dyom.entities.end() ? target->name + " [" + target->id + "]" :
                        objective.target.empty() ? "Select a target" : objective.target + " (missing)";
                    if (ImGui::BeginCombo("Target", preview.c_str())) {
                        if (objective.kind == StorylandDyomObjectiveKind::ReachPointInVehicle &&
                            ImGui::Selectable("Any vehicle", objective.target.empty())) {
                            objective.target.clear(); changed = true;
                        }
                        for (const auto& entity : dyom.entities) {
                            if ((actorTarget && entity.kind != StorylandDyomEntityKind::Actor) ||
                                (vehicleTarget && entity.kind != StorylandDyomEntityKind::Vehicle) ||
                                (pickupTarget && entity.kind != StorylandDyomEntityKind::Pickup)) continue;
                            const std::string label = entity.name + " [" + entity.id + "]";
                            if (ImGui::Selectable(label.c_str(), entity.id == objective.target)) {
                                objective.target = entity.id; changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (objective.kind == StorylandDyomObjectiveKind::Wait ||
                    objective.kind == StorylandDyomObjectiveKind::Survive)
                    changed |= ImGui::InputFloat("Duration (seconds)", &objective.durationSeconds, 1, 10, "%.1f");
                changed |= ImGui::InputFloat("Objective time limit (seconds)", &objective.timeLimitSeconds, 5, 30, "%.1f");
                ImGui::TextDisabled("A zero time limit means no deadline. Player death and protected-entity loss still fail the mission.");
                if (ImGui::TreeNode("Objective reference")) {
                    changed |= textField<128>("ID", objective.id);
                    ImGui::TreePop();
                }
                ImGui::BeginDisabled(selectedObjective == 0);
                const bool moveUp = ImGui::Button("Move up");
                ImGui::EndDisabled(); ImGui::SameLine();
                ImGui::BeginDisabled(size_t(selectedObjective + 1) >= dyom.objectives.size());
                const bool moveDown = ImGui::Button("Move down");
                ImGui::EndDisabled(); ImGui::SameLine();
                ImGui::BeginDisabled(dyom.objectives.size() >= 256);
                const bool duplicate = ImGui::Button("Duplicate objective");
                ImGui::EndDisabled(); ImGui::SameLine();
                const bool remove = ImGui::Button("Delete objective");
                if (moveUp) {
                    std::swap(dyom.objectives[size_t(selectedObjective)], dyom.objectives[size_t(selectedObjective - 1)]);
                    --selectedObjective; changed = true;
                } else if (moveDown) {
                    std::swap(dyom.objectives[size_t(selectedObjective)], dyom.objectives[size_t(selectedObjective + 1)]);
                    ++selectedObjective; changed = true;
                } else if (duplicate) {
                    StorylandDyomObjective copy = objective;
                    copy.id = uniqueObjectiveId();
                    dyom.objectives.insert(dyom.objectives.begin() + selectedObjective + 1, std::move(copy));
                    ++selectedObjective; changed = true;
                } else if (remove) {
                    dyom.objectives.erase(dyom.objectives.begin() + selectedObjective);
                    selectedObjective = std::min(selectedObjective, int(dyom.objectives.size()) - 1);
                    changed = true;
                }
            } else ImGui::TextUnformatted("Add the first objective to define the mission's goal.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Character Voices")) {
            changed |= drawCharacterVoices(services);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Validation")) {
            if (dyomIssues.empty()) ImGui::TextWrapped("Use Validate mission to check placements, objective references and supported values. Models and weapons are checked against VCS when the mission starts.");
            for (const auto& issue : dyomIssues) {
                ImGui::TextColored(issue.warning ? ImVec4(1, .8f, .4f, 1) : ImVec4(1, .5f, .4f, 1),
                    "%s: %s", issue.field.c_str(), issue.message.c_str());
            }
            ImGui::Separator();
            ImGui::TextWrapped("Draft projects can be saved before every objective is complete. Play requires valid targets and at least one complete objective. Each preview starts an isolated Play Test session; retail mission progress and game saves are not written.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (changed) {
        undoDyom.push_back(std::move(before));
        if (undoDyom.size() > 64) undoDyom.erase(undoDyom.begin());
        redoDyom.clear();
        dyomRevision = ++nextDyomRevision;
        dyomIssues.clear();
    }
    if (!dyomStatus.empty())
        ImGui::TextColored(dyomError ? ImVec4(1, .5f, .4f, 1) : ImVec4(.65f, .9f, .75f, 1),
            "%s", dyomStatus.c_str());
    ImGui::End();
}

StorylandModdingUi::StorylandModdingUi() : workspace(std::make_unique<Workspace>()) {}
StorylandModdingUi::~StorylandModdingUi() = default;

void StorylandModdingUi::showScm(const std::filesystem::path& path) {
    workspace->scmOpen = true;
    workspace->focusScm = true;
    if (!path.empty() && (!workspace->scm.loaded() || workspace->scm.sourcePath() != path))
        workspace->request(Workspace::Action::LoadScm, path);
}

void StorylandModdingUi::showDyom(const std::filesystem::path& path) {
    workspace->dyomOpen = true;
    workspace->focusDyom = true;
    if (!path.empty()) workspace->request(Workspace::Action::LoadDyom, path);
}

bool StorylandModdingUi::requestOpen(const std::filesystem::path& path) {
    const auto extension = lowerExtension(path);
    if (extension == ".scm") { showScm(path); return true; }
    if (extension == ".dyom" || extension == ".dyomvcs") { showDyom(path); return true; }
    if (extension == ".scmpatch") {
        showScm();
        workspace->scmPatchPath = path.u8string();
        workspace->request(Workspace::Action::ImportPatch, path);
        return true;
    }
    return false;
}

void StorylandModdingUi::draw(const StorylandModdingServices& services) {
    workspace->drawPending(services);
    workspace->drawScm(services);
    workspace->drawDyom(services);
}

bool StorylandModdingUi::visible() const { return workspace->scmOpen || workspace->dyomOpen; }

bool StorylandModdingUi::hasUnsavedChanges() const {
    return workspace->unsavedScm() || workspace->unsavedDyom();
}
