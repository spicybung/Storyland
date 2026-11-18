static void captureStorylandVehicleEnvironment(
    const std::vector<const StorylandArchiveBrowser*>& browsers,
    const StorylandNativePoint& eye,float farClip,
    const StorylandWorldDetailPlan* details=nullptr,
    const std::vector<uint8_t>* selected=nullptr,bool reflectivePreview=false) {
    auto& state=gVehicleEnvironment;
    glGetFloatv(GL_MODELVIEW_MATRIX,state.cameraView.data());
    if(!state.enabled) {state.ready=false;state.completedFaces=0;state.nextCaptureFace=0;return;}
    const bool directx=storylandDx::active();
    if(directx&&state.texture!=storylandDx::vehicleEnvironmentTexture()) {
        state.ready=false;
        state.completedFaces=0;
        state.nextCaptureFace=0;
    }
    if(!storylandVehicleEnvironmentHasConsumers(gStoriesConsoleProfile,state.viewerOpen,
        !gPlayTestVehicleAssets.empty(),!gPlayTestWeaponAssets.empty(),reflectivePreview))return;
    const bool progressive=gPlayTestActive&&gPlayTestGameMode&&!state.viewerOpen;
    const uint64_t captureTimeMs=GetTickCount64();
    const double uvSeconds = storylandWorldUvSeconds();
    const std::array<float,3> capturePosition{eye.x,eye.y,eye.z};
    if(!storylandVehicleEnvironmentProbeMoved(state,capturePosition,captureTimeMs,
        gPlayTestActive&&gPlayTestGameMode,progressive)||
        !storylandVehicleEnvironmentCaptureDue(state,captureTimeMs,
            gPlayTestActive&&gPlayTestGameMode,progressive))return;
    const unsigned firstFace=progressive?state.nextCaptureFace:0u;
    const unsigned endFace=progressive?firstFace+1u:6u;
    if(!directx&&!loadStoriesShaderFunctions()) {
        state.diagnostic="OpenGL environment capture functions are unavailable.";
        return;
    }
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    GLint previousProgram=0,previousActive=0,previousClient=0;
    using ClientActiveTextureProc=void(APIENTRY*)(GLenum);
    ClientActiveTextureProc clientActive=nullptr;
    if(!directx) {
        glGetIntegerv(0x8B8D,&previousProgram);
        glGetIntegerv(0x84E0,&previousActive);
        glGetIntegerv(0x84E1,&previousClient);
        clientActive=reinterpret_cast<ClientActiveTextureProc>(getOpenGlProcAddress("glClientActiveTexture"));
        GLint textureUnits=1;
        glGetIntegerv(0x84E2,&textureUnits);
        if(!pglActiveTexture)textureUnits=1;
        for(GLint unit=0;unit<textureUnits;++unit) {
            if(pglActiveTexture)pglActiveTexture(GL_TEXTURE0+unit);
            glDisable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,0);
            glDisable(GL_TEXTURE_1D);glDisable(0x806F);glDisable(0x8513);
        }
        if(pglActiveTexture)pglActiveTexture(GL_TEXTURE0);
        if(clientActive)clientActive(GL_TEXTURE0);
        auto& api=gVehicleEnvironmentTarget.api;
        api.genFramebuffers=reinterpret_cast<decltype(api.genFramebuffers)>(getOpenGlProcAddress("glGenFramebuffers"));
        api.bindFramebuffer=reinterpret_cast<decltype(api.bindFramebuffer)>(getOpenGlProcAddress("glBindFramebuffer"));
        api.framebufferTexture2D=reinterpret_cast<decltype(api.framebufferTexture2D)>(getOpenGlProcAddress("glFramebufferTexture2D"));
        api.checkFramebufferStatus=reinterpret_cast<decltype(api.checkFramebufferStatus)>(getOpenGlProcAddress("glCheckFramebufferStatus"));
        api.deleteFramebuffers=reinterpret_cast<decltype(api.deleteFramebuffers)>(getOpenGlProcAddress("glDeleteFramebuffers"));
        api.genRenderbuffers=reinterpret_cast<decltype(api.genRenderbuffers)>(getOpenGlProcAddress("glGenRenderbuffers"));
        api.bindRenderbuffer=reinterpret_cast<decltype(api.bindRenderbuffer)>(getOpenGlProcAddress("glBindRenderbuffer"));
        api.renderbufferStorage=reinterpret_cast<decltype(api.renderbufferStorage)>(getOpenGlProcAddress("glRenderbufferStorage"));
        api.framebufferRenderbuffer=reinterpret_cast<decltype(api.framebufferRenderbuffer)>(getOpenGlProcAddress("glFramebufferRenderbuffer"));
        api.deleteRenderbuffers=reinterpret_cast<decltype(api.deleteRenderbuffers)>(getOpenGlProcAddress("glDeleteRenderbuffers"));
    }
    const bool captureReady=directx?
        (storylandDx::beginVehicleEnvironmentCapture(state.faceSize,state.diagnostic)!=0):
        gVehicleEnvironmentTarget.begin(state.faceSize,state.diagnostic);
    if(!captureReady) {
        if(clientActive)clientActive(GLenum(previousClient));
        if(!directx&&pglActiveTexture)pglActiveTexture(GLenum(previousActive));
        glPopClientAttrib();glPopAttrib();
        return;
    }
    if(!progressive || firstFace==0u) {
        state.position={eye.x,eye.y,eye.z};
        state.renderedTriangles=0;
    }
    state.faceSize=std::clamp(state.faceSize,32,256);
    const auto projection=storylandEnvironmentProjection(std::min(farClip,250.0f));
    stopStoriesShaderProgram();
    glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glDepthMask(GL_TRUE);
    if(!directx)glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glViewport(0,0,state.faceSize*3,state.faceSize*2);
    const auto& sky=gStoriesSky.state();
    glClearColor(sky.fog.r,sky.fog.g,sky.fog.b,1.0f);
    if(!progressive)glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glMultMatrixf(projection.data());
    glMatrixMode(GL_MODELVIEW);glPushMatrix();
    struct EnvironmentArea {
        const std::map<uint64_t,const StorylandWorldMesh*>* meshes=nullptr;
        const std::map<uint32_t,std::vector<const StorylandDirectTextureResource*>>* materials=nullptr;
        std::vector<uint8_t> visible;
    };
    std::vector<EnvironmentArea> areas(browsers.size());
    const StorylandC2dWorldLightState worldLightState(gDtzArchive.leeds2dfxEffects(),
        gDtzArchive.leeds2dfxWorldInstances(),storylandWorldLightClock());
    for(size_t area=0;area<browsers.size();++area)if(browsers[area]) {
        const auto& browser=*browsers[area];
        auto& frame=areas[area];
        auto& meshes=residentWorldMeshLookup[&browser];
        if(meshes.empty())for(const auto& mesh:browser.worldMeshes())
            meshes.emplace((uint64_t(mesh.sectorIndex)<<32)|mesh.resourceIndex,&mesh);
        auto& materials=residentWorldMaterialLookup[&browser];
        if(materials.empty())for(const auto& texture:browser.directTextures())if(texture.materialId>=0)
            materials[uint32_t(texture.materialId)].push_back(&texture);
        frame.meshes=&meshes;
        frame.materials=&materials;
        frame.visible.resize(browser.placements().size());
        const auto sectors=gPlayTestActive?storylandIntroVisibleSectors(browser,eye,
            storylandIntroSceneAnchor(gDtzArchive.nativeWorld().zones,
                {gPlayTestMissionEnvironment.x,gPlayTestMissionEnvironment.y,gPlayTestMissionEnvironment.z},
                gPlayTestMissionEnvironment.introMissionProtected),gPlayTestMissionEnvironment.introMissionProtected,
            gPlayTestMissionEnvironment.camera.active||gPlayTestMissionEnvironment.cutscene.playing):
            browser.visibleSectors(false);
        const bool ownerVisible=!gPlayTestActive||storylandWorldSectorOwnerVisible(browser,gPlayTestWorldLevel);
        for(size_t index=0;index<browser.placements().size();++index) {
            const auto& placement=browser.placements()[index];
            bool visible=ownerVisible&&StorylandArchiveBrowser::placementVisible(placement,sectors);
            if(details&&selected) {
                const size_t offset=details->sourceOffsets[area]+index;
                visible=visible&&offset<selected->size()&&(*selected)[offset];
            } else visible=visible&&placement.passName!="LOD"&&placement.passName!="SUPERLOD";
            // The reflection probe is independent of the main world draw
            // distance. Limit its *own* scenery to the nearby 104m without
            // shortening detail, shadows or LODs in the playable camera.
            visible=visible&&storylandVehicleEnvironmentInsideProbe(
                state.position,placement.x,placement.y,placement.z)&&
                (!gPlayTestActive||browser.placementEnabled(placement,gPlayTestRuntime.clockHour));
            if(visible)visible=StorylandArchiveBrowser::nativePlacementVisible(placement,
                std::hypot(placement.x-state.position[0],placement.y-state.position[1],
                    placement.z-state.position[2]),
                gPlayTestActive?uint8_t(gPlayTestRuntime.visibleArea):uint8_t(0));
            frame.visible[index]=visible?1:0;
        }
    }
    for(unsigned face=firstFace;face<endFace;++face) {
        if(progressive) {
            // Retain the other five atlas cells. Clearing the full target for
            // a single updated face would turn the rest of the reflection black.
            glEnable(GL_SCISSOR_TEST);
            glScissor(int(face%3)*state.faceSize,int(face/3)*state.faceSize,
                state.faceSize,state.faceSize);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
        }
        const auto view=storylandEnvironmentFaceView(face,state.position);
        glViewport(int(face%3)*state.faceSize,int(face/3)*state.faceSize,state.faceSize,state.faceSize);
        glLoadIdentity();glMultMatrixf(view.data());
        gStoriesSky.drawBackground({},100.0f,true);
        glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        std::array<double,16> projectionDouble{},viewDouble{};
        std::copy(projection.begin(),projection.end(),projectionDouble.begin());
        std::copy(view.begin(),view.end(),viewDouble.begin());
        StorylandViewFrustum frustum;frustum.set(projectionDouble.data(),viewDouble.data());
        for(unsigned phase=0;phase<2;++phase)for(size_t area=0;area<browsers.size();++area) {
            const auto* browser=browsers[area];
            if(!browser)continue;
            const auto& frame=areas[area];
            for(size_t index=0;index<browser->placements().size();++index) {
                if(!frame.visible[index])continue;
                const auto& placement=browser->placements()[index];
                if(placement.passName=="LIGHTS")continue;
                const bool transparent=placement.passName=="TRANSPARENT"||placement.passName=="NOZWRITE";
                if(transparent!=(phase==1))continue;
                const auto found=frame.meshes->find((uint64_t(placement.sectorIndex)<<32)|placement.resourceIndex);
                const auto* mesh=gPlayTestActive?browser->placementModelSwap(index):nullptr;
                if(!mesh&&found!=frame.meshes->end())mesh=found->second;
                if(!mesh)continue;
                if(!worldLightState.object(mesh->nativeModelInfoId,placement.nativePoolHeader,
                    placement.nativePoolIndex).visible)continue;
                std::array<float,16> garageMatrix{};
                const float* matrix=placement.matrix;
                if(gPlayTestActive&&placement.nativeInstanceIndex==UINT32_MAX&&
                    storylandPlayTestGarageMatrix(mesh->nativeModelInfoId,matrix,garageMatrix))matrix=garageMatrix.data();
                if(!frustum.visible(mesh->bounds,matrix))continue;
                glPushMatrix();glMultMatrixf(matrix);
                state.renderedTriangles+=storylandDrawVehicleEnvironmentMesh(*mesh,placement,
                    [&](uint32_t material) {
                        const auto texture=frame.materials->find(material);
                        return texture==frame.materials->end()?GLuint(0):residentWorldTextures.get(
                            storylandResolveWorldTexture(texture->second,*mesh,placement));
                    }, [&](uint32_t material) {
                        const auto texture = frame.materials->find(material);
                        const auto* source = texture == frame.materials->end() ? nullptr :
                            storylandResolveWorldTexture(texture->second, *mesh, placement);
                        return storylandWorldUvOffset(source, uvSeconds);
                    });
                glPopMatrix();
            }
        }
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
    glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    if(directx)storylandDx::endVehicleEnvironmentCapture();
    else gVehicleEnvironmentTarget.end();
    if(!directx)pglUseProgram(GLuint(previousProgram));
    if(clientActive)clientActive(GLenum(previousClient));
    if(!directx&&pglActiveTexture)pglActiveTexture(GLenum(previousActive));
    glPopClientAttrib();glPopAttrib();
    state.texture=directx?storylandDx::vehicleEnvironmentTexture():gVehicleEnvironmentTarget.texture;
    state.lastCaptureTimeMs=GetTickCount64();
    state.nextCaptureFace=progressive?endFace%6u:0u;
    state.completedFaces=progressive?std::min(6u,state.completedFaces+1u):6u;
    state.ready=state.completedFaces==6u;
    if(!progressive||state.nextCaptureFace==0u)++state.captures;
}
