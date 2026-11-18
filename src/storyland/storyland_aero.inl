using PFNGLGENFRAMEBUFFERSPROC=void (APIENTRY*)(GLsizei,GLuint*);
using PFNGLBINDFRAMEBUFFERPROC=void (APIENTRY*)(GLenum,GLuint);
using PFNGLFRAMEBUFFERTEXTURE2DPROC=void (APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC=GLenum (APIENTRY*)(GLenum);
using PFNGLDELETEFRAMEBUFFERSPROC=void (APIENTRY*)(GLsizei,const GLuint*);
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
// Aero shadow-map lifetime is tied to Storyland's OpenGL context.
static GLuint gAeroDepth=0,gAeroFramebuffer=0;
static bool gAeroShadowReady=false;
static int gAeroShadowSize=2048;
static std::vector<StorylandC2dPointLight> gAeroPointLights;
static float gAeroView[16];
static float gAeroLightEye[3]={0,0,1},gAeroUpEye[3]={0,0,1};
static float gAeroSunVisibility=0,gAeroMoonVisibility=0;
static bool gAeroShadowCached=false;
static int gAeroShadowTick=0,gAeroLastShadowTick=-10;
static float gAeroShadowFocus[3]={},gAeroShadowDirection[3]={},gAeroShadowRadius=0;
static float gAeroShadowProjection[16]={},gAeroShadowModelView[16]={};
static PFNGLGENFRAMEBUFFERSPROC aeroGenFramebuffers=nullptr;
static PFNGLBINDFRAMEBUFFERPROC aeroBindFramebuffer=nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC aeroFramebufferTexture2D=nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC aeroCheckFramebufferStatus=nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC aeroDeleteFramebuffers=nullptr;
static void releaseAero(){
 if(gAeroDepth)glDeleteTextures(1,&gAeroDepth);
 if(gAeroFramebuffer&&aeroDeleteFramebuffers)aeroDeleteFramebuffers(1,&gAeroFramebuffer);
 gAeroDepth=gAeroFramebuffer=0;gAeroShadowReady=gAeroShadowCached=false;
 gAeroLastShadowTick=-10;
}
static bool aeroInverse(const float* m,float* out){
 double a[4][8]{};
 for(int r=0;r<4;r++)for(int c=0;c<4;c++){a[r][c]=m[c*4+r];a[r][c+4]=r==c;}
 for(int c=0;c<4;c++){
  int pivot=c;for(int r=c+1;r<4;r++)if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;
  if(std::abs(a[pivot][c])<1e-15)return false;
  for(int k=0;k<8;k++)std::swap(a[c][k],a[pivot][k]);
  double d=a[c][c];for(double& v:a[c])v/=d;
  for(int r=0;r<4;r++)if(r!=c){double f=a[r][c];for(int k=0;k<8;k++)a[r][k]-=f*a[c][k];}
 }
 for(int r=0;r<4;r++)for(int c=0;c<4;c++)out[c*4+r]=float(a[r][c+4]);return true;
}
static bool gAeroNormalsNeedReset=false;
static bool gAeroReleasePending=false;
static void destroyAeroDeferred();
static void clearAeroNormals();

// Keep the point lights in eye space, same as the normals/positions in the buffer.
static void aeroPointUniforms(GLuint program,bool enabled=true){
 const size_t count=enabled?std::min<size_t>(8,gAeroPointLights.size()):0;
 const float scale=std::hypot(gAeroView[0],gAeroView[1],gAeroView[2]);
 pglUniform1i(pglGetUniformLocation(program,"uPointCount"),int(count));
 for(size_t i=0;i<count;i++){
  const auto& light=gAeroPointLights[i];float eye[3];
  for(int k=0;k<3;k++)eye[k]=gAeroView[k]*light.position.x+gAeroView[4+k]*light.position.y+
   gAeroView[8+k]*light.position.z+gAeroView[12+k];
  const std::string suffix="["+std::to_string(i)+"]";
  pglUniform3f(pglGetUniformLocation(program,("uPointPosition"+suffix).c_str()),eye[0],eye[1],eye[2]);
  pglUniform3f(pglGetUniformLocation(program,("uPointColor"+suffix).c_str()),light.color[0],light.color[1],light.color[2]);
  pglUniform3f(pglGetUniformLocation(program,("uPointRange"+suffix).c_str()),light.radius*scale,light.wetReflection?1.0f:0.0f,0);
 }
 pglUniform3f(pglGetUniformLocation(program,"uUpEye"),gAeroUpEye[0],gAeroUpEye[1],gAeroUpEye[2]);
 pglUniform1f(pglGetUniformLocation(program,"uAeroWet"),std::clamp(gStoriesSky.state().rainIntensity,0.f,1.f));
}

// Caller supplies world-space casters, including their placement transforms.
// No frame delay: the same matrix is used by the caster and receiver passes.
template<class Draw> static void prepareAero(float x,float y,float z,float radius,const Draw& draw){
 gAeroShadowReady=false;gAeroSunVisibility=0;gAeroMoonVisibility=0;
 ++gAeroShadowTick;
 if(gAeroReleasePending){
  destroyAeroDeferred();clearAeroNormals();releaseAero();
  gAeroNormalsNeedReset=false;gAeroReleasePending=false;
 }
 if(!gAeroEnabled||storylandDx::active()||!loadStoriesShaderFunctions()||!pglActiveTexture)return;
 if(gAeroNormalsNeedReset){clearAeroNormals();gAeroNormalsNeedReset=false;}
 float view[16],inverse[16];glGetFloatv(GL_MODELVIEW_MATRIX,view);std::copy_n(view,16,gAeroView);if(!aeroInverse(view,inverse))return;
 const auto& sky=gStoriesSky.state();
 float lx=sky.sunDirectionX,ly=sky.sunDirectionY,lz=sky.sunDirectionZ;
 const float solarHeight=lz;
 const float daylight=std::clamp((solarHeight-.025f)/.16f,0.f,1.f);
 gAeroSunVisibility=daylight*daylight*(3.0f-2.0f*daylight)*
     (1.0f-.55f*std::clamp(sky.rainIntensity,0.0f,1.0f));
 if(gAeroQuality>=3&&gAeroSunVisibility<=.001f&&sky.moonDirectionZ>.035f){
     const float moonHeight=std::clamp((sky.moonDirectionZ-.035f)/.20f,0.0f,1.0f);
     gAeroMoonVisibility=moonHeight*moonHeight*(3.0f-2.0f*moonHeight)*
         (1.0f-.6f*std::clamp(sky.rainIntensity,0.0f,1.0f));
     lx=sky.moonDirectionX;ly=sky.moonDirectionY;lz=sky.moonDirectionZ;
 }
 float len=std::sqrt(lx*lx+ly*ly+lz*lz);if(len<1e-6f)return;
 lx/=len;ly/=len;lz/=len;
 for(int i=0;i<3;i++){
    gAeroLightEye[i]=view[i]*lx+view[4+i]*ly+view[8+i]*lz;
    gAeroUpEye[i]=view[8+i];
 }
 if(!aeroGenFramebuffers){
  aeroGenFramebuffers=reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(getOpenGlProcAddress("glGenFramebuffers"));
  aeroBindFramebuffer=reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(getOpenGlProcAddress("glBindFramebuffer"));
  aeroFramebufferTexture2D=reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(getOpenGlProcAddress("glFramebufferTexture2D"));
  aeroCheckFramebufferStatus=reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(getOpenGlProcAddress("glCheckFramebufferStatus"));
  aeroDeleteFramebuffers=reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(getOpenGlProcAddress("glDeleteFramebuffers"));
 }
 if(!aeroGenFramebuffers||!aeroBindFramebuffer||!aeroFramebufferTexture2D||!aeroCheckFramebufferStatus||!aeroDeleteFramebuffers)return;
 if(gAeroSunVisibility<=.001f&&gAeroMoonVisibility<=.001f)return;
 const auto& quality=storylandAeroQuality(gAeroQuality);
 const float movement=std::hypot(std::hypot(x-gAeroShadowFocus[0],y-gAeroShadowFocus[1]),
                                 z-gAeroShadowFocus[2]);
 const float lightDelta=std::abs(lx-gAeroShadowDirection[0])+
                        std::abs(ly-gAeroShadowDirection[1])+
                        std::abs(lz-gAeroShadowDirection[2]);
 const bool reuse=gAeroShadowCached&&gAeroDepth!=0&&
     gAeroShadowSize==quality.shadowSize&&
     (gAeroShadowTick-gAeroLastShadowTick)<quality.shadowRefreshInterval&&
     movement<2.0f&&lightDelta<.002f&&std::abs(radius-gAeroShadowRadius)<.001f;
 GLint previousFramebuffer=0,previousProgram=0,active=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previousFramebuffer);
 glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
 if(reuse){
     GLint previousMatrix=GL_MODELVIEW;
     glGetIntegerv(GL_MATRIX_MODE,&previousMatrix);
     pglActiveTexture(GL_TEXTURE0+2);
     glMatrixMode(GL_TEXTURE);glLoadMatrixf(gAeroShadowProjection);
     glMultMatrixf(gAeroShadowModelView);glMultMatrixf(inverse);
     glMatrixMode(GLenum(previousMatrix));pglActiveTexture(GLenum(active));
     gAeroShadowReady=true;
     return;
 }
 glPushAttrib(GL_ALL_ATTRIB_BITS);pglActiveTexture(GL_TEXTURE0);pglUseProgram(0);
 GLint maximumShadowSize=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximumShadowSize);
 const int desiredShadowSize=std::min(storylandAeroQuality(gAeroQuality).shadowSize,std::max(1,maximumShadowSize));
 if(gAeroDepth&&gAeroShadowSize!=desiredShadowSize)releaseAero();
 gAeroShadowSize=desiredShadowSize;
 if(!gAeroDepth){
  glGenTextures(1,&gAeroDepth);glBindTexture(GL_TEXTURE_2D,gAeroDepth);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,gAeroShadowSize,gAeroShadowSize,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
  aeroGenFramebuffers(1,&gAeroFramebuffer);
 }
 aeroBindFramebuffer(GL_FRAMEBUFFER,gAeroFramebuffer);
 aeroFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,gAeroDepth,0);
 glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
 if(aeroCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE){
  glViewport(0,0,gAeroShadowSize,gAeroShadowSize);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);glDisable(GL_FOG);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
  glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);
  radius=std::max(radius,1.0f);
  float rx=ly,ry=-lx,rz=0;len=std::sqrt(rx*rx+ry*ry);
  if(len<.001f){rx=1;ry=0;len=1;}rx/=len;ry/=len;
  const float ux=ly*rz-lz*ry,uy=lz*rx-lx*rz,uz=lx*ry-ly*rx;
  // Anchor the sun projection to its texel grid. Sub-texel camera movement
  // otherwise makes the same stationary edge alternate between shadow samples.
  const auto stable=storylandAeroShadowCenter({x,y,z},{rx,ry,rz},{ux,uy,uz},radius,gAeroShadowSize);
  x=stable[0];y=stable[1];z=stable[2];
  float lightView[16]={rx,ux,lx,0,ry,uy,ly,0,rz,uz,lz,0,0,0,0,1};
  glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(-radius,radius,-radius,radius,-radius*4,radius*4);
  glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadMatrixf(lightView);glTranslatef(-x,-y,-z);
  float projection[16],light[16];glGetFloatv(GL_PROJECTION_MATRIX,projection);glGetFloatv(GL_MODELVIEW_MATRIX,light);
  std::copy_n(projection,16,gAeroShadowProjection);
  std::copy_n(light,16,gAeroShadowModelView);
  pglActiveTexture(GL_TEXTURE0+2);glMatrixMode(GL_TEXTURE);glLoadMatrixf(projection);glMultMatrixf(light);glMultMatrixf(inverse);
  pglActiveTexture(GL_TEXTURE0);glMatrixMode(GL_MODELVIEW);
  draw();
  glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
  gAeroShadowReady=true;
  gAeroShadowCached=true;
  gAeroLastShadowTick=gAeroShadowTick;
  gAeroShadowFocus[0]=x;gAeroShadowFocus[1]=y;gAeroShadowFocus[2]=z;
  gAeroShadowDirection[0]=lx;gAeroShadowDirection[1]=ly;gAeroShadowDirection[2]=lz;
  gAeroShadowRadius=radius;
 }
 aeroBindFramebuffer(GL_FRAMEBUFFER,GLuint(previousFramebuffer));glPopAttrib();
 pglActiveTexture(GLenum(active));pglUseProgram(GLuint(previousProgram));
}

