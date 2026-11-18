void drawPs2PostEffects(const StorylandPs2PostEffects& settings) {
    if(!settings.enabled()||!device)return;
    const bool previousScene=inScene;
    if(!ready())return;
    IDirect3DStateBlock9* previousState=nullptr;
    std::array<IDirect3DSurface9*,4> previousTargets{};
    IDirect3DSurface9* previousDepth=nullptr;
    IDirect3DSurface9* captureSurface=nullptr;
    D3DSURFACE_DESC description{};
    D3DVIEWPORT9 previousViewport{};
    D3DCAPS9 caps{};
    HRESULT result=device->GetDeviceCaps(&caps);
    const DWORD targetCount=std::clamp<DWORD>(caps.NumSimultaneousRTs,1,4);
    for(DWORD index=0;SUCCEEDED(result)&&index<targetCount;++index) {
        result=device->GetRenderTarget(index,&previousTargets[index]);
        if(index&&result==D3DERR_NOTFOUND)result=S_OK;
    }
    if(SUCCEEDED(result))result=previousTargets[0]->GetDesc(&description);
    if(SUCCEEDED(result))result=device->GetViewport(&previousViewport);
    if(SUCCEEDED(result)) {
        result=device->GetDepthStencilSurface(&previousDepth);
        if(result==D3DERR_NOTFOUND)result=S_OK;
    }
    if(SUCCEEDED(result)&&(!previousViewport.Width||!previousViewport.Height))result=D3DERR_INVALIDCALL;
    std::string compileError;
    if(SUCCEEDED(result)&&!ps2PostEffectsShader) {
        ID3DBlob* code=nullptr;
        ID3DBlob* errors=nullptr;
        result=D3DCompile(StorylandPs2PostEffectsFragmentDx,std::strlen(StorylandPs2PostEffectsFragmentDx),
            "Storyland VCS PS2 post effects",nullptr,nullptr,"PSMain","ps_3_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&code,&errors);
        if(FAILED(result)&&errors)
            compileError.assign(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize());
        if(SUCCEEDED(result))result=device->CreatePixelShader(
            static_cast<const DWORD*>(code->GetBufferPointer()),&ps2PostEffectsShader);
        release(code);release(errors);
    }
    const std::array<UINT,4> widths{description.Width,256,256,512};
    const std::array<UINT,4> heights{description.Height,256,256,512};
    for(unsigned index=0;SUCCEEDED(result)&&index<ps2PostEffectsTextures.size();++index) {
        auto& texture=ps2PostEffectsTextures[index];
        const D3DFORMAT format=index?D3DFMT_A8R8G8B8:description.Format;
        D3DSURFACE_DESC currentDescription{};
        if(texture&&(FAILED(texture->GetLevelDesc(0,&currentDescription))||
            currentDescription.Width!=widths[index]||currentDescription.Height!=heights[index]||
            currentDescription.Format!=format))release(texture);
        if(!texture)result=device->CreateTexture(widths[index],heights[index],1,D3DUSAGE_RENDERTARGET,
            format,D3DPOOL_DEFAULT,&texture,nullptr);
    }
    if(SUCCEEDED(result))result=ps2PostEffectsTextures[0]->GetSurfaceLevel(0,&captureSurface);
    if(SUCCEEDED(result))result=device->CreateStateBlock(D3DSBT_ALL,&previousState);
    if(SUCCEEDED(result))result=previousState->Capture();
    bool changedState=false;
    auto record=[&](HRESULT value) {if(SUCCEEDED(result)&&FAILED(value))result=value;};
    auto copyScene=[&]() {
        if(FAILED(result))return;
        endScene();record(device->SetTexture(0,nullptr));record(device->SetTexture(1,nullptr));
        if(SUCCEEDED(result))record(device->StretchRect(previousTargets[0],nullptr,captureSurface,nullptr,D3DTEXF_NONE));
    };
    const DWORD rgbWrite=D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE;
    const std::array<float,4> captureUvScaleBias{
        float(previousViewport.Width)/float(std::max(1u,description.Width)),
        float(previousViewport.Height)/float(std::max(1u,description.Height)),
        float(previousViewport.X)/float(std::max(1u,description.Width)),
        float(previousViewport.Y)/float(std::max(1u,description.Height))};
    const std::array<float,4> captureUvBounds{
        (float(previousViewport.X)+0.5f)/float(std::max(1u,description.Width)),
        (float(previousViewport.Y)+0.5f)/float(std::max(1u,description.Height)),
        (float(previousViewport.X+previousViewport.Width)-0.5f)/float(std::max(1u,description.Width)),
        (float(previousViewport.Y+previousViewport.Height)-0.5f)/float(std::max(1u,description.Height))};
    const std::array<float,4> filterColor{float(settings.filter[0]),float(settings.filter[1]),float(settings.filter[2]),0};
    auto render=[&](IDirect3DTexture9* source,IDirect3DSurface9* target,
        const D3DVIEWPORT9& viewport,float operation,float x,float y,float z,bool finalPass) {
        if(FAILED(result))return;
        record(device->SetTexture(0,nullptr));record(device->SetTexture(1,nullptr));
        record(device->SetRenderTarget(0,target));record(device->SetViewport(&viewport));
        record(device->SetRenderState(D3DRS_COLORWRITEENABLE,finalPass?rgbWrite:rgbWrite|D3DCOLORWRITEENABLE_ALPHA));
        const std::array<float,4> parameters{operation,x,y,z};
        record(device->SetPixelShaderConstantF(0,parameters.data(),1));
        record(device->SetTexture(0,source));record(device->SetTexture(1,ps2PostEffectsTextures[0]));
        if(SUCCEEDED(result)&&!inScene) {
            record(device->BeginScene());if(SUCCEEDED(result))inScene=true;
        }
        const float left=float(viewport.X)-0.5f,top=float(viewport.Y)-0.5f;
        const float right=left+float(viewport.Width),bottom=top+float(viewport.Height);
        struct PostEffectsVertex {float x,y,z,rhw,u,v;};
        const PostEffectsVertex quad[]={{left,top,0,1,0,0},{right,top,0,1,1,0},
            {left,bottom,0,1,0,1},{right,bottom,0,1,1,1}};
        if(SUCCEEDED(result))record(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(PostEffectsVertex)));
    };
    auto renderTexture=[&](unsigned targetIndex,IDirect3DTexture9* source,float operation,float x=0,float y=0,float z=0) {
        if(FAILED(result))return;
        IDirect3DSurface9* target=nullptr;
        record(ps2PostEffectsTextures[targetIndex]->GetSurfaceLevel(0,&target));
        const D3DVIEWPORT9 viewport{0,0,widths[targetIndex],heights[targetIndex],0,1};
        if(SUCCEEDED(result))render(source,target,viewport,operation,x,y,z,false);
        release(target);
    };
    if(SUCCEEDED(result)) {
        changedState=true;copyScene();record(device->SetDepthStencilSurface(nullptr));
        for(DWORD index=1;index<targetCount;++index)record(device->SetRenderTarget(index,nullptr));
        record(device->SetVertexShader(nullptr));record(device->SetPixelShader(ps2PostEffectsShader));
        record(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
        const std::pair<D3DRENDERSTATETYPE,DWORD> states[]={
            {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_LIGHTING,FALSE},
            {D3DRS_FOGENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,FALSE},
            {D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},
            {D3DRS_STENCILENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_FILLMODE,D3DFILL_SOLID},
            {D3DRS_SHADEMODE,D3DSHADE_GOURAUD},{D3DRS_DITHERENABLE,FALSE},
            {D3DRS_MULTISAMPLEANTIALIAS,FALSE},{D3DRS_SRGBWRITEENABLE,FALSE},
            {D3DRS_CLIPPLANEENABLE,0},{D3DRS_WRAP0,0}};
        for(const auto& setting:states)record(device->SetRenderState(setting.first,setting.second));
        for(DWORD sampler=0;sampler<2;++sampler) {
            record(device->SetSamplerState(sampler,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));
            record(device->SetSamplerState(sampler,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
            record(device->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
            record(device->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));
            record(device->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
            record(device->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE));
        }
        record(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));
        record(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
        record(device->SetPixelShaderConstantF(1,filterColor.data(),1));
        record(device->SetPixelShaderConstantF(2,captureUvScaleBias.data(),1));
        record(device->SetPixelShaderConstantF(3,captureUvBounds.data(),1));
        if(settings.radiosityIntensity) {
            renderTexture(1,ps2PostEffectsTextures[0],0,float(settings.radiosityThreshold));
            for(unsigned pass=0;pass<4;++pass)
                renderTexture(2-pass%2,ps2PostEffectsTextures[1+pass%2],pass%2?2.0f:1.0f);
            render(ps2PostEffectsTextures[1],previousTargets[0],previousViewport,3,float(settings.radiosityIntensity),0,0,true);
            copyScene();
        }
        if(settings.glowMix||settings.filter[0]||settings.filter[1]||settings.filter[2]) {
            renderTexture(3,ps2PostEffectsTextures[0],6);
            render(ps2PostEffectsTextures[3],previousTargets[0],previousViewport,4,
                settings.glowOffset/float(previousViewport.Width),settings.glowOffset/float(previousViewport.Height),float(settings.glowMix),true);
        }
    }
    if(changedState) {
        record(device->SetTexture(0,nullptr));record(device->SetTexture(1,nullptr));
        for(DWORD index=1;index<targetCount;++index)record(device->SetRenderTarget(index,nullptr));
        record(device->SetRenderTarget(0,previousTargets[0]));
        for(DWORD index=1;index<targetCount;++index)record(device->SetRenderTarget(index,previousTargets[index]));
        record(device->SetDepthStencilSurface(previousDepth));record(previousState->Apply());
        record(device->SetViewport(&previousViewport));
    }
    if(previousScene&&!inScene) {
        const HRESULT beginResult=device->BeginScene();record(beginResult);if(SUCCEEDED(beginResult))inScene=true;
    } else if(!previousScene)endScene();
    release(previousState);release(previousDepth);release(captureSurface);
    for(auto& target:previousTargets)release(target);
    if(FAILED(result))diagnostic="VCS PS2 post effects failed (HRESULT "+std::to_string(static_cast<unsigned long>(result))+")."+
        (compileError.empty()?std::string{}:" "+compileError);
}
