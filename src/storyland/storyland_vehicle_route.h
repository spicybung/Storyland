#pragma once

#include "src/leeds/leeds_native_world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

inline void storylandInvalidateVehicleDestinationRoute(StorylandNativeTrafficActor& actor) {
    actor.node=actor.nextNode=actor.previousNode=UINT32_MAX;
    actor.distance=0;
    actor.groundOffset=0;
    actor.crossingCommitted=false;
    actor.destinationRoute.clear();
    actor.destinationRouteCursor=0;
    actor.destinationRouteValid=false;
    actor.destinationRouteFinished=false;
    actor.destinationRetryAfter=0;
}

inline uint64_t storylandVehicleRoadRevision(const std::vector<StorylandRoadChange>* changes,
    bool ignoreDisabledRoads = false) {
    uint64_t revision = 14695981039346656037ull;
    const auto include = [&](uint32_t value) {
        revision ^= value;
        revision *= 1099511628211ull;
    };
    include(changes ? uint32_t(changes->size()) : 0u);
    if (ignoreDisabledRoads) include(0x20u);
    if (!changes) return revision;
    for (const auto& change : *changes) {
        for (float bound : change.bounds) {
            uint32_t bits = 0;
            std::memcpy(&bits, &bound, sizeof(bits));
            include(bits);
        }
        uint32_t width = 0;
        std::memcpy(&width, &change.width, sizeof(width));
        include(width);
        include(uint32_t(change.vehicle));
        include(uint32_t(change.enabled));
        include(uint32_t(change.angled));
        include(uint32_t(change.mode));
    }
    return revision;
}

inline bool storylandVehicleRouteEdgeAllowed(const StorylandNativePaths& paths,
    uint32_t source, uint32_t target, const StorylandNativeZones* zones,
    bool avoidLevelTransitions, const std::vector<uint16_t>* flags = nullptr,
    bool ignoreDisabledRoads = false) {
    if (source >= paths.nodes.size() || target >= paths.nodes.size() || source == target) return false;
    const auto& from = paths.nodes[source];
    const auto& to = paths.nodes[target];
    if (!from.vehicle || !to.vehicle || from.water || to.water) return false;
    if (!paths.vcs || !ignoreDisabledRoads) {
        if (flags && flags->size() == paths.nodes.size()) {
            if (((*flags)[source] & 0x20u) || ((*flags)[target] & 0x20u)) return false;
        } else if (from.disabled || to.disabled) return false;
    }
    const StorylandNativePathLink* matching = nullptr;
    for (const auto& link : from.links) {
        if (link.target == target && (!link.nativeLanes || link.lanes != 0)) {
            matching = &link;
            break;
        }
    }
    if (!matching) return false;
    if (!avoidLevelTransitions) return true;
    if (!paths.vcs) {
        const uint16_t destinationFlags = flags && flags->size() == paths.nodes.size() ?
            (*flags)[target] : to.flags;
        return (destinationFlags & 0x40u) == 0;
    }
    if (!zones) return true;
    const auto* first = zones->at(from.position, false);
    const auto* second = zones->at(to.position, false);
    return !first || !second || first->level == second->level;
}

inline float storylandVehicleRouteDistance(const StorylandNativePoint& first,
    const StorylandNativePoint& second) {
    const float horizontal = std::hypot(first.x - second.x, first.y - second.y);
    return std::hypot(horizontal, (first.z - second.z) * 2.0f);
}

