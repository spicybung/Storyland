#include "src/storyland/storyland_play_test_camera.h"
#include "src/storyland/storyland_mission_vm.h"
#include "src/storyland/storyland_script_object.h"
#include "src/re/reVCS/camera/storyland_vcs_vehicle_camera.h"
#include "src/re/reVCS/vehicles/storyland_vcs_vehicle_follow_fov.h"
#include "src/re/reVCS/camera/storyland_vcs_remote_camera.h"
#include "src/re/reVCS/camera/storyland_vcs_ped_camera_zoom.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_stories_speed_motion.h"
#include <algorithm>
#include <cmath>

float storylandScriptCameraDistance(const StorylandMissionEnvironment& environment, float base, bool vehicle) {
    const auto& zoom = environment.cameraZoom;
    if(!vehicle&&environment.vcsCharacterHandleBridge)
        return StorylandVcsPedCameraZoom::distance(zoom.pedCurrent);
    const float offset = vehicle ? (zoom.vehicle ? zoom.vehicleCurrent : 0) : (zoom.ped ? zoom.pedCurrent : 0);
    return std::max(.03f, base + offset);
}

namespace {
constexpr float radians = 0.01745329251994329577f;
using Point = StorylandNativePoint;
Point add(Point a, Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point sub(Point a, Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point mul(Point a, float b) { return {a.x*b,a.y*b,a.z*b}; }
float dot(Point a, Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Point cross(Point a, Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
bool finite(Point a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
float length(Point a) { return std::sqrt(dot(a,a)); }
struct NativeCollisionCandidates {
    std::vector<uint32_t> owned;
    const std::vector<uint32_t>* borrowed=nullptr;
    const std::vector<uint32_t>& values() const {return borrowed?*borrowed:owned;}
};
std::vector<uint32_t> nativeCollisionInstancesForSegment(const StorylandNativeWorld& world,
    const Point& target,const Point& eye,int32_t x0,int32_t x1,int32_t y0,int32_t y1) {
    std::vector<uint32_t> candidates=world.collisionLargeInstances;
    const double directionX=double(eye.x)-target.x,directionY=double(eye.y)-target.y;
    constexpr double cellMargin=.151;
    for(int32_t y=y0;y<=y1;++y) {
        int32_t firstX=x0,lastX=x1;
        if(x0!=x1&&y0!=y1&&std::fabs(directionY)>1e-12) {
            // Restrict each grid row to the ray, retaining lens margins and border tolerance.
            const double lower=double(y)*StorylandCollisionCellSize-cellMargin;
            const double upper=double(y+1)*StorylandCollisionCellSize+cellMargin;
            double first=(lower-target.y)/directionY,second=(upper-target.y)/directionY;
            if(first>second)std::swap(first,second);
            first=std::max(0.0,first);second=std::min(1.0,second);
            if(first>second)continue;
            const double a=target.x+directionX*first,b=target.x+directionX*second;
            firstX=std::max(x0,int32_t(std::floor((std::min(a,b)-cellMargin)/StorylandCollisionCellSize)));
            lastX=std::min(x1,int32_t(std::floor((std::max(a,b)+cellMargin)/StorylandCollisionCellSize)));
        }
        for(int32_t x=firstX;x<=lastX;++x) {
            const auto cell=world.collisionCells.find({x,y});
            if(cell!=world.collisionCells.end())candidates.insert(candidates.end(),cell->second.begin(),cell->second.end());
        }
    }
    std::sort(candidates.begin(),candidates.end());
    candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
    return candidates;
}
// Invert the complete affine transform, including scaled/rotated instances.
bool localSegment(const std::array<float,16>& m, Point start, Point end, Point& a, Point& b) {
    const Point x{m[0],m[1],m[2]}, y{m[4],m[5],m[6]}, z{m[8],m[9],m[10]};
    const Point rx=cross(y,z), ry=cross(z,x), rz=cross(x,y);
    const float determinant=dot(x,rx);
    if (!std::isfinite(determinant)||std::fabs(determinant)<1e-8f) return false;
    const auto convert=[&](Point p) {
        p=sub(p,{m[12],m[13],m[14]});
        return Point{dot(p,rx)/determinant,dot(p,ry)/determinant,dot(p,rz)/determinant};
    };
    a=convert(start); b=convert(end);
    return finite(a)&&finite(b);
}
float boxHit(Point a, Point d, Point minimum, Point maximum) {
    float low=0, high=1;
    const float origin[]={a.x,a.y,a.z}, direction[]={d.x,d.y,d.z};
    const float lower[]={minimum.x,minimum.y,minimum.z}, upper[]={maximum.x,maximum.y,maximum.z};
    for(int axis=0;axis<3;++axis) {
        if (std::fabs(direction[axis])<1e-8f) {
            if (origin[axis]<lower[axis]||origin[axis]>upper[axis]) return 1;
        } else {
            float first=(lower[axis]-origin[axis])/direction[axis];
            float second=(upper[axis]-origin[axis])/direction[axis];
            if(first>second) std::swap(first,second);
            low=std::max(low,first); high=std::min(high,second);
            if(low>high) return 1;
        }
    }
    return low;
}
float sphereHit(Point a, Point d, Point center, float radius) {
    const Point offset=sub(a,center);
    const float aa=dot(d,d), bb=dot(offset,d), cc=dot(offset,offset)-radius*radius;
    if(cc<=0) return 0;
    const float discriminant=bb*bb-aa*cc;
    if(aa<1e-10f||discriminant<0) return 1;
    const float hit=(-bb-std::sqrt(discriminant))/aa;
    return hit>=0&&hit<=1?hit:1;
}
inline bool triangleOutsideBounds(const Point& a,const Point& b,const Point& c,
    const Point& minimum,const Point& maximum) {
    return std::max({a.x,b.x,c.x})<minimum.x||std::min({a.x,b.x,c.x})>maximum.x||
        std::max({a.y,b.y,c.y})<minimum.y||std::min({a.y,b.y,c.y})>maximum.y||
        std::max({a.z,b.z,c.z})<minimum.z||std::min({a.z,b.z,c.z})>maximum.z;
}
float triangleHit(Point origin, Point direction, Point a, Point b, Point c) {
    const Point edge1=sub(b,a), edge2=sub(c,a), p=cross(direction,edge2);
    const float determinant=dot(edge1,p);
    if(std::fabs(determinant)<1e-8f) return 1;
    const float inverse=1/determinant;
    const Point offset=sub(origin,a);
    const float u=dot(offset,p)*inverse;
    if(u<0||u>1) return 1;
    const Point q=cross(offset,edge1);
    const float v=dot(direction,q)*inverse;
    if(v<0||u+v>1) return 1;
    const float hit=dot(edge2,q)*inverse;
    return hit>=0&&hit<=1?hit:1;
}
Point desiredEye(const StorylandStoriesCamera& camera) {
    const float yaw=camera.yaw*radians, pitch=camera.pitch*radians;
    return add(camera.target,{-camera.requestedDistance*std::cos(pitch)*std::cos(yaw),
        -camera.requestedDistance*std::cos(pitch)*std::sin(yaw),0.75f+camera.requestedDistance*std::sin(pitch)});
}
}

static float traceNativeSegment(const StorylandNativeWorld& world, const Point& target, const Point& eye,
    bool lens, uint8_t* surface = nullptr, Point* normal = nullptr,
    bool buildings = true, bool dummies = true, uint16_t sectorLists = 0xFFFF,
    bool includeScriptObjects = true, bool requireNativeCollision = false,
    bool skipVisibleScriptObjects = false, bool faceNormalAgainstRay = true) {
    if(!finite(target)||!finite(eye)) return 1;
    const float distance=length(sub(eye,target));
    if(!std::isfinite(distance)||distance<1e-5f) return 1;
    float nearest=1;
    const auto record=[&](float hit,uint8_t material,Point localNormal,const std::array<float,16>& matrix) {
        if(hit>=nearest)return;
        nearest=hit;if(surface)*surface=material;
        if(normal) {
            const Point x{matrix[0],matrix[1],matrix[2]},y{matrix[4],matrix[5],matrix[6]},z{matrix[8],matrix[9],matrix[10]};
            const float determinant=dot(x,cross(y,z));
            auto result=add(mul(cross(y,z),localNormal.x),add(mul(cross(z,x),localNormal.y),mul(cross(x,y),localNormal.z)));
            if(determinant<0)result=mul(result,-1);
            const float magnitude=length(result);
            result=magnitude>1e-8f?mul(result,1/magnitude):Point{0,0,1};
            if(faceNormalAgainstRay&&dot(result,sub(eye,target))>0)result=mul(result,-1);
            *normal=result;
        }
    };
    // Cover the center and near-plane margins rather than allowing a wall to enter the lens.
    const Point margins[]={{0,0,0},{0.15f,0,0},{-0.15f,0,0},{0,0.15f,0},{0,-0.15f,0},{0,0,0.15f},{0,0,-0.15f}};
    NativeCollisionCandidates candidates;
    bool indexed=world.collisionIndexOwner==world.instances.data() &&
        world.collisionIndexCount==world.instances.size() &&
        world.collisionIndexModels==world.collisionModels.size();
    const float minX=std::min(target.x,eye.x)-0.15f,maxX=std::max(target.x,eye.x)+0.15f;
    const float minY=std::min(target.y,eye.y)-0.15f,maxY=std::max(target.y,eye.y)+0.15f;
    if(std::fabs(minX)>1e8f||std::fabs(maxX)>1e8f||std::fabs(minY)>1e8f||std::fabs(maxY)>1e8f) indexed=false;
    if(indexed) {
        const int32_t x0=int32_t(std::floor(minX/StorylandCollisionCellSize));
        const int32_t x1=int32_t(std::floor(maxX/StorylandCollisionCellSize));
        const int32_t y0=int32_t(std::floor(minY/StorylandCollisionCellSize));
        const int32_t y1=int32_t(std::floor(maxY/StorylandCollisionCellSize));
        if(int64_t(x1-x0+1)*int64_t(y1-y0+1)>4096) indexed=false;
        else if(x0==x1&&y0==y1&&world.collisionLargeInstances.empty()) {
            // Index construction and instance updates keep these lists sorted.
            // Vertical ground rays can visit the existing cell without a copy.
            const auto cell=world.collisionCells.find({x0,y0});
            if(cell!=world.collisionCells.end())candidates.borrowed=&cell->second;
        } else {
            candidates.owned=nativeCollisionInstancesForSegment(world,target,eye,x0,x1,y0,y1);
        }
    }
    const auto& selected=candidates.values();
    const size_t staticCount=indexed?selected.size():world.instances.size();
    const auto* scriptWorld=world.scriptEnvironment;
    const size_t scriptCount=includeScriptObjects&&scriptWorld?scriptWorld->objects.size():0;
    for(size_t query=0;query<staticCount+scriptCount;++query) {
        StorylandNativeCollisionInstance live;
        if(query>=staticCount) {
            const auto& object=scriptWorld->objects[query-staticCount];
            if((skipVisibleScriptObjects&&object.damage.visible)||
                !storylandScriptKinematicCollision(*scriptWorld,object,live))continue;
        }
        const auto& instance=query>=staticCount?live:world.instances[indexed?selected[query]:query];
        if(query<staticCount&&scriptWorld&&scriptWorld->lcsExtendedWorld.deletedNativeObjects.count(
            (uint64_t(instance.poolHeader)<<32)|instance.poolIndex))continue;
        if((sectorLists!=0xFFFF&&!(instance.nativeSectorLists&sectorLists))||
            (requireNativeCollision&&!instance.nativeUsesCollision))continue;
        const bool dummy = instance.poolHeader == 0x2Cu;
        if (dummy ? !dummies : !buildings) {
            continue;
        }
        const auto found=world.collisionModels.find(instance.modelId);
        if(found==world.collisionModels.end()) continue;
        const auto& model=found->second;
        if(!world.collisionModelEnabled(model))continue;
        const auto& matrix=instance.matrix;
        const Point center=mul(add(model.minimum,model.maximum),0.5f);
        const Point extent=mul(sub(model.maximum,model.minimum),0.5f);
        const Point worldCenter{
            matrix[0]*center.x+matrix[4]*center.y+matrix[8]*center.z+matrix[12],
            matrix[1]*center.x+matrix[5]*center.y+matrix[9]*center.z+matrix[13],
            matrix[2]*center.x+matrix[6]*center.y+matrix[10]*center.z+matrix[14]};
        const Point worldExtent{
            std::fabs(matrix[0])*extent.x+std::fabs(matrix[4])*extent.y+std::fabs(matrix[8])*extent.z+0.15f,
            std::fabs(matrix[1])*extent.x+std::fabs(matrix[5])*extent.y+std::fabs(matrix[9])*extent.z+0.15f,
            std::fabs(matrix[2])*extent.x+std::fabs(matrix[6])*extent.y+std::fabs(matrix[10])*extent.z+0.15f};
        if(!finite(worldCenter)||!finite(worldExtent)||
            boxHit(target,sub(eye,target),sub(worldCenter,worldExtent),add(worldCenter,worldExtent))>=nearest) continue;
        for(size_t marginIndex=0;marginIndex<(lens?7u:1u);++marginIndex) {
            const auto& margin=margins[marginIndex];
            Point a,b;
            if(!localSegment(instance.matrix,add(target,margin),add(eye,margin),a,b)) continue;
            const Point direction=sub(b,a);
            if(boxHit(a,direction,model.minimum,model.maximum)>=nearest) continue;
            for(const auto& box:model.boxes) {
                const float hit=boxHit(a,direction,box.minimum,box.maximum);
                Point face{0,0,1};
                if(normal&&hit<nearest) {
                    const auto point=add(a,mul(direction,hit));
                    const float gaps[]={std::fabs(point.x-box.minimum.x),std::fabs(point.x-box.maximum.x),
                        std::fabs(point.y-box.minimum.y),std::fabs(point.y-box.maximum.y),
                        std::fabs(point.z-box.minimum.z),std::fabs(point.z-box.maximum.z)};
                    const Point faces[]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
                    face=faces[std::min_element(std::begin(gaps),std::end(gaps))-std::begin(gaps)];
                }
                record(hit,box.surface,face,matrix);
            }
            for(const auto& sphere:model.spheres) {
                const float hit=sphereHit(a,direction,sphere.center,sphere.radius);
                record(hit,sphere.surface,normal&&hit<nearest?sub(add(a,mul(direction,hit)),sphere.center):Point{0,0,1},matrix);
            }
            if(model.triangles.empty())continue;
            const Point segmentMinimum{std::min(a.x,b.x)-.001f,std::min(a.y,b.y)-.001f,std::min(a.z,b.z)-.001f};
            const Point segmentMaximum{std::max(a.x,b.x)+.001f,std::max(a.y,b.y)+.001f,std::max(a.z,b.z)+.001f};
            for(const auto& triangle:model.triangles) {
                const auto& v=triangle.vertices;
                if(v[0]>=model.vertices.size()||v[1]>=model.vertices.size()||v[2]>=model.vertices.size()) continue;
                if(triangleOutsideBounds(model.vertices[v[0]],model.vertices[v[1]],model.vertices[v[2]],
                    segmentMinimum,segmentMaximum))continue;
                const float hit=triangleHit(a,direction,model.vertices[v[0]],model.vertices[v[1]],model.vertices[v[2]]);
                record(hit,triangle.surface,normal&&hit<nearest?cross(sub(model.vertices[v[1]],model.vertices[v[0]]),
                    sub(model.vertices[v[2]],model.vertices[v[0]])):Point{0,0,1},matrix);
            }
        }
    }
    return nearest<1?std::max(0.0f,nearest-(lens?0.12f:0.0f)/distance):1;
}

float storylandStoriesCameraVisibility(const StorylandNativeWorld& world,const Point& target,const Point& eye) {
    return traceNativeSegment(world,target,eye,true);
}
float storylandTraceNativeSegment(const StorylandNativeWorld& world,const Point& start,const Point& end) {
    return traceNativeSegment(world,start,end,false);
}
float storylandTraceNativeSegmentWithoutVisibleScriptObjects(const StorylandNativeWorld& world,
    const Point& start,const Point& end) {
    return traceNativeSegment(world,start,end,false,nullptr,nullptr,true,true,0xFFFF,true,false,true);
}
float storylandTraceNativeSegmentCategories(const StorylandNativeWorld& world,
    const Point& start, const Point& end, bool buildings, bool dummies) {
    return traceNativeSegment(world, start, end, false, nullptr, nullptr, buildings, dummies);
}

float storylandTraceNativeSectorSegment(const StorylandNativeWorld& world,
    const Point& start,const Point& end,uint16_t sectorLists) {
    return traceNativeSegment(world,start,end,false,nullptr,nullptr,true,true,sectorLists,true,true);
}

bool storylandCameraSphereVisible(const StorylandStoriesCamera& camera,const Point& point,float radius,
    float aspect,float nearClip,float farClip,const Point& up) {
    if(!camera.initialized||!finite(point)||!finite(camera.eye)||!finite(camera.target)||
        !std::isfinite(radius)||radius<0||!std::isfinite(aspect)||aspect<=0||
        !std::isfinite(nearClip)||!std::isfinite(farClip)||farClip<=nearClip||
        !std::isfinite(camera.fieldOfView)||camera.fieldOfView<=0||camera.fieldOfView>=180)return false;
    const Point direction=sub(camera.target,camera.eye);
    const float distance=length(direction);
    if(distance<1e-6f)return false;
    const Point forward=mul(direction,1/distance);
    Point right=cross(forward,up);
    const float rightLength=length(right);
    if(rightLength<1e-6f)return false;
    right=mul(right,1/rightLength);
    const Point vertical=cross(right,forward),relative=sub(point,camera.eye);
    const float depth=dot(relative,forward);
    if(depth+radius<nearClip||depth-radius>farClip)return false;
    const float verticalTangent=std::tan(camera.fieldOfView*radians*.5f);
    const float horizontalTangent=verticalTangent*aspect;
    // Native 0x3816F8 tests the sphere against normalized frustum planes.
    // A center outside the image can still intersect the edge of the frustum.
    return std::fabs(dot(relative,right))-depth*horizontalTangent<=radius*std::sqrt(1+horizontalTangent*horizontalTangent)&&
        std::fabs(dot(relative,vertical))-depth*verticalTangent<=radius*std::sqrt(1+verticalTangent*verticalTangent);
}

bool storylandCameraScreenCoordinates(const StorylandStoriesCamera& camera,const Point& point,
    float aspect,std::array<float,2>& screen,const Point& up) {
    if(!camera.initialized||!finite(point)||!finite(camera.eye)||!finite(camera.target)||
        !finite(up)||!std::isfinite(aspect)||aspect<=0||!std::isfinite(camera.fieldOfView)||
        camera.fieldOfView<=0||camera.fieldOfView>=180)return false;
    const Point direction=sub(camera.target,camera.eye);
    const float distance=length(direction);
    if(distance<1e-6f)return false;
    const Point forward=mul(direction,1/distance);
    Point right=cross(forward,up);
    const float rightLength=length(right);
    if(rightLength<1e-6f)return false;
    right=mul(right,1/rightLength);
    const Point vertical=cross(right,forward),relative=sub(point,camera.eye);
    const float depth=dot(relative,forward);
    // Native2F9E70 writes both coordinates as zero when homogeneous W<=0.
    // 040C ignores the clipping result from39FC48, retaining offscreen points.
    if(depth<=0) {screen={0,0};return true;}
    const float tangent=std::tan(camera.fieldOfView*radians*.5f);
    screen={240.0f+240.0f*dot(relative,right)/(depth*tangent*aspect),
        136.0f-136.0f*dot(relative,vertical)/(depth*tangent)};
    return std::isfinite(screen[0])&&std::isfinite(screen[1]);
}

void storylandUpdateScriptCameraFocus(StorylandMissionEnvironment& world) {
    const auto& focus=world.cameraFocus;
    if(!world.camera.active||!focus.kind||world.camera.cutsceneOwned)return;
    if(focus.kind==3&&(focus.mode==3||focus.mode==14||focus.mode==16)) {
        const auto found=world.missionVehicles.find(focus.handle);
        if(found==world.missionVehicles.end()||found->second.deleted||
            (focus.mode==16&&world.remoteVehicle!=focus.handle)) {
            if(focus.mode==16) {
                world.camera.active=false;world.camera.restoreRequested=true;
                world.camera.transitioning=false;world.camera.transitionPending=false;
                world.camera.vehicleUpdated=false;
            }
            world.cameraFocus={};return;
        }
        std::string error;
        StorylandVcsVehicleCamera::update(world,error);
        return;
    }
    Point target{};
    if(focus.kind==1||(focus.kind==2&&focus.handle==0))target={world.x,world.y,world.z};
    else if(focus.kind==2) {
        const auto found=world.missionCharacters.find(focus.handle);
        if(found==world.missionCharacters.end()) {world.cameraFocus={};return;}
        target={found->second.x,found->second.y,found->second.z};
    } else if(focus.kind==3) {
        const auto found=world.missionVehicles.find(focus.handle);
        if(found==world.missionVehicles.end()||found->second.deleted) {world.cameraFocus={};return;}
        target={found->second.x,found->second.y,found->second.z};
    } else return;
    world.camera.targetX=target.x;world.camera.targetY=target.y;world.camera.targetZ=target.z;
    world.camera.targetSet=true;
}

std::array<float,3> storylandSampleCameraShake(StorylandMissionEnvironment& environment) {
    auto& shake=environment.cameraShake;
    if(shake.sampled&&shake.sampledAt==environment.gameMilliseconds)return shake.offset;
    shake.sampled=true;shake.sampledAt=environment.gameMilliseconds;
    const float elapsed=float(uint32_t(environment.gameMilliseconds-shake.startedAt));
    const float strength=std::clamp(shake.strength-elapsed*0.00028f,0.0f,2.0f);
    // SLUS 0x408058 is the same Park-Miller stream used by native SCM random
    // commands. The camera takes one sample and consumes three low nibbles.
    const uint32_t random=environment.nextNativeRandom();
    shake.randomState=int32_t(random); // Retained archive/debug mirror.
    const float scale=strength*0.1f;
    for(unsigned axis=0;axis<3;++axis)shake.offset[axis]=float(int((random>>(axis*4))&15)-7)*scale;
    return shake.offset;
}

bool storylandShakeCameraAt(StorylandMissionEnvironment& environment,const Point& position,float strength) {
    if(!finite(position)||!std::isfinite(strength)||strength<0||!environment.activeCameraVectors)return false;
    std::array<float,3> source{},target{};
    if(!environment.activeCameraVectors(source,target))return false;
    const Point lens{source[0],source[1],source[2]};if(!finite(lens))return false;
    // CCamera::CamShake (0x376A28) attenuates both the decayed comparison
    // and the new request, using the active lens rather than the player's root.
    const float distance=std::clamp(length(sub(lens,position)),0.0f,100.0f);
    const float factor=1.0f-distance*.01f;
    auto& shake=environment.cameraShake;
    const float elapsed=float(uint32_t(environment.gameMilliseconds-shake.startedAt));
    const float previous=std::clamp((shake.strength-elapsed*.001f)*factor,0.0f,2.0f);
    const float requested=strength*factor;
    if(previous<requested) {
        shake.strength=requested;shake.startedAt=environment.gameMilliseconds;shake.sampled=false;
    }
    return true;
}

void storylandApplyScriptViewEffects(StorylandStoriesCamera& camera,StorylandMissionEnvironment& world) {
    if(!camera.initialized)return;
    const auto focus=world.cameraLookAt;
    Point target{};
    bool known=false;
    if(focus.kind==1) {target={world.x,world.y,world.z+1};known=world.health>0;}
    else if(focus.kind==2) {
        const auto found=world.missionCharacters.find(focus.handle);
        if(found!=world.missionCharacters.end()&&found->second.alive) {
            target={found->second.x,found->second.y,found->second.z+1};known=true;
        }
    } else if(focus.kind==3) {
        const auto found=world.missionVehicles.find(focus.handle);
        if(found!=world.missionVehicles.end()&&!found->second.deleted) {
            target={found->second.x,found->second.y,found->second.z};known=true;
        }
    }
    if(known) {camera.target=target;camera.aimLocked=false;}
    else if(focus.kind)world.cameraLookAt={};
    // Script flags and timing follow the native camera. The host applies a
    // bounded view perturbation rather than emulating the PS2 blur framebuffer.
    const float time=float(world.gameMilliseconds%600000u)*.001f;
    const float sway=float(world.drunkCameraIntensity)/255.0f*.16f;
    float visual=0;
    if(world.drunkVisualIntensity&&world.drunkVisualPeriod)
        visual=std::sin(float(uint32_t(world.gameMilliseconds-world.drunkVisualStartedAt)%world.drunkVisualPeriod)*
            6.283185307f/float(world.drunkVisualPeriod))*std::min(.3f,world.drunkVisualAmplitude*.02f);
    const auto forward=sub(camera.target,camera.eye);
    const float horizontal=std::hypot(forward.x,forward.y);
    if(horizontal>.0001f) {
        const Point right{forward.y/horizontal,-forward.x/horizontal,0};
        const auto displacement=add(mul(right,std::sin(time*1.7f)*sway+visual),
            {0,0,std::sin(time*1.13f)*sway*.5f});
        camera.target=add(camera.target,displacement);
    }
    if(known) {
        const auto direction=sub(camera.target,camera.eye);
        camera.yaw=std::atan2(direction.y,direction.x)*57.295779513f;
        camera.pitch=std::atan2(direction.z,std::hypot(direction.x,direction.y))*57.295779513f;
    }
}

StorylandSpeedCamera storylandVehicleSpeedCamera(bool vcs,int vehicleType,float speed,
    bool eligible,uint32_t random) {
    // SLUS-21590 0x37DF48 and constants at 0x488EA8. The corresponding
    // SLUS-21423 camera hook at 0x291E88 is an empty function.
    StorylandSpeedCamera effects;
    if(!vcs||!eligible||!std::isfinite(speed)||speed<=32.5f)return effects;
    if(vehicleType!=0&&vehicleType!=2&&vehicleType!=6&&vehicleType!=8&&vehicleType!=9)return effects;
    const float nativeSpeed=speed/50.0f;
    effects.blur=nativeSpeed>=1.0f?75:uint8_t(75.0f*(nativeSpeed-.65f)/(1.0f-.65f));
    if(vehicleType!=6) {
        const float scale=(nativeSpeed-.65f)/(.35f*200.0f);
        effects.shake={float(int(random&15)-7)*scale,float(int((random>>4)&15)-7)*scale,
            float(int((random>>8)&15)-7)*scale};
    }
    return effects;
}

void storylandUpdateSpeedCamera(StorylandStoriesCamera& camera,StorylandMissionEnvironment& environment,
    bool vcs,int vehicleType,float speed,bool eligible) {
    const auto probe=storylandVehicleSpeedCamera(vcs,vehicleType,speed,eligible,0);
    camera.speedEffects=probe;
    if(probe.shake.x!=0)
        camera.speedEffects=storylandVehicleSpeedCamera(vcs,vehicleType,speed,eligible,environment.nextNativeRandom());
}

void storylandApplySpeedCamera(StorylandStoriesCamera& camera) {
    camera.eye=add(camera.eye,camera.speedEffects.shake);
    const auto targetShift=add(camera.speedEffects.shake,camera.speedEffects.aimShift);
    camera.target=add(camera.target,targetShift);
    if(camera.aimLocked)camera.aimTarget=add(camera.aimTarget,targetShift);
}

void storylandResetVehicleSpeedCamera(StorylandStoriesCamera& camera) {
    camera.speedEffects={};
    camera.speedMotionIntensity=0;
}

void storylandApplyCameraShake(StorylandStoriesCamera& camera,StorylandMissionEnvironment& environment) {
    if(!camera.initialized)return;
    const auto offset=storylandSampleCameraShake(environment);
    // Translation happens after aim/shot sampling. Move target with the lens to
    // retain the native camera basis, without perturbing the string-camera state.
    camera.eye.x+=offset[0];camera.eye.y+=offset[1];camera.eye.z+=offset[2];
    camera.target.x+=offset[0];camera.target.y+=offset[1];camera.target.z+=offset[2];
    if(camera.aimLocked) {
        camera.aimTarget.x+=offset[0];camera.aimTarget.y+=offset[1];camera.aimTarget.z+=offset[2];
    }
}

int32_t storylandWeaponManualAimMode(int32_t nativeWeapon) {
    switch (nativeWeapon) {
        case 30:
        case 31: return 7;
        case 32: return 8;
        case 38: return 46;
        case 39: return 47;
        default: return 0;
    }
}

bool storylandPlayerSeatedDriveByVehicle(const StorylandMissionEnvironment& environment,
    int32_t nativeWeapon,std::array<float,16>* matrix) {
    if((!environment.playerSeatedDriveByEnabled&&!environment.manualAim.seatedActive)||!environment.inVehicle||
        environment.playerPassengerSeat<0||environment.playerVehicleTransition||
        environment.activeVcsCharacterAttachment(0)||nativeWeapon<24||nativeWeapon>27)return false;
    const int32_t handle=environment.occupiedMissionVehicle;
    const auto found=environment.missionVehicles.find(handle);
    if(found==environment.missionVehicles.end()||found->second.deleted||
        found->second.driver==0||found->second.spawnedSlot<0)return false;
    if(environment.vehiclePassengerCapacity) {
        const int32_t capacity=environment.vehiclePassengerCapacity(handle);
        if(capacity<0||environment.playerPassengerSeat>=capacity)return false;
    }
    if(matrix&&(!environment.vehicleWorldMatrix||!environment.vehicleWorldMatrix(handle,*matrix)||
        !std::all_of(matrix->begin(),matrix->end(),[](float value){return std::isfinite(value);})))return false;
    return true;
}

Point storylandSeatedDriveByLocalTarget(const StorylandMissionEnvironment& environment,
    const std::array<float,16>& matrix,bool manualActive) {
    if((!environment.manualAim.seatedActive&&(!manualActive||!environment.manualAim.initialized))||
        environment.manualAim.resetRequested) {
        if(environment.manualAim.resetRequested)return {0,1,0};
        return {environment.playerPassengerSeat==1||environment.playerPassengerSeat==3?-1.0f:1.0f,0,0};
    }
    const auto& target=environment.manualAim.target;
    const Point delta{target[0]-matrix[12],target[1]-matrix[13],target[2]-matrix[14]};
    return {delta.x*matrix[0]+delta.y*matrix[1]+delta.z*matrix[2],
        delta.x*matrix[4]+delta.y*matrix[5]+delta.z*matrix[6],
        delta.x*matrix[8]+delta.y*matrix[9]+delta.z*matrix[10]};
}

bool storylandUpdateManualAimCamera(StorylandStoriesCamera& camera,StorylandMissionEnvironment& environment,
    const Point& player,float heading,const StorylandManualAimInput& input,const StorylandNativeWorld& world) {
    const bool wasActive = camera.manualAimActive;
    camera.manualAimActive=false;camera.manualAimMode=0;camera.manualAimHit=false;camera.fieldOfView=70;
    auto& aim=environment.manualAim;
    const bool attached=environment.activeVcsCharacterAttachment(0);
    if(aim.seatedActive&&(!world.paths.vcs||!environment.inVehicle||
        environment.playerPassengerSeat<0||environment.playerVehicleTransition||attached||
        input.nativeWeapon<24||input.nativeWeapon>27||environment.health<=0||environment.playerArrested)) {
        aim.seatedActive=false;aim.initialized=false;
    }
    if(aim.seatedActive) {
        const auto vehicle=environment.missionVehicles.find(environment.occupiedMissionVehicle);
        if(vehicle==environment.missionVehicles.end()||vehicle->second.deleted||vehicle->second.driver==0) {
            aim.seatedActive=false;aim.initialized=false;
        }
    }
    std::array<float,16> vehicleFrame{};
    const bool seated=world.paths.vcs&&storylandPlayerSeatedDriveByVehicle(environment,input.nativeWeapon,&vehicleFrame)&&
        std::isfinite(input.weaponRange)&&input.weaponRange>0;
    const auto initializeSeatedTarget=[&]() {
        const bool forward=aim.resetRequested;
        const bool left=environment.playerPassengerSeat==1||environment.playerPassengerSeat==3;
        const size_t column=forward?4:0;
        const float range=input.weaponRange*(forward||!left?1.0f:-1.0f);
        const Point target{vehicleFrame[12]+vehicleFrame[column]*range,
            vehicleFrame[13]+vehicleFrame[column+1]*range,vehicleFrame[14]+vehicleFrame[column+2]*range};
        const auto direction=sub(target,input.head);
        aim.target={target.x,target.y,target.z};
        aim.yaw=std::atan2(direction.y,direction.x)/radians;
        aim.pitch=std::atan2(direction.z,std::hypot(direction.x,direction.y))/radians;
    };
    if(seated&&!aim.seatedActive&&environment.health>0&&!environment.playerArrested&&
        input.headValid&&finite(input.head)) {
        initializeSeatedTarget();
        aim.seatedActive=true;aim.initialized=true;aim.fieldOfView=70;
    }
    const int mode=seated?11:attached?45:aim.forced?aim.mode:input.weaponMode?input.weaponMode:input.cameraAiming?46:aim.mode;
    if((!aim.forced&&!attached&&!seated&&!input.cameraAiming&&!input.weaponMode)||input.suspended||
        (environment.inVehicle&&!seated)||
        environment.health<=0||environment.playerArrested||environment.cutscene.playing||
        environment.camera.active||!finite(player)||!std::isfinite(heading))return false;
    if(mode!=7&&mode!=8&&mode!=11&&mode!=45&&mode!=46&&mode!=47)return false;
    if(!input.headValid||!finite(input.head))return false;
    if(!wasActive&&!seated&&input.weaponMode&&!aim.forced&&!attached)aim.initialized=false;
    bool resetApplied=false;
    if(!aim.initialized||aim.resetRequested) {
        if(seated) {
            if(aim.resetRequested)initializeSeatedTarget();
            resetApplied=aim.resetRequested;
        } else {
            aim.yaw=!aim.resetRequested&&input.look.freeAim&&camera.initialized?camera.yaw:heading;
            aim.pitch=!aim.resetRequested&&input.look.freeAim&&camera.initialized?-camera.pitch:0;
            resetApplied=aim.resetRequested;
        }
        aim.fieldOfView=70;aim.initialized=true;
    }
    const float step=environment.nativeScriptTimeStep;
    if(!std::isfinite(step)||step<0)return false;
    float pitchUp=85,pitchDown=85;
    if(attached) {
        const auto& limits=environment.playerAttachedPitchLimits;
        if(limits[0]!=0||limits[1]!=0) {
            pitchUp=limits[0]/radians;pitchDown=limits[1]/radians;
        } else {pitchUp=60;pitchDown=30.5f;}
    }
    if(input.controlEnabled) {
        const float sensitivity=0.8f*step*(aim.fieldOfView/70.0f);
        const float horizontalScale=(mode==11||mode==45)?environment.weaponAimPadParameters[0]:1.0f;
        aim.yaw=std::remainder(aim.yaw+float(int(input.look.right)-int(input.look.left))*sensitivity*horizontalScale,360.0f);
        aim.pitch+=float(int(input.look.up)-int(input.look.down))*sensitivity;
        if(std::isfinite(input.look.yawDelta))
            aim.yaw=std::remainder(aim.yaw+std::clamp(input.look.yawDelta,-360.0f,360.0f)*(aim.fieldOfView/70.0f),360.0f);
        if(std::isfinite(input.look.pitchDelta))
            aim.pitch-=std::clamp(input.look.pitchDelta,-180.0f,180.0f)*(aim.fieldOfView/70.0f);
        if(world.paths.vcs) {
            if(mode==7||mode==46||mode==47) {
                const float pressureStep=step*255.0f;
                const float denominator=pressureStep+10000.0f;
                if(input.zoomOut)aim.fieldOfView=(aim.fieldOfView*0.0001f)*denominator;
                else if(input.zoomIn)aim.fieldOfView/=denominator*0.0001f;
            }
        } else if(mode!=11&&mode!=45) {
            const float factor=1.0f+0.0255f*step;
            if(input.zoomIn&&!input.zoomOut)aim.fieldOfView/=factor;
            else if(input.zoomOut&&!input.zoomIn)aim.fieldOfView*=factor;
        }
    }
    // Native 0x298270 tests the upper bound first and then the negated lower
    // bound. Keeping that order also defines reversed script limits safely.
    if(aim.pitch>pitchUp)aim.pitch=pitchUp;
    else if(aim.pitch<-pitchDown)aim.pitch=-pitchDown;
    if(world.paths.vcs) {
        aim.fieldOfView=mode==7||mode==46||mode==47?
            std::clamp(aim.fieldOfView,mode==46?4.8f:15.0f,70.0f):70.0f;
    } else aim.fieldOfView=std::clamp(aim.fieldOfView,4.8f,70.0f);
    const float yaw=aim.yaw*radians,pitch=aim.pitch*radians;
    const Point forward{std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),std::sin(pitch)};
    Point eye=input.head;
    if(mode==11) {
        // Shoulder view follows the authored pose rather than accumulating the
        // previous camera displacement. Its head/torso anchors also follow crouch.
        const Point anchor=input.shoulderValid&&finite(input.shoulder)?input.shoulder:
            input.torsoValid&&finite(input.torso)?input.torso:input.head;
        const Point right{std::sin(yaw),-std::cos(yaw),0};
        eye=add(anchor,add(mul(forward,-0.65f),add(mul(right,input.shoulderValid?0.1f:0.35f),{0,0,0.22f})));
        const float clear=storylandStoriesCameraVisibility(world,input.head,eye);
        eye=add(input.head,mul(sub(eye,input.head),clear));
    }
    const Point rayEnd=add(eye,mul(forward,1000.0f));
    const float hit=storylandTraceNativeSegment(world,eye,rayEnd);
    const Point target=add(eye,mul(sub(rayEnd,eye),hit));
    if(!finite(eye)||!finite(target))return false;
    aim.eye={eye.x,eye.y,eye.z};aim.target={target.x,target.y,target.z};
    if(resetApplied)aim.resetRequested=false;
    if(seated)aim.mode=11;
    camera.initialized=true;camera.manualAimActive=true;camera.manualAimMode=mode;
    camera.manualAimHit=hit<1;camera.manualAimHitPoint=target;
    camera.eye=eye;camera.target=target;camera.yaw=aim.yaw;camera.pitch=-aim.pitch;
    camera.fieldOfView=aim.fieldOfView;camera.aimLocked=false;
    camera.freeAimActive=false;
    camera.vehicleCameraActive=false;camera.vehicleController.initialized=false;
    return true;
}

bool storylandUpdateFreeAimCamera(StorylandStoriesCamera& camera,
    const Point& player,const Point& anchor,float heading,
    const StorylandPlayTestCameraInput& input,float seconds,float range,
    const StorylandNativeWorld& world) {
    if(!finite(player)||!finite(anchor)||!std::isfinite(heading)||
        !std::isfinite(seconds)||seconds<=0||!std::isfinite(range)||range<=0)return false;
    if(!camera.initialized)storylandResetStoriesCamera(camera,player,heading);
    const float dt=std::min(seconds,.1f);
    const float mouseYaw=std::isfinite(input.yawDelta)?std::clamp(input.yawDelta,-360.0f,360.0f):0;
    const float mousePitch=std::isfinite(input.pitchDelta)?std::clamp(input.pitchDelta,-180.0f,180.0f):0;
    camera.yaw=std::remainder(camera.yaw-float(int(input.right)-int(input.left))*105.0f*dt+mouseYaw,360.0f);
    camera.pitch=std::clamp(camera.pitch-float(int(input.up)-int(input.down))*63.0f*dt+mousePitch,-85.0f,85.0f);
    const float yaw=camera.yaw*radians,pitch=-camera.pitch*radians;
    const Point forward{std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),std::sin(pitch)};
    const Point right{std::sin(yaw),-std::cos(yaw),0};
    const Point pivot=add(anchor,add(mul(right,.48f),{0,0,.18f}));
    const Point desired=sub(pivot,mul(forward,2.35f));
    const float clear=storylandStoriesCameraVisibility(world,anchor,desired);
    camera.eye=add(anchor,mul(sub(desired,anchor),clear));
    camera.stringEye=camera.eye;
    camera.target=add(camera.eye,mul(forward,std::clamp(range+3.0f,4.0f,2000.0f)));
    camera.distance=length(sub(camera.eye,pivot));
    camera.fieldOfView=55;
    camera.aimLocked=false;camera.nativeLockActive=false;camera.refocusing=false;
    camera.manualAimActive=false;camera.manualAimMode=0;camera.freeAimActive=true;
    camera.vehicleCameraActive=false;camera.vehicleController.initialized=false;
    return finite(camera.eye)&&finite(camera.target);
}

namespace {
struct VehicleCameraSettings {
    float targetHeight,distanceOffset,heightSubtract,minimumLength,historyDistance;
    float alphaRetention,maximumAlphaStep,unused,betaRetention,maximumAngleSpeed;
    float velocityTurn,maximumBetaStep,inputMultiplier,maximumAlpha,minimumAlpha;
};

// SLUS_215.90 0x487BE8; row order is the native order, including the jet ski row.
constexpr VehicleCameraSettings vehicleCameraSettings[]={
    {1.3f,1,.4f,10,15,.5f,1,1,.85f,.2f,.075f,.05f,.8f,.785398f,1.553343f},
    {1.3f,1,.4f,10,15,.5f,1,1,.85f,.2f,.075f,.05f,.8f,-.18f,.7f},
    {1.1f,1,.1f,10,11,.5f,1,1,.85f,.2f,.075f,.05f,.75f,.785398f,1.553343f},
    {1.1f,1,.2f,10,15,.05f,.05f,0,.9f,.05f,.01f,.05f,1,0,1.52f},
    {1.1f,3.5f,.2f,10,25,.5f,1,1,.75f,.1f,.005f,.2f,1,1.553343f,1.553343f},
    {.9f,1,.1f,10,15,.5f,1,0,.9f,.05f,.005f,.05f,1,.349066f,1.221731f},
    {.9f,1,.1f,10,15,.5f,1,0,.06f,.62f,.071f,.05f,1,.349066f,1.221731f},
    {1.1f,1,.2f,10,5,.5f,1,1,.75f,.1f,.005f,.2f,1,.785398f,1.553343f},
    {1.1f,1,.2f,10,5,.5f,1,1,.75f,.1f,.005f,.2f,1,.349066f,1.221731f}
};
constexpr float vehicleZoomOffsets[3][5]={
    {-1,-.2f,-3.2f,.05f,-2.41f},
    {2,2.2f,1.65f,2.9f,6.49f},
    {6,6,15.9f,15.9f,15}
};
constexpr float vehicleZoomAlpha[3][5]={
    {.12f,.08f,.15f,.08f,.08f},
    {.1f,.08f,.3f,.08f,.08f},
    {.065f,.05f,.15f,.06f,.08f}
};

Point unit(Point value,Point fallback={0,0,1}) {
    const float magnitude=length(value);
    return magnitude>1e-6f?mul(value,1/magnitude):fallback;
}
unsigned vehicleCameraAppearance(const StorylandVehicleDefinition& definition) {
    if(!definition.vcsModelLayout) {
        const int native=storylandLcsVehicleHandlingClass(definition.handlingFlags);
        return native>=1&&native<=5?unsigned(native):1;
    }
    const uint32_t flags=definition.modelFlags;
    return flags&0x01000000u?2:flags&0x02000000u?3:flags&0x04000000u?5:flags&0x08000000u?4:1;
}
unsigned vehicleCameraRow(const StorylandVehicleDefinition& definition) {
    const int32_t model=int32_t(definition.modelId);
    if(definition.vcsModelLayout) {
        if(model==-943||model==-984)return 7;
        if(model==-972||model==-971)return 8;
        if(definition.nativeType==6||model==230)return 2;
        if(definition.nativeType==1)return 5;
        if(model==194)return 1;
        if(definition.nativeType==2)return 6;
        return 0;
    }
    if(definition.motorcycle())return 2;
    if(definition.domain==StorylandVehicleDomain::Water)return 5;
    const unsigned appearance=vehicleCameraAppearance(definition);
    if(appearance==3)return 3;
    if(appearance==5)return 4;
    return 0;
}
Point vehicleFramePoint(const std::array<float,16>& frame,Point local) {
    return {frame[12]+frame[0]*local.x+frame[4]*local.y+frame[8]*local.z,
        frame[13]+frame[1]*local.x+frame[5]*local.y+frame[9]*local.z,
        frame[14]+frame[2]*local.x+frame[6]*local.y+frame[10]*local.z};
}
Point vehicleCameraDirection(float yaw,float pitch) {
    const float beta=yaw*radians,alpha=pitch*radians;
    return {std::cos(alpha)*std::cos(beta),std::cos(alpha)*std::sin(beta),-std::sin(alpha)};
}
void vehicleCameraFinish(StorylandStoriesCamera& camera,Point eye,Point target,Point up,float fov) {
    camera.eye=eye;camera.target=target;camera.vehicleUp=unit(up);
    const Point front=unit(sub(target,eye),{1,0,0});
    camera.yaw=std::atan2(front.y,front.x)/radians;
    camera.pitch=-std::asin(std::clamp(front.z,-1.0f,1.0f))/radians;
    // Keep the native horizontal lens until the host builds its projection.
    // Converting here also applied the PS2 widescreen crop a second time.
    camera.fieldOfView=fov;
    camera.vehicleCameraActive=true;camera.initialized=true;
    camera.aimLocked=false;camera.nativeLockActive=false;camera.refocusing=false;
}

bool vehicleCinematicCamera(StorylandStoriesCamera& camera,const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,const StorylandVehicleCameraInput& input,
    const std::array<float,16>& frame,float seconds,const StorylandNativeWorld& world) {
    auto& state=camera.vehicleController;
    if(input.lookBehind||input.lookLeft||input.lookRight||input.look.refocus||
        input.look.up||input.look.down||input.look.left||input.look.right||
        (std::isfinite(input.look.yawDelta)&&input.look.yawDelta!=0)||
        (std::isfinite(input.look.pitchDelta)&&input.look.pitchDelta!=0)) {
        state.cinematicMode=-1;return false;
    }
    const Point forward=unit({frame[4],frame[5],frame[6]},{1,0,0});
    const Point right=unit({frame[0],frame[1],frame[2]},{0,-1,0});
    const auto& definition=vehicle.nativeDefinition;
    const Point target=add(actor.position,{0,0,.6f});
    const bool water=definition.domain==StorylandVehicleDomain::Water;
    const bool air=definition.domain==StorylandVehicleDomain::Air;
    const Point travel=unit({forward.x*vehicle.speed+right.x*vehicle.lateralSpeed,
        forward.y*vehicle.speed+right.y*vehicle.lateralSpeed,0},{1,0,0});
    const Point travelRight{travel.y,-travel.x,0};
    const float distance=length(sub(state.cinematicEye,target));
    state.cinematicSeconds+=seconds;
    bool expired=state.cinematicMode<0;
    if(state.cinematicMode==0)expired=state.cinematicSeconds>5;
    if(state.cinematicMode==6)expired=state.cinematicSeconds>3;
    if(state.cinematicMode==1||state.cinematicMode==2||state.cinematicMode==3||state.cinematicMode==5) {
        const float maximum=state.cinematicMode==1?40:state.cinematicMode==2?29:state.cinematicMode==3?48:38;
        const float minimum=state.cinematicMode==1?1.6f:state.cinematicMode==2?2:0;
        const Point velocity=add(mul(forward,vehicle.speed),mul(right,vehicle.lateralSpeed));
        expired=distance<minimum||(distance>maximum&&dot(sub(actor.position,state.cinematicEye),velocity)>0)||
            storylandStoriesCameraVisibility(world,target,state.cinematicEye)<.99f;
    }
    if(expired) {
        // Failed native candidates advance through the retail sequence. Native spline
        // flybys and cop-target shots require their own controllers; they may fail here.
        constexpr int carModes[]={0,21,7,3,20,1,21,8,2,20,5,2,22,1};
        constexpr int boatModes[]={5,20,5,7,18,5,3,20,5,8};
        constexpr int aircraftModes[]={26,27,23,20,22,28,23};
        const int* modes=water?boatModes:air?aircraftModes:carModes;
        const unsigned count=water?10:air?7:14;
        bool selected=false;
        for(unsigned attempt=0;attempt<count;++attempt) {
            const int mode=modes[state.cinematicIndex++%count];
            if(mode==0&&!water&&!air&&definition.modelId!=246&&definition.collision) {
                state.cinematicMode=0;selected=true;break;
            }
            if(mode!=1&&mode!=2&&mode!=3&&mode!=5)continue;
            const float ahead=mode==1?20:mode==2?16:30;
            const float side=mode==1?3:mode==2?2.5f:mode==3?8:6;
            Point eye=add(actor.position,add(mul(travel,ahead),mul(travelRight,side)));
            if(mode!=3) {
                Point start=add(eye,{0,0,5}),end=sub(eye,{0,0,100});
                float ground=storylandTraceNativeMapGround(world,start,end);
                bool found=ground<1;
                if(!found) {
                    start=sub(eye,{0,0,5});end=add(eye,{0,0,100});
                    ground=storylandTraceNativeMapGround(world,start,end);found=ground<1;
                }
                if(found)eye.z=start.z+(end.z-start.z)*ground+(mode==1?1.5f:mode==2?.5f:3.5f);
            }
            if(storylandStoriesCameraVisibility(world,target,eye)<.99f)continue;
            state.cinematicEye=eye;state.cinematicMode=mode;selected=true;break;
        }
        if(!selected)state.cinematicMode=6;
        state.cinematicSeconds=0;
    }
    if(state.cinematicMode==6)return false;
    Point eye=state.cinematicEye,up{0,0,1};
    if(state.cinematicMode==0) {
        const float minimumX=definition.collision->minimum.x;
        Point local{minimumX-.33f,-2.3f,.3f};
        Point wheelForward=forward,wheelRight=right;
        eye=vehicleFramePoint(frame,local);
        if(definition.motorcycle()) {
            wheelForward=unit(cross({0,0,1},right),forward);
            up=unit(cross(right,wheelForward));
            eye=add({frame[12],frame[13],frame[14]},add(mul(right,minimumX-.2f),
                add(mul(wheelForward,-2.3f),mul(up,.3f))));
        } else {
            wheelRight=unit(cross(forward,{0,0,1}),right);
            up=unit(cross(wheelRight,forward));
        }
        const uint32_t milliseconds=world.scriptEnvironment?world.scriptEnvironment->gameMilliseconds:
            uint32_t(state.cinematicSeconds*1000.0f);
        const float angle=.4f*std::cos(float(milliseconds&0x1FFFFu)*3.14159265359f/65536.0f);
        up=add(mul(up,std::cos(angle)),mul(wheelRight,std::sin(angle)));
        vehicleCameraFinish(camera,eye,add(eye,wheelForward),up,70);
    } else vehicleCameraFinish(camera,eye,target,up,70);
    camera.distance=length(sub(camera.eye,camera.target));
    camera.stringEye=camera.eye;camera.vehicleNearClip=.05f;
    return true;
}
}

int32_t storylandCycleVehicleCameraMode(int32_t current,int direction,int32_t model) {
    if(current<0||current>5||current==4)current=2;
    if(direction==0)return current;
    const int step=direction<0?-1:1;
    current+=step;
    if(current==4)current+=step;
    // Native GetArrPosForVehicleType excludes this model's close zoom on zoom-in.
    if(step<0&&current==1&&model==173)--current;
    if(current<0)current=5;
    if(current>5)current=0;
    return current;
}

bool storylandUpdateVehicleCamera(StorylandStoriesCamera& camera,
    const StorylandRuntimeVehicle& vehicle,const StorylandNativeTrafficActor& actor,
    const StorylandVehicleCameraInput& input,float seconds,const StorylandNativeWorld& world) {
    if(!finite(actor.position)||!std::isfinite(actor.yaw)||!std::isfinite(seconds)||seconds<=0)return false;
    camera.nativeFollowPedActive=false;
    const auto& definition=vehicle.nativeDefinition;
    auto& state=camera.vehicleController;
    const int mode=storylandCycleVehicleCameraMode(camera.vehicleMode,0);
    const bool reset=!camera.vehicleCameraActive||!state.initialized||
        state.model!=int32_t(definition.modelId)||
        state.handle!=vehicle.scriptHandle||state.vehicleSlot!=input.vehicleSlot||
        length(sub(state.lastPosition,actor.position))>50||
        (state.lastMode==0&&mode!=0);
    const float yawDelta=std::isfinite(input.look.yawDelta)?
        std::clamp(input.look.yawDelta,-360.0f,360.0f):0.0f;
    const float pitchDelta=std::isfinite(input.look.pitchDelta)?
        std::clamp(input.look.pitchDelta,-180.0f,180.0f):0.0f;
    const float dt=std::min(seconds,.1f);
    const auto mesh=storylandVehicleMeshFrame(definition,{actor.position.x,actor.position.y,actor.position.z},
        actor.yaw,definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
        vehicle.bodyPitch+vehicle.roadPitch,actor.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr);
    const auto frame=storylandVehicleMatrixProduct(mesh,definition.previewRoot);
    const Point forward=unit({frame[4],frame[5],frame[6]},{1,0,0});
    const Point right=unit({frame[0],frame[1],frame[2]},{0,-1,0});
    const Point up=unit({frame[8],frame[9],frame[10]});
    if(reset) {
        state={};state.initialized=true;state.model=int32_t(definition.modelId);state.handle=vehicle.scriptHandle;
        state.vehicleSlot=input.vehicleSlot;
        state.lastPosition=actor.position;state.lastHeading=actor.yaw;
        storylandResetVehicleSpeedCamera(camera);
    }
    if(state.lastMode!=mode&&mode==5) {
        state.cinematicIndex=0;state.cinematicMode=-1;state.cinematicSeconds=0;
    }
    const bool changedMode=state.lastMode!=mode;
    state.lastMode=mode;camera.vehicleMode=mode;
    camera.vehicleView=unsigned(std::clamp(mode,1,3)-1);
    camera.vehicleNearClip=.9f;
    const bool lookingAside=input.lookBehind||input.lookLeft||input.lookRight;
    if(mode==0) {
        if(changedMode) {state.betaSpeed=state.alphaSpeed=state.zoom=0;}
        const auto seat=definition.seatPosition(0);
        const bool automobile=definition.nativeType==0;
        Point local{automobile?0:seat[0],seat[1]+.08f,seat[2]+.62f};
        Point eye=vehicleFramePoint(frame,local),viewForward=forward,viewUp=up,viewRight=right;
        StorylandVcsRemoteCameraFrame nativeFrame;
        if(definition.vcsModelLayout&&automobile&&
            storylandVcsAutomobileRemoteCamera(frame,seat,changedMode,state.zoom,nativeFrame)) {
            eye={nativeFrame.eye[0],nativeFrame.eye[1],nativeFrame.eye[2]};
            viewForward={nativeFrame.forward[0],nativeFrame.forward[1],nativeFrame.forward[2]};
            viewUp={nativeFrame.up[0],nativeFrame.up[1],nativeFrame.up[2]};
            viewRight=unit(cross(viewForward,viewUp),right);
        } else {
            if(up.z<=-.9f)state.zoom=std::min(.5f,state.zoom+1.5f*dt);
            else state.zoom=0;
            eye.z+=state.zoom;
        }
        Point direction=viewForward;
        if(input.look.refocus)state.betaSpeed=state.alphaSpeed=0;
        state.betaSpeed=std::clamp(state.betaSpeed+yawDelta+
            (float(input.look.left)-float(input.look.right))*100*dt,-90.0f,90.0f);
        state.alphaSpeed=std::clamp(state.alphaSpeed+pitchDelta+
            (float(input.look.down)-float(input.look.up))*70*dt,-60.0f,60.0f);
        if(input.lookBehind)direction=mul(viewForward,-1);
        else if(input.lookLeft)direction=mul(viewRight,-1);
        else if(input.lookRight)direction=viewRight;
        if(!lookingAside) {
            const float yaw=state.betaSpeed*radians;
            const float pitch=state.alphaSpeed*radians;
            direction=unit(add(add(mul(direction,std::cos(yaw)*std::cos(pitch)),
                mul(viewRight,-std::sin(yaw)*std::cos(pitch))),mul(viewUp,-std::sin(pitch))),viewForward);
        }
        vehicleCameraFinish(camera,eye,add(eye,direction),viewUp,60);
        camera.distance=0;camera.stringEye=eye;camera.vehicleNearClip=.05f;
        state.lastPosition=actor.position;state.lastHeading=actor.yaw;return true;
    }
    const unsigned row=vehicleCameraRow(definition);
    const auto& settings=vehicleCameraSettings[row];
    const unsigned appearance=vehicleCameraAppearance(definition);
    constexpr unsigned zoomColumn[]={0,0,1,2,4,3};
    const unsigned column=zoomColumn[std::clamp(appearance,1u,5u)];
    const unsigned zoomMode=mode>=1&&mode<=3?unsigned(mode-1):1;
    const float wantedZoom=input.scriptedZoom&&std::isfinite(input.zoom)?input.zoom:vehicleZoomOffsets[zoomMode][column];
    if(reset)state.zoom=wantedZoom;
    state.zoom+=std::clamp(wantedZoom-state.zoom,-6*dt,6*dt);
    const float minimumY=definition.collision?definition.collision->minimum.y:-vehicle.halfLength;
    const float maximumZ=definition.collision?definition.collision->maximum.z:vehicle.bodyHeight*.5f;
    const float approximateLength=2*std::fabs(minimumY);
    float requested=std::max(.03f,state.zoom+settings.distanceOffset+approximateLength);
    float alphaOffset=vehicleZoomAlpha[zoomMode][column];
    Point target=actor.position;
    if(appearance==3)target=add(target,mul(up,.6f*maximumZ));
    else {
        const float radius=std::max(0.0f,maximumZ*settings.targetHeight-settings.heightSubtract);
        target.z+=radius;requested+=radius;alphaOffset+=.3f*radius/requested;
    }
    const float historyDistance=std::max(requested,settings.historyDistance*((mode==1&&row<=2)?.65f:1));
    if(reset) {
        camera.yaw=actor.yaw;camera.pitch=alphaOffset/radians;
        state.historyFar=sub(target,mul(vehicleCameraDirection(camera.yaw,0),historyDistance));
        state.historyNear=sub(target,mul(vehicleCameraDirection(camera.yaw,0),requested));
        state.fieldOfView=70;camera.distance=requested;
    }
    if(definition.vcsModelLayout) {
        const float yaw=actor.yaw*radians;
        const Point velocity{std::cos(yaw)*vehicle.speed-std::sin(yaw)*vehicle.lateralSpeed,
            std::sin(yaw)*vehicle.speed+std::cos(yaw)*vehicle.lateralSpeed,vehicle.verticalSpeed};
        const float nativeForwardVelocity=dot(velocity,forward)/50.0f;
        state.fieldOfView=storylandVcsFollowVehicleFov(state.fieldOfView,
            definition.nativeType,nativeForwardVelocity,dt*50.0f);
    } else {
        const float planarSpeed=std::hypot(vehicle.speed,vehicle.lateralSpeed);
        const float excess=(appearance==1||appearance==2)?std::max(0.0f,planarSpeed/50-.4f):0;
        const float fovRetention=std::pow(.98f,dt*50);
        const float steadyExcess=49*excess;
        state.fieldOfView=std::clamp(70+steadyExcess+(state.fieldOfView-70-steadyExcess)*fovRetention,70.0f,100.0f);
    }
    const bool manual=input.look.up||input.look.down||input.look.left||input.look.right||
        yawDelta!=0||pitchDelta!=0;
    if(manual)state.manualSeconds=1;
    else state.manualSeconds=std::max(0.0f,state.manualSeconds-dt);
    if(input.look.refocus)state.manualSeconds=0;
    // Integrate the native angular filters in bounded steps. Collision is resolved
    // once at the final render position, independent of the physics tick rate.
    const unsigned steps=std::max(1u,unsigned(std::ceil(dt*120-1e-4f)));
    const float step=dt/steps,nativeStep=step*50;
    for(unsigned index=0;index<steps;++index) {
        const float fraction=float(index+1)/steps;
        Point pivot=add(state.lastPosition,mul(sub(actor.position,state.lastPosition),fraction));
        pivot=add(pivot,sub(target,actor.position));
        Point front=unit(sub(pivot,state.historyFar),forward);
        const float draggedYaw=std::atan2(front.y,front.x)/radians;
        float targetYaw=draggedYaw;
        const Point velocity=add(mul(forward,vehicle.speed),mul(right,vehicle.lateralSpeed));
        if(length(velocity)>1) {
            const float velocityYaw=std::atan2(velocity.y,velocity.x)/radians;
            const Point transverse=sub(velocity,mul(front,dot(velocity,front)));
            const float turn=std::min(1.0f,nativeStep*settings.velocityTurn*length(transverse)/50)*
                std::remainder(velocityYaw-draggedYaw,360.0f);
            targetYaw+=std::clamp(turn,-nativeStep*settings.maximumBetaStep/radians,nativeStep*settings.maximumBetaStep/radians);
        }
        const float desiredSpeed=std::clamp(std::remainder(targetYaw-camera.yaw,360.0f)*radians,
            -settings.maximumAngleSpeed,settings.maximumAngleSpeed);
        const float retention=std::pow(settings.betaRetention,nativeStep);
        state.betaSpeed=retention*state.betaSpeed+(1-retention)*desiredSpeed;
        if(state.manualSeconds==0&&!lookingAside)camera.yaw+=nativeStep*state.betaSpeed/radians;
        float targetPitch=-std::asin(std::clamp(front.z,-1.0f,1.0f))+alphaOffset;
        targetPitch=std::clamp(targetPitch,-settings.maximumAlpha,settings.minimumAlpha);
        if(state.manualSeconds==0&&!lookingAside) {
            const float amount=(1-std::pow(settings.alphaRetention,nativeStep))*(targetPitch-camera.pitch*radians);
            camera.pitch+=std::clamp(amount,-settings.maximumAlphaStep*nativeStep,settings.maximumAlphaStep*nativeStep)/radians;
        }
        const float historyPitch=camera.pitch-alphaOffset/radians;
        state.historyFar=sub(pivot,mul(vehicleCameraDirection(camera.yaw,historyPitch),historyDistance));
        state.historyNear=sub(pivot,mul(vehicleCameraDirection(camera.yaw,historyPitch),requested));
    }
    if(manual) {
        camera.yaw+=yawDelta+(float(input.look.left)-float(input.look.right))*100*dt;
        camera.pitch+=pitchDelta+(float(input.look.down)-float(input.look.up))*70*dt;
        camera.pitch=std::clamp(camera.pitch,-settings.maximumAlpha/radians,settings.minimumAlpha/radians);
        state.historyFar=sub(target,mul(vehicleCameraDirection(camera.yaw,camera.pitch-alphaOffset/radians),historyDistance));
    }
    if(input.look.refocus) {
        camera.yaw=actor.yaw;camera.pitch=alphaOffset/radians;state.betaSpeed=state.alphaSpeed=0;
        state.historyFar=sub(target,mul(vehicleCameraDirection(camera.yaw,0),historyDistance));
    }
    float yaw=camera.yaw,pitch=camera.pitch;
    if(input.lookBehind) {yaw=actor.yaw+180;pitch=alphaOffset/radians;}
    else if(input.lookLeft) {yaw=actor.yaw+90;pitch=alphaOffset/radians;}
    else if(input.lookRight) {yaw=actor.yaw-90;pitch=alphaOffset/radians;}
    camera.requestedDistance=requested;
    camera.stringEye=sub(target,mul(vehicleCameraDirection(yaw,pitch),requested));
    const float visible=storylandStoriesCameraVisibility(world,target,camera.stringEye);
    const float safeDistance=std::max(.03f,requested*visible);
    camera.distance=safeDistance<camera.distance?safeDistance:
        safeDistance+(camera.distance-safeDistance)*std::exp(-4*dt);
    const Point eye=sub(target,mul(vehicleCameraDirection(yaw,pitch),camera.distance));
    const float followYaw=camera.yaw,followPitch=camera.pitch;
    vehicleCameraFinish(camera,eye,target,{0,0,1},state.fieldOfView);
    if(lookingAside) {camera.yaw=followYaw;camera.pitch=followPitch;}
    if(visible<1)camera.vehicleNearClip=std::clamp(camera.distance-.3f,.05f,.9f);
    if(mode==5) {
        const float followBeta=camera.yaw,followAlpha=camera.pitch;
        vehicleCinematicCamera(camera,vehicle,actor,input,frame,dt,world);
        camera.yaw=followBeta;camera.pitch=followAlpha;
    }
    state.lastPosition=actor.position;state.lastHeading=actor.yaw;
    return finite(camera.eye)&&finite(camera.target);
}

void storylandUpdateStoriesSpeedCamera(StorylandStoriesCamera& camera,
    StorylandMissionEnvironment& environment,const StorylandRuntimeVehicle& vehicle,
    const StorylandNativeTrafficActor& actor,float seconds,bool eligible) {
    const auto& definition=vehicle.nativeDefinition;
    const bool motorcycle=definition.motorcycle();
    const bool automobile=definition.nativeType==0||definition.nativeType==9;
    if(!eligible||!camera.vehicleCameraActive||!definition.vcsModelLayout||
        !finite(actor.position)||!std::isfinite(actor.yaw)) {
        storylandResetVehicleSpeedCamera(camera);
        return;
    }
    if(!std::isfinite(seconds)||seconds<=0)return;
    if(!definition.hasNativeHandling||definition.domain!=StorylandVehicleDomain::Road||
        (!motorcycle&&!automobile)) {
        camera.speedMotionIntensity=0;
        storylandUpdateSpeedCamera(camera,environment,true,definition.nativeType,
            std::hypot(vehicle.speed,vehicle.lateralSpeed,vehicle.verticalSpeed),
            camera.vehicleMode>=2&&camera.vehicleMode<=3);
        return;
    }
    const auto mesh=storylandVehicleMeshFrame(definition,{actor.position.x,actor.position.y,actor.position.z},
        actor.yaw,definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
        vehicle.bodyPitch+vehicle.roadPitch,actor.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr);
    const auto frame=storylandVehicleMatrixProduct(mesh,definition.previewRoot);
    const Point forward=unit({frame[4],frame[5],frame[6]},{1,0,0});
    const Point right=unit({frame[0],frame[1],frame[2]},{0,-1,0});
    const Point up=unit({frame[8],frame[9],frame[10]});
    const float yaw=actor.yaw*radians;
    const Point velocity{std::cos(yaw)*vehicle.speed-std::sin(yaw)*vehicle.lateralSpeed,
        std::sin(yaw)*vehicle.speed+std::cos(yaw)*vehicle.lateralSpeed,vehicle.verticalSpeed};
    const unsigned topGear=std::clamp<unsigned>(definition.numberOfGears,1,5);
    const float wanted=StorylandStoriesSpeedMotion::target(dot(velocity,forward)/50.0f,
        definition.maximumAiVelocity,definition.gears[topGear].downshift,motorcycle);
    camera.speedMotionIntensity=StorylandStoriesSpeedMotion::advance(camera.speedMotionIntensity,wanted,seconds);
    const auto motion=StorylandStoriesSpeedMotion::sample(camera.speedMotionIntensity,motorcycle,
        environment.gameMilliseconds);
    camera.speedEffects={};
    camera.speedEffects.blur=motion.blur;
    camera.speedEffects.shake=add(mul(right,motion.lateral),mul(up,motion.vertical));
    const auto view=sub(camera.target,camera.eye);
    const float distance=length(view);
    if(distance>1e-6f) {
        const auto direction=mul(view,1.0f/distance);
        const auto wobble=add(mul(right,motion.aimWobble),mul(up,motion.vertical*.12f));
        camera.speedEffects.aimShift=mul(sub(unit(add(direction,wobble),direction),direction),distance);
    }
}

void storylandResetStoriesCamera(StorylandStoriesCamera& camera, const Point& player, float heading) {
    camera={};
    if(!finite(player)||!std::isfinite(heading)) return;
    camera.initialized=true;
    camera.yaw=std::remainder(heading,360.0f);
    camera.target=add(player,{0,0,1.0f});
    camera.eye=camera.stringEye=desiredEye(camera);
}

bool storylandResetStoriesCameraForTransition(StorylandStoriesCamera& camera,
    const Point& player, float heading, bool preserveAim) {
    if(preserveAim) {
        // Aiming has already produced this frame's pose; do not discard it.
        camera.nativeFollowPedActive=false;
        return false;
    }
    const float distance=camera.requestedDistance;
    const unsigned view=camera.vehicleView;
    const int32_t mode=camera.vehicleMode;
    storylandResetStoriesCamera(camera,player,heading);
    camera.requestedDistance=distance;
    camera.vehicleView=view;
    camera.vehicleMode=mode;
    return true;
}

void storylandUpdateStoriesCamera(StorylandStoriesCamera& camera, const Point& player,
    float heading, const StorylandPlayTestCameraInput& input, float seconds,
    const StorylandNativeWorld& world, bool collisionEnabled, bool nativeFollowPed) {
    if(!finite(player)||!std::isfinite(heading)||!std::isfinite(seconds)||seconds<=0) return;
    camera.vehicleCameraActive=false;
    camera.vehicleController.initialized=false;
    const auto resetKeepingInputs=[&]() {
        const auto inputs=camera;
        storylandResetStoriesCamera(camera,player,heading);
        camera.requestedDistance=inputs.requestedDistance;camera.vehicleView=inputs.vehicleView;
        camera.vehicleMode=inputs.vehicleMode;
        camera.aimLocked=inputs.aimLocked;camera.aimTarget=inputs.aimTarget;
        camera.aimPlayerRoot=inputs.aimPlayerRoot;camera.aimPlayerRootValid=inputs.aimPlayerRootValid;
        camera.aimTargetRoot=inputs.aimTargetRoot;camera.aimTargetRootValid=inputs.aimTargetRootValid;
        camera.aimNativeWeapon=inputs.aimNativeWeapon;camera.aimFireType=inputs.aimFireType;
        camera.aspectRatio=inputs.aspectRatio;
    };
    if(!camera.initialized)resetKeepingInputs();
    const float dt=std::min(seconds,0.1f);
    // VCS mode11/28 dispatches to SLUS_215.90 0x29C280. Its lock camera
    // orbits a shoulder pivot, looking along that camera's own forward vector.
    // Looking directly at the enemy erases the native framing and puts the
    // enemy behind the player's back. LCS keeps its separate existing camera.
    if(world.paths.vcs&&camera.aimLocked&&camera.aimFireType>=0&&camera.aimNativeWeapon>=0&&
        camera.aimPlayerRootValid&&finite(camera.aimPlayerRoot)&&finite(camera.aimTarget)&&
        (camera.aimFireType!=0||(camera.aimTargetRootValid&&finite(camera.aimTargetRoot)))&&
        std::isfinite(camera.aspectRatio)&&camera.aspectRatio>0) {
        const bool melee=camera.aimFireType==0;
        const bool entering=!camera.nativeLockActive;
        const float wantedFov=camera.aimNativeWeapon==28||camera.aimNativeWeapon==29?50.0f:70.0f;
        if(entering)camera.nativeLockFieldOfView=wantedFov;
        else camera.nativeLockFieldOfView+=std::clamp(wantedFov-camera.nativeLockFieldOfView,-50.0f*dt,50.0f*dt);
        const float nativeFov=camera.nativeLockFieldOfView;
        // 0x2962D8: entity origin +0.6Z and +0.55 along its right axis.
        // Rifle FOV changes add the exact authored vertical/lateral correction.
        const float side=.55f+.1f*std::min((70.0f-nativeFov)/35.0f,1.0f);
        const float elevation=.6f+(melee?.15f:0.0f)+.1f*std::min((70.0f-nativeFov)*.05f,1.0f);
        const float body=heading*radians;
        Point pivot=add(camera.aimPlayerRoot,{side*std::sin(body),-side*std::cos(body),elevation});
        if(collisionEnabled) {
            Point normal{};
            const float hit=traceNativeSegment(world,camera.aimPlayerRoot,pivot,false,nullptr,&normal,true,false);
            if(hit<1)pivot=add(add(camera.aimPlayerRoot,mul(sub(pivot,camera.aimPlayerRoot),hit)),mul(normal,.1f));
        }
        // Native pivot follow at 0x29C6DC blends by min(distance*0.2,1).
        if(!entering) {
            const float blend=std::min(length(sub(pivot,camera.target))*.2f,1.0f);
            pivot=add(camera.target,mul(sub(pivot,camera.target),blend));
        }
        Point target=camera.aimTarget;
        if(melee) {
            // 0x29C840 uses the target entity matrix, not its torso node.
            // 0x29C870 adds 0.75 of the pivot's elevation above the player root.
            target=camera.aimTargetRoot;
            target.z+=.75f*(pivot.z-camera.aimPlayerRoot.z);
        }
        const Point aim=sub(target,pivot);
        const float horizontal=std::hypot(aim.x,aim.y),targetDistance=length(aim);
        if(horizontal>1e-6f&&targetDistance>1e-6f) {
            const float tangent=std::tan(nativeFov*radians*.5f);
            // CCamera initializes the aiming screen coordinates to(.53,.47)
            // at0x3778C0. 0x29C514 converts those offsets to angular offsets;
            // 0x29CA90 compensates by min(previous distance,1)/target distance.
            const float compensation=1.0f+std::min(length(sub(pivot,camera.eye)),1.0f)/targetDistance;
            const float horizontalOffset=melee?20.0f*radians:std::atan(.06f*tangent)*compensation;
            const float verticalOffset=melee?3.0f*radians:std::atan(.06f*tangent/camera.aspectRatio)*compensation;
            float yaw=std::atan2(aim.y,aim.x)+horizontalOffset;
            // Melee uses the authored20/3degree offsets without the firearm
            // distance correction, and scales elevation by cos(20) at0x29CA7C.
            float alpha=std::atan2(aim.z,horizontal)*(melee?std::cos(horizontalOffset):1.0f)-verticalOffset;
            if(!entering) {
                const float maximum=5.0f*dt; // 0.1 rad per native timestep, 0x29CBD8.
                yaw=camera.yaw*radians+std::clamp(std::remainder(yaw-camera.yaw*radians,6.28318530718f),-maximum,maximum);
                alpha=-camera.pitch*radians+std::clamp(alpha+camera.pitch*radians,-maximum,maximum);
            }
            // Native on-foot rows at0x487AD8: firearm1+1.6*cos(alpha),
            // melee2.5+.7*cos(alpha); both clamp pitch to -10..45degrees.
            alpha=std::clamp(alpha,-10.0f*radians,45.0f*radians);
            const float distance=melee?2.5f+.7f*std::cos(alpha):1.0f+1.6f*std::cos(alpha);
            const Point forward{std::cos(alpha)*std::cos(yaw),std::cos(alpha)*std::sin(yaw),std::sin(alpha)};
            camera.target=pivot;
            camera.stringEye=sub(pivot,mul(forward,distance));
            const float fraction=collisionEnabled?storylandStoriesCameraVisibility(world,pivot,camera.stringEye):1.0f;
            camera.distance=std::max(.03f,distance*fraction);
            camera.eye=sub(pivot,mul(forward,camera.distance));
            camera.yaw=std::remainder(yaw/radians,360.0f);camera.pitch=-alpha/radians;
            camera.fieldOfView=2.0f*std::atan(tangent/camera.aspectRatio)/radians;
            camera.refocusing=false;camera.nativeLockActive=true;camera.nativeFollowPedActive=false;
            return;
        }
    }
    if(camera.nativeLockActive)camera.fieldOfView=70;
    camera.nativeLockActive=false;
    const bool nativeFollow=nativeFollowPed&&world.paths.vcs&&camera.aimPlayerRootValid&&
        finite(camera.aimPlayerRoot)&&!camera.aimLocked;
    const Point target=nativeFollow?add(camera.aimPlayerRoot,{0,0,.6f}):add(player,{0,0,1.0f});
    if(length(sub(target,camera.target))>20) {
        resetKeepingInputs();
    }
    if(nativeFollow&&!camera.nativeFollowPedActive) {
        // Native mode 4 initializes its alpha to -0.15 radians (0x29E464).
        camera.pitch=.15f/radians;
        camera.distance=camera.requestedDistance;
    }
    camera.nativeFollowPedActive=nativeFollow;
    // Cam-on-a-string: walking drags the camera from its previous position. It does
    // not forcibly align to the ped every frame or orbit a fixed world pivot.
    const Point fromEye=sub(target,camera.stringEye);
    if(std::hypot(fromEye.x,fromEye.y)>0.01f)
        camera.yaw=std::atan2(fromEye.y,fromEye.x)/radians;
    camera.target.x=target.x; camera.target.y=target.y;
    if(nativeFollow)camera.target.z=target.z;
    else camera.target.z+=(target.z-camera.target.z)*(1-std::exp(-12*dt));
    const bool looking=input.up||input.down||input.left||input.right||input.yawDelta!=0||input.pitchDelta!=0;
    if(looking) camera.refocusing=false;
    else if(input.refocus) camera.refocusing=true;
    auto orbit=input;orbit.refocus=false;
    const auto manual=storylandMovePlayTestCamera(camera.yaw,camera.pitch,heading,orbit,dt);
    camera.yaw=manual.yaw; camera.pitch=manual.pitch;
    if(camera.refocusing) {
        const float delta=std::remainder(heading-camera.yaw,360.0f);
        const float blend=1-std::exp(-10*dt);
        camera.yaw+=delta*blend;
        camera.pitch+=(0-camera.pitch)*blend;
        if(std::fabs(delta)<0.05f&&std::fabs(camera.pitch)<0.05f) camera.refocusing=false;
    }
    if(!world.paths.vcs&&camera.aimLocked&&finite(camera.aimTarget)) {
        const Point aim=sub(camera.aimTarget,target);
        const float horizontal=std::hypot(aim.x,aim.y);
        if(horizontal>0.01f) {
            camera.yaw=std::atan2(aim.y,aim.x)/radians;
            camera.pitch=std::clamp(-std::atan2(aim.z,horizontal)/radians,-35.0f,45.0f);
            camera.refocusing=false;
        }
    }
    camera.yaw=std::remainder(camera.yaw,360.0f);
    camera.stringEye=desiredEye(camera);
    // 0x29EF54-0x29F024 projects one normalized direction from the
    // native pivot. The legacy camera's extra 0.75 height is not on that ray.
    if(nativeFollow)camera.stringEye.z-=.75f;
    const float safeDistance=camera.requestedDistance*(collisionEnabled?storylandStoriesCameraVisibility(world,camera.target,camera.stringEye):1.0f);
    // Obstruction shortens immediately; recovery eases out to avoid a camera pop.
    camera.distance=safeDistance<camera.distance?safeDistance:
        camera.distance+(safeDistance-camera.distance)*(1-std::exp(-4*dt));
    camera.distance=std::max(0.03f,camera.distance);
    camera.eye=add(camera.target,mul(sub(camera.stringEye,camera.target),camera.distance/std::max(0.03f,camera.requestedDistance)));
}

float storylandTraceNativeGround(const StorylandNativeWorld& world,const Point& start,const Point& end,uint8_t& surface,Point* normal) {
    surface=0;
    if(normal)*normal={0,0,1};
    return traceNativeSegment(world,start,end,false,&surface,normal);
}
float storylandTraceNativeVehicleGround(const StorylandNativeWorld& world,const Point& start,
    const Point& end,uint8_t& surface,Point* normal) {
    surface=0;
    if(normal)*normal={0,0,1};
    // Wheel lines use the same collision-enabled entities as the chassis.
    return traceNativeSegment(world,start,end,false,&surface,normal,true,true,0x3FFF,true,true);
}
float storylandTraceNativeMapGround(const StorylandNativeWorld& world,const Point& start,const Point& end) {
    // 196778 -> 192360: buildings 0/4/14/18, dummies 30/34; objects are off.
    return traceNativeSegment(world,start,end,false,nullptr,nullptr,true,true,0x3063,false,true);
}
float storylandTraceNativeTrafficSurface(const StorylandNativeWorld& world,const Point& start,
    const Point& end,uint8_t& surface,Point& normal) {
    surface=0;normal={0,0,1};
    // VCS GenerateOneRandomCar queries buildings and dummies, with objects off.
    return traceNativeSegment(world,start,end,false,&surface,&normal,true,true,0x3063,
        false,true,false,false);
}
float storylandTraceNativeBuildings(const StorylandNativeWorld& world,const Point& start,const Point& end) {
    // CCoronas::Update uses buildings only: VCS 27C140 -> 18FC88.
    return traceNativeSegment(world,start,end,false,nullptr,nullptr,true,false,0x63,false,true);
}

#include "src/storyland/storyland_native_body.inl"
