#pragma once

#include "src/graphics/storyland_world_alpha.h"

inline void storylandBindWorldAlphaPass(int consoleProfile, unsigned phase, unsigned pass) {
    const auto state = storylandWorldAlphaPass(consoleProfile, phase, pass);
    const GLenum comparison = state.comparison == StorylandWorldAlphaComparison::Less ? GL_LESS :
        state.comparison == StorylandWorldAlphaComparison::Greater ? GL_GREATER : GL_GEQUAL;
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(comparison, state.reference);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, state.additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(state.writeDepth ? GL_TRUE : GL_FALSE);
}
