#pragma once
#include "src/storyland/storyland_analysis_graph.h"
#include <imgui.h>
#include <vector>

class StorylandAnalysisCanvas {
public:
    void reset();
    int draw(const StorylandAnalysisGraph& graph, int selected);
private:
    std::vector<ImVec2> positions;
    std::vector<bool> collapsed;
    ImVec2 pan{32, 32};
    float zoom = 1;
    int dragging = -1;
    int lastSelected = -1;
    bool frameRoot = true;
    void arrange(const StorylandAnalysisGraph& graph);
};
