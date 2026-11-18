// Adapted from the user's reLCS v6.9.72 OpenSubdivBridge.cpp.
// Pixar Far CPU Loop refinement; RenderWare allocation is replaced by Storyland render buffers.
#include "src/graphics/storyland_subdivision.h"
#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/stencilTableFactory.h>
#include <opensubdiv/far/stencilTable.h>
#include <opensubdiv/sdc/crease.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <future>
#include <atomic>
#include <chrono>
using int32=int32_t; using uint32=uint32_t; using uint16=uint16_t; using uint8=uint8_t; using uint64=uint64_t;
using RwV3d=StorylandModelPoint; using RwTexCoords=StorylandModelTexcoord; using RwRGBA=StorylandModelPrelight;
struct RpTriangle { int32 v[3]; uint32 matId; };
namespace {
using namespace OpenSubdiv;
constexpr int32 Level=1,MaxSourceTriangles=6000,MaxOutputTriangles=28000;
constexpr float CreaseAngleDegrees=70.f,MinCurvedAngleDegrees=2.5f;
constexpr bool PreserveMaterialBoundaries=false;
enum Category { CATEGORY_WORLD,CATEGORY_VEHICLE,CATEGORY_WEAPON };
#ifdef STORYLAND_SUBDIV_TRACE
static void LogSkip(const char*,const char* reason){std::fprintf(stderr,"Subdivision: %s\n",reason);}
#else
static void LogSkip(const char*,const char*){}
#endif
static const int32 kMaxRwVertices = 65535;
static const float kNormalEpsilon = 1.0e-12f;
static const float kPi = 3.14159265358979323846f;
static const float kSourceNormalSeamCos = 0.9995f;

struct OSDVec3
{
    float x, y, z;

    void Clear(void) { x = y = z = 0.0f; }
    void AddWithWeight(OSDVec3 const &src, float weight)
    {
        x += src.x * weight;
        y += src.y * weight;
        z += src.z * weight;
    }
};

struct OSDVec2
{
    float x, y;

    void Clear(void) { x = y = 0.0f; }
    void AddWithWeight(OSDVec2 const &src, float weight)
    {
        x += src.x * weight;
        y += src.y * weight;
    }
};

struct OSDColor
{
    float r, g, b, a;

    void Clear(void) { r = g = b = a = 0.0f; }
    void AddWithWeight(OSDColor const &src, float weight)
    {
        r += src.r * weight;
        g += src.g * weight;
        b += src.b * weight;
        a += src.a * weight;
    }
};

struct EdgeKey
{
    int32 a, b;

    EdgeKey(void) : a(0), b(0) {}
    EdgeKey(int32 v0, int32 v1)
    {
        if(v0 < v1){
            a = v0;
            b = v1;
        }else{
            a = v1;
            b = v0;
        }
    }

    bool operator<(EdgeKey const &rhs) const
    {
        return a < rhs.a || (a == rhs.a && b < rhs.b);
    }
};

struct EdgeInfo
{
    int32 face[3];
    int32 count;

    EdgeInfo(void) : count(0)
    {
        face[0] = face[1] = face[2] = -1;
    }
};

struct PositionKey
{
    uint32 x, y, z;

    static uint32 Bits(float value)
    {
        uint32 bits = 0;
        memcpy(&bits, &value, sizeof(bits));
        if((bits & 0x7FFFFFFFu) == 0)
            bits = 0;
        return bits;
    }

    explicit PositionKey(RwV3d const &v) : x(Bits(v.x)), y(Bits(v.y)), z(Bits(v.z)) {}

    bool operator<(PositionKey const &rhs) const
    {
        if(x != rhs.x) return x < rhs.x;
        if(y != rhs.y) return y < rhs.y;
        return z < rhs.z;
    }
};

struct VertexKey
{
    int32 position;
    int32 normalGroup;
    int32 color;
    int32 uv[8];
    int32 numUV;

    VertexKey(void) : position(-1), normalGroup(-1), color(-1), numUV(0)
    {
        for(int32 i = 0; i < 8; i++)
            uv[i] = -1;
    }

    bool operator<(VertexKey const &rhs) const
    {
        if(position != rhs.position)
            return position < rhs.position;
        if(normalGroup != rhs.normalGroup)
            return normalGroup < rhs.normalGroup;
        if(color != rhs.color)
            return color < rhs.color;
        if(numUV != rhs.numUV)
            return numUV < rhs.numUV;
        for(int32 i = 0; i < numUV; i++){
            if(uv[i] != rhs.uv[i])
                return uv[i] < rhs.uv[i];
        }
        return false;
    }
};

struct OutputVertex
{
    int32 position;
    int32 normalGroup;
    int32 color;
    int32 uv[8];
};

struct DisjointSet
{
    std::vector<int32> parent;
    std::vector<uint8> rank;

    explicit DisjointSet(int32 count) : parent(count), rank(count, 0)
    {
        for(int32 i = 0; i < count; i++)
            parent[i] = i;
    }

    int32 Find(int32 x)
    {
        int32 root = x;
        while(parent[root] != root)
            root = parent[root];
        while(parent[x] != x){
            int32 next = parent[x];
            parent[x] = root;
            x = next;
        }
        return root;
    }

