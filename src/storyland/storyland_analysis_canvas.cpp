#include "src/storyland/storyland_analysis_canvas.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace {
ImU32 nodeColor(const StorylandAnalysisGraphNode& node) {
    const auto& label = node.label;
    if (node.type == 0x116 || label.find("Skin") != std::string::npos || label.find("bone") != std::string::npos || label.find("HAnim") != std::string::npos)
        return IM_COL32(165, 100, 220, 255);
    if (node.type == 0x15 || node.type == 0x16 || node.type == 6 || node.type == 7 || label.find("Texture") != std::string::npos || label.find("Raster") != std::string::npos)
        return IM_COL32(55, 165, 105, 255);
    if (node.type == 0xF || node.type == 0x14 || label.find("Geometry") != std::string::npos || label.find("DMA") != std::string::npos)
        return IM_COL32(45, 135, 205, 255);
    if (label.find("Collision") != std::string::npos || label.find("Surface") != std::string::npos)
        return IM_COL32(225, 120, 55, 255);
    if (label.find("reloc") != std::string::npos || label.find("Pointer") != std::string::npos)
        return IM_COL32(200, 165, 45, 255);
    if (node.type == 0xE || label.find("Frame") != std::string::npos)
        return IM_COL32(45, 165, 180, 255);
    if (node.parent < 0) return IM_COL32(210, 65, 140, 255);
    return IM_COL32(95, 112, 140, 255);
}
ImVec2 add(ImVec2 a, ImVec2 b) { return {a.x+b.x,a.y+b.y}; }
}
void StorylandAnalysisCanvas::reset() {
    positions.clear(); collapsed.clear(); pan = {32,32}; zoom = 1; dragging = -1; lastSelected = -1; frameRoot = true;
}
void StorylandAnalysisCanvas::arrange(const StorylandAnalysisGraph& graph) {
    const auto& nodes = graph.nodes();
    const bool firstLayout = positions.empty();
    positions.assign(nodes.size(), {});
    collapsed.resize(nodes.size(), false);
    std::vector<std::vector<size_t>> children(nodes.size());
    std::vector<size_t> roots;
    for (size_t index=0; index<nodes.size(); ++index) {
        if (nodes[index].parent >= 0 && size_t(nodes[index].parent) < index) children[size_t(nodes[index].parent)].push_back(index);
        else roots.push_back(index);
    }
    if (firstLayout) for (size_t index=0;index<nodes.size();++index)
        if (nodes[index].parent>=0 && children[index].size()>12) collapsed[index]=true;
    float row = 0;
    std::function<float(size_t,size_t)> place = [&](size_t index, size_t depth) {
        float y;
        if (depth > 128 || children[index].empty() || collapsed[index]) { y = row; row += 112; }
        else {
            float first = 0, last = 0;
            for (size_t child : children[index]) { last = place(child, depth+1); if (child == children[index].front()) first = last; }
            y = (first+last)*0.5f;
        }
        positions[index] = {float(depth)*310,y};
        return y;
    };
    for (size_t root : roots) place(root,0);
}
int StorylandAnalysisCanvas::draw(const StorylandAnalysisGraph& graph, int selected) {
    const auto& nodes = graph.nodes();
    if (positions.size()!=nodes.size()) arrange(graph);
    if (nodes.empty()) { ImGui::TextDisabled("No nodes."); return -1; }
    const bool focusNew = selected != lastSelected && selected >= 0 && size_t(selected) < nodes.size();
    if (focusNew) {
        bool changed = false;
        int parent = nodes[size_t(selected)].parent;
        for (size_t depth=0;parent>=0&&size_t(parent)<nodes.size()&&depth<128;++depth) {
            if (collapsed[size_t(parent)]) { collapsed[size_t(parent)]=false; changed=true; }
            parent=nodes[size_t(parent)].parent;
        }
        if (changed) arrange(graph);
        lastSelected=selected;
    }
    if (ImGui::Button("Arrange")) { arrange(graph); frameRoot = true; }
    ImGui::SameLine();
    const bool fit = ImGui::Button("Frame all");
    ImGui::SameLine();
    const bool frameSelected = ImGui::Button("Frame selected");
    ImGui::SameLine(); ImGui::TextDisabled("%.0f%%  |  wheel zoom  |  middle-drag pan  |  drag nodes", zoom*100);
    ImGui::BeginChild("##analysisCanvas", ImVec2(0,0), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size{std::max(1.0f,available.x),std::max(1.0f,available.y)};
    ImGui::InvisibleButton("##nodeCanvas", size, ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle|ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    auto& io = ImGui::GetIO();
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end{origin.x+size.x,origin.y+size.y};
    draw->PushClipRect(origin,end,true);
    draw->AddRectFilled(origin,end,IM_COL32(20,22,28,255));
    std::vector<bool> visible(nodes.size(), true);
    std::vector<size_t> childCounts(nodes.size(),0);
    for (const auto& node : nodes) if(node.parent>=0 && size_t(node.parent)<nodes.size()) ++childCounts[size_t(node.parent)];
    for (size_t index=0;index<nodes.size();++index) {
        const int parent=nodes[index].parent;
        if (parent>=0 && size_t(parent)<index) visible[index]=visible[size_t(parent)]&&!collapsed[size_t(parent)];
    }
    if (fit) {
        ImVec2 maximum{};
        for(size_t i=0;i<nodes.size();++i)if(visible[i]){maximum.x=std::max(maximum.x,positions[i].x+260);maximum.y=std::max(maximum.y,positions[i].y+78);}
        zoom=std::clamp(std::min((size.x-48)/std::max(1.0f,maximum.x),(size.y-48)/std::max(1.0f,maximum.y)),0.08f,1.5f);
        pan={24,24}; frameRoot=false;
    }
    if (frameRoot || frameSelected || focusNew) {
        const size_t index=selected>=0&&size_t(selected)<nodes.size()?size_t(selected):0;
        if(frameRoot)zoom=0.9f;
        pan={frameRoot?32.0f:size.x*0.5f-130*zoom-positions[index].x*zoom,
             size.y*0.5f-39*zoom-positions[index].y*zoom};
        frameRoot=false;
    }
    if (hovered && io.MouseWheel!=0) {
        const float old=zoom;
        zoom=std::clamp(zoom*std::pow(1.15f,io.MouseWheel),0.08f,2.0f);
        const ImVec2 local{io.MousePos.x-origin.x,io.MousePos.y-origin.y};
        pan={local.x-(local.x-pan.x)*zoom/old,local.y-(local.y-pan.y)*zoom/old};
    }
    if(hovered&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle,0)) pan=add(pan,io.MouseDelta);
    const float grid=32*zoom;
    if(grid>=8) {
        for(float x=std::fmod(pan.x,grid);x<size.x;x+=grid)draw->AddLine({origin.x+x,origin.y},{origin.x+x,end.y},IM_COL32(43,46,56,180));
        for(float y=std::fmod(pan.y,grid);y<size.y;y+=grid)draw->AddLine({origin.x,origin.y+y},{end.x,origin.y+y},IM_COL32(43,46,56,180));
    }
    const auto point=[&](size_t index){return ImVec2(origin.x+pan.x+positions[index].x*zoom,origin.y+pan.y+positions[index].y*zoom);};
    const auto link=[&](size_t from,size_t to,ImU32 color) {
        const auto a=point(from),b=point(to);
        const auto inView = [&](ImVec2 p) {
            return p.x+260*zoom>=origin.x && p.x<=end.x && p.y+78*zoom>=origin.y && p.y<=end.y;
        };
        if (!inView(a) || !inView(b)) return;
        ImVec2 start{a.x+260*zoom,a.y+39*zoom},finish{b.x,b.y+39*zoom};
        const float curve=std::max(35.0f,std::fabs(finish.x-start.x)*0.5f);
        draw->AddBezierCubic(start,{start.x+curve,start.y},{finish.x-curve,finish.y},finish,color,1.7f,16);
    };
    for(size_t index=0;index<nodes.size();++index){const int parent=nodes[index].parent;
        if(visible[index]&&parent>=0&&size_t(parent)<nodes.size()&&visible[size_t(parent)])link(size_t(parent),index,nodeColor(nodes[index]));}
    for(const auto& edge:graph.edges())if(edge.source<nodes.size()&&edge.target<nodes.size()&&visible[edge.source]&&visible[edge.target])
        link(edge.source,edge.target,IM_COL32(225,185,65,180));
    int clicked=-1;
    for(size_t index=0;index<nodes.size();++index) {
        if(!visible[index])continue;
        const auto position=point(index);const ImVec2 bottom{position.x+260*zoom,position.y+78*zoom};
        if(bottom.x<origin.x||position.x>end.x||bottom.y<origin.y||position.y>end.y)continue;
        const bool hit=hovered&&io.MousePos.x>=position.x&&io.MousePos.x<=bottom.x&&io.MousePos.y>=position.y&&io.MousePos.y<=bottom.y;
        draw->AddRectFilled({position.x+3,position.y+4},{bottom.x+3,bottom.y+4},IM_COL32(0,0,0,100),7*zoom);
        draw->AddRectFilled(position,bottom,IM_COL32(35,38,47,255),7*zoom);
        draw->AddRectFilled(position,{bottom.x,position.y+25*zoom},nodeColor(nodes[index]),7*zoom,ImDrawFlags_RoundCornersTop);
        draw->AddRect(position,bottom,selected==int(index)?IM_COL32(255,230,135,255):IM_COL32(85,92,110,255),7*zoom,0,selected==int(index)?2.5f:1.0f);
        draw->AddCircleFilled({position.x,position.y+39*zoom},4*zoom,nodeColor(nodes[index]));
        draw->AddCircleFilled({bottom.x,position.y+39*zoom},4*zoom,nodeColor(nodes[index]));
        draw->PushClipRect({position.x+7*zoom,position.y+3*zoom},{bottom.x-7*zoom,bottom.y-3*zoom},true);
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*zoom,{position.x+8*zoom,position.y+5*zoom},IM_COL32(255,255,255,255),nodes[index].label.c_str());
        char detail[128];std::snprintf(detail,sizeof(detail),"Offset 0x%llX\nSize %llu bytes",(unsigned long long)nodes[index].offset,(unsigned long long)nodes[index].size);
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*zoom,{position.x+8*zoom,position.y+32*zoom},IM_COL32(215,220,230,255),detail);
        draw->PopClipRect();
        if (childCounts[index]) draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*zoom,
            {bottom.x-18*zoom,position.y+5*zoom},IM_COL32(255,255,255,255),collapsed[index]?"+":"-");
        if(hit){
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)){clicked=int(index);dragging=int(index);lastSelected=int(index);}
            if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){collapsed[index]=!collapsed[index];arrange(graph);}
            if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))ImGui::SetTooltip("%s\n%s\nDouble-click to collapse or expand",nodes[index].label.c_str(),nodes[index].description.c_str());
        }
    }
    if(dragging>=0&&size_t(dragging)<positions.size()&&ImGui::IsMouseDown(ImGuiMouseButton_Left)&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        positions[size_t(dragging)].x+=io.MouseDelta.x/zoom;positions[size_t(dragging)].y+=io.MouseDelta.y/zoom;
    }
    if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))dragging=-1;
    draw->PopClipRect();ImGui::EndChild();
    return clicked;
}
