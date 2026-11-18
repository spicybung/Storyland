#pragma once

// Reflection is performed at the presentation boundary.  Script coordinates,
// traffic routes, collisions, and gravity keep their original world space.
inline float storylandInvertedMapHorizontal(float horizontal, bool invertedMap) {
    return invertedMap ? -horizontal : horizontal;
}
