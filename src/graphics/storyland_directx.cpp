#define NOMINMAX
#define STORYLAND_DIRECTX_IMPLEMENTATION
#include "src/graphics/storyland_directx.h"
#include "src/graphics/storyland_directx_depth.h"
#include "src/graphics/storyland_vehicle_environment.h"
#include "src/storyland/storyland_screen_droplets.h"
#include "src/graphics/storyland_texture_upload.h"
#include "src/graphics/storyland_underwater.h"
#include "src/re/reVCS/render/storyland_vcs_psp_lighting.h"
#include "src/shaders/storyland_aero_water_dx.h"
#include "src/re/reVCS/shaders/storyland_vcs_psp_dx.h"
#include "src/shaders/storyland_ps2_dx.h"
#include "src/shaders/storyland_ps2_postfx_dx.h"
#include "src/shaders/storyland_fxaa_dx.h"
#include <d3d9.h>
#include <d3dcompiler.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>

namespace storylandDx {
namespace {
template<class T> void release(T*& p) { if(p) p->Release(); p=nullptr; }
using Matrix=std::array<float,16>;
Matrix identity() { return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; }
Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix r{};
    for(int c=0;c<4;++c) for(int row=0;row<4;++row)
        for(int k=0;k<4;++k) r[c*4+row]+=a[k*4+row]*b[c*4+k];
    return r;
}
struct Vertex { float x=0,y=0,z=0,nx=0,ny=0,nz=1; DWORD color=0xffffffff; float u=0,v=0,u1=0,v1=0; };
constexpr DWORD vertexFormat=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX2;
struct Pointer { GLint size=0; GLenum type=GL_FLOAT; GLsizei stride=0; const unsigned char* data=nullptr; };
struct Client { Pointer position,color,normal,uv; std::set<GLenum> enabled; int alignment=4,rowLength=0; } client;
struct State {
    std::set<GLenum> enabled;
    GLenum matrixMode=GL_MODELVIEW,depthFunc=GL_LESS,src=GL_ONE,dst=GL_ZERO,alphaFunc=GL_ALWAYS;
    GLenum polygon=GL_FILL,cull=GL_BACK,frontFace=GL_CCW,shade=GL_SMOOTH,texEnv=GL_MODULATE,fogMode=GL_EXP;
    bool depthWrite=true; float alphaRef=0,point=1,line=1,offset=0,offsetFactor=0,fogStart=0,fogEnd=1;
    std::array<int,4> viewport{0,0,1,1},scissor{0,0,1,1};
    std::array<float,4> clear{0,0,0,0},fog{0,0,0,0},ambient{.2f,.2f,.2f,1},lightDiffuse{1,1,1,1},lightPosition{0,0,1,0};
    // Four-coordinate arrays contain the base and secondary UV sets.
    std::array<float,4> textureCoordinate{0,0,0,1};
    GLuint texture=0,listBase=0; Vertex vertex; float rasterX=0,rasterY=0,rasterZ=0;
} state;
struct Texture {
    IDirect3DTexture9* object=nullptr;
    GLint minFilter=GL_LINEAR,magFilter=GL_LINEAR,wrapS=GL_REPEAT,wrapT=GL_REPEAT;
    GLint maximumLevel=0;
};
D3DTEXTUREFILTERTYPE textureMinificationFilter(const Texture& texture) {
    return texture.minFilter==GL_NEAREST||texture.minFilter==GL_NEAREST_MIPMAP_NEAREST||
        texture.minFilter==GL_NEAREST_MIPMAP_LINEAR?D3DTEXF_POINT:D3DTEXF_LINEAR;
}
D3DTEXTUREFILTERTYPE textureMipFilter(const Texture& texture) {
    if(!texture.object||texture.object->GetLevelCount()<=1)return D3DTEXF_NONE;
    if(texture.minFilter==GL_NEAREST_MIPMAP_NEAREST||texture.minFilter==GL_LINEAR_MIPMAP_NEAREST)
        return D3DTEXF_POINT;
    if(texture.minFilter==GL_NEAREST_MIPMAP_LINEAR||texture.minFilter==GL_LINEAR_MIPMAP_LINEAR)
        return D3DTEXF_LINEAR;
    return D3DTEXF_NONE;
}
struct Glyph { GLuint texture=0; int width=0,height=0,advance=0,originX=0,originY=0; };
struct Shader {
    IDirect3DVertexShader9* vertex=nullptr;
    IDirect3DPixelShader9* pixel=nullptr;
    bool attempted=false,psp=false,ps2=false;
};
IDirect3D9* api=nullptr;
IDirect3DDevice9* device=nullptr;
IDirect3DTexture9* blurHistory=nullptr;
bool blurHistoryReady=false;
IDirect3DTexture9* screenDropletCapture=nullptr;
IDirect3DTexture9* screenDropletMask=nullptr;
StorylandScreenDropletPlayback screenDropletPlayback;
StorylandUnderwaterFrame underwaterFrame;
IDirect3DTexture9* underwaterSceneTexture=nullptr;
IDirect3DPixelShader9* underwaterRippleShader=nullptr;
std::array<IDirect3DTexture9*,4> ps2PostEffectsTextures{};
IDirect3DPixelShader9* ps2PostEffectsShader=nullptr;
void releasePs2PostEffects() {
    for(auto& texture:ps2PostEffectsTextures)release(texture);
    release(ps2PostEffectsShader);
}
void releaseUnderwater() {
    release(underwaterSceneTexture);release(underwaterRippleShader);
}
D3DCAPS9 deviceCapabilities{};
StorylandGraphicsQualitySettings graphicsQuality;
bool graphicsQualityTarget=false;
IDirect3DTexture9* fxaaSceneTexture=nullptr;
IDirect3DPixelShader9* fxaaPixelShader=nullptr;
void releaseFxaa() {release(fxaaSceneTexture);release(fxaaPixelShader);}
D3DPRESENT_PARAMETERS presentParameters{};
HWND window=nullptr;
HDC currentDeviceContext=nullptr;
bool chosen=false,current=false,inScene=false;
GLenum lastError=GL_NO_ERROR,primitive=GL_TRIANGLES;
std::string diagnostic;
std::vector<Vertex> vertices,expanded;
std::vector<State> attributes;
std::vector<Client> clients;
std::array<std::vector<Matrix>,3> matrices;
std::map<GLuint,Texture> textures;
std::map<GLuint,Glyph> glyphs;
std::array<Shader,10> shaders;
std::array<Shader,5> pspShaders;
std::array<Shader,10> ps2Shaders;
Shader aeroWaterShader;
IDirect3DTexture9* waterReactionTexture=nullptr;
std::array<float,40> aeroWaterConstants{};
Shader* surface=nullptr;
bool surfaceUsesTexture=false;
std::array<float,140> constants{};
GLuint surfaceDualTexture=0;
GLuint environmentTexture=0;
IDirect3DSurface9* environmentDepth=nullptr;
D3DFORMAT environmentDepthFormat=D3DFMT_UNKNOWN;
IDirect3DSurface9* environmentPreviousColor=nullptr;
IDirect3DSurface9* environmentPreviousDepth=nullptr;
D3DVIEWPORT9 environmentPreviousViewport{};
int environmentFaceSize=0;
int renderTargetWidth=0,renderTargetHeight=0;
bool environmentCaptureActive=false;
GLuint nextTexture=1,nextList=1;
D3DFORMAT currentDepthFormat() {
    return environmentCaptureActive?environmentDepthFormat:presentParameters.AutoDepthStencilFormat;
}
int currentDepthBits() {
    const auto format=currentDepthFormat();
    return format==D3DFMT_D16?16:(format==D3DFMT_D24S8||format==D3DFMT_D24X8)?24:0;
}
D3DFORMAT nativeDepthFormat(StorylandDirectXDepthFormat format) {
    switch(format) {
    case StorylandDirectXDepthFormat::Depth24Stencil8:return D3DFMT_D24S8;
    case StorylandDirectXDepthFormat::Depth24:return D3DFMT_D24X8;
    case StorylandDirectXDepthFormat::Depth16:return D3DFMT_D16;
    }
    return D3DFMT_UNKNOWN;
}
bool currentDepthHasStencil() {
    const auto format=currentDepthFormat();
    for(const auto candidate:storylandDirectXDepthFormats)
        if(nativeDepthFormat(candidate)==format)return storylandDirectXDepthHasStencil(candidate);
    return false;
}
HGLRC contextHandle() { return reinterpret_cast<HGLRC>(device); }
Matrix& matrix() { return matrices[state.matrixMode==GL_PROJECTION?1:state.matrixMode==GL_TEXTURE?2:0].back(); }
bool enabled(GLenum cap) { return state.enabled.count(cap)!=0; }
DWORD bits(float value) { DWORD r; std::memcpy(&r,&value,4);return r; }
DWORD color(float r,float g,float b,float a=1) {
    auto byte=[](float x){return DWORD(std::clamp(x,0.f,1.f)*255.f+.5f);};
    return D3DCOLOR_ARGB(byte(a),byte(r),byte(g),byte(b));
}
D3DCMPFUNC comparison(GLenum v) { return D3DCMPFUNC(D3DCMP_NEVER+std::clamp(int(v)-int(GL_NEVER),0,7)); }
D3DCULL cullMode(GLenum face,GLenum frontFace) {
    // The Direct3D viewport reverses the OpenGL window Y direction.
    const bool clockwise=(face==GL_FRONT)==(frontFace==GL_CCW);
    return clockwise?D3DCULL_CW:D3DCULL_CCW;
}
D3DBLEND blend(GLenum v) {
    switch(v) {case GL_ZERO:return D3DBLEND_ZERO;case GL_ONE:return D3DBLEND_ONE;
    case GL_SRC_ALPHA:return D3DBLEND_SRCALPHA;case GL_ONE_MINUS_SRC_ALPHA:return D3DBLEND_INVSRCALPHA;
    case GL_DST_ALPHA:return D3DBLEND_DESTALPHA;case GL_ONE_MINUS_DST_ALPHA:return D3DBLEND_INVDESTALPHA;
    case GL_SRC_COLOR:return D3DBLEND_SRCCOLOR;case GL_ONE_MINUS_SRC_COLOR:return D3DBLEND_INVSRCCOLOR;
    case GL_DST_COLOR:return D3DBLEND_DESTCOLOR;case GL_ONE_MINUS_DST_COLOR:return D3DBLEND_INVDESTCOLOR;
    default:return D3DBLEND_ONE;}
}
void check(HRESULT hr,const char* operation) {
    if(FAILED(hr)) { diagnostic=std::string(operation)+" failed (HRESULT "+std::to_string(static_cast<unsigned long>(hr))+")"; throw std::runtime_error(diagnostic); }
}
void endScene() { if(inScene) {device->EndScene();inScene=false;} }
bool ready() {
    if(!device) return false;
    HRESULT hr=device->TestCooperativeLevel();
    if(hr==D3DERR_DEVICELOST) return false;
    RECT rect{};GetClientRect(window,&rect);
    const UINT w=UINT(std::max(1L,rect.right)),h=UINT(std::max(1L,rect.bottom));
    if(hr==D3DERR_DEVICENOTRESET||w!=presentParameters.BackBufferWidth||h!=presentParameters.BackBufferHeight) {
        endScene();
        device->SetTexture(0,nullptr);
        device->SetTexture(1,nullptr);
        releaseVehicleEnvironmentCapture();
        release(blurHistory);blurHistoryReady=false;
        release(screenDropletCapture);
        releaseUnderwater();
        releasePs2PostEffects();
        releaseFxaa();
        presentParameters.BackBufferWidth=w;presentParameters.BackBufferHeight=h;
        // Authored textures use D3DPOOL_MANAGED; release live render targets before Reset.
        hr=device->Reset(&presentParameters);
        if(hr==D3DERR_DEVICELOST) return false;
        check(hr,"Direct3D Reset");
    } else check(hr,"Direct3D device status");
    if(!inScene) {check(device->BeginScene(),"Direct3D BeginScene");inScene=true;}
    return true;
}
Matrix projection() {
    Matrix p=matrices[1].back();
    // OpenGL clip Z [-W,W] -> Direct3D [0,W], plus the D3D9 pixel-center correction.
    for(int col=0;col<4;++col) {
        p[col*4+2]=(p[col*4+2]+p[col*4+3])*.5f;
        p[col*4]-=p[col*4+3]/float(std::max(1,state.viewport[2]));
        p[col*4+1]+=p[col*4+3]/float(std::max(1,state.viewport[3]));
    }
    return p;
}
void applyState() {
    const auto& v=state.viewport;
    const int bw=renderTargetWidth?renderTargetWidth:int(presentParameters.BackBufferWidth);
    const int bh=renderTargetHeight?renderTargetHeight:int(presentParameters.BackBufferHeight);
    const int left=std::clamp(v[0],0,bw-1),top=std::clamp(bh-v[1]-v[3],0,bh-1);
    D3DVIEWPORT9 viewport{DWORD(left),DWORD(top),DWORD(std::clamp(v[2],1,bw-left)),DWORD(std::clamp(v[3],1,bh-top)),0,1};
    device->SetViewport(&viewport);
    RECT scissor{std::clamp(state.scissor[0],0,bw),std::clamp(bh-state.scissor[1]-state.scissor[3],0,bh),
        std::clamp(state.scissor[0]+state.scissor[2],0,bw),std::clamp(bh-state.scissor[1],0,bh)};
    device->SetScissorRect(&scissor);
    auto rs=[](D3DRENDERSTATETYPE k,DWORD value){device->SetRenderState(k,value);};
    rs(D3DRS_MULTISAMPLEANTIALIAS,enabled(0x809D));
    rs(D3DRS_SCISSORTESTENABLE,enabled(GL_SCISSOR_TEST));
    rs(D3DRS_ZENABLE,enabled(GL_DEPTH_TEST));rs(D3DRS_ZWRITEENABLE,state.depthWrite);rs(D3DRS_ZFUNC,comparison(state.depthFunc));
    rs(D3DRS_ALPHABLENDENABLE,enabled(GL_BLEND));rs(D3DRS_SRCBLEND,blend(state.src));rs(D3DRS_DESTBLEND,blend(state.dst));
    rs(D3DRS_ALPHATESTENABLE,enabled(GL_ALPHA_TEST));rs(D3DRS_ALPHAFUNC,comparison(state.alphaFunc));rs(D3DRS_ALPHAREF,DWORD(std::clamp(state.alphaRef,0.f,1.f)*255.f));
    rs(D3DRS_CULLMODE,enabled(GL_CULL_FACE)?cullMode(state.cull,state.frontFace):D3DCULL_NONE);
    rs(D3DRS_FILLMODE,state.polygon==GL_LINE?D3DFILL_WIREFRAME:state.polygon==GL_POINT?D3DFILL_POINT:D3DFILL_SOLID);
    rs(D3DRS_SHADEMODE,state.shade==GL_FLAT?D3DSHADE_FLAT:D3DSHADE_GOURAUD);
    rs(D3DRS_POINTSIZE,bits(state.point));
    rs(D3DRS_DEPTHBIAS,bits(enabled(GL_POLYGON_OFFSET_FILL)?
        storylandDirectXDepthBias(state.offset,currentDepthBits()==16):0));
    rs(D3DRS_SLOPESCALEDEPTHBIAS,bits(enabled(GL_POLYGON_OFFSET_FILL)?state.offsetFactor:0));
    const bool fixedLighting=!surface&&enabled(GL_LIGHTING);
    rs(D3DRS_LIGHTING,fixedLighting);
    if(fixedLighting) {
        rs(D3DRS_NORMALIZENORMALS,enabled(GL_NORMALIZE));
        rs(D3DRS_COLORVERTEX,TRUE);rs(D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_COLOR1);
        rs(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_COLOR1);
        rs(D3DRS_AMBIENT,color(state.ambient[0],state.ambient[1],state.ambient[2]));
        D3DMATERIAL9 material{};material.Diffuse={1,1,1,1};material.Ambient={1,1,1,1};device->SetMaterial(&material);
        D3DLIGHT9 light{};light.Type=D3DLIGHT_DIRECTIONAL;
        light.Direction={-state.lightPosition[0],-state.lightPosition[1],-state.lightPosition[2]};
        light.Diffuse={state.lightDiffuse[0],state.lightDiffuse[1],state.lightDiffuse[2],1};
        device->SetLight(0,&light);device->LightEnable(0,enabled(GL_LIGHT0));
    }
    const bool fixedFog=!surface&&enabled(GL_FOG);
    rs(D3DRS_FOGENABLE,fixedFog);
    if(fixedFog) {
        rs(D3DRS_FOGTABLEMODE,D3DFOG_NONE);
        rs(D3DRS_FOGVERTEXMODE,state.fogMode==GL_LINEAR?D3DFOG_LINEAR:state.fogMode==GL_EXP2?D3DFOG_EXP2:D3DFOG_EXP);
        const float immersion=underwaterFrame.enabled&&!environmentCaptureActive?underwaterFrame.immersion:0.0f;
        const float start=state.fogStart+(std::min(state.fogStart,5.0f)-state.fogStart)*immersion;
        const float end=state.fogEnd+(std::min(state.fogEnd,90.0f)-state.fogEnd)*immersion;
        std::array<float,3> fog{};
        for(unsigned axis=0;axis<3;++axis)fog[axis]=state.fog[axis]+(underwaterFrame.fogColor[axis]-state.fog[axis])*immersion;
        rs(D3DRS_RANGEFOGENABLE,TRUE);rs(D3DRS_FOGSTART,bits(start));rs(D3DRS_FOGEND,bits(end));
        rs(D3DRS_FOGCOLOR,color(fog[0],fog[1],fog[2]));
    }
    Matrix p=projection(),mv=matrices[0].back();
    if(!surface) {
        const Matrix unit=identity();
        device->SetTransform(D3DTS_WORLD,reinterpret_cast<const D3DMATRIX*>(mv.data()));
        device->SetTransform(D3DTS_VIEW,reinterpret_cast<const D3DMATRIX*>(unit.data()));
        device->SetTransform(D3DTS_PROJECTION,reinterpret_cast<const D3DMATRIX*>(p.data()));
    }
    auto it=textures.find(state.texture);
    const bool textured=enabled(GL_TEXTURE_2D)&&it!=textures.end()&&it->second.object;
    device->SetTexture(0,textured?it->second.object:nullptr);
    if(textured) {
        auto& t=it->second;
        const bool qualityWorld=graphicsQualityTarget&&surface&&!surface->psp&&surface!=&aeroWaterShader;
        const DWORD anisotropy=qualityWorld?DWORD(storylandRequestedAnisotropy(graphicsQuality,true,maximumAnisotropy())):1;
        device->SetSamplerState(0,D3DSAMP_MINFILTER,qualityWorld?(anisotropy>1?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR):textureMinificationFilter(t));
        device->SetSamplerState(0,D3DSAMP_MAGFILTER,qualityWorld?D3DTEXF_LINEAR:(t.magFilter==GL_NEAREST?D3DTEXF_POINT:D3DTEXF_LINEAR));
        device->SetSamplerState(0,D3DSAMP_MIPFILTER,qualityWorld?(t.object->GetLevelCount()>1?D3DTEXF_LINEAR:D3DTEXF_NONE):textureMipFilter(t));
        device->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,anisotropy);
        device->SetSamplerState(0,D3DSAMP_ADDRESSU,t.wrapS==GL_REPEAT?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP);
        device->SetSamplerState(0,D3DSAMP_ADDRESSV,t.wrapT==GL_REPEAT?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP);
    }
    if(!surface) {
        device->SetTextureStageState(0,D3DTSS_COLOROP,textured?(state.texEnv==GL_REPLACE?D3DTOP_SELECTARG1:D3DTOP_MODULATE):D3DTOP_SELECTARG2);
        device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,textured?(state.texEnv==GL_REPLACE?D3DTOP_SELECTARG1:D3DTOP_MODULATE):D3DTOP_SELECTARG2);
        device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    }
    device->SetFVF(vertexFormat);
    const auto secondary=textures.find(surfaceDualTexture);
    IDirect3DTexture9* second=surface&&secondary!=textures.end()?secondary->second.object:nullptr;
    if(surface==&aeroWaterShader)second=waterReactionTexture;
    device->SetTexture(1,second);
    if(second) {
        const bool pspSecondary=surface&&surface->psp&&secondary!=textures.end();
        const bool qualitySecondary=graphicsQualityTarget&&surface&&!surface->psp&&
            surface!=&aeroWaterShader&&secondary!=textures.end()&&surfaceDualTexture!=environmentTexture;
        const DWORD anisotropy=qualitySecondary?DWORD(storylandRequestedAnisotropy(
            graphicsQuality,true,maximumAnisotropy())):1;
        device->SetSamplerState(1,D3DSAMP_MINFILTER,qualitySecondary?
            (anisotropy>1?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR):
            pspSecondary?textureMinificationFilter(secondary->second):D3DTEXF_LINEAR);
        device->SetSamplerState(1,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        device->SetSamplerState(1,D3DSAMP_MIPFILTER,qualitySecondary?
            (second->GetLevelCount()>1?D3DTEXF_LINEAR:D3DTEXF_NONE):
            pspSecondary?textureMipFilter(secondary->second):D3DTEXF_NONE);
        device->SetSamplerState(1,D3DSAMP_MAXANISOTROPY,anisotropy);
        const bool clampSecondary=surface==&aeroWaterShader||
            (constants[56]==2.0f&&(!surface||!surface->ps2))||constants[56]==4.0f;
        const bool repeatU=!clampSecondary&&secondary!=textures.end()&&secondary->second.wrapS==GL_REPEAT;
        const bool repeatV=!clampSecondary&&secondary!=textures.end()&&secondary->second.wrapT==GL_REPEAT;
        device->SetSamplerState(1,D3DSAMP_ADDRESSU,repeatU?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP);
        device->SetSamplerState(1,D3DSAMP_ADDRESSV,repeatV?D3DTADDRESS_WRAP:D3DTADDRESS_CLAMP);
    }
    device->SetVertexShader(surface?surface->vertex:nullptr);device->SetPixelShader(surface?surface->pixel:nullptr);
    if(surface) {
        Matrix mvp=multiply(p,mv);
        std::copy(mv.begin(),mv.end(),constants.begin());std::copy(mvp.begin(),mvp.end(),constants.begin()+16);
        constants[46]=textured&&surfaceUsesTexture?1.f:0.f;constants[50]=state.alphaRef;constants[51]=enabled(GL_ALPHA_TEST)?1.f:0.f;
        if(surface->psp) {
            D3DSURFACE_DESC description{};
            constants[76]=constants[77]=constants[78]=0.0f;
            if(textured&&surfaceUsesTexture&&SUCCEEDED(it->second.object->GetLevelDesc(0,&description))) {
                constants[76]=float(description.Width);
                constants[77]=float(description.Height);
                constants[78]=float(it->second.object->GetLevelCount()-1);
            }
            constants[116]=constants[117]=0.0f;
            if(second&&SUCCEEDED(second->GetLevelDesc(0,&description))) {
                constants[116]=float(description.Width);
                constants[117]=float(description.Height);
            }
        }
        const UINT constantCount=surface->ps2?35:surface->psp?30:19;
        device->SetVertexShaderConstantF(0,constants.data(),constantCount);
        device->SetPixelShaderConstantF(0,constants.data(),constantCount);
        if(surface==&aeroWaterShader) {
            device->SetVertexShaderConstantF(14,aeroWaterConstants.data(),10);
            device->SetPixelShaderConstantF(14,aeroWaterConstants.data(),10);
        }
    }
}
void submit(GLenum mode,const std::vector<Vertex>& input) {
    // Clear or the first draw opens the scene; present closes it for the next frame's device check.
    if(input.empty()||(!inScene&&!ready()))return;
    const std::vector<Vertex>* data=&input;expanded.clear();
    D3DPRIMITIVETYPE topology=D3DPT_TRIANGLELIST;UINT count=0;
    if(mode==GL_QUADS) {
        for(size_t i=0;i+3<input.size();i+=4) for(int k:{0,1,2,0,2,3}) expanded.push_back(input[i+k]);
        data=&expanded;count=UINT(expanded.size()/3);
    } else if(mode==GL_QUAD_STRIP) {
        for(size_t i=0;i+3<input.size();i+=2) for(int k:{0,1,3,0,3,2}) expanded.push_back(input[i+k]);
        data=&expanded;count=UINT(expanded.size()/3);
    } else if(mode==GL_LINE_LOOP) {
        expanded=input;expanded.push_back(input.front());data=&expanded;topology=D3DPT_LINESTRIP;count=UINT(input.size());
    } else if(mode==GL_TRIANGLES)count=UINT(input.size()/3);
    else if(mode==GL_TRIANGLE_STRIP){topology=D3DPT_TRIANGLESTRIP;count=input.size()>2?UINT(input.size()-2):0;}
    else if(mode==GL_TRIANGLE_FAN||mode==GL_POLYGON){topology=D3DPT_TRIANGLEFAN;count=input.size()>2?UINT(input.size()-2):0;}
    else if(mode==GL_LINES){topology=D3DPT_LINELIST;count=UINT(input.size()/2);}
    else if(mode==GL_LINE_STRIP){topology=D3DPT_LINESTRIP;count=input.size()>1?UINT(input.size()-1):0;}
    else if(mode==GL_POINTS){topology=D3DPT_POINTLIST;count=UINT(input.size());}
    else {lastError=GL_INVALID_ENUM;return;}
    if(!count)return;
    applyState();
    check(device->DrawPrimitiveUP(topology,count,data->data(),sizeof(Vertex)),"Direct3D draw");
}
void shutdown() {
    releaseVehicleEnvironmentCapture();
    if(device) {endScene();device->SetTexture(0,nullptr);device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);}
    release(blurHistory);blurHistoryReady=false;
    release(screenDropletCapture);release(screenDropletMask);screenDropletPlayback.reset();
    releaseUnderwater();
    releasePs2PostEffects();
    releaseFxaa();
    for(auto& t:textures) release(t.second.object);
    textures.clear();glyphs.clear();
    for(auto& s:shaders){release(s.vertex);release(s.pixel);s={};}
    for(auto& s:pspShaders){release(s.vertex);release(s.pixel);s={};}
    for(auto& s:ps2Shaders){release(s.vertex);release(s.pixel);s={};}
    release(aeroWaterShader.vertex);release(aeroWaterShader.pixel);aeroWaterShader={};
    release(waterReactionTexture);surface=nullptr;
    release(device);release(api);window=nullptr;current=false;currentDeviceContext=nullptr;
    vertices.clear();expanded.clear();attributes.clear();clients.clear();
}
void replaceAll(std::string& s,const std::string& from,const std::string& to) {
    size_t p=0;while((p=s.find(from,p))!=std::string::npos){s.replace(p,from.size(),to);p+=to.size();}
}
bool compileSurface(Shader& shader,bool vcs,StorylandShaderKind kind,int consoleProfile=-1) {
    if(shader.attempted)return shader.vertex&&shader.pixel;
    shader.attempted=true;
    shader.psp=consoleProfile==3;
    shader.ps2=consoleProfile==0||consoleProfile==2;
    std::string source;
    if(shader.psp) {
        source=storylandVcsPspShaderSource(kind);
    } else if(shader.ps2) {
        source=storylandPs2ShaderSource(consoleProfile==2,kind);
    } else {
        source=storylandStoriesHlslSource(vcs,kind);
        const auto start=source.find("cbuffer StoriesFrame"),end=source.find("struct VertexInput");
        if(start==std::string::npos||end==std::string::npos)return false;
        source.replace(start,end-start,R"HLSL(
row_major float4x4 modelView : register(c0);
row_major float4x4 modelViewProjection : register(c4);
float4 ambientColor : register(c8);
float4 directionalColor : register(c9);
float4 fogColor : register(c10);
float4 frameParameters : register(c11);
float4 surfaceParameters : register(c12);
float4 sunDirection : register(c13);
float4 dualParameters : register(c14);
float4 environmentParameters : register(c15);
float4 environmentViewRight : register(c16);
float4 environmentViewUp : register(c17);
float4 environmentViewBack : register(c18);
#define fogStart frameParameters.x
#define farClip frameParameters.y
#define useTexture frameParameters.z
#define renderMode frameParameters.w
#define prelightMode surfaceParameters.x
#define prelightScale surfaceParameters.y
#define alphaReference surfaceParameters.z
#define alphaTest surfaceParameters.w
sampler2D diffuseSampler : register(s0);
sampler2D environmentSampler : register(s1);
        )HLSL");
        replaceAll(source,"SV_POSITION","POSITION");replaceAll(source,"SV_TARGET","COLOR0");
        replaceAll(source,"diffuseTexture.Sample(diffuseSampler, input.uv)","tex2D(diffuseSampler, input.uv)");
        replaceAll(source,"environmentTexture.Sample(environmentSampler, secondaryUv)","tex2D(environmentSampler, secondaryUv)");
    }
    ID3DBlob *vs=nullptr,*ps=nullptr,*errors=nullptr;
    HRESULT hr=D3DCompile(source.data(),source.size(),"Storyland surface",nullptr,nullptr,"VSMain","vs_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&errors);
    if(FAILED(hr)){diagnostic=errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"DirectX vertex shader compilation failed";release(errors);return false;}release(errors);
    hr=D3DCompile(source.data(),source.size(),"Storyland surface",nullptr,nullptr,"PSMain","ps_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&errors);
    if(FAILED(hr)){diagnostic=errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"DirectX pixel shader compilation failed";release(errors);release(vs);return false;}release(errors);
    hr=device->CreateVertexShader(static_cast<const DWORD*>(vs->GetBufferPointer()),&shader.vertex);
    if(SUCCEEDED(hr))hr=device->CreatePixelShader(static_cast<const DWORD*>(ps->GetBufferPointer()),&shader.pixel);
    release(vs);release(ps);
    if(FAILED(hr)){release(shader.vertex);release(shader.pixel);diagnostic="DirectX shader creation failed";return false;}
    return true;
}
}
bool active(){return device!=nullptr;}
bool requested(){return chosen;}
const std::string& error(){return diagnostic;}
void selectFromCommandLine() {
    wchar_t value[32]{};GetEnvironmentVariableW(L"STORYLAND_RENDERER",value,32);
    chosen=_wcsicmp(value,L"directx")==0||_wcsicmp(value,L"d3d9")==0;
    int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argv){for(int i=1;i<argc;++i){
        if(_wcsicmp(argv[i],L"--renderer=directx")==0||_wcsicmp(argv[i],L"--renderer=d3d9")==0)chosen=true;
        else if(_wcsicmp(argv[i],L"--renderer=opengl")==0)chosen=false;
    }LocalFree(argv);}
}
HGLRC wglCreateContext(HDC dc) {
    window=WindowFromDC(dc);api=Direct3DCreate9(D3D_SDK_VERSION);
    HRESULT hr=E_FAIL;
    if(api) {
        RECT r{};GetClientRect(window,&r);presentParameters={};
        presentParameters.Windowed=TRUE;presentParameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
        presentParameters.hDeviceWindow=window;presentParameters.BackBufferFormat=D3DFMT_UNKNOWN;
        presentParameters.BackBufferWidth=UINT(std::max(1L,r.right));presentParameters.BackBufferHeight=UINT(std::max(1L,r.bottom));
        presentParameters.EnableAutoDepthStencil=TRUE;presentParameters.AutoDepthStencilFormat=D3DFMT_D24S8;
        presentParameters.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
        D3DDISPLAYMODE displayMode{};
        const bool haveDisplayMode = SUCCEEDED(api->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode));
        for (auto samples : {D3DMULTISAMPLE_4_SAMPLES, D3DMULTISAMPLE_2_SAMPLES, D3DMULTISAMPLE_NONE}) {
            for (auto format : storylandDirectXDepthFormats) {
                const D3DFORMAT depth=nativeDepthFormat(format);
                if (samples != D3DMULTISAMPLE_NONE) {
                    DWORD colorQuality = 0;
                    DWORD depthQuality = 0;
                    if (!haveDisplayMode ||
                        FAILED(api->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                            displayMode.Format, TRUE, samples, &colorQuality)) ||
                        FAILED(api->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                            depth, TRUE, samples, &depthQuality)) || !colorQuality || !depthQuality) continue;
                }
                presentParameters.AutoDepthStencilFormat = depth;
                presentParameters.MultiSampleType = samples;
                presentParameters.MultiSampleQuality = 0;
                hr = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                    D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &presentParameters, &device);
                if (FAILED(hr)) hr = api->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                    D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &presentParameters, &device);
                if (SUCCEEDED(hr)) break;
            }
            if (SUCCEEDED(hr)) break;
        }
    }
    if(SUCCEEDED(hr)) {
        D3DCAPS9 caps{};
        hr=device->GetDeviceCaps(&caps);
        if(SUCCEEDED(hr))deviceCapabilities=caps;
        if(SUCCEEDED(hr)&&(caps.VertexShaderVersion<D3DVS_VERSION(3,0)||caps.PixelShaderVersion<D3DPS_VERSION(3,0))) {
            diagnostic="Direct3D Shader Model 3 is unavailable.";hr=E_FAIL;
        }
        if(SUCCEEDED(hr)) for(int game=0;game<2&&SUCCEEDED(hr);++game) for(int kind=0;kind<5;++kind) {
            if(!compileSurface(shaders[game*5+kind],game!=0,StorylandShaderKind(kind))) {hr=E_FAIL;break;}
        }
    }
    if(FAILED(hr)) {
        shutdown();chosen=false;
        if(diagnostic.empty())diagnostic="Direct3D device creation failed.";
        diagnostic+=" Using OpenGL.";
        MessageBoxA(WindowFromDC(dc),diagnostic.c_str(),"Storyland renderer",MB_OK|MB_ICONWARNING);
        return ::wglCreateContext(dc);
    }
    for(auto& stack:matrices){stack.clear();stack.push_back(identity());}
    state={};client={};nextTexture=1;nextList=1;diagnostic.clear();
    return contextHandle();
}
BOOL wglDeleteContext(HGLRC){shutdown();return TRUE;}
BOOL wglMakeCurrent(HDC dc,HGLRC context){current=context!=nullptr;currentDeviceContext=current?dc:nullptr;return TRUE;}
HGLRC wglGetCurrentContext(){return current?contextHandle():nullptr;}
HDC wglGetCurrentDC(){return current?currentDeviceContext:nullptr;}
BOOL present(HDC) {
    endScene();HRESULT hr=device->Present(nullptr,nullptr,nullptr,nullptr);
    if(hr==D3DERR_DEVICELOST)return FALSE;
    check(hr,"Direct3D Present");return TRUE;
}
void drawFrameBlur(unsigned opacity) {
    if(!opacity||!ready()) {blurHistoryReady=false;return;}
    static std::array<int,4> previousViewport{};
    if(previousViewport!=state.viewport)blurHistoryReady=false;
    previousViewport=state.viewport;
    IDirect3DSurface9* source=nullptr;
    D3DSURFACE_DESC desc{};
    HRESULT result=device->GetRenderTarget(0,&source);
    if(SUCCEEDED(result))result=source->GetDesc(&desc);
    if(SUCCEEDED(result)&&!blurHistory)
        result=device->CreateTexture(desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,desc.Format,
            D3DPOOL_DEFAULT,&blurHistory,nullptr);
    if(SUCCEEDED(result)&&blurHistoryReady) {
        const State previous=state;
        state.enabled={GL_BLEND,GL_TEXTURE_2D};
        state.viewport={0,0,int(desc.Width),int(desc.Height)};
        state.polygon=GL_FILL;state.depthWrite=false;
        state.src=GL_SRC_ALPHA;state.dst=GL_ONE_MINUS_SRC_ALPHA;
        applyState();
        device->SetTexture(0,blurHistory);
        device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);
        device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
        device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
        struct ScreenVertex { float x,y,z,rhw; DWORD colour; float u,v; };
        const float left=float(previous.viewport[0])-.5f;
        const float top=float(int(desc.Height)-previous.viewport[1]-previous.viewport[3])-.5f;
        const float right=left+previous.viewport[2],bottom=top+previous.viewport[3];
        const float u0=(left+.5f)/desc.Width,u1=(right+.5f)/desc.Width;
        const float v0=(top+.5f)/desc.Height,v1=(bottom+.5f)/desc.Height;
        const DWORD alpha=(DWORD(opacity)<<24)|0xffffff;
        const ScreenVertex quad[]={{left,top,0,1,alpha,u0,v0},{right,top,0,1,alpha,u1,v0},
            {left,bottom,0,1,alpha,u0,v1},{right,bottom,0,1,alpha,u1,v1}};
        device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        result=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(ScreenVertex));
        state=previous;
        applyState();
    }
    IDirect3DSurface9* history=nullptr;
    if(SUCCEEDED(result))result=blurHistory->GetSurfaceLevel(0,&history);
    if(SUCCEEDED(result)) {
        endScene();
        result=device->StretchRect(source,nullptr,history,nullptr,D3DTEXF_NONE);
    }
    blurHistoryReady=SUCCEEDED(result);
    release(history);release(source);
}
#include "src/graphics/storyland_ps2_postfx_dx.inc"