    void Union(int32 a, int32 b)
    {
        a = Find(a);
        b = Find(b);
        if(a == b)
            return;
        if(rank[a] < rank[b])
            std::swap(a, b);
        parent[b] = a;
        if(rank[a] == rank[b])
            rank[a]++;
    }
};

static inline float ClampFloat(float value, float minValue, float maxValue)
{
    return std::max(minValue, std::min(maxValue, value));
}

static inline uint8 FloatToByte(float value)
{
    value = ClampFloat(value, 0.0f, 1.0f);
    return (uint8)(value * 255.0f + 0.5f);
}

static inline OSDVec3 Sub(OSDVec3 const &a, OSDVec3 const &b)
{
    OSDVec3 out = { a.x - b.x, a.y - b.y, a.z - b.z };
    return out;
}

static inline OSDVec3 Cross(OSDVec3 const &a, OSDVec3 const &b)
{
    OSDVec3 out = {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
    return out;
}

static inline float Dot(OSDVec3 const &a, OSDVec3 const &b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static inline float LengthSq(OSDVec3 const &v)
{
    return Dot(v, v);
}

static inline OSDVec3 Normalize(OSDVec3 const &v)
{
    float lenSq = LengthSq(v);
    if(!(lenSq > kNormalEpsilon) || lenSq > FLT_MAX){
        OSDVec3 fallback = { 0.0f, 0.0f, 1.0f };
        return fallback;
    }
    float invLen = 1.0f / sqrtf(lenSq);
    OSDVec3 out = { v.x * invLen, v.y * invLen, v.z * invLen };
    return out;
}

static bool RefinePositions(Far::TopologyRefiner const &refiner,
                            std::vector<OSDVec3> const &base,
                            std::vector<OSDVec3> &result)
{
    Far::PrimvarRefiner primvar(refiner);
    std::vector<OSDVec3> src(base);
    std::vector<OSDVec3> dst;

    for(int32 level = 1; level <= Level; level++){
        dst.assign(refiner.GetLevel(level).GetNumVertices(), OSDVec3());
        primvar.Interpolate(level, src, dst);
        src.swap(dst);
    }
    result.swap(src);
    return !result.empty();
}

static bool RefineFaceVaryingColors(Far::TopologyRefiner const &refiner, int32 channel,
                                    std::vector<OSDColor> const &base,
                                    std::vector<OSDColor> &result)
{
    Far::PrimvarRefiner primvar(refiner);
    std::vector<OSDColor> src(base);
    std::vector<OSDColor> dst;

    for(int32 level = 1; level <= Level; level++){
        dst.assign(refiner.GetLevel(level).GetNumFVarValues(channel), OSDColor());
        primvar.InterpolateFaceVarying(level, src, dst, channel);
        src.swap(dst);
    }
    result.swap(src);
    return !result.empty();
}

static bool RefineUVs(Far::TopologyRefiner const &refiner, int32 channel,
                      std::vector<OSDVec2> const &base,
                      std::vector<OSDVec2> &result)
{
    Far::PrimvarRefiner primvar(refiner);
    std::vector<OSDVec2> src(base);
    std::vector<OSDVec2> dst;

    for(int32 level = 1; level <= Level; level++){
        dst.assign(refiner.GetLevel(level).GetNumFVarValues(channel), OSDVec2());
        primvar.InterpolateFaceVarying(level, src, dst, channel);
        src.swap(dst);
    }
    result.swap(src);
    return !result.empty();
}


static bool IsFinitePosition(RwV3d const &v)
{
    return v.x == v.x && v.y == v.y && v.z == v.z &&
           v.x <= FLT_MAX && v.x >= -FLT_MAX &&
           v.y <= FLT_MAX && v.y >= -FLT_MAX &&
           v.z <= FLT_MAX && v.z >= -FLT_MAX;
}

static int32 SourceCornerForTopoVertex(std::vector<Far::Index> const &topoFaceVertices,
                                       std::vector<Far::Index> const &sourceFaceVertices,
                                       int32 face, int32 topoVertex)
{
    const int32 base = face * 3;
    for(int32 corner = 0; corner < 3; corner++)
        if(topoFaceVertices[base + corner] == topoVertex)
            return sourceFaceVertices[base + corner];
    return -1;
}

static bool SourceNormalSeam(std::vector<Far::Index> const &topoFaceVertices,
                             std::vector<Far::Index> const &sourceFaceVertices,
                             RwV3d const *sourceNormals, int32 face0, int32 face1, EdgeKey const &edge)
{
    const int32 endpoints[2] = { edge.a, edge.b };
    for(int32 endpoint = 0; endpoint < 2; endpoint++){
        const int32 s0 = SourceCornerForTopoVertex(topoFaceVertices, sourceFaceVertices, face0, endpoints[endpoint]);
        const int32 s1 = SourceCornerForTopoVertex(topoFaceVertices, sourceFaceVertices, face1, endpoints[endpoint]);
        if(s0 < 0 || s1 < 0)
            return true;
        if(s0 == s1)
            continue;
        OSDVec3 n0 = { sourceNormals[s0].x, sourceNormals[s0].y, sourceNormals[s0].z };
        OSDVec3 n1 = { sourceNormals[s1].x, sourceNormals[s1].y, sourceNormals[s1].z };
        n0 = Normalize(n0);
        n1 = Normalize(n1);
        if(Dot(n0, n1) < kSourceNormalSeamCos)
            return true;
    }
    return false;
}

static int32 CornerForVertex(Far::ConstIndexArray const &faceVerts, int32 face, int32 vertex)
{
    for(int32 i = 0; i < (int32)faceVerts.size(); i++)
        if(faceVerts[i] == vertex)
            return face * 3 + i;
    return -1;
}

static int32 FindBaseFace(Far::TopologyRefiner const &refiner, int32 level, int32 face)
{
    int32 baseFace = face;
    for(int32 currentLevel = level; currentLevel > 0; currentLevel--)
        baseFace = refiner.GetLevel(currentLevel).GetFaceParentFace(baseFace);
    return baseFace;
}

static int32 FaceEdgeDirection(std::vector<Far::Index> const &faceVertices,
                               int32 face, int32 a, int32 b)
{
    const int32 base = face * 3;
    for(int32 corner = 0; corner < 3; corner++){
        const int32 v0 = faceVertices[base + corner];
        const int32 v1 = faceVertices[base + ((corner + 1) % 3)];
        if(v0 == a && v1 == b)
            return 1;
        if(v0 == b && v1 == a)
            return -1;
    }
    return 0;
}

static bool ValidateWeldedManifoldTopology(std::vector<Far::Index> const &faceVertices,
                                           int32 numVertices,
                                           std::map<EdgeKey, EdgeInfo> const &edges)
{
    if(numVertices <= 0)
        return false;

    std::vector< std::vector<int32> > incidentFaces(numVertices);
    for(int32 face = 0; face < (int32)faceVertices.size() / 3; face++){
        for(int32 corner = 0; corner < 3; corner++){
            const int32 vertex = faceVertices[face*3 + corner];
            if(vertex < 0 || vertex >= numVertices)
                return false;
            incidentFaces[vertex].push_back(face);
        }
    }

    // Every interior edge of an orientable triangle manifold must be traversed in
    // opposite directions by its two faces. Same-direction use is a folded/non-orientable
    // seam that OpenSubdiv's Loop topology cannot safely consume here.
    for(std::map<EdgeKey, EdgeInfo>::const_iterator it = edges.begin(); it != edges.end(); ++it){
        EdgeInfo const &info = it->second;
        if(info.count == 2){
            const int32 d0 = FaceEdgeDirection(faceVertices, info.face[0], it->first.a, it->first.b);
            const int32 d1 = FaceEdgeDirection(faceVertices, info.face[1], it->first.a, it->first.b);
            if(d0 == 0 || d1 == 0 || d0 == d1)
                return false;
        }
    }

    std::vector<std::vector<const std::pair<const EdgeKey, EdgeInfo>*>> incidentEdges(numVertices);
    for(const auto& edge:edges){
        incidentEdges[edge.first.a].push_back(&edge);
        incidentEdges[edge.first.b].push_back(&edge);
    }

    // Exact-position welding can accidentally join two otherwise unrelated shells at a
    // single coordinate. Edge-manifold checks do not catch that "bow-tie vertex" case.
    // Require each welded vertex's incident faces to form one connected fan and require
    // either zero boundary edges (closed fan) or exactly two (ordinary open boundary).
    for(int32 vertex = 0; vertex < numVertices; vertex++){
        std::vector<int32> const &faces = incidentFaces[vertex];
        if(faces.empty())
            continue;

        int32 boundaryEdges = 0;
        std::map<int32, std::vector<int32> > adjacency;
        for(size_t i = 0; i < faces.size(); i++)
            adjacency[faces[i]];

        for(const auto* it:incidentEdges[vertex]){
            EdgeInfo const &info = it->second;
            if(info.count == 1){
                boundaryEdges++;
            }else if(info.count == 2){
                adjacency[info.face[0]].push_back(info.face[1]);
                adjacency[info.face[1]].push_back(info.face[0]);
            }else{
                return false;
            }
        }

        if(boundaryEdges != 0 && boundaryEdges != 2)
            return false;

        std::vector<int32> stack;
        std::map<int32, bool> visited;
        stack.push_back(faces[0]);
        while(!stack.empty()){
            const int32 face = stack.back();
            stack.pop_back();
            if(visited[face])
                continue;
            visited[face] = true;
            std::vector<int32> const &neighbors = adjacency[face];
            for(size_t i = 0; i < neighbors.size(); i++)
                if(!visited[neighbors[i]])
                    stack.push_back(neighbors[i]);
        }
        if(visited.size() != faces.size())
            return false;
    }

    return true;
}

std::map<uint64_t,std::shared_ptr<const StorylandSubdivMesh>> cache;
size_t cacheBytes=0;
// Pass a copy to the worker. The IMG can be closed while it is still running.
struct WorldSubdivisionJob {
    uint64_t key = 0;
    uint64_t generation = 0;
    std::future<std::shared_ptr<const StorylandSubdivMesh>> result;
};
WorldSubdivisionJob worldSubdivisionJob;
std::atomic<uint64_t> subdivisionGeneration{0};
uint64_t keyFor(const std::vector<StorylandModelPoint>& p,const std::vector<StorylandModelTriangle>& t,
 const std::vector<StorylandModelTexcoord>& uv,const std::vector<StorylandModelPrelight>& c){
 uint64_t h=1469598103934665603ull;
 auto add=[&](const auto& value){const auto* b=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(value);++i){h^=b[i];h*=1099511628211ull;}};
 add(p.size());add(t.size());add(uv.size());add(c.size());
 for(const auto& v:p){add(v.x);add(v.y);add(v.z);add(v.hasNormal);add(v.nx);add(v.ny);add(v.nz);}
 for(const auto& f:t){add(f.a);add(f.b);add(f.c);add(f.materialIndex);add(f.damagedPart);add(f.vehicleWheel);add(f.vehicleFrontWheel);add(f.vehiclePivot.x);add(f.vehiclePivot.y);add(f.vehiclePivot.z);for(char b:f.vehiclePart)add(b);add(f.vehiclePart.size());}
 for(const auto& v:uv){add(v.u);add(v.v);add(v.secondaryU);add(v.secondaryV);add(v.hasSecondary);}
 for(const auto& v:c){add(v.red);add(v.green);add(v.blue);add(v.alpha);add(v.valid);}
 return h;
}
static std::shared_ptr<StorylandSubdivMesh> refine(const std::vector<StorylandModelPoint>& points,
 const std::vector<StorylandModelTriangle>& triangles,const std::vector<StorylandModelTexcoord>& uv,
 const std::vector<StorylandModelPrelight>& colours){
 const char* modelName="";Category category=CATEGORY_VEHICLE;
 const int32 numVertices=int32(points.size()),numTriangles=int32(triangles.size());
 const bool secondary=uv.size()==points.size()&&std::all_of(uv.begin(),uv.end(),[](const auto& v){return v.hasSecondary;});
 const int32 numTexCoords=uv.size()==points.size()?(secondary?2:1):0;
 if(numVertices<4||numTriangles<4||numTriangles>MaxSourceTriangles||uint64(numTriangles)*4>MaxOutputTriangles)return {};
 std::vector<RwV3d> normalStorage(points.size());
 bool authored=true;
 for(size_t i=0;i<points.size();++i){normalStorage[i]={points[i].nx,points[i].ny,points[i].nz};authored&=points[i].hasNormal;}
 std::vector<RpTriangle> triangleStorage;triangleStorage.reserve(triangles.size());
 uint32 numMaterials=0;
 for(const auto& t:triangles){
  if(t.a>=points.size()||t.b>=points.size()||t.c>=points.size())return {};
  // IDs are opaque: convert them to dense local material indices only for validation.
  triangleStorage.push_back({{int32(t.a),int32(t.b),int32(t.c)},uint32(triangleStorage.size())});
 }
 // Preserve actual material identity (and moving vehicle parts) when tracing children.
 std::map<uint32_t,uint32_t> materialIds;
 for(size_t i=0;i<triangles.size();++i){auto pair=materialIds.emplace(triangles[i].materialIndex,uint32(materialIds.size()));triangleStorage[i].matId=pair.first->second;}
 numMaterials=uint32(materialIds.size());
 if(!authored){
  for(auto& n:normalStorage)n={};
  for(const auto& t:triangles){OSDVec3 a{points[t.a].x,points[t.a].y,points[t.a].z},b{points[t.b].x,points[t.b].y,points[t.b].z},c{points[t.c].x,points[t.c].y,points[t.c].z};auto n=Cross(Sub(b,a),Sub(c,a));
   for(auto i:{t.a,t.b,t.c}){normalStorage[i].x+=n.x;normalStorage[i].y+=n.y;normalStorage[i].z+=n.z;}}
  for(auto& n:normalStorage){auto v=Normalize({n.x,n.y,n.z});n={v.x,v.y,v.z};}
 }
 const RwV3d* sourceVertices=points.data();const RwV3d* sourceNormals=normalStorage.data();
 const RpTriangle* sourceTriangles=triangleStorage.data();const RwRGBA* sourceColors=colours.size()==points.size()?colours.data():nullptr;
 std::vector<RwTexCoords> secondaryUV(secondary?uv.size():0);
 if(secondary)for(size_t i=0;i<uv.size();++i)secondaryUV[i]={uv[i].secondaryU,uv[i].secondaryV};
 std::vector<const RwTexCoords*> sourceUV;if(numTexCoords)sourceUV.push_back(uv.data());if(secondary)sourceUV.push_back(secondaryUV.data());
    // RenderWare commonly duplicates vertices at UV and hard-normal seams.  OpenSubdiv
    // positions must describe geometric topology independently of those corner attributes,
    // otherwise every texture seam becomes a subdivision boundary and visible cracks appear.
    // Ambiguous joins are separated into manifold patches below, with their
    // shared boundary positions pinned so subdivision does not open cracks.
    std::map<PositionKey, int32> positionMap;
    std::vector<int32> sourceToTopo(numVertices, -1);
    std::vector<OSDVec3> basePositions;
    basePositions.reserve(numVertices);
    for(int32 i = 0; i < numVertices; i++){
        if(!IsFinitePosition(sourceVertices[i])){
            LogSkip(modelName, "non-finite source position");
            return nullptr;
        }
        PositionKey key(sourceVertices[i]);
        std::map<PositionKey, int32>::iterator found = positionMap.find(key);
        if(found == positionMap.end()){
            int32 topo = (int32)basePositions.size();
            OSDVec3 p = { sourceVertices[i].x, sourceVertices[i].y, sourceVertices[i].z };
            basePositions.push_back(p);
            positionMap[key] = topo;
            sourceToTopo[i] = topo;
        }else{
            sourceToTopo[i] = found->second;
        }
    }
    if(basePositions.size() < 4){
        LogSkip(modelName, "insufficient welded topology");
        return nullptr;
    }

    std::vector<int> vertsPerFace(numTriangles, 3);
    std::vector<Far::Index> faceVertices(numTriangles * 3);
    std::vector<Far::Index> sourceFaceVertices(numTriangles * 3);
    std::vector<OSDVec3> baseFaceNormals(numTriangles);
    std::map<EdgeKey, EdgeInfo> edges;

    for(int32 face = 0; face < numTriangles; face++){
        const RpTriangle &tri = sourceTriangles[face];
        const int32 s0 = tri.v[0];
        const int32 s1 = tri.v[1];
        const int32 s2 = tri.v[2];
        if(s0 < 0 || s1 < 0 || s2 < 0 || s0 >= numVertices || s1 >= numVertices || s2 >= numVertices ||
           s0 == s1 || s1 == s2 || s2 == s0 || tri.matId >= numMaterials){
            LogSkip(modelName, "invalid triangle topology/material");
            return nullptr;
        }
        const int32 v0 = sourceToTopo[s0];
        const int32 v1 = sourceToTopo[s1];
        const int32 v2 = sourceToTopo[s2];
        if(v0 == v1 || v1 == v2 || v2 == v0){
            LogSkip(modelName, "degenerate welded triangle");
            return nullptr;
        }

        faceVertices[face*3 + 0] = v0;
        faceVertices[face*3 + 1] = v1;
        faceVertices[face*3 + 2] = v2;
        sourceFaceVertices[face*3 + 0] = s0;
        sourceFaceVertices[face*3 + 1] = s1;
        sourceFaceVertices[face*3 + 2] = s2;

        OSDVec3 e0 = Sub(basePositions[v1], basePositions[v0]);
        OSDVec3 e1 = Sub(basePositions[v2], basePositions[v0]);
        OSDVec3 n = Cross(e0, e1);
        if(LengthSq(n) <= kNormalEpsilon){
            LogSkip(modelName, "degenerate triangle");
            return nullptr;
        }
        baseFaceNormals[face] = Normalize(n);

    }

    // Native strips may reverse winding or meet at overlapping shells. Keep face
    // attributes intact, orient each manifold patch, and split ambiguous seams.
    std::map<EdgeKey,std::vector<int32>> uses;
    for(int32 f=0;f<numTriangles;++f)for(int k=0;k<3;++k)
        uses[EdgeKey(faceVertices[f*3+k],faceVertices[f*3+(k+1)%3])].push_back(f);
    std::vector<std::vector<std::pair<int32,bool>>> adjacency(numTriangles);
    std::set<int32> pinnedOriginal;
    for(const auto& e:uses){
        if(e.second.size()!=2){if(e.second.size()>2){pinnedOriginal.insert(e.first.a);pinnedOriginal.insert(e.first.b);}continue;}
        int a=e.second[0],b=e.second[1];
        bool same=FaceEdgeDirection(faceVertices,a,e.first.a,e.first.b)==FaceEdgeDirection(faceVertices,b,e.first.a,e.first.b);
        adjacency[a].push_back({b,same});adjacency[b].push_back({a,same});
    }
    std::vector<int> flip(numTriangles,-1);
    for(int seed=0;seed<numTriangles;++seed)if(flip[seed]<0){
        flip[seed]=0;std::vector<int> pending{seed};
        while(!pending.empty()){int f=pending.back();pending.pop_back();
            for(auto e:adjacency[f]){int wanted=flip[f]^int(e.second);
                if(flip[e.first]<0){flip[e.first]=wanted;pending.push_back(e.first);}
                else if(flip[e.first]!=wanted){LogSkip(modelName,"non-orientable patch");return {};}}}
    }
    for(int f=0;f<numTriangles;++f)if(flip[f]){
        std::swap(faceVertices[f*3+1],faceVertices[f*3+2]);
        std::swap(sourceFaceVertices[f*3+1],sourceFaceVertices[f*3+2]);
        auto& n=baseFaceNormals[f];n.x=-n.x;n.y=-n.y;n.z=-n.z;
    }
    DisjointSet corners(numTriangles*3);
    for(const auto& e:uses)if(e.second.size()==2){
        int a=e.second[0],b=e.second[1];
        for(int v:{e.first.a,e.first.b}){int ca=-1,cb=-1;
            for(int k=0;k<3;++k){if(faceVertices[a*3+k]==v)ca=a*3+k;if(faceVertices[b*3+k]==v)cb=b*3+k;}
            corners.Union(ca,cb);}
    }
    std::map<int,int> roots;std::vector<OSDVec3> splitPositions;
    std::vector<std::vector<uint32_t>> weldedSources;
    std::vector<Far::Index> pinnedCorners;
    for(int c=0;c<numTriangles*3;++c){int old=faceVertices[c],root=corners.Find(c);
        auto inserted=roots.emplace(root,int(roots.size()));int v=inserted.first->second;
        if(inserted.second){splitPositions.push_back(basePositions[old]);weldedSources.emplace_back();
            if(pinnedOriginal.count(old))pinnedCorners.push_back(v);}
        weldedSources[v].push_back(sourceFaceVertices[c]);faceVertices[c]=v;
    }
    for(auto& controls:weldedSources){std::sort(controls.begin(),controls.end());controls.erase(std::unique(controls.begin(),controls.end()),controls.end());}
    basePositions.swap(splitPositions);
    for(int f=0;f<numTriangles;++f)for(int k=0;k<3;++k){auto& e=edges[EdgeKey(faceVertices[f*3+k],faceVertices[f*3+(k+1)%3])];
        if(e.count>=2){LogSkip(modelName,"ambiguous connected seam");return {};}
        e.face[e.count++]=f;}
    if(!ValidateWeldedManifoldTopology(faceVertices,int32(basePositions.size()),edges)){
        LogSkip(modelName,"invalid split topology");return {};}
    std::vector<float> cornerWeights(pinnedCorners.size(),Sdc::Crease::SHARPNESS_INFINITE);

    const float creaseCos = cosf(CreaseAngleDegrees * (kPi / 180.0f));
    const float curvedCos = cosf(MinCurvedAngleDegrees * (kPi / 180.0f));
    int32 interiorEdges = 0;
    int32 curvedEdges = 0;
    std::vector<Far::Index> creasePairs;
    std::vector<float> creaseWeights;

    for(std::map<EdgeKey, EdgeInfo>::const_iterator it = edges.begin(); it != edges.end(); ++it){
        EdgeInfo const &info = it->second;
        if(info.count != 2)
            continue;
        interiorEdges++;

        const int32 face0 = info.face[0];
        const int32 face1 = info.face[1];
        float normalDot = ClampFloat(Dot(baseFaceNormals[face0], baseFaceNormals[face1]), -1.0f, 1.0f);
        bool materialBoundary = sourceTriangles[face0].matId != sourceTriangles[face1].matId;
        bool authoredNormalSeam = authored && SourceNormalSeam(faceVertices, sourceFaceVertices, sourceNormals, face0, face1, it->first);
        bool partBoundary = triangles[face0].vehiclePart != triangles[face1].vehiclePart || triangles[face0].damagedPart != triangles[face1].damagedPart || triangles[face0].vehicleWheel != triangles[face1].vehicleWheel;
        bool sharp = normalDot <= creaseCos || authoredNormalSeam || partBoundary ||
                     (PreserveMaterialBoundaries && materialBoundary);

        if(sharp){
            creasePairs.push_back(it->first.a);
            creasePairs.push_back(it->first.b);
            creaseWeights.push_back(Sdc::Crease::SHARPNESS_INFINITE);
        }else if(normalDot < curvedCos){
            curvedEdges++;
        }
    }

    const int32 minCurvedEdges = category == CATEGORY_WORLD ?
        std::max(4, interiorEdges / 25) : std::max(2, interiorEdges / 100);
    if(interiorEdges == 0 || curvedEdges < minCurvedEdges){
        LogSkip(modelName, "mesh is predominantly flat/hard-edged");
        return nullptr;
    }

    typedef Far::TopologyDescriptor Descriptor;
    const int32 colorFVarChannel = sourceColors != nullptr ? numTexCoords : -1;
    const int32 numFVarChannels = numTexCoords + (sourceColors != nullptr ? 1 : 0);
    std::vector<Descriptor::FVarChannel> fvarChannels(numFVarChannels);
    for(int32 channel = 0; channel < numFVarChannels; channel++){
        fvarChannels[channel].numValues = numVertices;
        fvarChannels[channel].valueIndices = sourceFaceVertices.empty() ? nullptr : &sourceFaceVertices[0];
    }

    Descriptor desc;
    desc.numVertices = (int32)basePositions.size();
    desc.numFaces = numTriangles;
    desc.numVertsPerFace = vertsPerFace.empty() ? nullptr : &vertsPerFace[0];
    desc.vertIndicesPerFace = faceVertices.empty() ? nullptr : &faceVertices[0];
    desc.numCreases = (int32)creaseWeights.size();
    desc.creaseVertexIndexPairs = creasePairs.empty() ? nullptr : &creasePairs[0];
    desc.creaseWeights = creaseWeights.empty() ? nullptr : &creaseWeights[0];
    desc.numCorners = int(pinnedCorners.size());
    desc.cornerVertexIndices = pinnedCorners.empty()?nullptr:pinnedCorners.data();
    desc.cornerWeights = cornerWeights.empty()?nullptr:cornerWeights.data();
    desc.numFVarChannels = numFVarChannels;
    desc.fvarChannels = fvarChannels.empty() ? nullptr : &fvarChannels[0];

    Sdc::Options schemeOptions;
    schemeOptions.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);
    schemeOptions.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_BOUNDARIES);
    schemeOptions.SetCreasingMethod(Sdc::Options::CREASE_UNIFORM);