inline uint32_t storylandFindVcsVehicleRouteNode(const StorylandNativePaths& paths,
    const StorylandNativePoint& position,const std::vector<uint16_t>* roadFlags=nullptr,
    bool ignoreDisabledRoads=false) {
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))
        return UINT32_MAX;
    const auto cell=[](float coordinate,float origin) {
        return int(std::clamp(coordinate*.0125f+origin,0.0f,49.0f));
    };
    const int minimumX=cell(position.x-100.0f,30.0f),maximumX=cell(position.x+100.0f,30.0f);
    const int minimumY=cell(position.y-100.0f,25.0f),maximumY=cell(position.y+100.0f,25.0f);
    float nearest=10000.0f;
    uint32_t result=UINT32_MAX;
    const auto consider=[&](uint32_t index) {
        if(index>=paths.nodes.size())return false;
        const auto& node=paths.nodes[index];
        const bool hasRoadFlags=roadFlags&&roadFlags->size()==paths.nodes.size();
        const uint16_t flags=hasRoadFlags?(*roadFlags)[index]:node.flags;
        if(!node.vehicle||node.water||(flags&0x200u)||
            (!ignoreDisabledRoads&&((flags&0x20u)||
                (!hasRoadFlags&&node.disabled))))return false;
        const float distance=std::fabs(node.position.x-position.x)+
            std::fabs(node.position.y-position.y)+3.0f*std::fabs(node.position.z-position.z);
        if(distance<nearest) {nearest=distance;result=index;}
        return distance<0.010000000707805157f;
    };
    if(paths.vcsSearchGrid.size()==2500) {
        // FindNodeClosestToCoors (0x268880) scans Y, then X, then the
        // retained vehicle list. A strict comparison keeps the first tie.
        for(int y=minimumY;y<=maximumY;++y)for(int x=minimumX;x<=maximumX;++x)
            for(uint16_t index:paths.vcsSearchGrid[size_t(x)*50+size_t(y)][0])
                if(consider(index))return result;
    } else {
        // Synthetic path fixtures can omit the decoded DTZ search grid.
        for(uint32_t index=0;index<paths.nodes.size();++index) {
            const auto& node=paths.nodes[index];
            const int x=cell(node.position.x,30.0f),y=cell(node.position.y,25.0f);
            if(x<minimumX||x>maximumX||y<minimumY||y>maximumY)continue;
            if(consider(index))return result;
        }
    }
    return result;
}

inline bool storylandVehicleDestinationReached(const StorylandNativePoint& position,
    const StorylandNativePoint& destination) {
    return std::isfinite(position.x)&&std::isfinite(position.y)&&
        std::isfinite(destination.x)&&std::isfinite(destination.y)&&
        std::hypot(position.x-destination.x,position.y-destination.y)<=2.0f;
}

inline void storylandAdvanceVehicleTerminalApproach(StorylandNativeTrafficActor& actor,
    const StorylandNativePoint& destination,float turnRate,float speed,float seconds) {
    if(!std::isfinite(speed)||!std::isfinite(seconds)||seconds<=0||
        storylandVehicleDestinationReached(actor.position,destination))return;
    constexpr float radians=0.01745329251994329577f;
    const float desired=std::atan2(destination.y-actor.position.y,
        destination.x-actor.position.x)/radians;
    actor.yaw+=std::clamp(std::remainder(desired-actor.yaw,360.0f),
        -std::max(0.0f,turnRate)*seconds,std::max(0.0f,turnRate)*seconds);
    const float remaining=std::hypot(destination.x-actor.position.x,destination.y-actor.position.y);
    const float displacement=std::min(std::max(0.0f,speed)*seconds,
        std::max(0.0f,remaining-1.0f));
    actor.position.x+=std::cos(actor.yaw*radians)*displacement;
    actor.position.y+=std::sin(actor.yaw*radians)*displacement;
}

