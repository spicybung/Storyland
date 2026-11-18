#pragma once

#include "src/storyland/storyland_time.h"
#include "src/storyland/storyland_sky.h"
#include "imgui.h"
#include <array>

inline constexpr std::array<const char*,24> StorylandClockHourLabels{{
    "00:00", "01:00", "02:00", "03:00", "04:00", "05:00",
    "06:00", "07:00", "08:00", "09:00", "10:00", "11:00",
    "12:00", "13:00", "14:00", "15:00", "16:00", "17:00",
    "18:00", "19:00", "20:00", "21:00", "22:00", "23:00"
}};

inline void storylandSelectClockHour(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandSky& sky,int hour) {
    storylandSetClockTime(runtime,world,hour,0);
    sky.setTime(world.hour);
}

inline void storylandDrawTimeMenu(StorylandPlayTestRuntime& runtime,
    StorylandMissionEnvironment& world,StorylandSky& sky) {
    if(!ImGui::CollapsingHeader("Time"))return;
    int current=storylandClockMinute(world.hour);
    ImGui::Text("Current time: %02d:%02d",current/60,current%60);
    if(ImGui::BeginTable("Hours##Time",4,ImGuiTableFlags_SizingStretchSame)) {
        for(int hour=0;hour<int(StorylandClockHourLabels.size());++hour) {
            ImGui::TableNextColumn();
            if(ImGui::RadioButton(StorylandClockHourLabels[size_t(hour)],current/60==hour)) {
                storylandSelectClockHour(runtime,world,sky,hour);
                current=storylandClockMinute(world.hour);
            }
        }
        ImGui::EndTable();
    }
    int hour=current/60,minute=current%60;
    if(ImGui::SliderInt("Minute##Time",&minute,0,59)) {
        storylandSetClockTime(runtime,world,hour,minute);
        sky.setTime(world.hour);
    }
    ImGui::Separator();
    ImGui::Checkbox("Freeze clock",&runtime.debugClockFrozen);
    ImGui::SliderFloat("Game minutes per second",&runtime.gameMinutesPerSecond,
        0.1f,60.0f,"%.1f",ImGuiSliderFlags_Logarithmic);
    if(ImGui::Button("Normal clock speed"))runtime.gameMinutesPerSecond=1;
    if(runtime.scriptTimeFrozen)ImGui::TextDisabled("The mission has paused the clock.");
}