    typedef Far::TopologyRefinerFactory<Descriptor> Factory;
    Factory::Options factoryOptions(Sdc::SCHEME_LOOP, schemeOptions);
    factoryOptions.validateFullTopology = true;

    Far::TopologyRefiner *refiner = Factory::Create(desc, factoryOptions);
    if(refiner == nullptr){
        LogSkip(modelName, "OpenSubdiv rejected topology");
        return nullptr;
    }

    std::unique_ptr<Far::TopologyRefiner> owner(refiner);
    Far::TopologyRefiner::UniformOptions uniform(Level);uniform.fullTopologyInLastLevel=true;
    refiner->RefineUniform(uniform);
    Far::TopologyLevel const &finalLevel = refiner->GetLevel(Level);
    const int32 refinedPositionCount = finalLevel.GetNumVertices();
    const int32 refinedFaceCount = finalLevel.GetNumFaces();
    if(refinedPositionCount <= 0 || refinedFaceCount <= 0 || refinedFaceCount > MaxOutputTriangles){

        LogSkip(modelName, "OpenSubdiv output budget");
        return nullptr;
    }

    std::vector<OSDVec3> refinedPositions;
    if(!RefinePositions(*refiner, basePositions, refinedPositions)){

        LogSkip(modelName, "position interpolation failed");
        return nullptr;
    }