void setUnderwater(const StorylandUnderwaterFrame& settings) {
    underwaterFrame=settings;
}

void drawUnderwater(const StorylandUnderwaterFrame& settings) {
    if(!device||!settings.enabled||settings.immersion<=0.0f)return;
    const bool previousScene=inScene;
    if(!ready())return;
    IDirect3DStateBlock9* previousState=nullptr;
    IDirect3DSurface9* source=nullptr;
    IDirect3DSurface9* destination=nullptr;
    D3DVIEWPORT9 viewport{};D3DSURFACE_DESC description{};
    HRESULT result=device->CreateStateBlock(D3DSBT_ALL,&previousState);
    const auto record=[&](HRESULT value){if(SUCCEEDED(result)&&FAILED(value))result=value;};
    if(SUCCEEDED(result))record(previousState->Capture());
    if(SUCCEEDED(result))record(device->GetViewport(&viewport));
    if(SUCCEEDED(result))record(device->GetRenderTarget(0,&source));
    if(SUCCEEDED(result))record(source->GetDesc(&description));
    if(SUCCEEDED(result)&&underwaterSceneTexture) {
        D3DSURFACE_DESC currentDescription{};
        record(underwaterSceneTexture->GetLevelDesc(0,&currentDescription));
        if(currentDescription.Width!=description.Width||currentDescription.Height!=description.Height||
            currentDescription.Format!=description.Format)release(underwaterSceneTexture);
    }
    if(SUCCEEDED(result)&&!underwaterSceneTexture)
        record(device->CreateTexture(description.Width,description.Height,1,D3DUSAGE_RENDERTARGET,
            description.Format,D3DPOOL_DEFAULT,&underwaterSceneTexture,nullptr));
    if(SUCCEEDED(result)&&!underwaterRippleShader) {
        constexpr const char* shader=R"HLSL(
sampler2D sceneTexture:register(s0);
float4 parameters:register(c0);
float4 colorGain:register(c1);
float4 viewportUv:register(c2);
float4 PSMain(float2 uv:TEXCOORD0):COLOR0 {
 float rasterY=uv.y*448.0,strip=floor(rasterY/24.0)*24.0;
 float wave=lerp(sin(strip*.04+parameters.x*1.5),sin((strip+24.0)*.04+parameters.x*1.5),frac(rasterY/24.0));
 uv.x=clamp(uv.x+wave*(4.0/640.0)*parameters.y,parameters.z*.5,1.0-parameters.z*.5);
 float4 color=tex2D(sceneTexture,uv*viewportUv.xy+viewportUv.zw);
 color.rgb*=lerp(float3(1,1,1),colorGain.rgb,parameters.y);
 color.rgb*=parameters.w;
 return color;
}
)HLSL";
        ID3DBlob* code=nullptr;ID3DBlob* errors=nullptr;
        record(D3DCompile(shader,std::strlen(shader),"SA underwater ripple",nullptr,nullptr,
            "PSMain","ps_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors));
        if(SUCCEEDED(result))record(device->CreatePixelShader(
            static_cast<const DWORD*>(code->GetBufferPointer()),&underwaterRippleShader));
        if(errors)diagnostic=std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize());
        release(code);release(errors);
    }
    if(SUCCEEDED(result))record(underwaterSceneTexture->GetSurfaceLevel(0,&destination));
    if(SUCCEEDED(result)) {
        endScene();
        record(device->StretchRect(source,nullptr,destination,nullptr,D3DTEXF_NONE));
    }
    if(SUCCEEDED(result)) {
        record(device->SetVertexShader(nullptr));record(device->SetPixelShader(underwaterRippleShader));
        record(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));record(device->SetTexture(0,underwaterSceneTexture));
        record(device->SetTexture(1,nullptr));
        for(const auto& setting:std::array<std::pair<D3DRENDERSTATETYPE,DWORD>,14>{{
            {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_LIGHTING,FALSE},{D3DRS_FOGENABLE,FALSE},
            {D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},
            {D3DRS_STENCILENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_FILLMODE,D3DFILL_SOLID},
            {D3DRS_SRGBWRITEENABLE,FALSE},{D3DRS_CLIPPLANEENABLE,0},{D3DRS_WRAP0,0},
            {D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE}}})
            record(device->SetRenderState(setting.first,setting.second));
        record(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));
        record(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
        record(device->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff));
        record(device->SetRenderState(D3DRS_DITHERENABLE,FALSE));
        record(device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));
        record(device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
        record(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
        record(device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));
        record(device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
        record(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE));
        const std::array<float,4> parameters{settings.gameSeconds,std::clamp((settings.immersion-.535f)/.465f,0.0f,1.0f),
            1.0f/float(std::max<DWORD>(1,viewport.Width)),std::max(.04f,1.0f-settings.cameraDepth/90.0f)};
        const std::array<float,4> gain{settings.colorGain[0],settings.colorGain[1],settings.colorGain[2],1};
        record(device->SetPixelShaderConstantF(0,parameters.data(),1));
        record(device->SetPixelShaderConstantF(1,gain.data(),1));
        const std::array<float,4> viewportUv{float(viewport.Width)/description.Width,float(viewport.Height)/description.Height,
            float(viewport.X)/description.Width,float(viewport.Y)/description.Height};
        record(device->SetPixelShaderConstantF(2,viewportUv.data(),1));
        if(SUCCEEDED(result)){record(device->BeginScene());if(SUCCEEDED(result))inScene=true;}
        struct RippleVertex{float x,y,z,rhw,u,v;};
        const float left=float(viewport.X)-.5f,top=float(viewport.Y)-.5f;
        const float right=left+viewport.Width,bottom=top+viewport.Height;
        const RippleVertex quad[]{{left,top,0,1,0,0},{right,top,0,1,1,0},{left,bottom,0,1,0,1},{right,bottom,0,1,1,1}};
        if(SUCCEEDED(result))record(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(RippleVertex)));
    }
    if(previousState)record(previousState->Apply());
    if(previousScene&&!inScene){const HRESULT value=device->BeginScene();record(value);if(SUCCEEDED(value))inScene=true;}
    else if(!previousScene)endScene();
    release(previousState);release(source);release(destination);
    if(FAILED(result))diagnostic="Underwater Direct3D scene refraction failed (HRESULT "+
        std::to_string(static_cast<unsigned long>(result))+").";
}

