#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace storyland_vcs_jetski_driving {

struct ControlState {
    float longitudinal = 0;
    float lateral = 0;
};

struct PoseState {
    float lateral = 0;
    float longitudinal = 0;
};

struct ControlInput {
    int16_t horizontal = 0;
    int16_t vertical = 0;
    float nativeStep = 0;
};

struct Association {
    bool present = false;
    float weight = 0;
    float blendDelta = 0;
};

struct Associations {
    Association reverse210;
    Association idle211;
    Association special201;
    Association special202;
    Association special203;
};

struct DrivingInput {
    bool player = false;
    ControlState controls;
    float nativeForwardSpeed = 0;
    float nativeAngularSpeedSquared = 0;
    float upZ = 1;
    float throttle = 0;
    float brake = 0;
    float maximumForwardSpeed = 0;
    float reverseSpeedLimit = 0;
    float nativeStep = 0;
    float blendPredictionTime = 0;
};

struct AssociationRequests {
    bool knockOff = false;
    bool fastReversePose = false;
    int32_t selectedAnimation = -1;
    int32_t blendAnimation = -1;
    float blendRate = 0;
    bool fade210 = false;
    bool fade211 = false;
    float fadeDelta = -4;
};

struct AxisSample {
    int32_t animation = -1;
    int32_t oppositeAnimation = -1;
    float weight = 0;
    float fraction = 0;
    bool stopAnimationClock = true;
};

struct PoseOutput {
    int32_t group = 3;
    float availableWeight = 1;
    AxisSample lateral;
    AxisSample longitudinal;
    bool hasLongitudinal = false;
};

inline bool finite(const ControlState& state) {
    return std::isfinite(state.longitudinal) && std::isfinite(state.lateral);
}

inline bool finite(const PoseState& state) {
    return std::isfinite(state.longitudinal) && std::isfinite(state.lateral);
}

inline bool finite(const Association& association) {
    return !association.present || (std::isfinite(association.weight) &&
        std::isfinite(association.blendDelta));
}

inline bool finite(const Associations& associations) {
    return finite(associations.reverse210) && finite(associations.idle211) &&
        finite(associations.special201) && finite(associations.special202) &&
        finite(associations.special203);
}

inline bool valid(const DrivingInput& input) {
    return finite(input.controls) && std::isfinite(input.nativeForwardSpeed) &&
        std::isfinite(input.nativeAngularSpeedSquared) && input.nativeAngularSpeedSquared >= 0 &&
        std::isfinite(input.upZ) && std::isfinite(input.throttle) &&
        std::isfinite(input.brake) && std::isfinite(input.maximumForwardSpeed) &&
        std::isfinite(input.reverseSpeedLimit) && std::isfinite(input.nativeStep) &&
        input.nativeStep >= 0 && std::isfinite(input.blendPredictionTime) &&
        input.blendPredictionTime >= 0;
}

inline const Association* specialAssociation(const Associations& associations) {
    if (associations.special201.present) return &associations.special201;
    if (associations.special202.present) return &associations.special202;
    if (associations.special203.present) return &associations.special203;
    return nullptr;
}

inline bool advanceControls(const ControlInput& input, ControlState& state) {
    if (!finite(state) || !std::isfinite(input.nativeStep) || input.nativeStep < 0)
        return false;
    const float gain = input.nativeStep * .2f;
    const float longitudinalTarget = -float(input.vertical) / 128.0f;
    const float lateralTarget = -float(input.horizontal) / 128.0f;
    const float longitudinalChange = (longitudinalTarget - state.longitudinal) * gain;
    const float lateralChange = (lateralTarget - state.lateral) * gain;
    ControlState result;
    result.longitudinal = std::clamp(state.longitudinal + longitudinalChange, -1.0f, 1.0f);
    result.lateral = std::clamp(state.lateral + lateralChange, -1.0f, 1.0f);
    if (!finite(result)) return false;
    state = result;
    return true;
}

inline bool associationRequests(const DrivingInput& input,
    const Associations& associations, AssociationRequests& output) {
    if (!valid(input) || !finite(associations)) return false;
    AssociationRequests result;
    if (input.nativeAngularSpeedSquared > .09f) {
        result.knockOff = true;
        output = result;
        return true;
    }
    const Association* selected = nullptr;
    if (!specialAssociation(associations) && std::fabs(input.nativeForwardSpeed) < .02f) {
        result.selectedAnimation = 211;
        result.blendRate = 2;
        selected = &associations.idle211;
    } else if (input.nativeForwardSpeed < 0) {
        const float knockOffSpeed = input.reverseSpeedLimit * 3.5f;
        if (input.nativeForwardSpeed < knockOffSpeed && input.upZ < -.5f) {
            result.knockOff = true;
            output = result;
            return true;
        }
        const float reverseBlendSpeed = input.reverseSpeedLimit * 1.5f;
        if (input.throttle < 0 && reverseBlendSpeed < input.nativeForwardSpeed) {
            result.selectedAnimation = 210;
            result.blendRate = 4;
            selected = &associations.reverse210;
        } else if (input.player && input.nativeForwardSpeed < reverseBlendSpeed) {
            result.fastReversePose = true;
        }
    }
    if (selected) {
        if (!selected->present || (selected->weight < 1 && selected->blendDelta <= 0))
            result.blendAnimation = result.selectedAnimation;
    } else {
        result.fade211 = associations.idle211.present && associations.idle211.blendDelta >= 0;
        result.fade210 = associations.reverse210.present && associations.reverse210.blendDelta >= 0;
    }
    output = result;
    return true;
}