    std::vector<OSDColor> refinedColors;
    if(sourceColors != nullptr){
        std::vector<OSDColor> baseColors(numVertices);
        for(int32 i = 0; i < numVertices; i++){
            baseColors[i].r = sourceColors[i].red / 255.0f;
            baseColors[i].g = sourceColors[i].green / 255.0f;
            baseColors[i].b = sourceColors[i].blue / 255.0f;
            baseColors[i].a = sourceColors[i].alpha / 255.0f;
        }
        if(!RefineFaceVaryingColors(*refiner, colorFVarChannel, baseColors, refinedColors)){
    
            LogSkip(modelName, "prelight face-varying interpolation failed");
            return nullptr;
        }
    }

    std::vector< std::vector<OSDVec2> > refinedUV(numTexCoords);
    for(int32 channel = 0; channel < numTexCoords; channel++){
        std::vector<OSDVec2> baseUV(numVertices);
        for(int32 i = 0; i < numVertices; i++){
            baseUV[i].x = sourceUV[channel][i].u;
            baseUV[i].y = sourceUV[channel][i].v;
        }
        if(!RefineUVs(*refiner, channel, baseUV, refinedUV[channel])){
    
            LogSkip(modelName, "UV interpolation failed");
            return nullptr;
        }
    }

    // Build smooth-normal islands from final topology. Faces are joined across an edge only
    // when OpenSubdiv says that edge is smooth, so infinitely sharp creases and boundaries
    // become proper RenderWare vertex splits instead of being incorrectly normal-averaged.
    const int32 numCorners = refinedFaceCount * 3;
    DisjointSet normalGroups(numCorners);
    std::vector<OSDVec3> faceAreaNormals(refinedFaceCount);