inline bool storylandFindVehicleDestinationRoute(const StorylandNativePaths& paths,
    uint32_t start, const StorylandNativePoint& destination,
    const StorylandNativeZones* zones, bool avoidLevelTransitions,
    std::vector<uint32_t>& result, const std::vector<uint16_t>* roadFlags = nullptr,
    bool ignoreDisabledRoads = false) {
    result.clear();
    if (start >= paths.nodes.size() || !std::isfinite(destination.x) ||
        !std::isfinite(destination.y) || !std::isfinite(destination.z)) return false;
    const size_t count = paths.nodes.size();
    const auto& first = paths.nodes[start];
    ignoreDisabledRoads = paths.vcs && ignoreDisabledRoads;
    if (!first.vehicle || first.water ||
        (!ignoreDisabledRoads && (roadFlags && roadFlags->size() == count ?
            ((*roadFlags)[start] & 0x20u) != 0 : first.disabled)))
        return false;

    uint32_t goal = UINT32_MAX;
    if (paths.vcs) {
        goal = storylandFindVcsVehicleRouteNode(paths,destination,roadFlags,ignoreDisabledRoads);
        if (goal == UINT32_MAX) return false;
        if (goal == start) {
            result.push_back(start);
            return true;
        }
    }

    const float infinity = std::numeric_limits<float>::infinity();
    std::vector<float> distances(count, infinity);
    std::vector<uint32_t> parents(count, UINT32_MAX);
    using QueueItem = std::pair<float,uint32_t>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
    distances[start] = 0;
    queue.push(QueueItem{0.0f,start});
    while (!queue.empty()) {
        const auto item = queue.top();
        queue.pop();
        const uint32_t node = item.second;
        if (item.first > distances[node]) continue;
        for (const auto& link : paths.nodes[node].links) {
            if (!storylandVehicleRouteEdgeAllowed(paths,node,link.target,zones,
                avoidLevelTransitions,roadFlags,ignoreDisabledRoads)) continue;
            const float length = storylandVehicleRouteDistance(paths.nodes[node].position,
                paths.nodes[link.target].position);
            if (!std::isfinite(length) || length < 0.001f) continue;
            const float candidate = item.first + length;
            if (candidate >= distances[link.target]) continue;
            distances[link.target] = candidate;
            parents[link.target] = node;
            queue.push({candidate,link.target});
        }
    }

    if (paths.vcs) {
        if (!std::isfinite(distances[goal])) return false;
    } else {
        float closestToDestination = infinity;
        for (uint32_t node = 0; node < count; ++node) {
            const auto& candidate = paths.nodes[node];
            if (!candidate.vehicle || candidate.water ||
                (roadFlags && roadFlags->size() == count ? ((*roadFlags)[node] & 0x20u) != 0 : candidate.disabled))
                continue;
            closestToDestination = std::min(closestToDestination,
                storylandVehicleRouteDistance(candidate.position,destination));
        }
        if (!std::isfinite(closestToDestination)) return false;

        float bestScore = infinity;
        for (uint32_t node = 0; node < count; ++node) {
            if (!std::isfinite(distances[node])) continue;
            const float separation = storylandVehicleRouteDistance(paths.nodes[node].position,destination);
            if (separation > closestToDestination + 8.0f) continue;
            const float score = separation * 100.0f + distances[node] * 0.01f;
            if (score < bestScore) { bestScore = score; goal = node; }
        }
    }
    if (goal == UINT32_MAX) return false;

    for (uint32_t node = goal; node != UINT32_MAX; node = parents[node]) {
        result.push_back(node);
        if (node == start) break;
        if (result.size() > count) { result.clear(); return false; }
    }
    if (result.empty() || result.back() != start) { result.clear(); return false; }
    std::reverse(result.begin(),result.end());
    return true;
}

