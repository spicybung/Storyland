#pragma once

struct StorylandPcShoulderButtons {
    bool l1 = false;
    bool l2 = false;
    bool r1 = false;
    bool r2 = false;
};

inline StorylandPcShoulderButtons storylandPcShoulderButtons(
    bool driving, bool q, bool e, bool r, bool z, bool c, bool space, bool aim, bool centre) {
    return {
        driving ? r : centre,
        z || (driving && q),
        driving ? space : aim,
        c || (driving && e)
    };
}