    for(int32 face = 0; face < refinedFaceCount; face++){
        Far::ConstIndexArray fverts = finalLevel.GetFaceVertices(face);
        if(fverts.size() != 3){
    
            LogSkip(modelName, "Loop produced non-triangle face");
            return nullptr;
        }
        OSDVec3 e0 = Sub(refinedPositions[fverts[1]], refinedPositions[fverts[0]]);
        OSDVec3 e1 = Sub(refinedPositions[fverts[2]], refinedPositions[fverts[0]]);
        faceAreaNormals[face] = Cross(e0, e1);
    }

    for(int32 edge = 0; edge < finalLevel.GetNumEdges(); edge++){
        Far::ConstIndexArray efaces = finalLevel.GetEdgeFaces(edge);
        if(efaces.size() != 2 || finalLevel.IsEdgeBoundary(edge) || finalLevel.IsEdgeNonManifold(edge) ||
           finalLevel.GetEdgeSharpness(edge) > Sdc::Crease::SHARPNESS_SMOOTH)
            continue;

        Far::ConstIndexArray everts = finalLevel.GetEdgeVertices(edge);
        if(everts.size() != 2)
            continue;
        const int32 f0 = efaces[0];
        const int32 f1 = efaces[1];
        Far::ConstIndexArray f0verts = finalLevel.GetFaceVertices(f0);
        Far::ConstIndexArray f1verts = finalLevel.GetFaceVertices(f1);

        for(int32 endpoint = 0; endpoint < 2; endpoint++){
            const int32 vertex = everts[endpoint];
            const int32 c0 = CornerForVertex(f0verts, f0, vertex);
            const int32 c1 = CornerForVertex(f1verts, f1, vertex);
            if(c0 >= 0 && c1 >= 0)
                normalGroups.Union(c0, c1);
        }
    }