inline bool storylandRejoinVehicleDestinationRoad(const StorylandNativePaths& paths,
    StorylandNativeTrafficActor& actor, const StorylandNativeZones* zones,
    bool avoidLevelTransitions, const std::vector<uint16_t>* roadFlags) {
    float closest = std::numeric_limits<float>::infinity();
    uint32_t bestFrom = UINT32_MAX, bestTo = UINT32_MAX;
    float bestFraction = 0, bestLength = 0;
    const float radians = actor.yaw * 0.01745329251994329577f;
    const float headingX = std::cos(radians), headingY = std::sin(radians);
    for (uint32_t source = 0; source < paths.nodes.size(); ++source) {
        const auto& node = paths.nodes[source];
        if (!node.vehicle || node.water) continue;
        for (const auto& link : node.links) {
            if (!storylandVehicleRouteEdgeAllowed(paths,source,link.target,zones,
                avoidLevelTransitions,roadFlags)) continue;
            const auto& target = paths.nodes[link.target].position;
            const float dx = target.x-node.position.x;
            const float dy = target.y-node.position.y;
            const float dz = target.z-node.position.z;
            const float lengthSquared = dx*dx+dy*dy+dz*dz;
            if (lengthSquared < .0001f) continue;
            const float fraction = std::clamp(((actor.position.x-node.position.x)*dx+
                (actor.position.y-node.position.y)*dy+
                (actor.position.z-node.position.z)*dz)/lengthSquared,0.0f,1.0f);
            const StorylandNativePoint nearest{node.position.x+dx*fraction,
                node.position.y+dy*fraction,node.position.z+dz*fraction};
            const float horizontal = std::hypot(dx,dy);
            const float alignment = horizontal>.001f ?
                (headingX*dx+headingY*dy)/horizontal : 0;
            // A forward-facing mission truck must not attach to a nearby
            // opposite-direction edge. Direction is more important than the
            // final metre of lateral distance when initially joining a road.
            const float oppositePenalty=alignment<0.0f?12.0f*(-alignment):0.0f;
            const float score = storylandVehicleRouteDistance(actor.position,nearest)+
                (1.0f-alignment)*3.0f+oppositePenalty;
            if (score < closest) {
                closest=score;
                bestFrom=source;
                bestTo=link.target;
                bestFraction=fraction;
                bestLength=std::sqrt(lengthSquared);
            }
        }
    }
    if (bestFrom==UINT32_MAX) return false;
    actor.previousNode=UINT32_MAX;
    actor.node=bestFrom;
    actor.nextNode=bestTo;
    actor.distance=bestLength*bestFraction;
    actor.groundOffset=actor.position.z-(paths.nodes[bestFrom].position.z*(1.0f-bestFraction)+
        paths.nodes[bestTo].position.z*bestFraction);
    actor.lane=0;
    actor.crossingCommitted=false;
    return true;
}

