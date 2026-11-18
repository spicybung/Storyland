#include "src/storyland/storyland_sun_flare.h"
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_MAX_DRAW_BUFFERS
#define GL_MAX_DRAW_BUFFERS 0x8824
#endif
#ifndef GL_MAX_TEXTURE_IMAGE_UNITS
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#endif
using AeroDrawBuffersProc=void (APIENTRY*)(GLsizei,const GLenum*);
using AeroBlitProc=void (APIENTRY*)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
static AeroDrawBuffersProc aeroDrawBuffers=nullptr;
static AeroBlitProc aeroBlit=nullptr;
static GLuint aeroBuffer=0,aeroTargets[4]={},aeroDepthBuffer=0,aeroResolve=0;
static GLuint aeroBloomBuffer=0,aeroBloomTargets[2]={},aeroBloomProgram=0;
static int aeroBloomWidth=0,aeroBloomHeight=0;
static bool aeroBloomUnavailable=false;
static int aeroWidth=0,aeroHeight=0;
static GLint aeroDestination=0,aeroViewport[4]={};
static bool aeroDeferredActive=false,aeroDeferredSunRays=false;
static void releaseAeroBloom(){
 if(aeroBloomBuffer&&aeroDeleteFramebuffers)aeroDeleteFramebuffers(1,&aeroBloomBuffer);
 glDeleteTextures(2,aeroBloomTargets);
 if(aeroBloomProgram&&pglDeleteProgram)pglDeleteProgram(aeroBloomProgram);
 aeroBloomBuffer=aeroBloomProgram=0;
 for(auto& target:aeroBloomTargets)target=0;
 aeroBloomWidth=aeroBloomHeight=0;
}
static void destroyAeroDeferred(){
 releaseAeroBloom();aeroBloomUnavailable=false;
 if(aeroBuffer&&aeroDeleteFramebuffers)aeroDeleteFramebuffers(1,&aeroBuffer);
 glDeleteTextures(4,aeroTargets);glDeleteTextures(1,&aeroDepthBuffer);
 if(aeroResolve&&pglDeleteProgram)pglDeleteProgram(aeroResolve);
 aeroBuffer=aeroDepthBuffer=aeroResolve=0;for(auto& t:aeroTargets)t=0;
 aeroWidth=aeroHeight=0;aeroDeferredActive=false;
}
static void aeroFullScreenQuad(){
 glBegin(GL_QUADS);
 glTexCoord2f(0,0);glVertex2f(-1,-1);
 glTexCoord2f(1,0);glVertex2f(1,-1);
 glTexCoord2f(1,1);glVertex2f(1,1);
 glTexCoord2f(0,1);glVertex2f(-1,1);
 glEnd();
}
static bool drawAeroHighDefinitionBloom(){
 if(gAeroQuality!=3){
  if(aeroBloomBuffer||aeroBloomProgram)releaseAeroBloom();
  return false;
 }
 if(aeroBloomUnavailable||aeroWidth<1||aeroHeight<1)return false;
 GLint textureUnits=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&textureUnits);
 if(textureUnits<6)return false;
 if(!aeroBloomProgram){
  std::string error;
  GLuint vertex=compileStoriesShader(GL_VERTEX_SHADER,StorylandShaderSources::deferred_vertex,error);
  GLuint fragment=compileStoriesShader(GL_FRAGMENT_SHADER,StorylandShaderSources::aero_bloom_fragment,error);
  if(!vertex||!fragment){
   if(vertex)pglDeleteShader(vertex);if(fragment)pglDeleteShader(fragment);
   aeroBloomUnavailable=true;return false;
  }
  aeroBloomProgram=pglCreateProgram();
  if(!aeroBloomProgram){
   pglDeleteShader(vertex);pglDeleteShader(fragment);
   aeroBloomUnavailable=true;return false;
  }
  pglAttachShader(aeroBloomProgram,vertex);
  pglAttachShader(aeroBloomProgram,fragment);pglLinkProgram(aeroBloomProgram);
  pglDeleteShader(vertex);pglDeleteShader(fragment);
  GLint linked=0;pglGetProgramiv(aeroBloomProgram,GL_LINK_STATUS,&linked);
  if(!linked){releaseAeroBloom();aeroBloomUnavailable=true;return false;}
 }
 const float resolution=std::min(1.0f,std::min(1920.0f/float(aeroWidth),1080.0f/float(aeroHeight)));
 const int width=std::max(1,int(std::round(float(aeroWidth)*resolution)));
 const int height=std::max(1,int(std::round(float(aeroHeight)*resolution)));
 if(aeroBloomBuffer&&(width!=aeroBloomWidth||height!=aeroBloomHeight)){
  if(aeroDeleteFramebuffers)aeroDeleteFramebuffers(1,&aeroBloomBuffer);
  glDeleteTextures(2,aeroBloomTargets);
  aeroBloomBuffer=0;for(auto& target:aeroBloomTargets)target=0;
 }
 if(!aeroBloomBuffer){
  aeroGenFramebuffers(1,&aeroBloomBuffer);glGenTextures(2,aeroBloomTargets);
  if(!aeroBloomBuffer||!aeroBloomTargets[0]||!aeroBloomTargets[1]){
   releaseAeroBloom();aeroBloomUnavailable=true;return false;
  }
  for(GLuint texture:aeroBloomTargets){
   glBindTexture(GL_TEXTURE_2D,texture);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
  }
  aeroBloomWidth=width;aeroBloomHeight=height;
 }
 pglUseProgram(aeroBloomProgram);
 pglUniform1i(pglGetUniformLocation(aeroBloomProgram,"uSource"),0);
 pglUniform1i(pglGetUniformLocation(aeroBloomProgram,"uPosition"),1);
 const GLint extraction=pglGetUniformLocation(aeroBloomProgram,"uExtract");
 const GLint direction=pglGetUniformLocation(aeroBloomProgram,"uDirection");
 const GLuint sources[]={aeroTargets[0],aeroBloomTargets[0],aeroBloomTargets[1]};
 const GLuint destinations[]={aeroBloomTargets[0],aeroBloomTargets[1],aeroBloomTargets[0]};
 bool complete=true;
 GLint originalDrawBuffer=GL_BACK;glGetIntegerv(GL_DRAW_BUFFER,&originalDrawBuffer);
 for(int pass=0;pass<3;++pass){
  aeroBindFramebuffer(GL_FRAMEBUFFER,aeroBloomBuffer);
  aeroFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,destinations[pass],0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  if(aeroCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){complete=false;break;}
  glViewport(0,0,width,height);
  pglActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,sources[pass]);
  pglActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,aeroTargets[3]);
  pglUniform1i(extraction,pass==0);
  pglUniform2f(direction,pass==1?2.0f/width:0.0f,pass==2?2.0f/height:0.0f);
  aeroFullScreenQuad();
 }
 aeroBindFramebuffer(GL_FRAMEBUFFER,aeroDestination);
 glDrawBuffer(GLenum(originalDrawBuffer));
 glViewport(aeroViewport[0],aeroViewport[1],aeroWidth,aeroHeight);
 if(!complete){releaseAeroBloom();aeroBloomUnavailable=true;}
 return complete;
}
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_STENCIL 0x84F9
#define GL_UNSIGNED_INT_24_8 0x84FA
#endif
static void beginAeroDeferred(bool sunRays=false){
 if(!gAeroEnabled||storylandAeroQuality(gAeroQuality).aoDirections==0||storylandDx::active()||!aeroBindFramebuffer||aeroDeferredActive)return;
 aeroDrawBuffers=reinterpret_cast<AeroDrawBuffersProc>(getOpenGlProcAddress("glDrawBuffers"));
 aeroBlit=reinterpret_cast<AeroBlitProc>(getOpenGlProcAddress("glBlitFramebuffer"));
 GLint maxBuffers=0;glGetIntegerv(GL_MAX_DRAW_BUFFERS,&maxBuffers);
 if(!aeroDrawBuffers||!aeroBlit||maxBuffers<4)return;
 GLint depthBits=0,stencilBits=0;glGetIntegerv(GL_DEPTH_BITS,&depthBits);glGetIntegerv(GL_STENCIL_BITS,&stencilBits);
 if(depthBits!=16&&depthBits!=24)return;
 glGetIntegerv(GL_VIEWPORT,aeroViewport);const int w=aeroViewport[2],h=aeroViewport[3];
 if(w<1||h<1||uint64_t(w)*h>4194304)return;
 if(aeroWidth!=w||aeroHeight!=h)destroyAeroDeferred();
 if(!aeroResolve){
  std::string error;GLuint vs=compileStoriesShader(GL_VERTEX_SHADER,StorylandShaderSources::deferred_vertex,error);
  GLuint fs=compileStoriesShader(GL_FRAGMENT_SHADER,StorylandShaderSources::deferred_fragment,error);
  if(!vs||!fs){if(vs)pglDeleteShader(vs);if(fs)pglDeleteShader(fs);return;}
  aeroResolve=pglCreateProgram();pglAttachShader(aeroResolve,vs);pglAttachShader(aeroResolve,fs);pglLinkProgram(aeroResolve);
  pglDeleteShader(vs);pglDeleteShader(fs);GLint ok=0;pglGetProgramiv(aeroResolve,GL_LINK_STATUS,&ok);
  if(!ok){destroyAeroDeferred();return;}
 }
 glGetIntegerv(GL_FRAMEBUFFER_BINDING,&aeroDestination);
 glPushAttrib(GL_ALL_ATTRIB_BITS);pglActiveTexture(GL_TEXTURE0);
 if(!aeroBuffer){
  aeroGenFramebuffers(1,&aeroBuffer);glGenTextures(4,aeroTargets);glGenTextures(1,&aeroDepthBuffer);
  for(int index=0;index<4;++index){glBindTexture(GL_TEXTURE_2D,aeroTargets[index]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D,0,(index==0||index==2)?GL_RGBA8:GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);}
  glBindTexture(GL_TEXTURE_2D,aeroDepthBuffer);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,stencilBits?GL_DEPTH24_STENCIL8:depthBits==16?GL_DEPTH_COMPONENT16:GL_DEPTH_COMPONENT24,
      w,h,0,stencilBits?GL_DEPTH_STENCIL:GL_DEPTH_COMPONENT,stencilBits?GL_UNSIGNED_INT_24_8:GL_UNSIGNED_INT,nullptr);
  aeroWidth=w;aeroHeight=h;
 }
 aeroBindFramebuffer(GL_FRAMEBUFFER,aeroBuffer);
 for(int i=0;i<4;i++)aeroFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,aeroTargets[i],0);
 aeroFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,aeroDepthBuffer,0);
 GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT0+1,GL_COLOR_ATTACHMENT0+2,GL_COLOR_ATTACHMENT0+3};aeroDrawBuffers(4,buffers);
 if(aeroCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){aeroBindFramebuffer(GL_FRAMEBUFFER,aeroDestination);glPopAttrib();destroyAeroDeferred();return;}
 glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 // Bring the sky and any depth already in the viewport into the opaque pass.
 glDrawBuffer(GL_COLOR_ATTACHMENT0);aeroBindFramebuffer(GL_READ_FRAMEBUFFER,aeroDestination);
 aeroBlit(aeroViewport[0],aeroViewport[1],aeroViewport[0]+w,aeroViewport[1]+h,0,0,w,h,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);
 aeroBindFramebuffer(GL_FRAMEBUFFER,aeroDestination);glPopAttrib();
 aeroBindFramebuffer(GL_FRAMEBUFFER,aeroBuffer);glViewport(0,0,w,h);glDrawBuffer(GL_COLOR_ATTACHMENT0);
 aeroDeferredActive=true;aeroDeferredSunRays=sunRays;
}
static void endAeroDeferred(){
 if(!aeroDeferredActive)return;
 aeroDeferredActive=false;
 glPushAttrib(GL_ALL_ATTRIB_BITS);GLint oldProgram=0;glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);
 // The bloom passes and resolve draw screen quads. Keep the fixed-function
 // matrices independent of the world camera while compositing the frame.
 GLint previousMatrixMode=GL_MODELVIEW;
 glGetIntegerv(GL_MATRIX_MODE,&previousMatrixMode);
 GLfloat worldProjection[16]{};
 glGetFloatv(GL_PROJECTION_MATRIX,worldProjection);
 glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
 glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
 aeroBindFramebuffer(GL_FRAMEBUFFER,aeroDestination);glViewport(aeroViewport[0],aeroViewport[1],aeroWidth,aeroHeight);
 glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 const bool highDefinitionBloom=drawAeroHighDefinitionBloom();
 pglUseProgram(aeroResolve);
 const char* names[]={"sceneTex","normalTex","albedoTex","positionTex"};
 for(int i=0;i<4;i++){pglActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,aeroTargets[i]);pglUniform1i(pglGetUniformLocation(aeroResolve,names[i]),i);}
 aeroPointUniforms(aeroResolve);
 const float worldScale=std::sqrt(gAeroView[0]*gAeroView[0]+gAeroView[1]*gAeroView[1]+gAeroView[2]*gAeroView[2]);
 pglUniform3f(pglGetUniformLocation(aeroResolve,"uAoParameters"),float(aeroWidth),float(aeroHeight),2.5f*worldScale);
 pglUniform1f(pglGetUniformLocation(aeroResolve,"uAoStrength"),gAeroAoStrength);
 pglUniform1f(pglGetUniformLocation(aeroResolve,"uBloomStrength"),storylandAeroQuality(gAeroQuality).bloomStrength);
 pglUniform1i(pglGetUniformLocation(aeroResolve,"uBloomHighDefinition"),highDefinitionBloom?1:0);
 pglUniform1i(pglGetUniformLocation(aeroResolve,"uLightingModel"),storylandAeroQuality(gAeroQuality).lightingModel);
 pglUniform1i(pglGetUniformLocation(aeroResolve,"uAoDirections"),storylandAeroQuality(gAeroQuality).aoDirections);
 // Keep the sky's original sun. The resolve only adds light spilling past geometry.
 pglActiveTexture(GL_TEXTURE0+4);glBindTexture(GL_TEXTURE_2D,aeroDepthBuffer);
 pglUniform1i(pglGetUniformLocation(aeroResolve,"sceneDepth"),4);
 if(highDefinitionBloom){
  pglActiveTexture(GL_TEXTURE0+5);glBindTexture(GL_TEXTURE_2D,aeroBloomTargets[0]);
  pglUniform1i(pglGetUniformLocation(aeroResolve,"bloomTex"),5);
 }else pglUniform1i(pglGetUniformLocation(aeroResolve,"bloomTex"),0);
 const auto& sky=gStoriesSky.state();
 const int samples=aeroDeferredSunRays&&gStoriesSky.isEnabled()&&gPrelightViewMode!=StorylandPrelightViewMode::Raw?
  storylandAeroQuality(gAeroQuality).sunRaySamples:0;
 pglUniform1i(pglGetUniformLocation(aeroResolve,"uSunRaySamples"),samples);
 if(samples){
  const auto sun=storylandSunFlareAt(sky,gAeroView,worldProjection);
  const float visibility=sun.visible?sun.intensity:0.0f;
  pglUniform3f(pglGetUniformLocation(aeroResolve,"uSunScreen"),
      sun.visible?sun.x*.5f+.5f:0.0f,
      sun.visible?sun.y*.5f+.5f:0.0f,visibility);
  pglUniform3f(pglGetUniformLocation(aeroResolve,"uSunColor"),
      sun.color[0],sun.color[1],sun.color[2]);
 }
 pglActiveTexture(GL_TEXTURE0);
 glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(-1,-1);glTexCoord2f(1,0);glVertex2f(1,-1);glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(-1,1);glEnd();
 aeroBindFramebuffer(GL_READ_FRAMEBUFFER,aeroBuffer);aeroBindFramebuffer(GL_DRAW_FRAMEBUFFER,aeroDestination);
 aeroBlit(0,0,aeroWidth,aeroHeight,aeroViewport[0],aeroViewport[1],aeroViewport[0]+aeroWidth,aeroViewport[1]+aeroHeight,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
 aeroBindFramebuffer(GL_FRAMEBUFFER,aeroBuffer);glPopAttrib();
 aeroBindFramebuffer(GL_FRAMEBUFFER,aeroDestination);glViewport(aeroViewport[0],aeroViewport[1],aeroWidth,aeroHeight);
 glMatrixMode(GL_MODELVIEW);glPopMatrix();
 glMatrixMode(GL_PROJECTION);glPopMatrix();
 glMatrixMode(GLenum(previousMatrixMode));
 pglUseProgram(GLuint(oldProgram));
}