    std::vector<OSDVec3> groupNormalSum(numCorners);
    for(int32 i = 0; i < numCorners; i++)
        groupNormalSum[i].Clear();
    for(int32 face = 0; face < refinedFaceCount; face++){
        for(int32 corner = 0; corner < 3; corner++){
            int32 root = normalGroups.Find(face*3 + corner);
            groupNormalSum[root].x += faceAreaNormals[face].x;
            groupNormalSum[root].y += faceAreaNormals[face].y;
            groupNormalSum[root].z += faceAreaNormals[face].z;
        }
    }
    for(int32 i = 0; i < numCorners; i++)
        if(normalGroups.Find(i) == i)
            groupNormalSum[i] = Normalize(groupNormalSum[i]);

    std::map<VertexKey, int32> vertexMap;
    std::vector<OutputVertex> outputVertices;
    std::vector<uint16> outputIndices(refinedFaceCount * 3);
    std::vector<int32> outputBaseFaces(refinedFaceCount);

    for(int32 face = 0; face < refinedFaceCount; face++){
        Far::ConstIndexArray fverts = finalLevel.GetFaceVertices(face);

        for(int32 corner = 0; corner < 3; corner++){
            VertexKey key;
            key.position = fverts[corner];
            key.normalGroup = normalGroups.Find(face*3 + corner);
            key.numUV = numTexCoords;
            if(colorFVarChannel >= 0){
                Far::ConstIndexArray faceColor = finalLevel.GetFaceFVarValues(face, colorFVarChannel);
                if(faceColor.size() != 3){
            
                    LogSkip(modelName, "invalid face-varying prelight topology");
                    return nullptr;
                }
                key.color = faceColor[corner];
            }
            for(int32 channel = 0; channel < numTexCoords; channel++){
                // Do not retain ConstIndexArray views in a container. They are lightweight
                // views owned by the final TopologyLevel; consume each view immediately.
                Far::ConstIndexArray faceUV = finalLevel.GetFaceFVarValues(face, channel);
                if(faceUV.size() != 3){
            
                    LogSkip(modelName, "invalid face-varying UV topology");
                    return nullptr;
                }
                key.uv[channel] = faceUV[corner];
            }

            std::map<VertexKey, int32>::iterator found = vertexMap.find(key);
            int32 outputIndex;
            if(found == vertexMap.end()){
                outputIndex = (int32)outputVertices.size();
                if(outputIndex >= kMaxRwVertices){
            
                    LogSkip(modelName, "RenderWare 16-bit vertex limit");
                    return nullptr;
                }
                OutputVertex out;
                out.position = key.position;
                out.normalGroup = key.normalGroup;
                out.color = key.color;
                for(int32 channel = 0; channel < 8; channel++)
                    out.uv[channel] = key.uv[channel];
                outputVertices.push_back(out);
                vertexMap[key] = outputIndex;
            }else{
                outputIndex = found->second;
            }
            outputIndices[face*3 + corner] = (uint16)outputIndex;
        }
        outputBaseFaces[face] = FindBaseFace(*refiner, Level, face);
        if(outputBaseFaces[face] < 0 || outputBaseFaces[face] >= numTriangles){
    
            LogSkip(modelName, "invalid parent-face mapping");
            return nullptr;
        }
    }