#include "src/graphics/storyland_fxaa_dx.inc"

void drawScreenDroplets(bool enabled,float gameSeconds,std::uint32_t waterSerial,
    std::uint32_t bloodSerial,float waterStrength,float bloodStrength,
    const StorylandScreenDropletCamera& camera) {
    if(!enabled||!device) {
        screenDropletPlayback.advance(false,gameSeconds,waterSerial,bloodSerial,
            waterStrength,bloodStrength,camera,1,1);
        return;
    }
    if(!ready())return;
    const int width=state.viewport[2],height=state.viewport[3];
    if(width<=0||height<=0||width>8192||height>8192)return;
    screenDropletPlayback.advance(true,gameSeconds,waterSerial,bloodSerial,
        waterStrength,bloodStrength,camera,width,height);
    const auto& simulation=screenDropletPlayback.simulation();
    if(simulation.count()==0)return;

    IDirect3DSurface9* source=nullptr;
    IDirect3DSurface9* target=nullptr;
    IDirect3DStateBlock9* previous=nullptr;
    D3DSURFACE_DESC description{};
    HRESULT result=device->GetRenderTarget(0,&source);
    if(SUCCEEDED(result))result=source->GetDesc(&description);
    if(SUCCEEDED(result)&&screenDropletCapture) {
        D3DSURFACE_DESC currentDescription{};
        if(FAILED(screenDropletCapture->GetLevelDesc(0,&currentDescription))||
            currentDescription.Width!=description.Width||currentDescription.Height!=description.Height||
            currentDescription.Format!=description.Format)release(screenDropletCapture);
    }
    if(SUCCEEDED(result)&&!screenDropletCapture)
        result=device->CreateTexture(description.Width,description.Height,1,D3DUSAGE_RENDERTARGET,
            description.Format,D3DPOOL_DEFAULT,&screenDropletCapture,nullptr);
    if(SUCCEEDED(result))result=screenDropletCapture->GetSurfaceLevel(0,&target);
    if(SUCCEEDED(result)) {
        endScene();
        result=device->StretchRect(source,nullptr,target,nullptr,D3DTEXF_NONE);
    }
    if(SUCCEEDED(result)&&!screenDropletMask) {
        result=device->CreateTexture(64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,
            &screenDropletMask,nullptr);
        if(SUCCEEDED(result)) {
            D3DLOCKED_RECT locked{};
            result=screenDropletMask->LockRect(0,&locked,nullptr,0);
            if(SUCCEEDED(result)) {
                const auto pixels=storylandScreenDropletMask();
                for(int y=0;y<64;++y) {
                    auto* row=reinterpret_cast<DWORD*>(static_cast<unsigned char*>(locked.pBits)+
                        std::size_t(y)*std::size_t(locked.Pitch));
                    for(int x=0;x<64;++x)row[x]=pixels[(y*64+x)*4]?0xffffffffu:0u;
                }
                result=screenDropletMask->UnlockRect(0);
            }
            if(FAILED(result))release(screenDropletMask);
        }
    }
    if(SUCCEEDED(result))result=device->CreateStateBlock(D3DSBT_ALL,&previous);
    if(SUCCEEDED(result))result=previous->Capture();
    if(SUCCEEDED(result)&&ready()) {
        const int top=int(description.Height)-state.viewport[1]-height;
        const int left=state.viewport[0];
        const RECT clipping{std::max(0,left),std::max(0,top),
            std::min(int(description.Width),left+width),std::min(int(description.Height),top+height)};
        const D3DVIEWPORT9 fullViewport{0,0,description.Width,description.Height,0,1};
        device->SetViewport(&fullViewport);
        device->SetScissorRect(&clipping);
        device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
        device->SetRenderState(D3DRS_ZENABLE,FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        device->SetRenderState(D3DRS_LIGHTING,FALSE);
        device->SetRenderState(D3DRS_FOGENABLE,FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);
        device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
        device->SetRenderState(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
        device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED|
            D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_ALPHA);
        device->SetRenderState(D3DRS_STENCILENABLE,FALSE);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
        device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        device->SetTexture(0,screenDropletMask);device->SetTexture(1,screenDropletCapture);
        for(DWORD stage=0;stage<2;++stage) {
            device->SetSamplerState(stage,D3DSAMP_MINFILTER,stage?D3DTEXF_LINEAR:D3DTEXF_POINT);
            device->SetSamplerState(stage,D3DSAMP_MAGFILTER,stage?D3DTEXF_LINEAR:D3DTEXF_POINT);
            device->SetSamplerState(stage,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
            device->SetSamplerState(stage,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
            device->SetSamplerState(stage,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
            device->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage);
            device->SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
            device->SetTextureStageState(stage,D3DTSS_COLOROP,D3DTOP_MODULATE);
            device->SetTextureStageState(stage,D3DTSS_COLORARG1,D3DTA_TEXTURE);
            device->SetTextureStageState(stage,D3DTSS_COLORARG2,stage?D3DTA_CURRENT:D3DTA_DIFFUSE);
            device->SetTextureStageState(stage,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
            device->SetTextureStageState(stage,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
            device->SetTextureStageState(stage,D3DTSS_ALPHAARG2,stage?D3DTA_CURRENT:D3DTA_DIFFUSE);
        }
        device->SetTextureStageState(2,D3DTSS_COLOROP,D3DTOP_DISABLE);
        device->SetTextureStageState(2,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
        struct DropVertex {float x,y,z,rhw;DWORD tint;float maskU,maskV,sceneU,sceneV;};
        std::vector<DropVertex> draws;draws.reserve(simulation.count()*6);
        constexpr int order[6]={0,1,2,2,3,0};
        for(const auto& drop:simulation.droplets())if(drop.active&&drop.color[3]) {
            const auto quad=storylandScreenDropletQuad(drop,width,height);
            const DWORD tint=D3DCOLOR_ARGB(drop.color[3],drop.color[0],drop.color[1],drop.color[2]);
            for(int index:order) {
                const auto& vertex=quad[std::size_t(index)];
                draws.push_back({float(left)+vertex.x-.5f,float(top)+vertex.y-.5f,0,1,tint,
                    vertex.maskU,vertex.maskV,
                    (float(left)+vertex.sceneU*width)/float(description.Width),
                    (float(top)+(1.0f-vertex.sceneV)*height)/float(description.Height)});
            }
        }
        if(!draws.empty()) {
            device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX2);
            result=device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(draws.size()/3),draws.data(),sizeof(DropVertex));
        }
        previous->Apply();
    }
    release(previous);release(target);release(source);
    if(FAILED(result))diagnostic="Screen droplet drawing failed (HRESULT "+
        std::to_string(static_cast<unsigned long>(result))+").";
}
bool captureFrame(std::vector<uint8_t>& rgba, int& width, int& height, std::string& error) {
    if (!device) { error="Direct3D device is unavailable."; return false; }
    endScene();
    IDirect3DSurface9* source=nullptr;
    IDirect3DSurface9* resolved=nullptr;
    IDirect3DSurface9* copy=nullptr;
    D3DSURFACE_DESC description{};
    HRESULT hr=device->GetRenderTarget(0,&source);
    if (SUCCEEDED(hr)) hr=source->GetDesc(&description);
    if (SUCCEEDED(hr) && (description.Width==0 || description.Height==0 ||
        uint64_t(description.Width)*description.Height>67108864 ||
        (description.Format!=D3DFMT_X8R8G8B8 && description.Format!=D3DFMT_A8R8G8B8))) hr=E_FAIL;
    if (SUCCEEDED(hr)) hr=device->CreateOffscreenPlainSurface(description.Width,description.Height,
        description.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr);
    if (SUCCEEDED(hr)&&description.MultiSampleType!=D3DMULTISAMPLE_NONE) {
        hr=device->CreateRenderTarget(description.Width,description.Height,description.Format,
            D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr);
        if(SUCCEEDED(hr))hr=device->StretchRect(source,nullptr,resolved,nullptr,D3DTEXF_NONE);
    }
    if (SUCCEEDED(hr)) hr=device->GetRenderTargetData(resolved?resolved:source,copy);
    D3DLOCKED_RECT lock{};
    if (SUCCEEDED(hr)) hr=copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    if (SUCCEEDED(hr)) {
        width=int(description.Width);height=int(description.Height);
        rgba.resize(size_t(width)*size_t(height)*4);
        for (int y=0;y<height;++y) {
            const auto* row=static_cast<const uint8_t*>(lock.pBits)+size_t(y)*size_t(lock.Pitch);
            for (int x=0;x<width;++x) {
                const size_t output=(size_t(y)*size_t(width)+size_t(x))*4;
                rgba[output]=row[x*4+2];rgba[output+1]=row[x*4+1];
                rgba[output+2]=row[x*4];rgba[output+3]=255;
            }
        }
        copy->UnlockRect();
    }
    release(copy);release(resolved);release(source);
    if (FAILED(hr)) {error="Could not capture the Direct3D frame (HRESULT "+std::to_string(static_cast<unsigned long>(hr))+").";return false;}
    error.clear();return true;
}
bool beginSurface(bool vcs,StorylandShaderKind kind,bool texture,int mode,int prelight,
    const float* ambient,const float* directional,const float* fog,const float* sunDirectionValue,float start,float end,
    int consoleProfile,const float* cameraView,const float* consoleAmbient,
    const float* consoleEmissive,float textureScale,float environmentStrength) {
    surface=nullptr;
    if(!device)return false;
    std::array<float,3> underwaterFog{};
    if(vcs&&underwaterFrame.enabled&&underwaterFrame.immersion>0&&!environmentCaptureActive) {
        const float amount=underwaterFrame.immersion;
        start+=(std::min(start,5.0f)-start)*amount;
        end+=(std::min(end,90.0f)-end)*amount;
        for(unsigned axis=0;axis<3;++axis)underwaterFog[axis]=fog[axis]+(underwaterFrame.fogColor[axis]-fog[axis])*amount;
        fog=underwaterFog.data();
    }
    const bool ps2=consoleProfile==0||consoleProfile==2;
    Shader& s=consoleProfile==3?pspShaders[size_t(kind)]:
        ps2?ps2Shaders[(consoleProfile==2?5:0)+size_t(kind)]:shaders[(vcs?5:0)+size_t(kind)];
    if(!compileSurface(s,vcs,kind,consoleProfile))throw std::runtime_error(diagnostic);
    surface=&s;surfaceUsesTexture=texture;
    surfaceDualTexture=0;constants[56]=0.0f;
    std::copy_n(ambient,3,constants.begin()+32);std::copy_n(directional,3,constants.begin()+36);std::copy_n(fog,3,constants.begin()+40);
    constants[44]=start;constants[45]=end;constants[46]=texture?1.f:0.f;constants[47]=float(mode);constants[48]=float(prelight);
    constants[49]=1.0f;
    std::copy_n(sunDirectionValue,3,constants.begin()+52);constants[55]=0.0f;
    if(s.psp) {
        std::fill(constants.begin()+76,constants.end(),0.0f);
        std::copy_n(consoleAmbient?consoleAmbient:ambient,3,constants.begin()+80);
        if(consoleEmissive)std::copy_n(consoleEmissive,3,constants.begin()+84);
        constants[83]=textureScale;
        const Matrix identityView=identity();
        const float* view=cameraView?cameraView:identityView.data();
        for(size_t axis=0;axis<3;++axis) {
            constants[88+axis]=view[axis];
            constants[92+axis]=-view[8+axis];
        }
        constants[97]=1.0f;
        constants[99]=environmentStrength;
        const auto fogParameters=storylandPspFogParameters(end,start);
        constants[45]=fogParameters.farClip;
        constants[118]=fogParameters.slope;
    } else if(s.ps2) {
        std::fill(constants.begin()+76,constants.end(),0.0f);
        std::fill(constants.begin()+76,constants.begin()+80,1.0f);
        std::fill(constants.begin()+84,constants.begin()+88,255.0f);
        for(size_t axis=0;axis<3;++axis) {
            constants[88+axis]=ambient[axis]*255.0f;
            constants[92+axis]=directional[axis]*255.0f;
        }
        const auto fogParameters=storylandPs2FogParameters(start,end);
        constants[96]=fogParameters.slope;
        constants[97]=fogParameters.intercept;
        constants[98]=fogParameters.enabled?1.0f:0.0f;
        constants[100]=1.0f;
        constants[105]=1.0f;
        constants[133]=1.0f;
        constants[134]=1.0f;
    }
    return true;
}
void endSurface(){surface=nullptr;surfaceDualTexture=0;}

bool compileWaterShader() {
    if(aeroWaterShader.attempted)return aeroWaterShader.vertex&&aeroWaterShader.pixel;
    aeroWaterShader.attempted=true;
    ID3DBlob* vertex=nullptr;
    ID3DBlob* fragment=nullptr;
    ID3DBlob* errors=nullptr;
    const auto size=std::strlen(StorylandAeroWaterHlsl);
    HRESULT hr=D3DCompile(StorylandAeroWaterHlsl,size,"Aero water",nullptr,nullptr,
        "VSMain","vs_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vertex,&errors);
    if(FAILED(hr)) {
        diagnostic=errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):
            "Aero water Direct3D vertex compilation failed";
        release(errors);release(vertex);return false;
    }
    release(errors);
    hr=D3DCompile(StorylandAeroWaterHlsl,size,"Aero water",nullptr,nullptr,
        "PSMain","ps_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&fragment,&errors);
    if(FAILED(hr)) {
        diagnostic=errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):
            "Aero water Direct3D pixel compilation failed";
        release(errors);release(vertex);release(fragment);return false;
    }
    release(errors);
    hr=device->CreateVertexShader(static_cast<const DWORD*>(vertex->GetBufferPointer()),&aeroWaterShader.vertex);
    if(SUCCEEDED(hr))hr=device->CreatePixelShader(static_cast<const DWORD*>(fragment->GetBufferPointer()),&aeroWaterShader.pixel);
    release(vertex);release(fragment);
    if(FAILED(hr)) {
        diagnostic="Aero water Direct3D shader creation failed";
        release(aeroWaterShader.vertex);release(aeroWaterShader.pixel);return false;
    }
    return true;
}

bool beginWater(const WaterSettings& settings) {
    if(!device||!compileWaterShader())return false;
    if(settings.reactionPixels&&settings.reactionWidth>0&&settings.reactionHeight>0&&
        settings.reactionWidth<=256&&settings.reactionHeight<=256) {
        if(!waterReactionTexture) {
            HRESULT hr=device->CreateTexture(UINT(settings.reactionWidth),UINT(settings.reactionHeight),1,0,
                D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&waterReactionTexture,nullptr);
            if(FAILED(hr))diagnostic="Could not allocate Aero water reaction texture";
        }
        if(waterReactionTexture) {
            D3DLOCKED_RECT locked{};
            if(SUCCEEDED(waterReactionTexture->LockRect(0,&locked,nullptr,0))) {
                for(int row=0;row<settings.reactionHeight;++row)
                    std::memcpy(static_cast<uint8_t*>(locked.pBits)+size_t(row)*locked.Pitch,
                        settings.reactionPixels+size_t(row)*size_t(settings.reactionWidth)*4,
                        size_t(settings.reactionWidth)*4);
                waterReactionTexture->UnlockRect(0);
            }
        }
    }
    const auto copyColor=[&](size_t registerIndex,const std::array<float,3>& color,float scalar) {
        const size_t offset=registerIndex*4;
        for(size_t i=0;i<3;++i)aeroWaterConstants[offset+i]=color[i];
        aeroWaterConstants[offset+3]=scalar;
    };
    aeroWaterConstants[0]=settings.time;
    aeroWaterConstants[1]=settings.wind;
    aeroWaterConstants[2]=settings.rain;
    aeroWaterConstants[3]=settings.sunVisibility;
    copyColor(1,settings.horizon,settings.fogStart);
    copyColor(2,settings.zenith,settings.fogEnd);
    const bool reactions=waterReactionTexture&&settings.reactionPixels;
    copyColor(3,settings.lower,reactions?1.0f:0.0f);
    copyColor(4,settings.sunDirection,0);
    copyColor(5,settings.sunColor,settings.textured?1.0f:0.0f);
    copyColor(6,settings.fogColor,0);
    copyColor(7,settings.camera,settings.reactionExtent);
    aeroWaterConstants[32]=settings.reactionOriginX;
    aeroWaterConstants[33]=settings.reactionOriginY;
    aeroWaterConstants[34]=reactions?1.0f/std::max(.01f,settings.reactionExtent):0.0f;
    aeroWaterConstants[35]=0;
    aeroWaterConstants[36]=settings.reactionWidth?1.0f/settings.reactionWidth:0;
    aeroWaterConstants[37]=settings.reactionHeight?1.0f/settings.reactionHeight:0;
    aeroWaterConstants[38]=aeroWaterConstants[39]=0;
    surface=&aeroWaterShader;
    surfaceUsesTexture=settings.textured;
    return true;
}

void endWater(){if(surface==&aeroWaterShader)surface=nullptr;}
GLuint vehicleEnvironmentTexture() {return environmentTexture;}

void endVehicleEnvironmentCapture() {
    if(!environmentCaptureActive)return;
    device->SetTexture(0,nullptr);
    device->SetTexture(1,nullptr);
    device->SetRenderTarget(0,environmentPreviousColor);
    device->SetDepthStencilSurface(environmentPreviousDepth);
    device->SetViewport(&environmentPreviousViewport);
    release(environmentPreviousColor);
    release(environmentPreviousDepth);
    renderTargetWidth=renderTargetHeight=0;
    environmentCaptureActive=false;
}

void releaseVehicleEnvironmentCapture() {
    endVehicleEnvironmentCapture();
    if(device)device->SetTexture(1,nullptr);
    const auto found=textures.find(environmentTexture);
    if(found!=textures.end()) {
        release(found->second.object);
        textures.erase(found);
    }
    if(state.texture==environmentTexture)state.texture=0;
    if(surfaceDualTexture==environmentTexture)surfaceDualTexture=0;
    release(environmentDepth);
    environmentDepthFormat=D3DFMT_UNKNOWN;
    environmentTexture=0;
    environmentFaceSize=0;
}

GLuint beginVehicleEnvironmentCapture(int requestedSize,std::string& error) {
    if(!device||environmentCaptureActive) {
        error="Direct3D environment capture is unavailable.";
        return 0;
    }
    if(!ready()) {error="The Direct3D device is unavailable.";return 0;}
    const int size=std::clamp(requestedSize,32,256);
    if(size!=environmentFaceSize) {
        releaseVehicleEnvironmentCapture();
        IDirect3DTexture9* target=nullptr;
        HRESULT result=device->CreateTexture(size*3,size*2,1,D3DUSAGE_RENDERTARGET,
            D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&target,nullptr);
        D3DFORMAT depthFormat=D3DFMT_UNKNOWN;
        if(SUCCEEDED(result)) {
            for(auto format:storylandDirectXDepthFormats) {
                depthFormat=nativeDepthFormat(format);
                result=device->CreateDepthStencilSurface(size*3,size*2,depthFormat,
                    D3DMULTISAMPLE_NONE,0,TRUE,&environmentDepth,nullptr);
                if(SUCCEEDED(result))break;
                release(environmentDepth);
            }
        }
        if(FAILED(result)) {
            release(target);release(environmentDepth);
            error="Direct3D could not allocate the environment render target.";
            return 0;
        }
        environmentDepthFormat=depthFormat;
        environmentTexture=nextTexture++;
        textures[environmentTexture].object=target;
        environmentFaceSize=size;
    }
    IDirect3DSurface9* target=nullptr;
    HRESULT result=device->GetRenderTarget(0,&environmentPreviousColor);
    if(SUCCEEDED(result))result=device->GetDepthStencilSurface(&environmentPreviousDepth);
    if(SUCCEEDED(result))result=device->GetViewport(&environmentPreviousViewport);
    if(SUCCEEDED(result))result=textures[environmentTexture].object->GetSurfaceLevel(0,&target);
    if(SUCCEEDED(result)) {
        device->SetTexture(0,nullptr);device->SetTexture(1,nullptr);
        result=device->SetRenderTarget(0,target);
    }
    if(SUCCEEDED(result))result=device->SetDepthStencilSurface(environmentDepth);
    release(target);
    if(FAILED(result)) {
        if(environmentPreviousColor)device->SetRenderTarget(0,environmentPreviousColor);
        device->SetDepthStencilSurface(environmentPreviousDepth);
        device->SetViewport(&environmentPreviousViewport);
        release(environmentPreviousColor);release(environmentPreviousDepth);
        error="Direct3D could not bind the environment render target.";
        return 0;
    }
    renderTargetWidth=size*3;renderTargetHeight=size*2;
    environmentCaptureActive=true;
    error.clear();
    return environmentTexture;
}

void setDualTexture(GLuint texture,int mode,float strength,const float* cameraView,
    int faceSize,float scaleU,float scaleV,int uvSet) {
    const auto found=textures.find(texture);
    surfaceDualTexture=found!=textures.end()&&found->second.object?texture:0;
    constants[56]=surfaceDualTexture?float(std::clamp(mode,0,4)):0.0f;
    constants[57]=std::isfinite(strength)?std::clamp(strength,0.0f,1.0f):0.0f;
    constants[58]=std::isfinite(scaleU)?scaleU:1.0f;
    constants[59]=std::isfinite(scaleV)?scaleV:1.0f;
    constants[60]=uvSet?1.0f:0.0f;
    constants[61]=float(std::clamp(faceSize,32,256));
    std::array<float,16> camera=identity();
    if(cameraView)std::copy_n(cameraView,16,camera.begin());
    const auto basis=storylandEnvironmentEyeToWorld(camera.data());
    for(size_t axis=0;axis<3;++axis)
        std::copy_n(basis[axis].begin(),3,constants.begin()+64+axis*4);
}

void setSecondaryUv(float u,float v) {state.vertex.u1=u;state.vertex.v1=v;}

void setMaterialEffect(float strength){constants[55]=std::isfinite(strength)?std::clamp(strength,0.f,1.f):0.f;}
void setPrelightScale(float scale) {
    constants[49]=std::isfinite(scale)&&scale>=0.0f?scale:1.0f;
}
void setPspStripState(int lodMode,float lodSlope,float depthOffset) {
    if(!surface||!surface->psp)return;
    constants[96]=float(lodMode);
    constants[97]=std::isfinite(lodSlope)?lodSlope:1.0f;
    constants[98]=std::isfinite(depthOffset)?depthOffset:0.0f;
}
void setPspPrelitModel(bool prelit) {
    if(surface&&surface->psp)constants[87]=prelit?1.0f:0.0f;
}
void setPspLocalLight(unsigned index,const float* eyeDirection,const float* color) {
    if(!surface||!surface->psp||index>=2)return;
    const size_t offset=100+size_t(index)*8;
    for(size_t axis=0;axis<3;++axis) {
        constants[offset+axis]=eyeDirection&&std::isfinite(eyeDirection[axis])?eyeDirection[axis]:0.0f;
        constants[offset+4+axis]=color&&std::isfinite(color[axis])?std::clamp(color[axis],0.0f,1.0f):0.0f;
    }
}
void setPs2Lighting(const StorylandPs2LightingColors& colors,const StorylandPs2FogParameters& fog) {
    if(!surface||!surface->ps2)return;
    std::copy(colors.world.multiplier.begin(),colors.world.multiplier.end(),constants.begin()+76);
    std::copy(colors.world.additive.begin(),colors.world.additive.end(),constants.begin()+80);
    std::copy(colors.world.ceiling.begin(),colors.world.ceiling.end(),constants.begin()+84);
    std::copy(colors.actorAmbient.begin(),colors.actorAmbient.end(),constants.begin()+88);
    std::copy(colors.actorDirectional.begin(),colors.actorDirectional.end(),constants.begin()+92);
    const auto selectedFog=underwaterFrame.enabled&&underwaterFrame.immersion>0&&!environmentCaptureActive?
        storylandPs2FogParameters(constants[44],constants[45]):fog;
    constants[96]=selectedFog.slope;
    constants[97]=selectedFog.intercept;
    constants[98]=selectedFog.enabled?1.0f:0.0f;
}
void setPs2PrelitModel(bool prelit) {
    if(surface&&surface->ps2)constants[135]=prelit?1.0f:0.0f;
}
void setPs2LocalLight(unsigned index,const float* eyeDirection,const float* color) {
    if(!surface||!surface->ps2||index>=3)return;
    const size_t offset=108+size_t(index)*8;
    for(size_t axis=0;axis<3;++axis) {
        constants[offset+axis]=eyeDirection&&std::isfinite(eyeDirection[axis])?eyeDirection[axis]:0.0f;
        constants[offset+4+axis]=color&&std::isfinite(color[axis])?std::clamp(color[axis],0.0f,255.0f):0.0f;
    }
}
void setPs2EnvironmentCoefficient(float coefficient) {
    if(surface&&surface->ps2)
        constants[136]=std::isfinite(coefficient)?std::clamp(coefficient,0.0f,255.0f):0.0f;
}
void setPs2BudokaiReflection(bool enabled) {
    if(surface&&surface->ps2)constants[137]=enabled?1.0f:0.0f;
}
void setPs2EnvironmentBasis(const float* s,const float* t) {
    if(!surface||!surface->ps2)return;
    for(size_t axis=0;axis<3;++axis) {
        constants[100+axis]=s&&std::isfinite(s[axis])?s[axis]:0.0f;
        constants[104+axis]=t&&std::isfinite(t[axis])?t[axis]:0.0f;
    }
}
void glBegin(GLenum mode){primitive=mode;vertices.clear();}
void glEnd(){submit(primitive,vertices);vertices.clear();}
void glVertex3f(GLfloat x,GLfloat y,GLfloat z){Vertex v=state.vertex;v.x=x;v.y=y;v.z=z;vertices.push_back(v);}
void glVertex2f(GLfloat x,GLfloat y){glVertex3f(x,y,0);}
void glVertex3fv(const GLfloat* p){glVertex3f(p[0],p[1],p[2]);}
void glColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a){state.vertex.color=color(r,g,b,a);}
void glColor4fv(const GLfloat* p){glColor4f(p[0],p[1],p[2],p[3]);}
void glColor3f(GLfloat r,GLfloat g,GLfloat b){glColor4f(r,g,b,1);}
void glColor3fv(const GLfloat* p){glColor3f(p[0],p[1],p[2]);}
void glColor4ub(GLubyte r,GLubyte g,GLubyte b,GLubyte a){state.vertex.color=D3DCOLOR_ARGB(a,r,g,b);}
void glColor3ub(GLubyte r,GLubyte g,GLubyte b){glColor4ub(r,g,b,255);}
void glNormal3f(GLfloat x,GLfloat y,GLfloat z){state.vertex.nx=x;state.vertex.ny=y;state.vertex.nz=z;}
void glTexCoord2f(GLfloat u,GLfloat v){state.textureCoordinate={u,v,0,1};state.vertex.u=u;state.vertex.v=v;}
void glTexCoord4fv(const GLfloat* p){std::copy_n(p,4,state.textureCoordinate.begin());state.vertex.u=p[0];state.vertex.v=p[1];state.vertex.u1=p[2];state.vertex.v1=p[3];}
void glEnable(GLenum cap){state.enabled.insert(cap);}
void glDisable(GLenum cap){state.enabled.erase(cap);}
void glEnableClientState(GLenum cap){client.enabled.insert(cap);}
void glDisableClientState(GLenum cap){client.enabled.erase(cap);}
void glClearColor(GLfloat r,GLfloat g,GLfloat b,GLfloat a){state.clear={r,g,b,a};}
void glClear(GLbitfield mask) {
    if(!ready())return;
    applyState();
    DWORD flags=0;
    if(mask&GL_COLOR_BUFFER_BIT)flags|=D3DCLEAR_TARGET;
    if(mask&GL_DEPTH_BUFFER_BIT)flags|=D3DCLEAR_ZBUFFER;
    if((mask&GL_STENCIL_BUFFER_BIT)&&currentDepthHasStencil())
        flags|=D3DCLEAR_STENCIL;
    if(!flags)return;
    D3DRECT rect{};
    DWORD count=0;
    if(enabled(GL_SCISSOR_TEST)) {
        const int width=renderTargetWidth?renderTargetWidth:int(presentParameters.BackBufferWidth);
        const int height=renderTargetHeight?renderTargetHeight:int(presentParameters.BackBufferHeight);
        rect.x1=std::clamp(state.scissor[0],0,width);
        rect.y1=std::clamp(height-state.scissor[1]-state.scissor[3],0,height);
        rect.x2=std::clamp(state.scissor[0]+state.scissor[2],0,width);
        rect.y2=std::clamp(height-state.scissor[1],0,height);
        if(rect.x2<=rect.x1||rect.y2<=rect.y1)return;
        count=1;
    }
    check(device->Clear(count,count?&rect:nullptr,flags,
        color(state.clear[0],state.clear[1],state.clear[2],state.clear[3]),1,0),
        "Direct3D clear");
}
void glViewport(GLint x,GLint y,GLsizei w,GLsizei h){state.viewport={x,y,w,h};}
void glScissor(GLint x,GLint y,GLsizei w,GLsizei h){state.scissor={x,y,w,h};}
void glMatrixMode(GLenum mode){state.matrixMode=mode;}
void glLoadIdentity(){matrix()=identity();}
void glLoadMatrixd(const GLdouble* p){for(int i=0;i<16;++i)matrix()[i]=float(p[i]);}
void glMultMatrixf(const GLfloat* p){Matrix m;std::copy_n(p,16,m.begin());matrix()=multiply(matrix(),m);}
void glPushMatrix(){auto& s=matrices[state.matrixMode==GL_PROJECTION?1:state.matrixMode==GL_TEXTURE?2:0];s.push_back(s.back());}
void glPopMatrix(){auto& s=matrices[state.matrixMode==GL_PROJECTION?1:state.matrixMode==GL_TEXTURE?2:0];if(s.size()>1)s.pop_back();else lastError=GL_STACK_UNDERFLOW;}
void glTranslatef(GLfloat x,GLfloat y,GLfloat z){Matrix m=identity();m[12]=x;m[13]=y;m[14]=z;glMultMatrixf(m.data());}
void glScalef(GLfloat x,GLfloat y,GLfloat z){Matrix m=identity();m[0]=x;m[5]=y;m[10]=z;glMultMatrixf(m.data());}
void glRotatef(GLfloat a,GLfloat x,GLfloat y,GLfloat z){float len=std::sqrt(x*x+y*y+z*z);if(len==0)return;x/=len;y/=len;z/=len;float c=std::cos(a*.0174532925199433f),s=std::sin(a*.0174532925199433f),t=1-c;Matrix m={t*x*x+c,t*x*y+s*z,t*x*z-s*y,0,t*x*y-s*z,t*y*y+c,t*y*z+s*x,0,t*x*z+s*y,t*y*z-s*x,t*z*z+c,0,0,0,0,1};glMultMatrixf(m.data());}
void glOrtho(GLdouble l,GLdouble r,GLdouble b,GLdouble t,GLdouble n,GLdouble f){Matrix m=identity();m[0]=float(2/(r-l));m[5]=float(2/(t-b));m[10]=float(-2/(f-n));m[12]=float(-(r+l)/(r-l));m[13]=float(-(t+b)/(t-b));m[14]=float(-(f+n)/(f-n));glMultMatrixf(m.data());}
void glPushAttrib(GLbitfield){attributes.push_back(state);}
void glPopAttrib(){if(!attributes.empty()){state=attributes.back();attributes.pop_back();}}
void glPushClientAttrib(GLbitfield){clients.push_back(client);}
void glPopClientAttrib(){if(!clients.empty()){client=clients.back();clients.pop_back();}}
void glGetIntegerv(GLenum key,GLint* p){switch(key){case GL_VIEWPORT:std::copy(state.viewport.begin(),state.viewport.end(),p);break;case GL_SCISSOR_BOX:std::copy(state.scissor.begin(),state.scissor.end(),p);break;case GL_POLYGON_MODE:p[0]=p[1]=state.polygon;break;case GL_FRONT_FACE:*p=state.frontFace;break;case GL_CULL_FACE_MODE:*p=state.cull;break;case GL_MATRIX_MODE:*p=state.matrixMode;break;case GL_TEXTURE_BINDING_2D:*p=state.texture;break;case GL_SHADE_MODEL:*p=state.shade;break;case GL_UNPACK_ALIGNMENT:*p=client.alignment;break;case GL_UNPACK_ROW_LENGTH:*p=client.rowLength;break;case GL_DEPTH_BITS:*p=currentDepthBits();break;case GL_STENCIL_BITS:*p=currentDepthHasStencil()?8:0;break;default:*p=0;lastError=GL_INVALID_ENUM;}}
static size_t copyFloatState(GLenum key,GLfloat* p){
    if(key==GL_CURRENT_TEXTURE_COORDS){
        std::copy(state.textureCoordinate.begin(),state.textureCoordinate.end(),p);return 4;
    }
    if(key==GL_MODELVIEW_MATRIX||key==GL_PROJECTION_MATRIX||key==GL_TEXTURE_MATRIX){
        const auto& m=matrices[key==GL_PROJECTION_MATRIX?1:key==GL_TEXTURE_MATRIX?2:0].back();
        std::copy(m.begin(),m.end(),p);return m.size();
    }
    // Unknown queries must not write a matrix into a possibly scalar destination.
    lastError=GL_INVALID_ENUM;return 0;
}
void glGetFloatv(GLenum key,GLfloat* p){copyFloatState(key,p);}
void glGetDoublev(GLenum key,GLdouble* p){float values[16];const size_t count=copyFloatState(key,values);std::copy_n(values,count,p);}
void glGetTexEnviv(GLenum,GLenum,GLint* p){*p=state.texEnv;}
void glGetTexLevelParameteriv(GLenum,GLint level,GLenum key,GLint* value) {
    *value=0;
    if(level<0) {lastError=GL_INVALID_VALUE;return;}
    if(key!=GL_TEXTURE_WIDTH&&key!=GL_TEXTURE_HEIGHT) {lastError=GL_INVALID_ENUM;return;}
    const auto found=textures.find(state.texture);
    if(found==textures.end()||!found->second.object)return;
    auto* texture=found->second.object;
    D3DSURFACE_DESC description{};
    if(UINT(level)>=texture->GetLevelCount()||FAILED(texture->GetLevelDesc(UINT(level),&description)))return;
    *value=GLint(key==GL_TEXTURE_WIDTH?description.Width:description.Height);
}
void glGetTexParameteriv(GLenum,GLenum key,GLint* value) {
    *value=0;
    const auto found=textures.find(state.texture);
    if(found==textures.end())return;
    const auto& texture=found->second;
    if(key==GL_TEXTURE_MIN_FILTER)*value=texture.minFilter;
    else if(key==GL_TEXTURE_MAG_FILTER)*value=texture.magFilter;
    else if(key==GL_TEXTURE_WRAP_S)*value=texture.wrapS;
    else if(key==GL_TEXTURE_WRAP_T)*value=texture.wrapT;
    else if(key==StorylandTextureMaxLevel)*value=texture.maximumLevel;
    else lastError=GL_INVALID_ENUM;
}
GLenum glGetError(){GLenum r=lastError;lastError=GL_NO_ERROR;return r;}
void glGenTextures(GLsizei n,GLuint* p){for(int i=0;i<n;++i){p[i]=nextTexture++;textures.emplace(p[i],Texture{});}}
void glDeleteTextures(GLsizei n,const GLuint* p){for(int i=0;i<n;++i){auto it=textures.find(p[i]);if(it!=textures.end()){release(it->second.object);textures.erase(it);}if(state.texture==p[i])state.texture=0;}}
void glBindTexture(GLenum,GLuint id){state.texture=id;if(id)textures.try_emplace(id);}
void glTexParameteri(GLenum,GLenum key,GLint value) {
    auto& texture=textures[state.texture];
    if(key==GL_TEXTURE_MIN_FILTER)texture.minFilter=value;
    else if(key==GL_TEXTURE_MAG_FILTER)texture.magFilter=value;
    else if(key==GL_TEXTURE_WRAP_S)texture.wrapS=value;
    else if(key==GL_TEXTURE_WRAP_T)texture.wrapT=value;
    else if(key==StorylandTextureMaxLevel) {
        if(value<0)lastError=GL_INVALID_VALUE;
        else texture.maximumLevel=value;
    }
}
void glTexEnvi(GLenum,GLenum,GLint value){state.texEnv=value;}
void glPixelStorei(GLenum key,GLint value){if(key==GL_UNPACK_ALIGNMENT)client.alignment=value;else if(key==GL_UNPACK_ROW_LENGTH)client.rowLength=value;}
void glTexImage2D(GLenum,GLint level,GLint,GLsizei w,GLsizei h,GLint,GLenum format,GLenum type,const GLvoid* pixels){
    if(level<0||w<=0||h<=0||type!=GL_UNSIGNED_BYTE||
        (format!=GL_RGBA&&format!=GL_RGB&&format!=0x80E1u)) {
        lastError=GL_INVALID_VALUE;
        return;
    }
    auto& texture=textures[state.texture];
    IDirect3DTexture9* fresh=nullptr;
    IDirect3DTexture9* target=texture.object;
    if(level==0) {
        int completeLevels=1;
        for(int span=std::max(w,h);span>1;span/=2)++completeLevels;
        const UINT levels=UINT(1+std::min(texture.maximumLevel,completeLevels-1));
        check(device->CreateTexture(w,h,levels,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&fresh,nullptr),
            "Direct3D texture allocation");
        target=fresh;
    } else {
        D3DSURFACE_DESC description{};
        if(!target||UINT(level)>=target->GetLevelCount()||
            FAILED(target->GetLevelDesc(UINT(level),&description))||
            description.Width!=UINT(w)||description.Height!=UINT(h)) {
            lastError=GL_INVALID_VALUE;
            return;
        }
    }
    D3DLOCKED_RECT lock{};
    HRESULT hr=target->LockRect(UINT(level),&lock,nullptr,0);
    if(FAILED(hr)){release(fresh);check(hr,"Direct3D texture lock");}
    const int channels=format==GL_RGB?3:4,alignment=std::max(1,client.alignment);
    size_t row=size_t(client.rowLength?client.rowLength:w)*channels;row=(row+alignment-1)/alignment*alignment;
    for(int y=0;y<h;++y){auto* dst=reinterpret_cast<DWORD*>(static_cast<unsigned char*>(lock.pBits)+size_t(y)*lock.Pitch);
        const auto* src=pixels?static_cast<const unsigned char*>(pixels)+size_t(y)*row:nullptr;
        for(int x=0;x<w;++x){if(!src){dst[x]=0;continue;}const auto* p=src+x*channels;dst[x]=format==0x80E1u?D3DCOLOR_ARGB(p[3],p[2],p[1],p[0]):D3DCOLOR_ARGB(channels==4?p[3]:255,p[0],p[1],p[2]);}}
    target->UnlockRect(UINT(level));
    if(fresh) {
        release(texture.object);
        texture.object=fresh;
    }
}
void glTexSubImage2D(GLenum,GLint level,GLint x,GLint y,GLsizei w,GLsizei h,
    GLenum format,GLenum type,const GLvoid* pixels) {
    if(level<0||x<0||y<0||w<0||h<0||type!=GL_UNSIGNED_BYTE||
        (format!=GL_RGBA&&format!=GL_RGB&&format!=0x80E1u)) {
        lastError=GL_INVALID_VALUE;
        return;
    }
    if(!w||!h)return;
    const auto found=textures.find(state.texture);
    D3DSURFACE_DESC description{};
    if(!pixels||found==textures.end()||!found->second.object||
        UINT(level)>=found->second.object->GetLevelCount()||
        FAILED(found->second.object->GetLevelDesc(UINT(level),&description))||
        uint64_t(x)+unsigned(w)>description.Width||uint64_t(y)+unsigned(h)>description.Height) {
        lastError=GL_INVALID_VALUE;
        return;
    }
    auto* texture=found->second.object;
    D3DLOCKED_RECT lock{};
    check(texture->LockRect(UINT(level),&lock,nullptr,0),"Direct3D texture update lock");
    const int channels=format==GL_RGB?3:4,alignment=std::max(1,client.alignment);
    size_t row=size_t(client.rowLength?client.rowLength:w)*channels;
    row=(row+alignment-1)/alignment*alignment;
    for(int destinationY=0;destinationY<h;++destinationY) {
        auto* destination=reinterpret_cast<DWORD*>(static_cast<uint8_t*>(lock.pBits)+
            size_t(y+destinationY)*size_t(lock.Pitch))+x;
        const auto* source=static_cast<const uint8_t*>(pixels)+size_t(destinationY)*row;
        for(int destinationX=0;destinationX<w;++destinationX) {
            const auto* pixel=source+size_t(destinationX)*size_t(channels);
            destination[destinationX]=format==0x80E1u?D3DCOLOR_ARGB(pixel[3],pixel[2],pixel[1],pixel[0]):
                D3DCOLOR_ARGB(channels==4?pixel[3]:255,pixel[0],pixel[1],pixel[2]);
        }
    }
    texture->UnlockRect(UINT(level));
}
void glVertexPointer(GLint size,GLenum type,GLsizei stride,const GLvoid* p){client.position={size,type,stride,static_cast<const unsigned char*>(p)};}
void glColorPointer(GLint size,GLenum type,GLsizei stride,const GLvoid* p){client.color={size,type,stride,static_cast<const unsigned char*>(p)};}
void glNormalPointer(GLenum type,GLsizei stride,const GLvoid* p) {
    if(stride<0) {
        lastError=GL_INVALID_VALUE;
        return;
    }
    if(type!=GL_BYTE&&type!=GL_SHORT&&type!=GL_INT&&type!=GL_FLOAT&&type!=GL_DOUBLE) {
        lastError=GL_INVALID_ENUM;
        return;
    }
    client.normal={3,type,stride,static_cast<const unsigned char*>(p)};
}
void glTexCoordPointer(GLint size,GLenum type,GLsizei stride,const GLvoid* p){client.uv={size,type,stride,static_cast<const unsigned char*>(p)};}
static float component(const Pointer& p,size_t index,int c,float fallback){if(!p.data||c>=p.size)return fallback;size_t unit=p.type==GL_UNSIGNED_BYTE?1:4;const auto* at=p.data+index*(p.stride?p.stride:p.size*unit)+c*unit;if(p.type==GL_UNSIGNED_BYTE)return *at/255.f;float f;std::memcpy(&f,at,4);return f;}
template<class T>
float normalComponentValue(const Pointer& pointer,size_t index,int channel,double divisor) {
    const size_t stride=pointer.stride?size_t(pointer.stride):3u*sizeof(T);
    const auto* address=pointer.data+index*stride+size_t(channel)*sizeof(T);
    T value;
    std::memcpy(&value,address,sizeof(value));
    const double normalized=double(value)/divisor;
    return float(divisor==1.0?normalized:std::max(-1.0,normalized));
}
static float normalComponent(const Pointer& pointer,size_t index,int channel,float fallback) {
    if(!pointer.data)return fallback;
    switch(pointer.type) {
    case GL_BYTE:return normalComponentValue<int8_t>(pointer,index,channel,127.0);
    case GL_SHORT:return normalComponentValue<int16_t>(pointer,index,channel,32767.0);
    case GL_INT:return normalComponentValue<int32_t>(pointer,index,channel,2147483647.0);
    case GL_FLOAT:return normalComponentValue<float>(pointer,index,channel,1.0);
    case GL_DOUBLE:return normalComponentValue<double>(pointer,index,channel,1.0);
    default:return fallback;
    }
}
static Vertex arrayVertex(size_t index) {
    Vertex vertex=state.vertex;
    vertex.x=component(client.position,index,0,0);
    vertex.y=component(client.position,index,1,0);
    vertex.z=component(client.position,index,2,0);
    if(client.enabled.count(GL_COLOR_ARRAY)) {
        vertex.color=color(component(client.color,index,0,1),component(client.color,index,1,1),
            component(client.color,index,2,1),component(client.color,index,3,1));
    }
    if(client.enabled.count(GL_NORMAL_ARRAY)) {
        vertex.nx=normalComponent(client.normal,index,0,vertex.nx);
        vertex.ny=normalComponent(client.normal,index,1,vertex.ny);
        vertex.nz=normalComponent(client.normal,index,2,vertex.nz);
    }
    if(client.enabled.count(GL_TEXTURE_COORD_ARRAY)) {
        vertex.u=component(client.uv,index,0,0);
        vertex.v=component(client.uv,index,1,0);
        vertex.u1=component(client.uv,index,2,vertex.u);
        vertex.v1=component(client.uv,index,3,vertex.v);
    }
    return vertex;
}
void glDrawArrays(GLenum mode,GLint first,GLsizei count){if(first<0||count<0)return;vertices.clear();for(int i=0;i<count;++i)vertices.push_back(arrayVertex(size_t(first)+i));submit(mode,vertices);vertices.clear();}
void glDrawElements(GLenum mode,GLsizei count,GLenum type,const GLvoid* indices){if(count<=0||!indices)return;vertices.clear();for(int i=0;i<count;++i){size_t index=type==GL_UNSIGNED_SHORT?static_cast<const unsigned short*>(indices)[i]:type==GL_UNSIGNED_INT?static_cast<const unsigned int*>(indices)[i]:static_cast<const unsigned char*>(indices)[i];vertices.push_back(arrayVertex(index));}submit(mode,vertices);vertices.clear();}
void glDepthFunc(GLenum value){state.depthFunc=value;}void glDepthMask(GLboolean value){state.depthWrite=value!=0;}
void glBlendFunc(GLenum src,GLenum dst){state.src=src;state.dst=dst;}void glAlphaFunc(GLenum func,GLclampf ref){state.alphaFunc=func;state.alphaRef=ref;}
void glPolygonMode(GLenum,GLenum mode){state.polygon=mode;}void glPolygonOffset(GLfloat factor,GLfloat units){state.offsetFactor=factor;state.offset=units;}void glCullFace(GLenum face){state.cull=face;}
void glFrontFace(GLenum winding) {
    if(winding!=GL_CCW&&winding!=GL_CW){lastError=GL_INVALID_ENUM;return;}
    state.frontFace=winding;
}
void glLineWidth(GLfloat value){state.line=value;}void glPointSize(GLfloat value){state.point=value;}void glShadeModel(GLenum value){state.shade=value;}void glHint(GLenum,GLenum){}
void glFogf(GLenum key,GLfloat value){if(key==GL_FOG_START)state.fogStart=value;else if(key==GL_FOG_END)state.fogEnd=value;}void glFogi(GLenum key,GLint value){if(key==GL_FOG_MODE)state.fogMode=value;}void glFogfv(GLenum key,const GLfloat* p){if(key==GL_FOG_COLOR)std::copy_n(p,4,state.fog.begin());}
void glLightModelfv(GLenum key,const GLfloat* p){if(key==GL_LIGHT_MODEL_AMBIENT)std::copy_n(p,4,state.ambient.begin());}
void glLightfv(GLenum light,GLenum key,const GLfloat* p){if(light!=GL_LIGHT0)return;if(key==GL_DIFFUSE)std::copy_n(p,4,state.lightDiffuse.begin());else if(key==GL_POSITION){const auto& m=matrices[0].back();for(int row=0;row<4;++row){state.lightPosition[row]=0;for(int k=0;k<4;++k)state.lightPosition[row]+=m[k*4+row]*p[k];}}}
void glMaterialf(GLenum,GLenum,GLfloat){}void glMaterialfv(GLenum,GLenum,const GLfloat*){}void glColorMaterial(GLenum,GLenum){}
GLuint glGenLists(GLsizei range){GLuint first=nextList;nextList+=std::max(0,range);return first;}
void glDeleteLists(GLuint list,GLsizei range){for(int i=0;i<range;++i){auto it=glyphs.find(list+i);if(it!=glyphs.end()){glDeleteTextures(1,&it->second.texture);glyphs.erase(it);}}}
void glListBase(GLuint base){state.listBase=base;}
void glRasterPos2f(GLfloat x,GLfloat y){Matrix m=multiply(matrices[1].back(),matrices[0].back());float c[4];for(int r=0;r<4;++r)c[r]=m[r]*x+m[4+r]*y+m[12+r];if(c[3]==0)return;state.rasterX=state.viewport[0]+(c[0]/c[3]+1)*state.viewport[2]*.5f;state.rasterY=state.viewport[1]+(c[1]/c[3]+1)*state.viewport[3]*.5f;state.rasterZ=c[2]/c[3];}
void glRasterPos2i(GLint x,GLint y){glRasterPos2f(float(x),float(y));}
// Bitmap text and the legacy raster canvas are textured quads, never framebuffer readbacks.
static void bitmapQuad(GLuint texture,float x,float y,float w,float h,bool topDown){
    State saved=state;Shader* oldSurface=surface;surface=nullptr;
    auto oldMv=matrices[0].back(),oldProj=matrices[1].back();
    state.matrixMode=GL_MODELVIEW;glLoadIdentity();state.matrixMode=GL_PROJECTION;glLoadIdentity();
    glOrtho(state.viewport[0],state.viewport[0]+state.viewport[2],state.viewport[1],state.viewport[1]+state.viewport[3],-1,1);
    state.texture=texture;state.enabled.insert(GL_TEXTURE_2D);state.enabled.erase(GL_LIGHTING);state.enabled.erase(GL_FOG);
    glBegin(GL_QUADS);glTexCoord2f(0,topDown?1.f:0.f);glVertex3f(x,y,-state.rasterZ);glTexCoord2f(1,topDown?1.f:0.f);glVertex3f(x+w,y,-state.rasterZ);glTexCoord2f(1,topDown?0.f:1.f);glVertex3f(x+w,y+h,-state.rasterZ);glTexCoord2f(0,topDown?0.f:1.f);glVertex3f(x,y+h,-state.rasterZ);glEnd();
    matrices[0].back()=oldMv;matrices[1].back()=oldProj;state=saved;surface=oldSurface;
}
BOOL wglUseFontBitmapsW(HDC dc,DWORD first,DWORD count,DWORD base){
    const GLuint oldTexture=state.texture;const Client oldClient=client;client.alignment=1;client.rowLength=0;
    MAT2 transform{{0,1},{0,0},{0,0},{0,1}};
    for(DWORD i=0;i<count;++i){GLYPHMETRICS metrics{};DWORD size=GetGlyphOutlineW(dc,first+i,GGO_GRAY8_BITMAP,&metrics,0,nullptr,&transform);
        if(size==GDI_ERROR){glDeleteLists(base,GLsizei(i));state.texture=oldTexture;client=oldClient;return FALSE;}
        Glyph glyph;glyph.width=int(metrics.gmBlackBoxX);glyph.height=int(metrics.gmBlackBoxY);glyph.advance=metrics.gmCellIncX;glyph.originX=int(metrics.gmptGlyphOrigin.x);glyph.originY=int(metrics.gmptGlyphOrigin.y);
        if(size&&glyph.width&&glyph.height){std::vector<unsigned char> data(size);GetGlyphOutlineW(dc,first+i,GGO_GRAY8_BITMAP,&metrics,size,data.data(),&transform);
            std::vector<unsigned char> rgba(size_t(glyph.width)*glyph.height*4,255);size_t pitch=(glyph.width+3)&~size_t(3);
            for(int y=0;y<glyph.height;++y)for(int x=0;x<glyph.width;++x)rgba[(size_t(y)*glyph.width+x)*4+3]=static_cast<unsigned char>(std::min(255,int(data[size_t(y)*pitch+x])*255/64));
            glGenTextures(1,&glyph.texture);glBindTexture(GL_TEXTURE_2D,glyph.texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,glyph.width,glyph.height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());}
        glyphs[base+i]=glyph;
    }state.texture=oldTexture;client=oldClient;return TRUE;
}
void glCallLists(GLsizei n,GLenum type,const GLvoid* p){if(type!=GL_UNSIGNED_BYTE||!p)return;for(int i=0;i<n;++i){auto it=glyphs.find(state.listBase+static_cast<const unsigned char*>(p)[i]);if(it==glyphs.end())continue;const auto& g=it->second;if(g.texture)bitmapQuad(g.texture,state.rasterX+g.originX,state.rasterY+g.originY-g.height,float(g.width),float(g.height),true);state.rasterX+=g.advance;}}
void glDrawPixels(GLsizei w,GLsizei h,GLenum format,GLenum type,const GLvoid* p){State saved=state;GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,format,type,p);glColor4f(1,1,1,1);bitmapQuad(texture,state.rasterX,state.rasterY,float(w),float(h),false);glDeleteTextures(1,&texture);state=saved;}
void glFlush(){}
void glFinish(){if(!device)return;IDirect3DQuery9* query=nullptr;if(SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_EVENT,&query))){query->Issue(D3DISSUE_END);while(query->GetData(nullptr,0,D3DGETDATA_FLUSH)==S_FALSE){if(device->TestCooperativeLevel()!=D3D_OK)break;SwitchToThread();}release(query);}}
}
