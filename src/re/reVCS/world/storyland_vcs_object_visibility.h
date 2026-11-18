#pragma once

// Original Soldier uses visible area 2 while drawing ordinary area-0 props.
// The complete native area predicate is still under investigation.
inline bool storylandVcsScriptObjectAreaVisible(int objectArea, int visibleArea) {
    return objectArea == visibleArea || (objectArea == 0 && visibleArea == 2);
}
