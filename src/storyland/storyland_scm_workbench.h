#pragma once

#include "src/storyland/storyland_scm.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class StorylandScmOperandKind {
    Integer, Float, Text, Local, Global, LocalArray, GlobalArray, FunctionHeader
};

enum class StorylandScmValueFormat { Automatic, Integer, Float, Text, Reference, Target };

struct StorylandScmOperandView {
    size_t offset = 0;
    size_t size = 0;
    uint8_t encoding = 0;
    StorylandScmOperandKind kind = StorylandScmOperandKind::Integer;
    int32_t bits = 0;
    uint16_t variable = 0;
    uint8_t indexVariable = 0;
    uint8_t arrayCount = 0;
    bool branch = false;
    bool hasTarget = false;
    size_t target = 0;
    bool editable = true;
    std::string text;
    std::string value;
    std::string description;
};

struct StorylandScmInstructionView {
    size_t offset = 0;
    size_t end = 0;
    uint16_t opcode = 0;
    bool inverted = false;
    bool valid = false;
    std::string definition;
    std::string text;
    std::string hex;
    std::string diagnostic;
    std::vector<StorylandScmOperandView> operands;
};

struct StorylandScmScope {
    int mission = -1;
    size_t begin = 0;
    size_t end = 0;
    std::string name;
    std::string scriptName;
    std::string category;
};

struct StorylandScmReference {
    size_t scope = 0;
    size_t instruction = 0;
    size_t operand = 0;
    std::string text;
};

struct StorylandScmChange {
    size_t offset = 0;
    std::vector<uint8_t> before;
    std::vector<uint8_t> after;
};

class StorylandScmWorkbench {
public:
    bool load(const std::filesystem::path& path, std::string& error);
    bool load(const StorylandScmFile& source, std::string& error);
    bool loadFragment(const std::vector<uint8_t>& bytes, StorylandScmDialect dialect,
        const std::string& name, std::string& error);
    void clear();

    bool loaded() const { return !original.empty(); }
    bool dirty() const;
    StorylandScmDialect dialect() const { return gameDialect; }
    const StorylandScmHeader& header() const { return nativeHeader; }
    const std::filesystem::path& sourcePath() const { return openedPath; }
    const std::string& originalSha256() const { return sourceFingerprint; }
    const std::vector<uint8_t>& originalBytes() const { return original; }
    const std::vector<uint8_t>& bytes() const { return working; }
    const std::vector<StorylandScmScope>& scopes() const { return scriptScopes; }
    size_t selectedScope() const { return currentScope; }
    const std::vector<StorylandScmInstructionView>& instructions() const { return decoded; }
    const std::vector<std::string>& diagnostics() const { return scopeDiagnostics; }
    bool selectScope(size_t index, std::string& error);
    bool selectOffset(size_t offset, std::string& error);
    std::vector<size_t> find(const std::string& query) const;
    std::vector<StorylandScmReference> referencesTo(size_t target) const;
    std::vector<StorylandScmReference> variableReferences(bool global, uint16_t slot,
        bool allScopes = true) const;

    bool editOperand(size_t instructionOffset, size_t operandIndex,
        const std::string& value, StorylandScmValueFormat format, std::string& error);
    bool canUndo() const { return !undoHistory.empty(); }
    bool canRedo() const { return !redoHistory.empty(); }
    bool undo(std::string& error);
    bool redo(std::string& error);
    void revert();
    std::vector<StorylandScmChange> changes() const;
    bool validate(std::vector<std::string>& issues) const;

    std::string disassembly(bool allScopes = false) const;
    bool exportDisassembly(const std::filesystem::path& path, bool allScopes,
        std::string& error) const;
    bool saveCopy(const std::filesystem::path& path, std::string& error) const;
    std::string patchText() const;
    bool applyPatchText(const std::string& text, std::string& error);
    bool exportPatch(const std::filesystem::path& path, std::string& error) const;
    bool importPatch(const std::filesystem::path& path, std::string& error);

private:
    struct Edit {
        size_t scope = 0;
        std::vector<StorylandScmChange> changes;
    };
    std::filesystem::path openedPath;
    StorylandScmHeader nativeHeader;
    StorylandScmDialect gameDialect = StorylandScmDialect::Unknown;
    std::string sourceFingerprint;
    std::vector<uint8_t> original, working;
    std::vector<StorylandScmScope> scriptScopes;
    size_t currentScope = 0;
    size_t modifiedBytes = 0;
    std::vector<StorylandScmInstructionView> decoded;
    std::vector<std::string> scopeDiagnostics;
    std::vector<Edit> undoHistory, redoHistory;

    void decodeSelected();
    bool commit(Edit edit, std::string& error);
    bool checkChanges(const std::vector<StorylandScmChange>& edits, std::string& error) const;
};