inline float predictedWeight(const Association& association, float blendPredictionTime) {
    const float interval = blendPredictionTime * .02f;
    const float change = association.blendDelta * interval;
    return association.weight + change;
}

inline float remainingWeight(const Associations& associations, float blendPredictionTime) {
    float available = 1;
    if (associations.idle211.present)
        available -= std::min(1.0f, predictedWeight(associations.idle211, blendPredictionTime));
    if (const Association* special = specialAssociation(associations))
        available -= std::min(available, predictedWeight(*special, blendPredictionTime));
    if (associations.reverse210.present)
        available -= std::min(available,
            predictedWeight(associations.reverse210, blendPredictionTime));
    return available;
}

inline float retained(float base, float nativeStep) {
    return static_cast<float>(std::pow(static_cast<double>(base),
        static_cast<double>(nativeStep)));
}

inline bool advancePose(const DrivingInput& input, const AssociationRequests& requests,
    const Associations& currentAssociations, PoseState& state, PoseOutput& output) {
    if (!valid(input) || !finite(state) || !finite(currentAssociations) || requests.knockOff)
        return false;
    constexpr float lateralScale = .921052634716033935546875f;
    constexpr float lateralReduction = 3.8461539745330810546875f;
    const float lateralTarget = requests.fastReversePose ? 0.0f :
        std::clamp(-(input.controls.lateral * lateralScale), -1.0f, 1.0f);
    const float lateralRetention = retained(.86f, input.nativeStep);
    const float lateralTargetPart = lateralTarget * (1.0f - lateralRetention);
    const float lateralPreviousPart = state.lateral * lateralRetention;
    PoseState result;
    result.lateral = lateralTargetPart + lateralPreviousPart;
    if (input.player) {
        float longitudinalTarget = requests.fastReversePose ? -1.0f : input.controls.longitudinal;
        if (!requests.fastReversePose) {
            if (input.brake > .5f && input.nativeForwardSpeed > .01f) {
                longitudinalTarget = std::max(.1f, longitudinalTarget);
            } else if (input.throttle > .5f && input.controls.longitudinal <= 0 &&
                input.nativeForwardSpeed < input.maximumForwardSpeed * .3f) {
                longitudinalTarget = std::min(.1f, input.controls.longitudinal);
            }
            if (std::fabs(lateralTarget) > .3f) {
                const float difference = std::fabs(lateralTarget) - .3f;
                const float reduction = difference * lateralReduction;
                longitudinalTarget *= std::max(0.0f, 1.0f - reduction);
            }
        }
        const float longitudinalRetention = retained(.89f, input.nativeStep);
        const float longitudinalTargetPart = longitudinalTarget * (1.0f - longitudinalRetention);
        const float longitudinalPreviousPart = state.longitudinal * longitudinalRetention;
        result.longitudinal = longitudinalTargetPart + longitudinalPreviousPart;
    }
    if (!finite(result)) return false;
    float lateralWeight = 0;
    float longitudinalWeight = 0;
    if (!specialAssociation(currentAssociations)) {
        if (std::fabs(result.lateral) > .56f || !input.player) {
            lateralWeight = 1;
        } else if (std::fabs(result.longitudinal) > .56f) {
            longitudinalWeight = 1;
        } else {
            const float longitudinalSquared = result.longitudinal * result.longitudinal;
            const float lateralSquared = result.lateral * result.lateral;
            const float magnitude = std::sqrt(longitudinalSquared + lateralSquared);
            if (magnitude > .01f) {
                lateralWeight = std::fabs(result.lateral / magnitude);
                longitudinalWeight = std::fabs(result.longitudinal / magnitude);
            } else {
                lateralWeight = std::fabs(result.lateral);
                longitudinalWeight = std::fabs(result.longitudinal);
            }
        }
    }
    PoseOutput pose;
    pose.availableWeight = remainingWeight(currentAssociations, input.blendPredictionTime);
    pose.lateral.animation = result.lateral < 0 ? 189 : 190;
    pose.lateral.oppositeAnimation = result.lateral < 0 ? 190 : 189;
    pose.lateral.weight = lateralWeight * pose.availableWeight;
    pose.lateral.fraction = std::fabs(result.lateral);
    pose.hasLongitudinal = input.player;
    if (input.player) {
        pose.longitudinal.animation = result.longitudinal < 0 ? 191 : 192;
        pose.longitudinal.oppositeAnimation = result.longitudinal < 0 ? 192 : 191;
        pose.longitudinal.weight = longitudinalWeight * pose.availableWeight;
        pose.longitudinal.fraction = std::fabs(result.longitudinal);
    }
    if (!std::isfinite(pose.availableWeight) || !std::isfinite(pose.lateral.weight) ||
        !std::isfinite(pose.longitudinal.weight)) return false;
    state = result;
    output = pose;
    return true;
}

}
