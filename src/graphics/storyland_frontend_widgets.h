#pragma once

#include "src/storyland/storyland_play_test_hud.h"
#include <algorithm>
#include <cstring>
#include <string>

inline const char* storylandFrontendFont(const StorylandRetailHudAssets* assets, const char* native = "gametext") {
    const char* frontend = std::strcmp(native,"fancytext")==0 ? "frontend:fancytext" :
        std::strcmp(native,"bigtext")==0 ? "frontend:bigtext" :
        std::strcmp(native,"pagertext")==0 ? "frontend:pagertext" : "frontend:gametext";
    return assets && assets->fonts.count(frontend) && assets->sprites.count(frontend) ? frontend : native;
}

inline float storylandFrontendFontHeight(const StorylandRetailHudAssets* assets) {
    const bool hdFont=std::strcmp(storylandFrontendFont(assets),"gametext")!=0;
    return ImGui::GetFontSize()*(hdFont?1.5f:1.0f);
}

inline bool storylandDrawFrontendBackground(ImDrawList* draw, const StorylandRetailHudAssets* assets,
        ImVec2 first, ImVec2 last, ImU32 tint = IM_COL32_WHITE) {
    if (!draw || !assets || last.x<=first.x || last.y<=first.y) return false;
    const auto image=assets->sprites.find("frontend:background");
    if(image==assets->sprites.end()||!image->second||!image->second->width||!image->second->height)return false;
    const ImTextureID texture=storylandRetailSpriteTexture(assets,"frontend:background");
    if(!texture)return false;
    const float sourceAspect=float(image->second->width)/float(image->second->height);
    const float targetAspect=(last.x-first.x)/(last.y-first.y);
    ImVec2 uvFirst(0,0),uvLast(1,1);
    if(sourceAspect>targetAspect) {
        const float extent=targetAspect/sourceAspect;
        uvFirst.x=(1-extent)*.5f;uvLast.x=uvFirst.x+extent;
    } else {
        const float extent=sourceAspect/targetAspect;
        uvFirst.y=(1-extent)*.5f;uvLast.y=uvFirst.y+extent;
    }
    draw->AddImage(texture,first,last,uvFirst,uvLast,tint);
    return true;
}

inline void storylandFrontendText(const StorylandRetailHudAssets* assets, const char* text,
        float height = 0, const char* font = "gametext", ImU32 color = 0) {
    if(height<=0)height=storylandFrontendFontHeight(assets);
    const float width=std::max(1.0f,ImGui::GetContentRegionAvail().x);
    const char* selected=storylandFrontendFont(assets,font);
    const ImVec2 measured=storylandMeasurePlayTestText(assets,text,height,width,selected);
    storylandDrawPlayTestText(ImGui::GetWindowDrawList(),assets,ImGui::GetCursorScreenPos(),
        color?color:ImGui::GetColorU32(ImGuiCol_Text),text,height,width,selected);
    ImGui::Dummy(ImVec2(measured.x,measured.y));
}

inline bool storylandFrontendButton(const StorylandRetailHudAssets* assets, const char* label,
        ImVec2 size = ImVec2(0,0), float height = 0) {
    const char* font=storylandFrontendFont(assets);
    if(!assets||std::strcmp(font,"gametext")==0)return ImGui::Button(label,size);
    if(height<=0)height=storylandFrontendFontHeight(assets);
    const char* hidden=std::strstr(label,"##");
    const std::string caption=hidden?std::string(label,hidden):std::string(label);
    const auto padding=ImGui::GetStyle().FramePadding;
    if(size.x==0)size.x=storylandMeasurePlayTestText(assets,caption.c_str(),height,0,font).x+padding.x*2;
    const float width=size.x<0?ImGui::GetContentRegionAvail().x:size.x;
    const float wrap=std::max(1.0f,width-padding.x*2);
    const auto measured=storylandMeasurePlayTestText(assets,caption.c_str(),height,wrap,font);
    size.y=std::max(size.y,measured.y+padding.y*2);
    const ImU32 color=ImGui::GetColorU32(ImGuiCol_Text);
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
    const bool clicked=ImGui::Button(label,size);
    ImGui::PopStyleColor();
    const auto first=ImGui::GetItemRectMin(),last=ImGui::GetItemRectMax();
    const auto align=ImGui::GetStyle().ButtonTextAlign;
    const ImVec2 position(first.x+padding.x+std::max(0.0f,last.x-first.x-padding.x*2-measured.x)*align.x,
        first.y+padding.y+std::max(0.0f,last.y-first.y-padding.y*2-measured.y)*align.y);
    ImGui::GetWindowDrawList()->PushClipRect(first,last,true);
    storylandDrawPlayTestText(ImGui::GetWindowDrawList(),assets,position,color,caption.c_str(),height,wrap,font);
    ImGui::GetWindowDrawList()->PopClipRect();
    return clicked;
}
