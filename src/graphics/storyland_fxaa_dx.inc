void setGraphicsQuality(const StorylandGraphicsQualitySettings& settings, bool targetProfile) {
    graphicsQuality = settings;
    graphicsQualityTarget = targetProfile;
}

float maximumAnisotropy() {
    return deviceCapabilities.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC ?
        float(std::max<DWORD>(1, deviceCapabilities.MaxAnisotropy)) : 1.0f;
}

void drawFxaa() {
    if (!graphicsQuality.fxaa || !device || !ready()) return;
    IDirect3DStateBlock9* savedState = nullptr;
    std::array<IDirect3DSurface9*, 4> savedTargets{};
    IDirect3DSurface9* savedDepth = nullptr;
    D3DVIEWPORT9 viewport{};
    bool viewportCaptured=false,targetsConfigured=false;
    D3DSURFACE_DESC description{};
    HRESULT result = device->CreateStateBlock(D3DSBT_ALL, &savedState);
    const auto record = [&](HRESULT value) { if (SUCCEEDED(result) && FAILED(value)) result = value; };
    if (SUCCEEDED(result)) record(savedState->Capture());
    const unsigned targets = std::min<unsigned>(4, std::max<DWORD>(1, deviceCapabilities.NumSimultaneousRTs));
    for (unsigned index = 0; index < targets; ++index) {
        const HRESULT value = device->GetRenderTarget(index, &savedTargets[index]);
        if (index == 0 || value != D3DERR_NOTFOUND) record(value);
    }
    const HRESULT depthResult = device->GetDepthStencilSurface(&savedDepth);
    if (depthResult != D3DERR_NOTFOUND) record(depthResult);
    if (SUCCEEDED(result)) {
        const HRESULT value=device->GetViewport(&viewport);
        viewportCaptured=SUCCEEDED(value);
        record(value);
    }
    if (SUCCEEDED(result)) record(savedTargets[0]->GetDesc(&description));
    if (SUCCEEDED(result) && !fxaaPixelShader) {
        ID3DBlob* binary = nullptr;
        ID3DBlob* messages = nullptr;
        const char* source = storylandFxaaDxShader();
        record(D3DCompile(source, std::strlen(source), "StorylandFXAA", nullptr, nullptr,
            "main", "ps_3_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &binary, &messages));
        if (SUCCEEDED(result)) record(device->CreatePixelShader(
            reinterpret_cast<const DWORD*>(binary->GetBufferPointer()), &fxaaPixelShader));
        if (FAILED(result) && messages) diagnostic.assign(
            static_cast<const char*>(messages->GetBufferPointer()), messages->GetBufferSize());
        release(binary);
        release(messages);
    }
    if (SUCCEEDED(result)) {
        D3DSURFACE_DESC existing{};
        if (fxaaSceneTexture) fxaaSceneTexture->GetLevelDesc(0, &existing);
        if (existing.Width != description.Width || existing.Height != description.Height || existing.Format != description.Format) {
            release(fxaaSceneTexture);
            record(device->CreateTexture(description.Width, description.Height, 1, D3DUSAGE_RENDERTARGET,
                description.Format, D3DPOOL_DEFAULT, &fxaaSceneTexture, nullptr));
        }
    }
    const bool previousScene = inScene;
    if (SUCCEEDED(result)) {
        endScene();
        IDirect3DSurface9* capture = nullptr;
        record(fxaaSceneTexture->GetSurfaceLevel(0, &capture));
        if (SUCCEEDED(result)) record(device->StretchRect(savedTargets[0], nullptr, capture, nullptr, D3DTEXF_NONE));
        release(capture);
    }
    if (SUCCEEDED(result)) {
        targetsConfigured=true;
        for (unsigned index = 1; index < targets; ++index) record(device->SetRenderTarget(index, nullptr));
        record(device->SetDepthStencilSurface(nullptr));
        record(device->SetViewport(&viewport));
        record(device->SetVertexShader(nullptr));
        record(device->SetPixelShader(fxaaPixelShader));
        record(device->SetTexture(0, fxaaSceneTexture));
        record(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0));
        record(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
        for (unsigned index = 1; index < 16; ++index) record(device->SetTexture(index, nullptr));
        const float texel[4]{1.0f / float(description.Width), 1.0f / float(description.Height), 0, 0};
        const float transform[4]{float(viewport.Width) * texel[0], float(viewport.Height) * texel[1],
            float(viewport.X) * texel[0], float(viewport.Y) * texel[1]};
        const float bounds[4]{(float(viewport.X) + .5f) * texel[0], (float(viewport.Y) + .5f) * texel[1],
            (float(viewport.X + viewport.Width) - .5f) * texel[0],
            (float(viewport.Y + viewport.Height) - .5f) * texel[1]};
        record(device->SetPixelShaderConstantF(0, texel, 1));
        record(device->SetPixelShaderConstantF(1, transform, 1));
        record(device->SetPixelShaderConstantF(2, bounds, 1));
        for (const auto option : {D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_STENCILENABLE,
            D3DRS_ALPHATESTENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_FOGENABLE,
            D3DRS_LIGHTING, D3DRS_SCISSORTESTENABLE, D3DRS_SRGBWRITEENABLE,
            D3DRS_CLIPPLANEENABLE, D3DRS_DITHERENABLE}) record(device->SetRenderState(option, FALSE));
        record(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
        record(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID));
        record(device->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xffffffff));
        record(device->SetRenderState(D3DRS_WRAP0, 0));
        record(device->SetRenderState(D3DRS_COLORWRITEENABLE,
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE));
        record(device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
        record(device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
        record(device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
        record(device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1));
        record(device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE));
        record(device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
        record(device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
        record(device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0));
        record(device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0));
        struct ScreenVertex { float x, y, z, rhw, u, v; };
        const float left = float(viewport.X) - .5f, top = float(viewport.Y) - .5f;
        const float right = left + float(viewport.Width), bottom = top + float(viewport.Height);
        const ScreenVertex quad[]{{left, top, 0, 1, 0, 0}, {right, top, 0, 1, 1, 0},
            {left, bottom, 0, 1, 0, 1}, {right, bottom, 0, 1, 1, 1}};
        record(device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1));
        if (SUCCEEDED(result)) {
            record(device->BeginScene());
            if (SUCCEEDED(result)) inScene = true;
        }
        if (SUCCEEDED(result)) record(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex)));
        endScene();
    }
    if (targetsConfigured&&savedState) record(savedState->Apply());
    for (unsigned index = 0; index < targets; ++index) {
        if (targetsConfigured&&(index != 0 || savedTargets[index]))
            record(device->SetRenderTarget(index, savedTargets[index]));
        release(savedTargets[index]);
    }
    if(targetsConfigured)record(device->SetDepthStencilSurface(savedDepth));
    release(savedDepth);
    if (targetsConfigured&&viewportCaptured) record(device->SetViewport(&viewport));
    release(savedState);
    if (previousScene && !inScene) {
        const HRESULT value = device->BeginScene();
        record(value);
        if (SUCCEEDED(value)) inScene = true;
    }
    if (FAILED(result)) {
        lastError = GL_INVALID_OPERATION;
        if (diagnostic.empty()) diagnostic = "Direct3D FXAA failed (HRESULT " +
            std::to_string(static_cast<unsigned long>(result)) + ")";
    }
}