    Far::StencilTableFactory::Options stencilOptions;
    stencilOptions.generateIntermediateLevels=false;stencilOptions.generateOffsets=true;stencilOptions.maxLevel=Level;
    std::unique_ptr<const Far::StencilTable> stencils(Far::StencilTableFactory::Create(*refiner,stencilOptions));
    if(!stencils || stencils->GetNumStencils()!=refinedPositionCount)return {};

    auto result=std::make_shared<StorylandSubdivMesh>();result->sourceCount=points.size();
    for(const auto& vertex:outputVertices){
        const auto stencil=stencils->GetStencil(vertex.position);std::vector<StorylandSubdivWeight> weights;
        for(int i=0;i<stencil.GetSize();++i){const auto& controls=weldedSources[stencil.GetVertexIndices()[i]];
            for(auto original:controls)weights.push_back({original,stencil.GetWeights()[i]/float(controls.size())});}
        result->stencils.push_back(std::move(weights));result->normalGroups.push_back(uint32_t(vertex.normalGroup));
    }
    result->points.reserve(outputVertices.size());
    for(const auto& vertex:outputVertices){
        const auto& p=refinedPositions[vertex.position];const auto& n=groupNormalSum[vertex.normalGroup];
        result->points.push_back({p.x,p.y,p.z,n.x,n.y,n.z,true});
        if(numTexCoords){const auto& u=refinedUV[0][vertex.uv[0]];StorylandModelTexcoord out{u.x,u.y};
            if(secondary){const auto& v=refinedUV[1][vertex.uv[1]];out.secondaryU=v.x;out.secondaryV=v.y;out.hasSecondary=true;}result->uv.push_back(out);}
        if(sourceColors){const auto& c=refinedColors[vertex.color];StorylandModelPrelight out;
            out.red=FloatToByte(c.r);out.green=FloatToByte(c.g);out.blue=FloatToByte(c.b);out.alpha=FloatToByte(c.a);out.valid=true;result->colours.push_back(out);}
    }
    for(int32 f=0;f<refinedFaceCount;++f){auto tri=triangles[outputBaseFaces[f]];
        tri.a=outputIndices[f*3];tri.b=outputIndices[f*3+1];tri.c=outputIndices[f*3+2];result->triangles.push_back(std::move(tri));}
    return result;
}
}
static size_t subdivisionStorageBytes(const std::shared_ptr<const StorylandSubdivMesh>& mesh){
    size_t bytes=sizeof(uint64_t)+sizeof(std::shared_ptr<const StorylandSubdivMesh>)+4*sizeof(void*);
    if(!mesh)return bytes;
    bytes+=sizeof(StorylandSubdivMesh)+mesh->points.capacity()*sizeof(StorylandModelPoint);
    bytes+=mesh->triangles.capacity()*sizeof(StorylandModelTriangle);
    bytes+=mesh->uv.capacity()*sizeof(StorylandModelTexcoord)+mesh->colours.capacity()*sizeof(StorylandModelPrelight);
    bytes+=mesh->normalGroups.capacity()*sizeof(uint32_t);
    bytes+=mesh->stencils.capacity()*sizeof(std::vector<StorylandSubdivWeight>);
    for(const auto& weights:mesh->stencils)bytes+=weights.capacity()*sizeof(StorylandSubdivWeight);
    for(const auto& triangle:mesh->triangles)bytes+=triangle.vehiclePart.capacity()+1;
    return bytes;
}
static void releaseSubdivisionStorage(size_t limit){
    for(auto it=cache.begin();it!=cache.end()&&(cacheBytes>limit||cache.size()>4096);){
        if(it->second&&it->second.use_count()>1){++it;continue;}
        const size_t bytes=subdivisionStorageBytes(it->second);
        cacheBytes=bytes>cacheBytes?0:cacheBytes-bytes;
        it=cache.erase(it);
    }
}
void storylandClearSubdivisionCache(){cache.clear();cacheBytes=0;++subdivisionGeneration;}
std::shared_ptr<const StorylandSubdivMesh> storylandAeroSubdivide(const std::vector<StorylandModelPoint>& p,
 const std::vector<StorylandModelTriangle>& t,const std::vector<StorylandModelTexcoord>& uv,const std::vector<StorylandModelPrelight>& c){
    if(p.empty()||p.size()>65535||t.empty()||t.size()>6000)return {};
    const uint64_t key=keyFor(p,t,uv,c);
    auto found=cache.find(key);if(found!=cache.end())return found->second;
    constexpr size_t budget=128u*1024u*1024u;
    // Leave working space before OpenSubdiv allocates its temporary topology.
    releaseSubdivisionStorage(budget-32u*1024u*1024u);
    try{
        auto mesh=refine(p,t,uv,c);
        const size_t bytes=subdivisionStorageBytes(mesh);
        if(bytes<=budget){
            releaseSubdivisionStorage(budget-bytes);
            if(cacheBytes<=budget-bytes){cache.emplace(key,mesh);cacheBytes+=bytes;}
        }
        return mesh;
    }catch(const std::bad_alloc&){storylandClearSubdivisionCache();return {};}
}