#include "src/graphics/storyland_aero_normals.h"
// Bounded, content-keyed generated normals; texture-name reuse cannot serve stale maps.
static std::map<uint64_t,GLuint> gAeroNormals;
struct StorylandNormalFastEntry {
 uint64_t hash=0,signature=0;
 size_t bytes=0;
 unsigned width=0,height=0;
 GLuint texture=0;
 std::string name;
};
static std::unordered_map<const uint8_t*,StorylandNormalFastEntry> gAeroNormalFastCache;
static uint64_t gAeroNormalGeneration=0;
static size_t gAeroNormalBytes=0;
static void clearAeroNormals(){
 for(const auto& e:gAeroNormals)glDeleteTextures(1,&e.second);
 gAeroNormals.clear();gAeroNormalFastCache.clear();gAeroNormalBytes=0;
 ++gAeroNormalGeneration;
}
static GLuint aeroNormalTexture(const std::vector<uint8_t>& rgba,unsigned w,unsigned h,const std::string& name){
 if(!gAeroEnabled||!storylandAeroQuality(gAeroQuality).normalMaps||rgba.empty()||!w||!h||w>1024||h>1024)return 0;
 uint64_t signature=1469598103934665603ull;
 for(size_t sample=0;sample<16;++sample){
  const size_t offset=(sample*(rgba.size()-1))/15;
  signature^=rgba[offset];signature*=1099511628211ull;
 }
 auto fast=gAeroNormalFastCache.find(rgba.data());
 if(fast!=gAeroNormalFastCache.end()&&fast->second.signature==signature&&
    fast->second.bytes==rgba.size()&&fast->second.width==w&&fast->second.height==h&&
    fast->second.name==name)return fast->second.texture;
 uint64_t hash=1469598103934665603ull;
 for(uint8_t b:rgba){hash^=b;hash*=1099511628211ull;}
 for(unsigned char b:name){hash^=b;hash*=1099511628211ull;}
 hash^=uint64_t(w)<<32|h;
 auto known=gAeroNormals.find(hash);
 if(known!=gAeroNormals.end()){
  gAeroNormalFastCache[rgba.data()]={hash,signature,rgba.size(),w,h,known->second,name};
  return known->second;
 }
 size_t mipBytes=0;
 for(unsigned mw=w,mh=h;;mw=std::max(1u,mw/2),mh=std::max(1u,mh/2)) {
  mipBytes+=size_t(mw)*mh*4;if(mw==1&&mh==1)break;
 }
 if(gAeroNormalBytes+mipBytes>64u*1024u*1024u)return 0;
 std::vector<uint8_t> normal;
 if(!storyland_aero::makeNormals(rgba,w,h,name.c_str(),normal))return 0;
 if(gAeroNormals.size()>=128)return 0;
 GLuint id=0;glGenTextures(1,&id);pglActiveTexture(GL_TEXTURE0+3);glBindTexture(GL_TEXTURE_2D,id);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,normal.data());
 using GenerateMipmaps=void (APIENTRY*)(GLenum);
 const auto mipmaps=reinterpret_cast<GenerateMipmaps>(getOpenGlProcAddress("glGenerateMipmap"));
 if(mipmaps){mipmaps(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);}
 pglActiveTexture(GL_TEXTURE0);gAeroNormals[hash]=id;
 gAeroNormalFastCache[rgba.data()]={hash,signature,rgba.size(),w,h,id,name};
 gAeroNormalBytes+=mipBytes;return id;
}
static void bindAeroNormal(GLuint texture){
 if(!gAeroEnabled||!pglActiveTexture)return;
 pglActiveTexture(GL_TEXTURE0+3);glBindTexture(GL_TEXTURE_2D,texture);pglActiveTexture(GL_TEXTURE0);
 pglUniform1i(pglGetUniformLocation(gStoriesShaderProgram,"uNormalMap"),3);
 pglUniform1i(pglGetUniformLocation(gStoriesShaderProgram,"uHasNormalMap"),texture?1:0);
}
