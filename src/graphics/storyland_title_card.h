#pragma once

class StorylandTitleCard {
public:
    void begin(bool available) {
        phase = available ? Phase::ReleasePreviousInput : Phase::Closed;
    }

    void cancel() {
        phase = Phase::Closed;
    }

    bool active() const {
        return phase != Phase::Closed;
    }

    bool advance(bool confirmationHeld) {
        switch(phase) {
        case Phase::Closed:
            return false;
        case Phase::ReleasePreviousInput:
            if(!confirmationHeld) phase = Phase::WaitForConfirmation;
            return false;
        case Phase::WaitForConfirmation:
            if(confirmationHeld) phase = Phase::ReleaseConfirmation;
            return false;
        case Phase::ReleaseConfirmation:
            if(confirmationHeld) return false;
            phase = Phase::Closed;
            return true;
        }
        return false;
    }

private:
    enum class Phase { Closed, ReleasePreviousInput, WaitForConfirmation, ReleaseConfirmation };
    Phase phase = Phase::Closed;
};