std::shared_ptr<const StorylandSubdivMesh> storylandAeroSubdivideWorld(
    std::vector<StorylandModelPoint> points,
    std::vector<StorylandModelTriangle> triangles,
    std::vector<StorylandModelTexcoord> uv,
    std::vector<StorylandModelPrelight> colours) {
    constexpr size_t budget = 128u * 1024u * 1024u;
    if (worldSubdivisionJob.result.valid() &&
        worldSubdivisionJob.result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const auto completed = worldSubdivisionJob.result.get();
        if (worldSubdivisionJob.generation == subdivisionGeneration) {
            const size_t bytes = subdivisionStorageBytes(completed);
            releaseSubdivisionStorage(budget > bytes ? budget - bytes : 0);
            if (bytes <= budget && cacheBytes <= budget - bytes) {
                const auto inserted = cache.emplace(worldSubdivisionJob.key, completed);
                if (inserted.second) cacheBytes += bytes;
            }
        }
    }
    if (points.empty() || points.size() > 65535 || triangles.empty() || triangles.size() > 6000) return {};
    const uint64_t key = keyFor(points, triangles, uv, colours);
    const auto found = cache.find(key);
    if (found != cache.end()) return found->second;
    if (!worldSubdivisionJob.result.valid()) {
        releaseSubdivisionStorage(budget - 32u * 1024u * 1024u);
        worldSubdivisionJob.key = key;
        worldSubdivisionJob.generation = subdivisionGeneration;
        try {
        worldSubdivisionJob.result = std::async(std::launch::async,
            [points = std::move(points), triangles = std::move(triangles),
             uv = std::move(uv), colours = std::move(colours), generation = worldSubdivisionJob.generation]() -> std::shared_ptr<const StorylandSubdivMesh> {
                try {
                    if (generation != subdivisionGeneration.load()) return {};
                    auto result = refine(points, triangles, uv, colours);
                    if (generation != subdivisionGeneration.load()) return {};
                    return result;
                } catch (const std::exception&) {
                    return {};
                }
            });
        } catch (const std::system_error&) {
            // A failed optional worker must not terminate the viewer.
            return {};
        }
    }
    return {};
}

bool storylandPoseSubdivision(const StorylandSubdivMesh& mesh,const std::vector<StorylandModelPoint>& posed,
 std::vector<StorylandModelPoint>& output){
 if(posed.size()!=mesh.sourceCount||mesh.stencils.size()!=mesh.points.size())return false;
 output.assign(mesh.points.size(),{});
 for(size_t i=0;i<output.size();++i){auto& p=output[i];for(const auto& w:mesh.stencils[i]){
    const auto& v=posed[w.source];p.x+=v.x*w.weight;p.y+=v.y*w.weight;p.z+=v.z*w.weight;}
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){output.clear();return false;}}
 std::vector<OSDVec3> sums(mesh.triangles.size()*3);
 for(const auto& t:mesh.triangles){const auto& a=output[t.a];const auto& b=output[t.b];const auto& c=output[t.c];
    auto n=Cross(Sub({b.x,b.y,b.z},{a.x,a.y,a.z}),Sub({c.x,c.y,c.z},{a.x,a.y,a.z}));
    for(auto i:{t.a,t.b,t.c}){auto& sum=sums[mesh.normalGroups[i]];sum.x+=n.x;sum.y+=n.y;sum.z+=n.z;}}
 for(size_t i=0;i<output.size();++i){auto n=Normalize(sums[mesh.normalGroups[i]]);output[i].nx=n.x;output[i].ny=n.y;output[i].nz=n.z;output[i].hasNormal=true;}
 return true;
}