inline bool storylandPrepareVehicleDestinationRoute(const StorylandNativePaths& paths,
    StorylandNativeTrafficActor& actor, const StorylandNativePoint& destination,
    const StorylandNativeZones* zones, bool avoidLevelTransitions,
    const std::vector<StorylandRoadChange>* roadChanges = nullptr,
    uint8_t straightLineDistance = 0, bool ignoreDisabledRoads = false) {
    if (actor.water || !actor.vehicle || !std::isfinite(destination.x) ||
        !std::isfinite(destination.y) || !std::isfinite(destination.z)) return false;
    ignoreDisabledRoads = paths.vcs && ignoreDisabledRoads;
    const uint64_t roadRevision = storylandVehicleRoadRevision(roadChanges,ignoreDisabledRoads);
    const auto directApproach = [&]() {
        storylandInvalidateVehicleDestinationRoute(actor);
        actor.destinationRouteTarget = destination;
        actor.destinationRoadRevision = roadRevision;
        actor.destinationAvoidLevelTransitions = avoidLevelTransitions;
        actor.destinationRouteValid = true;
        actor.destinationRouteFinished = true;
        return true;
    };
    if (paths.vcs && std::hypot(destination.x-actor.position.x,
        destination.y-actor.position.y) < float(straightLineDistance))
        return directApproach();
    const bool changed = !actor.destinationRouteValid ||
        storylandVehicleRouteDistance(actor.destinationRouteTarget,destination) > 0.25f ||
        actor.destinationRoadRevision != roadRevision ||
        actor.destinationAvoidLevelTransitions != avoidLevelTransitions;
    const bool refill = paths.vcs && !changed && actor.destinationRouteFinished &&
        !actor.destinationRoute.empty();
    if (!changed && actor.destinationRouteFinished && !refill) return true;
    if (!changed && actor.destinationRouteCursor < actor.destinationRoute.size() &&
        actor.destinationRoute[actor.destinationRouteCursor] == actor.nextNode &&
        storylandVehicleRouteEdgeAllowed(paths,actor.node,actor.nextNode,zones,avoidLevelTransitions,
            nullptr,ignoreDisabledRoads)) return true;

    const bool hadDestinationRoute=actor.destinationRouteValid;
    actor.destinationRouteValid = false;
    actor.destinationRouteFinished = false;
    actor.destinationRoute.clear();
    actor.destinationRouteCursor = 0;
    std::vector<uint16_t> roadFlags;
    if (roadChanges && !roadChanges->empty())
        roadFlags = storylandNativeRoadNodeFlags(paths,*roadChanges);
    const auto* flags = roadFlags.empty() ? nullptr : &roadFlags;

    if (paths.vcs) {
        // Initial 43B858 selects a fresh start; 437408 refills from its road node.
        uint32_t start = refill ? actor.node :
            storylandFindVcsVehicleRouteNode(paths,actor.position,flags,ignoreDisabledRoads);
        if (start == UINT32_MAX || !storylandFindVehicleDestinationRoute(paths,start,
            destination,zones,avoidLevelTransitions,actor.destinationRoute,flags,ignoreDisabledRoads))
            return directApproach();

        // 266E80 excludes the source; 267208 removes a passed first node.
        actor.destinationRoute.erase(actor.destinationRoute.begin());
        if (actor.destinationRoute.size() > 8) actor.destinationRoute.resize(8);
        if (!refill && actor.destinationRoute.size() >= 2) {
            const auto& first = paths.nodes[actor.destinationRoute[0]].position;
            const auto& second = paths.nodes[actor.destinationRoute[1]].position;
            const float dot = (first.x-actor.position.x)*(second.x-actor.position.x) +
                (first.y-actor.position.y)*(second.y-actor.position.y);
            if (dot < 0.0f) {
                start = actor.destinationRoute.front();
                actor.destinationRoute.erase(actor.destinationRoute.begin());
            }
        }
        // Initial road steering needs two nodes; 437408 can refill one last edge.
        if (actor.destinationRoute.size() < (refill ? 1u : 2u)) return directApproach();

        if (!refill) actor.previousNode = UINT32_MAX;
        actor.node = start;
        actor.nextNode = actor.destinationRoute.front();
        const auto& from = paths.nodes[actor.node].position;
        const auto& to = paths.nodes[actor.nextNode].position;
        const float dx = to.x-from.x, dy = to.y-from.y, dz = to.z-from.z;
        const float lengthSquared = dx*dx+dy*dy+dz*dz;
        const float fraction = lengthSquared > 0.0001f ? std::clamp(
            ((actor.position.x-from.x)*dx+(actor.position.y-from.y)*dy+
                (actor.position.z-from.z)*dz)/lengthSquared,0.0f,1.0f) : 0.0f;
        actor.distance = std::sqrt(lengthSquared)*fraction;
        actor.groundOffset = actor.position.z-(from.z+dz*fraction);
        if (!refill) actor.lane = 0;
        actor.crossingCommitted = false;
        actor.destinationRouteTarget = destination;
        actor.destinationRoadRevision = roadRevision;
        actor.destinationAvoidLevelTransitions = avoidLevelTransitions;
        actor.destinationRouteValid = true;
        return true;
    }

    bool wrongStartingDirection=false;
    if(actor.node<paths.nodes.size()&&actor.nextNode<paths.nodes.size()&&
       !hadDestinationRoute){
        const auto& from=paths.nodes[actor.node].position;
        const auto& to=paths.nodes[actor.nextNode].position;
        const float dx=to.x-from.x,dy=to.y-from.y;
        const float distance=std::hypot(dx,dy);
        if(distance>0.01f){
            const float radians=actor.yaw*0.01745329251994329577f;
            const float alignment=(std::cos(radians)*dx+std::sin(radians)*dy)/distance;
            wrongStartingDirection=alignment < -0.35f;
        }
    }
    if (wrongStartingDirection || !storylandVehicleRouteEdgeAllowed(paths,actor.node,actor.nextNode,zones,avoidLevelTransitions,flags)) {
        if (!storylandRejoinVehicleDestinationRoad(paths,actor,zones,avoidLevelTransitions,flags))
            return false;
        if (!storylandVehicleRouteEdgeAllowed(paths,actor.node,actor.nextNode,zones,avoidLevelTransitions,flags))
            return false;
    }
    if (!storylandFindVehicleDestinationRoute(paths,actor.nextNode,destination,zones,
        avoidLevelTransitions,actor.destinationRoute,flags)) return false;
    actor.destinationRouteCursor = 0;
    actor.destinationRouteTarget = destination;
    actor.destinationRoadRevision = roadRevision;
    actor.destinationAvoidLevelTransitions = avoidLevelTransitions;
    actor.destinationRouteValid = true;
    return true;
}
