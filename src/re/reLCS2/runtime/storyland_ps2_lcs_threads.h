#pragma once
#include "src/re/reLCS2/runtime/storyland_ps2_lcs_conversion.h"

namespace storyland_ps2_save {
// This constructs execution frames only. Installing a save also requires world,
// player, garage and script-handle restoration; callers must commit those together.
inline StorylandMissionVm restoreLcsThread(const LcsThread& native,
    std::shared_ptr<const std::vector<uint8_t>> scm, uint32_t mainSize) {
    if (!scm || mainSize > scm->size() || !mainSize || native.state.vcs ||
        native.state.pc >= mainSize || native.localsBase >= 104 ||
        native.nativeReturnValues.size() > 16)
        throw std::runtime_error("Invalid LCS execution context.");
    const auto& code = *scm;
    auto take = [&](size_t& cursor) -> uint8_t {
        if (cursor >= mainSize) throw std::runtime_error("Truncated LCS function return.");
        return code[cursor++];
    };
    auto window = [&](size_t base) {
        std::array<int32_t,128> locals{};
        if (base >= 104) throw std::runtime_error("Invalid LCS function locals base.");
        for (size_t i = base; i < 104; ++i) locals[i-base] = native.state.locals[i];
        locals[104] = native.state.locals[104];
        locals[105] = native.state.locals[105];
        return locals;
    };
    StorylandMissionVm vm = native.state;
    vm.returnStack.clear();
    vm.functionFrames.clear();
    vm.sharedRetailScript = std::move(scm);
    vm.script.clear();
    vm.missionBase = 0;
    vm.fileOffset = 0;
    vm.wholeScm = true;
    // Unwind from the current frame to recover each caller's native local base.
    // Ordinary GOSUB entries share the current frame and consume no local space.
    size_t base = native.localsBase;
    std::vector<StorylandMissionVm::FunctionFrame> reversed;
    for (size_t n = native.nativeReturnValues.size(); n-- > 0;) {
        const uint32_t packed = native.nativeReturnValues[n];
        if (!(packed & (1u << 23))) {
            if (packed >= mainSize) throw std::runtime_error("Invalid LCS GOSUB return.");
            continue;
        }
        size_t cursor = packed & 0x003fffffu;
        const uint8_t inputs = take(cursor), outputs = take(cursor), offset = take(cursor);
        if (inputs > 32 || outputs > 32 || offset > base || base + inputs + outputs > 104)
            throw std::runtime_error("Invalid LCS function frame range.");
        StorylandMissionVm::FunctionFrame frame;
        frame.inputCount = inputs;
        frame.outputCount = outputs;
        frame.localBase = offset;
        frame.invertReturn = (packed & (1u << 22)) != 0;
        base -= offset;
        frame.previousLocals = window(base);
        const size_t skip = packed >> 24;
        if (skip > mainSize - cursor) throw std::runtime_error("Invalid LCS argument offset.");
        cursor += skip;
        for (size_t i = 0; i < outputs; ++i) {
            const uint8_t kind = take(cursor);
            bool global = kind >= 204;
            size_t slot;
            if (global) slot = size_t(kind - (kind >= 230 ? 230 : 204)) * 256 + take(cursor);
            else if (kind >= 108) slot = kind - 108;
            else if (kind >= 12) slot = kind - 12;
            else if (kind >= 10) slot = 104 + kind - 10;
            else throw std::runtime_error("LCS function output is not a variable.");
            if (kind >= 230 || (kind >= 108 && kind < 204)) {
                const uint8_t indexSlot = take(cursor), count = take(cursor);
                if (indexSlot >= 96 || !count || base + indexSlot >= 104)
                    throw std::runtime_error("Invalid LCS output array.");
                const int32_t index = frame.previousLocals[indexSlot];
                if (index < 0 || uint32_t(index) >= count)
                    throw std::runtime_error("LCS output array index is out of bounds.");
                slot += size_t(index);
            }
            if (global ? slot >= vm.globals.size() : (slot >= 106 || (slot < 104 && base + slot >= 104)))
                throw std::runtime_error("LCS function output is out of bounds.");
            frame.outputs.emplace_back(uint16_t(slot), global);
        }
        if (cursor >= mainSize) throw std::runtime_error("LCS return is outside the main script.");
        frame.returnPc = cursor;
        for (size_t i = 0; i < n; ++i)
            if (!(native.nativeReturnValues[i] & (1u << 23))) ++frame.returnDepth;
        reversed.push_back(std::move(frame));
    }
    if (base != 0) throw std::runtime_error("LCS locals base does not match its function stack.");
    for (uint32_t packed : native.nativeReturnValues)
        if (!(packed & (1u << 23))) vm.returnStack.push_back(packed);
    vm.functionFrames.assign(reversed.rbegin(), reversed.rend());
    vm.locals = window(native.localsBase);
    vm.state = !native.active ? StorylandMissionState::Idle :
        vm.delayMilliseconds ? StorylandMissionState::Waiting : StorylandMissionState::Running;
    vm.diagnostic.clear();
    if (!vm.validSavedExecution(code.size())) throw std::runtime_error("Invalid restored LCS thread.");
    return vm;
}
}
