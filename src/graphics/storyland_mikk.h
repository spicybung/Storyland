#pragma once
#include "src/leeds/leeds_model.h"
#include "vendors/mikktspace/mikktspace.h"
#include <array>
#include <climits>
// Per-corner output preserves mirrored UV seams. Never average tangent handedness.
inline std::vector<std::array<float,4>> storylandMikkTangents(
 const std::vector<StorylandModelPoint>& points,const std::vector<StorylandModelPoint>& normals,
 const std::vector<StorylandModelTriangle>& triangles,const std::vector<StorylandModelTexcoord>& uv){
 std::vector<std::array<float,4>> result;
 if(triangles.size()>size_t(INT_MAX)||uv.size()!=points.size()||normals.size()!=points.size())return result;
 for(const auto& t:triangles)if(t.a>=points.size()||t.b>=points.size()||t.c>=points.size())return result;
 result.resize(triangles.size()*3,{1,0,0,1});
 struct Context {const std::vector<StorylandModelPoint>& p;const std::vector<StorylandModelPoint>& n;
 const std::vector<StorylandModelTriangle>& t;const std::vector<StorylandModelTexcoord>& uv;std::vector<std::array<float,4>>& out;
 uint32_t index(int f,int v)const {const auto& x=t[f];return v==0?x.a:v==1?x.b:x.c;}} data{points,normals,triangles,uv,result};
 SMikkTSpaceInterface api{};
 api.m_getNumFaces=[](const SMikkTSpaceContext* c){return int(static_cast<Context*>(c->m_pUserData)->t.size());};
 api.m_getNumVerticesOfFace=[](const SMikkTSpaceContext*,int){return 3;};
 api.m_getPosition=[](const SMikkTSpaceContext* c,float* out,int f,int v){auto& d=*static_cast<Context*>(c->m_pUserData);const auto& p=d.p[d.index(f,v)];out[0]=p.x;out[1]=p.y;out[2]=p.z;};
 api.m_getNormal=[](const SMikkTSpaceContext* c,float* out,int f,int v){auto& d=*static_cast<Context*>(c->m_pUserData);const auto& p=d.n[d.index(f,v)];out[0]=p.x;out[1]=p.y;out[2]=p.z;};
 api.m_getTexCoord=[](const SMikkTSpaceContext* c,float* out,int f,int v){auto& d=*static_cast<Context*>(c->m_pUserData);const auto& p=d.uv[d.index(f,v)];out[0]=p.u;out[1]=p.v;};
 api.m_setTSpaceBasic=[](const SMikkTSpaceContext* c,const float* t,float sign,int f,int v){auto& d=*static_cast<Context*>(c->m_pUserData);d.out[size_t(f)*3+v]={t[0],t[1],t[2],sign};};
 SMikkTSpaceContext context{&api,&data};if(!genTangSpaceDefault(&context))result.clear();return result;
}
