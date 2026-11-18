#include "src/storyland/storyland_stats_menu.h"
#include "src/storyland/storyland_ui_glow.h"
// VCS frontend definitions are supplied by the game's FRONTEND tree (beside PS2 and AUDIO).
// Missing authored pages use the shared host menu; they never leave a blank screen.
static void storylandLcsMenuSound(uint16_t event);
static bool storylandPlayMenuSound(uint16_t id);
static bool storylandLcsInputReady();
static void storylandOpenPauseMenu();
static void storylandResumeFromPauseMenu();
static StorylandFrontend gStorylandRetailFrontend;
static bool gStorylandRetailFrontendWasOpen=false;
static int gStorylandRetailFrontendSlot=1;
static bool gStorylandFrontendHasAuthoredSound=false;
static float gStorylandRetailFrontendScroll=0;
static std::string gStorylandDisplaySettingsError;

// Unlike ImGui's key events, these states do not depend on which child HWND
// happened to own focus when Play Game transitioned to the retail frontend.
static void storylandRetailFrontendAction(const StorylandFrontendAction& action,
    const StorylandFrontendElement& element);
static bool storylandRetailFrontendTrigger(const std::string& event,
    const StorylandFrontend::Handler& handler);
static std::array<bool,7> gStorylandFrontendKeyHeld{};
static std::array<ULONGLONG,7> gStorylandFrontendKeyNextRepeat{};
static void storylandResetFrontendKeyboard() {
    gStorylandFrontendKeyHeld.fill(false);
    gStorylandFrontendKeyNextRepeat.fill(0);
}
static void storylandUpdateFrontendKeyboard() {
    if(!storylandLcsInputReady())return;
    if(!gMainWindow||!IsWindow(gMainWindow))return;
    const HWND foreground=GetForegroundWindow();
    const bool active=foreground==gMainWindow||
        (foreground&&GetAncestor(foreground,GA_ROOT)==gMainWindow);
    if(!active){storylandResetFrontendKeyboard();return;}
    const std::array<int,7> keys={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_RETURN,VK_BACK,VK_ESCAPE};
    const std::array<const char*,7> events={
        "TRIGGER_UP_PRESSED","TRIGGER_DOWN_PRESSED",
        "TRIGGER_LEFT_PRESSED","TRIGGER_RIGHT_PRESSED",
        "TRIGGER_OK_PRESSED","TRIGGER_CANCEL_PRESSED","TRIGGER_CANCEL_PRESSED"};
    const ULONGLONG now=GetTickCount64();
    for(size_t index=0;index<keys.size();++index) {
        const bool down=(GetAsyncKeyState(keys[index])&0x8000)!=0;
        if(!down) {
            gStorylandFrontendKeyHeld[index]=false;
            gStorylandFrontendKeyNextRepeat[index]=0;
            continue;
        }
        const bool initial=!gStorylandFrontendKeyHeld[index];
        const bool repeat=index<4&&now>=gStorylandFrontendKeyNextRepeat[index];
        gStorylandFrontendKeyHeld[index]=true;
        if(!initial&&!repeat)continue;
        gStorylandFrontendKeyNextRepeat[index]=now+(initial?280ull:105ull);
        if(index<2&&!gStorylandRetailFrontend.masterFocus&&
           gStorylandRetailFrontend.activePage=="STATS_PAGE") {
            const size_t count=storylandStatsMenuRows(gPlayTestMissionEnvironment,true).size();
            const float limit=std::max(0.0f,float(count)*20.0f-150.0f);
            const float previous=gStorylandRetailFrontendScroll;
            gStorylandRetailFrontendScroll=std::clamp(previous+(index==0?-20.0f:20.0f),0.0f,limit);
            if(previous!=gStorylandRetailFrontendScroll)storylandPlayMenuSound(197);
            continue;
        }
        storylandRetailFrontendTrigger(events[index],storylandRetailFrontendAction);
        if(!gPlayTestRuntime.pauseMenu)break;
    }
}

static void storylandRetailFrontendPreferences(bool save) {
    const auto directory=storylandUserFilesDirectory(gPlayTestMissionVm.vcs);
    if(directory.empty())return;
    auto path=directory/L"frontend.cfg";
    if(!save&&!std::filesystem::exists(path))
        path=storylandOldUserFilesDirectory(gPlayTestMissionVm.vcs)/L"frontend.cfg";
    if(save) {
        std::ofstream file(path,std::ios::trunc);
        if(file)file<<"music "<<gStorylandMusicVolume<<"\nsfx "<<gStorylandSfxVolume
            <<"\nvibration "<<gStorylandController.vibrationEnabled<<"\ninvert "<<gStorylandInvertLook<<"\nhud "<<gPlayTestRuntime.hud
            <<"\nsubtitles "<<gPlayTestMissionEnvironment.missionAudioSubtitleEnabled<<"\n";
    }else {
        std::ifstream file(path);std::string key;float value=0;
        while(file>>key>>value) {
            if(!std::isfinite(value))continue;
            if(key=="music")gStorylandMusicVolume=std::clamp(value,0.0f,1.0f);
            else if(key=="sfx")gStorylandSfxVolume=std::clamp(value,0.0f,1.0f);
            else if(key=="vibration")gStorylandController.setVibration(value!=0);
            else if(key=="invert")gStorylandInvertLook=value!=0;
            else if(key=="hud")gPlayTestRuntime.hud=value!=0;
            else if(key=="subtitles")gPlayTestMissionEnvironment.missionAudioSubtitleEnabled=value!=0;
        }
    }
}
static int storylandRetailFrontendValue(const StorylandFrontendElement& element) {
    const auto& hook=element.hook;
    if(hook=="HOOK_SHOW_SUBTITLES")return gPlayTestMissionEnvironment.missionAudioSubtitleEnabled;
    if(hook=="HOOK_HUD_MODE")return gPlayTestRuntime.hud;
    if(hook=="HOOK_RADAR_MODE")return gPlayTestMissionEnvironment.radarDisplayMode;
    if(hook=="HOOK_MUSIC_VOLUME")return int(std::lround(gStorylandMusicVolume*element.maximum));
    if(hook=="HOOK_SFX_VOLUME")return int(std::lround(gStorylandSfxVolume*element.maximum));
    if(hook=="HOOK_VIBRATION_MODE")return gStorylandController.vibrationEnabled;
    if(hook=="HOOK_PAD_INVERT_LOOK")return gStorylandInvertLook;
    if(hook=="HOOK_RADIO_STATION")return int(gPlayTestRadioStation);
    if(hook=="HOOK_TRAILS")return gStorylandDisplaySettings.trails;
    return 0;
}
static bool storylandRetailFrontendSetting(const std::string& hook) {
    return hook=="HOOK_NONE"||hook.empty()||hook=="HOOK_SHOW_SUBTITLES"||hook=="HOOK_HUD_MODE"||
        hook=="HOOK_RADAR_MODE"||hook=="HOOK_MUSIC_VOLUME"||hook=="HOOK_SFX_VOLUME"||
        hook=="HOOK_PAD_INVERT_LOOK"||hook=="HOOK_RADIO_STATION"||hook=="HOOK_VIBRATION_MODE"||
        hook=="HOOK_TRAILS";
}
static void storylandRetailFrontendPage(const std::string& name) {
    auto& frontend=gStorylandRetailFrontend;
    const auto page=frontend.pages.find(name);
    if(page==frontend.pages.end()){frontend.error="Missing frontend page: "+name;return;}
    frontend.activePage=name;frontend.highlighted=page->second.initial;frontend.masterFocus=false;
    gStorylandRetailFrontendScroll=0;frontend.error.clear();
}
static bool storylandRetailFrontendSlotExists(int slot) {
    std::error_code error;
    return std::filesystem::is_regular_file(storylandSaveSlotPath(slot),error)||
        std::filesystem::is_regular_file(storylandPreviousSaveSlotPath(slot),error)||
        std::filesystem::is_regular_file(storylandLegacySaveSlotPath(slot),error);
}
static bool storylandPlayMenuSound(uint16_t id) {
    if(!gPlayTestRuntime.sounds||!gPlayTestMissionEnvironment.frontendAudioEnabled)return false;
    const bool vcs=gDtzArchive.gameVariant()==StorylandDtzGameVariant::Vcs;
    const auto definition=storylandMissionEventSound(vcs,id,true);
    if(definition.nativeSilent)return false;
    const auto* sample=definition.sampleName?
        gPlayTestNativeAudio.namedVehicleSample(definition.sampleName):
        definition.sample>=0?gPlayTestNativeAudio.vcsSample(definition.bank,size_t(definition.sample)):nullptr;
    if(!sample)return false;
    auto ticket=gPlayTestGameAudio.submit(sample->bankPath,sample->slot,true,
        gStorylandSfxVolume,true,false,true,StorylandFrontendAmbience::Menu);
    if(ticket) {
        gStorylandFrontendHasAuthoredSound=true;
        if(definition.frequency>0&&sample->frequency)
            ticket->pitch=definition.frequency/float(sample->frequency);
        return true;
    }
    return false;
}

static void storylandRetailFrontendAction(const StorylandFrontendAction& action,const StorylandFrontendElement& element) {
    auto& frontend=gStorylandRetailFrontend;
    if(action.name=="ACTION_PLAY_SOUND") {
        try {
            size_t end=0;const unsigned id=unsigned(std::stoul(action.parameter,&end,0));
            if(end==action.parameter.size()&&id<=UINT16_MAX) {
                const auto definition=storylandMissionEventSound(
                    gDtzArchive.gameVariant()==StorylandDtzGameVariant::Vcs,uint16_t(id),true);
                // A retail silent event stays silent; an unresolved bank does
                // not suppress the selected/moved fallback event.
                if(definition.nativeSilent||storylandPlayMenuSound(uint16_t(id)))
                    gStorylandFrontendHasAuthoredSound=true;
            }
        } catch(const std::exception&) {}
    }else if(action.name=="ACTION_NEW_GAME") {
        // Dispatch after the ImGui frame; the loading screen owns the viewport.
        PostMessageW(gMainWindow,WM_COMMAND,ID_VIEW_PLAY_TEST,4);
    }else if(action.name=="ACTION_PRE_LOAD_SLOT_CHECK")storylandRetailFrontendPage("LOAD_GAME_SLOT_PAGE");
    else if(action.name=="ACTION_PRE_SAVE_SLOT_CHECK")storylandRetailFrontendPage("SAVE_GAME_SLOT_PAGE");
    else if(action.name=="ACTION_PRE_DELETE_SLOT_CHECK")storylandRetailFrontendPage("DELETE_GAME_SLOT_PAGE");
    else if(action.name=="ACTION_SET_ACTIVE_PAGE_IF_SLOT_VALID") {
        if(element.custom.rfind("CUSTOM_ITEM_SAVESLOT",0)==0) {
            const auto number=element.custom.substr(sizeof("CUSTOM_ITEM_SAVESLOT")-1);
            if(number.size()==1&&number[0]>='1'&&number[0]<='8') {
                const int slot=number[0]-'0';
                if(frontend.activePage=="SAVE_GAME_SLOT_PAGE"||storylandRetailFrontendSlotExists(slot)) {
                    gStorylandRetailFrontendSlot=slot;storylandRetailFrontendPage(action.parameter);
                }
            }
        }
    }else if(action.name=="ACTION_LOAD_GAME_CONFIRM_YES") {
        if(gStorylandFrontendBootPending||gStorylandFrontend) {
            PostMessageW(gMainWindow,WM_COMMAND,ID_VIEW_PLAY_TEST,4+gStorylandRetailFrontendSlot);
        } else {
            storylandGameSaveSlot(gStorylandRetailFrontendSlot,true);
            frontend.error=gStorylandSaveStatus;
        }
    }else if(action.name=="ACTION_SAVE_GAME") {
        if(storylandCanSaveGame())storylandGameSaveSlot(gStorylandRetailFrontendSlot,false);
        else gStorylandSaveStatus="Saving is unavailable during this mission or player state.";
        const auto status=gStorylandSaveStatus;
        storylandRetailFrontendPage("SAVE_RESULT");frontend.error=status;
    }else if(action.name=="ACTION_DELETE_GAME") {
        std::error_code error;
        std::filesystem::remove(storylandSaveSlotPath(gStorylandRetailFrontendSlot),error);
        if(!error)std::filesystem::remove(storylandPreviousSaveSlotPath(gStorylandRetailFrontendSlot),error);
        if(!error)std::filesystem::remove(storylandLegacySaveSlotPath(gStorylandRetailFrontendSlot),error);
        storylandRetailFrontendPage(error?"DELETE_FAILED":"DELETE_SUCCESS");
    }else if(action.name=="ACTION_NEXT_MENU_STATE"||action.name=="ACTION_PREV_MENU_STATE") {
        const int direction=action.name=="ACTION_NEXT_MENU_STATE"?1:-1;
        if(!storylandRetailFrontendSetting(element.hook))return;
        int current=storylandRetailFrontendValue(element);
        const int count=element.hook=="HOOK_RADIO_STATION"?int(gPlayTestRadioStations.size()+1):int(element.states.size());
        if(element.type=="SLIDER_MENU_ITEM")current=std::clamp(current+direction*element.step,element.minimum,element.maximum);
        else if(count>0)current=(current+direction+count)%count;
        if(element.hook=="HOOK_SHOW_SUBTITLES")gPlayTestMissionEnvironment.missionAudioSubtitleEnabled=current!=0;
        if(element.hook=="HOOK_HUD_MODE")gPlayTestRuntime.hud=current!=0;
        if(element.hook=="HOOK_RADAR_MODE")gPlayTestMissionEnvironment.radarDisplayMode=current;
        if(element.hook=="HOOK_MUSIC_VOLUME")gStorylandMusicVolume=float(current)/std::max(1,element.maximum);
        if(element.hook=="HOOK_SFX_VOLUME")gStorylandSfxVolume=float(current)/std::max(1,element.maximum);
        if(element.hook=="HOOK_VIBRATION_MODE")gStorylandController.setVibration(current!=0);
        if(element.hook=="HOOK_PAD_INVERT_LOOK")gStorylandInvertLook=current!=0;
        if(element.hook=="HOOK_TRAILS") {
            gStorylandDisplaySettings.trails=current!=0;
            gStorylandDisplaySettings.save(storylandUserFilesDirectory(gPlayTestMissionVm.vcs),gStorylandDisplaySettingsError);
        }
        if(element.hook=="HOOK_RADIO_STATION") {
            gPlayTestRadioStation=size_t(current);
            gPlayTestRuntime.radioEnabled=current<int(gPlayTestRadioStations.size());
            startStorylandPlayTestRadio();
        }
        storylandRetailFrontendPreferences(true);
    }else if(action.name=="ACTION_RETURN_TO_GAME") {
        if(!gStorylandFrontend) {
            storylandResumeFromPauseMenu();
            gPlayTestMissionEnvironment.saveGameFinished=true;
        }
    }else if(action.name=="ACTION_MEM_CARD_RECHECK"||action.name=="ACTION_MEM_CARD_PROCEED") {
        storylandRetailFrontendPage(gPlayTestMissionEnvironment.saveMenuRequested?"SAVE_GAME_SLOT_PAGE":"LOAD_GAME_SLOT_PAGE");
    }else if(action.name=="ACTION_RESET_SCROLLING_TEXT")gStorylandRetailFrontendScroll=0;
    else if(action.name=="ACTION_VIBRATE_PAD")gStorylandController.vibrate(.5f,200);
    else if(action.name!="ACTION_REFRESH_PAGE") {
        frontend.error="Frontend action is not connected yet: "+action.name;
    }
}
static bool storylandRetailFrontendTrigger(const std::string& event,const StorylandFrontend::Handler& handler) {
    auto& frontend=gStorylandRetailFrontend;
    const std::string previousPage=frontend.activePage;
    const std::string previousHighlight=frontend.masterFocus?frontend.masterHighlight:frontend.highlighted;
    const bool previousMasterFocus=frontend.masterFocus;
    gStorylandFrontendHasAuthoredSound=false;
    const bool handled=frontend.trigger(event,handler,0,
        storylandShouldConfirmNewGame(gPlayTestActive&&gPlayTestGameMode&&!gStorylandFrontend,
            storylandHasAnyGameSave(storylandRetailFrontendSlotExists)));
    const bool changed=previousPage!=frontend.activePage||previousMasterFocus!=frontend.masterFocus||
        previousHighlight!=(frontend.masterFocus?frontend.masterHighlight:frontend.highlighted);
    const int direction=event=="TRIGGER_UP_PRESSED"?-1:event=="TRIGGER_DOWN_PRESSED"?1:0;
    bool moved=false;
    if(direction&&!changed) {
        const auto* page=frontend.focusedPage();
        if(page) {
            std::vector<const StorylandFrontendElement*> choices;
            for(const auto& element:page->elements) {
                if(element.name.empty()||element.triggers.empty()||
                    element.type=="IMAGE_MENU_ITEM"||
                    !storylandRetailFrontendSetting(element.hook))continue;
                const bool actionable=element.triggers.count("TRIGGER_OK_PRESSED")||
                    element.triggers.count("TRIGGER_ON_HIGHLIGHT")||
                    element.triggers.count("TRIGGER_LEFT_PRESSED")||
                    element.triggers.count("TRIGGER_RIGHT_PRESSED");
                if(actionable)choices.push_back(&element);
            }
            std::stable_sort(choices.begin(),choices.end(),[](const auto* a,const auto* b) {
                return a->y==b->y?a->x<b->x:a->y<b->y;
            });
            if(choices.size()>1) {
                const auto selected=frontend.masterFocus?frontend.masterHighlight:frontend.highlighted;
                auto current=std::find_if(choices.begin(),choices.end(),[&](const auto* choice) {
                    return choice->name==selected;
                });
                const size_t index=current==choices.end()?0:
                    size_t(current-choices.begin());
                const size_t next=(index+choices.size()+size_t(direction>0?1:choices.size()-1))%choices.size();
                const std::string& name=choices[next]->name;
                (frontend.masterFocus?frontend.masterHighlight:frontend.highlighted)=name;
                moved=name!=selected;
                if(moved)frontend.trigger("TRIGGER_ON_HIGHLIGHT",handler,0,true);
            }
        }
    }
    if((handled||moved||changed)&&event!="TRIGGER_ON_HIGHLIGHT"&&
       !gStorylandFrontendHasAuthoredSound)
        storylandPlayMenuSound(event=="TRIGGER_CANCEL_PRESSED"?196:
            event=="TRIGGER_OK_PRESSED"?198:197);
    return handled||moved||changed;
}

static bool storylandDrawRetailFrontend(float gameTop) {
    auto& frontend=gStorylandRetailFrontend;
    if(gPlayTestActive&&gPlayTestMissionEnvironment.saveMenuRequested)gPlayTestRuntime.pauseMenu=true;
    const bool open=(gPlayTestActive||gStorylandFrontendBootPending)&&
        gPlayTestRuntime.pauseMenu&&!gPlayTestOpeningMovies.active();
    if(!open){gStorylandRetailFrontendWasOpen=false;storylandResetFrontendKeyboard();return false;}
    if(!gStorylandRetailFrontendWasOpen) {
        ImGui::SetWindowFocus(nullptr);
        if(gPreview)SetFocus(gPreview);
        storylandRetailFrontendPreferences(false);
        const bool loaded=!frontend.pages.empty()||frontend.loadFromGamePaths({
            std::filesystem::path(gDtzArchive.sourcePath()),
            std::filesystem::path(gArchiveBrowser.lvzPath()),
            std::filesystem::path(gPlayTestScmPath)});
        if(!loaded)gPlayTestStatus=frontend.error;
        else {
            frontend.reset();frontend.masterHighlight=gStorylandFrontend?"Game_t":"Map_t";
            storylandRetailFrontendTrigger("TRIGGER_ON_HIGHLIGHT",storylandRetailFrontendAction);
            if(gStorylandFrontend)frontend.masterFocus=false;
        }
    }
    gStorylandRetailFrontendWasOpen=true;
    if(frontend.pages.empty())return false;
    if(gPlayTestMissionEnvironment.saveMenuRequested) {
        gPlayTestMissionEnvironment.saveMenuRequested=false;
        storylandRetailFrontendPage("SAVE_GAME_SLOT_PAGE");
    }
    const auto windowSize=ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0,gameTop),windowSize,IM_COL32_BLACK);
    const float scale=std::min(windowSize.x/480.0f,std::max(1.0f,windowSize.y-gameTop)/272.0f);
    const int canvasWidth=std::max(1,int(std::lround(480.0f*scale)));
    const int canvasHeight=std::max(1,int(std::lround(272.0f*scale)));
    const auto size=ImGui::GetIO().DisplaySize;
    auto* draw=ImGui::GetBackgroundDrawList();
    draw->PushClipRect(ImVec2(0,gameTop),size,true);
    draw->AddRectFilled(ImVec2(0,gameTop),size,IM_COL32_BLACK);
    const float sx=float(canvasWidth)/480.0f,sy=float(canvasHeight)/272.0f;
    const float originX=float((windowSize.x-canvasWidth)*.5f);
    const float originY=gameTop+float((windowSize.y-gameTop-canvasHeight)*.5f);
    draw->PushClipRect(ImVec2(originX,originY),
        ImVec2(originX+canvasWidth,originY+canvasHeight),true);
    storylandDrawFrontendBackground(draw,&gPlayTestHudAssets,ImVec2(originX,originY),
        ImVec2(originX+canvasWidth,originY+canvasHeight));
    const auto translate=[](const std::string& key) {
        std::u16string text;
        if(gPlayTestMissionGxt.lookup(key,"",text))
            return narrow(std::wstring(text.begin(),text.end()));
        static const std::map<std::string,std::string> fallback={
            {"FES_LOA","Load Game"},{"FES_NGA","New Game"},
            {"FES_DGM","Delete Game"},{"FEM_SEL","Select"},
            {"FEM_NAV","Navigate"},{"FEM_BAK","Back"}
        };
        const auto found=fallback.find(key);
        return found!=fallback.end()?found->second:key;
    };
    const auto text=[&](const StorylandFrontendElement& element,const std::string& caption,float x,float y,ImU32 color,bool glow=false) {
        static constexpr const char* fonts[]={"fancytext","gametext","bigtext","pagertext"};
        const char* nativeFont=fonts[std::clamp(element.font,0,3)];
        const char* font=storylandFrontendFont(&gPlayTestHudAssets,nativeFont);
        StorylandTextLayout layout;layout.horizontalScale=element.scaleX/std::max(.01f,element.scaleY)*sx/sy;
        auto metrics=gPlayTestHudAssets.fonts.find(std::string("retail:")+nativeFont);
        if(metrics==gPlayTestHudAssets.fonts.end())metrics=gPlayTestHudAssets.fonts.find(nativeFont);
        const float lineHeight=metrics==gPlayTestHudAssets.fonts.end()?32.0f:float(metrics->second.height);
        const float height=lineHeight*element.scaleY*sy;
        const bool centered=element.horizontal==1;
        const float anchor=x+(centered?element.width*.5f:element.horizontal==2?element.width:0);
        layout.rightAligned=element.horizontal==2;
        const ImVec2 position(originX+anchor*sx,originY+y*sy);
        if(glow)storylandDrawUiGlow(position,ImVec2(sx*1.25f,sy*1.25f),color,
            [&](ImVec2 point,ImU32 tint) {
                storylandDrawPlayTestText(draw,&gPlayTestHudAssets,point,tint,
                    caption.c_str(),height,element.width*sx,font,centered,&layout);
            });
        storylandDrawPlayTestText(draw,&gPlayTestHudAssets,position,color,
            caption.c_str(),height,element.width*sx,font,centered,&layout);
    };
    const auto render=[&](const std::string& name,bool master) {
        const auto page=frontend.pages.find(name);if(page==frontend.pages.end())return;
        for(const auto& element:page->second.elements) {
            const bool selected=master?element.name==frontend.masterHighlight:
                !frontend.masterFocus&&element.name==frontend.highlighted;
            const bool enabled=storylandRetailFrontendSetting(element.hook);
            const auto color=enabled?(selected?element.highlightColor:element.color):IM_COL32(110,110,110,255);
            if(element.type=="IMAGE_MENU_ITEM") {
                std::string textureName=element.texture;
                std::transform(textureName.begin(),textureName.end(),textureName.begin(),
                    [](unsigned char character){return char(std::tolower(character));});
                auto texture=storylandRetailSpriteTexture(&gPlayTestHudAssets,"frontend:"+textureName);
                if(!texture)texture=storylandRetailSpriteTexture(&gPlayTestHudAssets,element.texture);
                if(texture)draw->AddImage(texture,ImVec2(originX+element.x*sx,originY+element.y*sy),
                    ImVec2(originX+(element.x+element.width)*sx,originY+(element.y+element.height)*sy));
            }else if(!element.states.empty()) {
                int value=storylandRetailFrontendValue(element);
                value=std::clamp(value,0,int(element.states.size())-1);
                const auto caption=element.hook=="HOOK_RADIO_STATION"?
                    (gPlayTestRadioStation<gPlayTestRadioStations.size()?gPlayTestRadioStations[gPlayTestRadioStation].label:translate("FEA_NON")):
                    translate(element.states[size_t(value)]);
                text(element,caption,element.x,element.y,color,selected&&enabled);
            }else if(element.type=="SLIDER_MENU_ITEM") {
                const float range=float(std::max(1,element.maximum-element.minimum));
                const float fraction=std::clamp((storylandRetailFrontendValue(element)-element.minimum)/range,0.0f,1.0f);
                const int segments=std::max(1,(element.maximum-element.minimum+element.step-1)/element.step);
                for(int segment=0;segment<segments;++segment) {
                    const float left=element.x+float(segment)*element.width/segments;
                    const float height=4+float(segment+1)*8/segments;
                    draw->AddRectFilled(ImVec2(originX+left*sx,originY+(element.y+12-height)*sy),
                        ImVec2(originX+(left+element.width/segments-2)*sx,originY+(element.y+12)*sy),
                        float(segment)/segments<fraction?color:IM_COL32(70,70,70,255));
                }
            }else if(element.custom.rfind("CUSTOM_ITEM_SAVESLOT",0)==0) {
                const auto number=element.custom.substr(sizeof("CUSTOM_ITEM_SAVESLOT")-1);
                if(number.size()==1&&number[0]>='1'&&number[0]<='8')
                    text(element,"Slot "+number+(storylandRetailFrontendSlotExists(number[0]-'0')?"":" - "+translate("FESZ_EF")),element.x,element.y,color);
            }
            if(!element.triggers.empty()&&enabled) {
                const ImVec2 minimum(originX+element.x*sx,originY+element.y*sy);
                const ImVec2 maximum(originX+(element.x+element.width)*sx,originY+(element.y+std::min(element.height,22.0f))*sy);
                if(!ImGui::GetIO().WantCaptureMouse&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&ImGui::IsMouseHoveringRect(minimum,maximum)&&
                    !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) {
                    ImGui::SetWindowFocus(nullptr);
                    frontend.masterFocus=master;
                    (master?frontend.masterHighlight:frontend.highlighted)=element.name;
                    storylandRetailFrontendTrigger("TRIGGER_ON_HIGHLIGHT",storylandRetailFrontendAction);
                    storylandRetailFrontendTrigger("TRIGGER_OK_PRESSED",storylandRetailFrontendAction);
                }
            }
            if(!element.label.empty()) {
                auto label=element;label.width=0;
                label.horizontal=element.labelAlignment==1?2:0;
                text(label,translate(element.label),element.x+element.labelX,element.y+element.labelY,color,selected&&enabled);
            }
        }
    };
    gPlayTestRuntime.pauseMap=frontend.activePage=="MAP_PAGE";
    render(frontend.activePage,false);
    if(frontend.masterFocus) {
        render("MASTER",true);render("MASTER_BUTTONS",false);
    }else {
        const char* buttons=frontend.activePage=="MAP_PAGE"?"MAP_BUTTONS":
            frontend.activePage=="BRIEF_PAGE"?"BRIEF_BUTTONS":
            frontend.activePage=="AUDIO_PAGE"?"AUDIOCTRL_BUTTONS":
            frontend.activePage=="STATS_PAGE"?"UPDOWN_BUTTONS":"SELECT_BUTTONS";
        render(buttons,false);
    }
    if(frontend.activePage=="BRIEF_PAGE") {
        StorylandFrontendElement e;e.width=430;e.scaleX=.4f;e.scaleY=.5f;
        float y=50-gStorylandRetailFrontendScroll;
        draw->PushClipRect(ImVec2(originX+20*sx,originY+45*sy),ImVec2(originX+460*sx,originY+205*sy),true);
        for(auto it=gPlayTestMissionEnvironment.briefMessages.rbegin();it!=gPlayTestMissionEnvironment.briefMessages.rend()&&y<200;++it) {
            const auto units=storylandMissionFormattedText(gPlayTestMissionEnvironment,it->key,it->numbers,it->stringKey);
            const auto caption=narrow(std::wstring(units.begin(),units.end()));
            text(e,caption,25,y,IM_COL32_WHITE);
            y+=std::max(24.0f,storylandMeasurePlayTestText(&gPlayTestHudAssets,caption.c_str(),16*sy,430*sx,
                storylandFrontendFont(&gPlayTestHudAssets)).y/sy+5);
        }
        draw->PopClipRect();
    }
    if(frontend.activePage=="STATS_PAGE") {
        StorylandFrontendElement label;label.width=325;label.scaleX=.35f;label.scaleY=.45f;
        StorylandFrontendElement value=label;value.width=430;value.horizontal=2;
        float y=50-gStorylandRetailFrontendScroll;
        draw->PushClipRect(ImVec2(originX+20*sx,originY+45*sy),ImVec2(originX+460*sx,originY+205*sy),true);
        for(const auto& row:storylandStatsMenuRows(gPlayTestMissionEnvironment,true)) {
            const auto translated=translate(row.labelKey);
            draw->PushClipRect(ImVec2(originX+20*sx,originY+45*sy),ImVec2(originX+350*sx,originY+205*sy),true);
            text(label,translated==row.labelKey?row.fallback:translated,25,y,IM_COL32_WHITE);
            draw->PopClipRect();
            text(value,row.value,25,y,IM_COL32_WHITE);
            y+=20;
        }
        draw->PopClipRect();
    }
    if(!frontend.error.empty()) {
        StorylandFrontendElement e;e.width=440;e.scaleX=.3f;e.scaleY=.4f;
        text(e,frontend.error,20,180,IM_COL32(255,190,160,255));
    }
    draw->PopClipRect();
    draw->PopClipRect();
    // Poll at the frame boundary, even when ImGui considers the preview or
    // a docked panel to be the keyboard owner.
    if(!gPlayTestRuntime.pauseMenu)return true;
    const bool inputReady=storylandLcsInputReady();
    if(inputReady&&!ImGui::GetIO().WantTextInput&&
        !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))
        storylandUpdateFrontendKeyboard();
    // Use ImGui only for the input event stream, not widgets, focus, or layout.
    if(inputReady&&!ImGui::GetIO().WantTextInput&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) {
        if(frontend.activePage=="BRIEF_PAGE"||frontend.activePage=="STATS_PAGE") {
            const size_t lines=frontend.activePage=="BRIEF_PAGE"?gPlayTestMissionEnvironment.briefMessages.size():storylandStatsMenuRows(gPlayTestMissionEnvironment,true).size();
            gStorylandRetailFrontendScroll=std::clamp(gStorylandRetailFrontendScroll-ImGui::GetIO().MouseWheel*24.0f,
                0.0f,std::max(0.0f,float(lines)*(frontend.activePage=="STATS_PAGE"?20.0f:24.0f)-150.0f));
        }
        const auto pressed=[](ImGuiKey key,ImGuiKey pad){(void)key;return ImGui::IsKeyPressed(pad);};
        const bool mapFocused=!frontend.masterFocus&&frontend.activePage=="MAP_PAGE";
        if(mapFocused) {
            const float speed=gPlayTestHudAssets.radarTileSize*gPlayTestHudAssets.radarGrid*
                std::min(.1f,ImGui::GetIO().DeltaTime)*.5f/gPlayTestRuntime.pauseMapZoom;
            if(ImGui::IsKeyDown(ImGuiKey_LeftArrow)||ImGui::IsKeyDown(ImGuiKey_GamepadDpadLeft))gPlayTestRuntime.pauseMapPanX-=speed;
            if(ImGui::IsKeyDown(ImGuiKey_RightArrow)||ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight))gPlayTestRuntime.pauseMapPanX+=speed;
            if(ImGui::IsKeyDown(ImGuiKey_UpArrow)||ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp))gPlayTestRuntime.pauseMapPanY+=speed;
            if(ImGui::IsKeyDown(ImGuiKey_DownArrow)||ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown))gPlayTestRuntime.pauseMapPanY-=speed;
            float zoom=ImGui::GetIO().MouseWheel;
            if(pressed(ImGuiKey_Equal,ImGuiKey_GamepadR1))zoom+=1;
            if(pressed(ImGuiKey_Minus,ImGuiKey_GamepadL1))zoom-=1;
            gPlayTestRuntime.pauseMapZoom=std::clamp(gPlayTestRuntime.pauseMapZoom*std::pow(1.2f,zoom),1.0f,8.0f);
        }else {
            if(pressed(ImGuiKey_LeftArrow,ImGuiKey_GamepadDpadLeft))storylandRetailFrontendTrigger("TRIGGER_LEFT_PRESSED",storylandRetailFrontendAction);
            if(pressed(ImGuiKey_RightArrow,ImGuiKey_GamepadDpadRight))storylandRetailFrontendTrigger("TRIGGER_RIGHT_PRESSED",storylandRetailFrontendAction);
        }
        const bool scrolling=!frontend.masterFocus&&(frontend.activePage=="BRIEF_PAGE"||frontend.activePage=="STATS_PAGE");
        if(pressed(ImGuiKey_UpArrow,ImGuiKey_GamepadDpadUp)) {
            if(scrolling)gStorylandRetailFrontendScroll=std::max(0.0f,gStorylandRetailFrontendScroll-
                (frontend.activePage=="STATS_PAGE"?20.0f:24.0f));
            else storylandRetailFrontendTrigger("TRIGGER_UP_PRESSED",storylandRetailFrontendAction);
        }
        if(pressed(ImGuiKey_DownArrow,ImGuiKey_GamepadDpadDown)) {
            if(scrolling) {
                const size_t lines=frontend.activePage=="BRIEF_PAGE" ? gPlayTestMissionEnvironment.briefMessages.size() :
                    storylandStatsMenuRows(gPlayTestMissionEnvironment,true).size();
                const float step=frontend.activePage=="STATS_PAGE"?20.0f:24.0f;
                gStorylandRetailFrontendScroll=std::min(gStorylandRetailFrontendScroll+step,
                    std::max(0.0f,float(lines)*step-150.0f));
            }
            else storylandRetailFrontendTrigger("TRIGGER_DOWN_PRESSED",storylandRetailFrontendAction);
        }
        if(pressed(ImGuiKey_Enter,ImGuiKey_GamepadFaceDown))storylandRetailFrontendTrigger("TRIGGER_OK_PRESSED",storylandRetailFrontendAction);
        if(pressed(ImGuiKey_Backspace,ImGuiKey_GamepadFaceRight))storylandRetailFrontendTrigger("TRIGGER_CANCEL_PRESSED",storylandRetailFrontendAction);
    }
    return true;
}

static StorylandGameMenuServices storylandWindowsMenuServices() {
    StorylandGameMenuServices services;
    services.artwork=&gPlayTestHudAssets;
    services.loaded=gPlayTestActive||gStorylandFrontendBootPending;
    services.vcsVisualGlow=services.loaded&&gPlayTestMissionVm.vcs;
    services.active=gPlayTestActive&&gPlayTestGameMode&&!gStorylandFrontend;
    services.title=gPlayTestMissionVm.vcs?"Grand Theft Auto: Vice City Stories":"Grand Theft Auto: Liberty City Stories";
    services.frontendSound=[](uint16_t event) {
        if(gPlayTestMissionVm.vcs)storylandPlayMenuSound(event);
        else storylandLcsMenuSound(event);
    };
    services.canSave=storylandCanSaveGame();
    services.saveRestriction="Save in free roam, on foot, outside missions and without a wanted level.";
    services.newGame=[](std::string& error) {
        if(!gPlayTestActive&&!gStorylandFrontendBootPending) {
            error="Open game data and select Play Game first.";
            return false;
        }
        if(!PostMessageW(gMainWindow,WM_COMMAND,ID_VIEW_PLAY_TEST,4)) {
            error="Could not queue New Game loading.";
            return false;
        }
        error.clear();
        return true;
    };
    services.resume=[]() {
        storylandResumeFromPauseMenu();
    };
    services.returnToMain=[]() {
        PostMessageW(gMainWindow,WM_COMMAND,ID_VIEW_PLAY_TEST,2);
    };
    services.quit=[]() {PostMessageW(gMainWindow,WM_CLOSE,0,0);};
    services.gallery=[]() {storylandOpenGallery();};
    services.debug=[]() {
        if(gStorylandFrontend) {
            PostMessageW(gMainWindow,WM_COMMAND,ID_VIEW_PLAY_TEST,3);
            gStorylandGameMenu.close();return;
        }
        storylandSetPlayTestPanelsHidden(false);
        gPlayTestDebugMenuOpen=true;gPlayTestRuntime.pauseMenu=false;
        gStorylandGameMenu.close();
    };
    services.slotExists=[](int slot) {return storylandRetailFrontendSlotExists(slot);};
    services.slotLabel=[](int) {return std::string("Saved game");};
    services.loadGame=[](int slot,std::string& error) {
        if(gStorylandFrontendBootPending||gStorylandFrontend) {
            if(slot<1||slot>8||!PostMessageW(gMainWindow,WM_COMMAND,ID_VIEW_PLAY_TEST,4+slot)) {
                error="Could not queue saved-game loading.";
                return false;
            }
            error.clear();
            return true;
        }
        const bool loaded=storylandGameSaveSlot(slot,true);
        error=loaded?std::string{}:gStorylandSaveStatus;
        return loaded;
    };
    services.saveGame=[](int slot,std::string& error) {
        const bool saved=storylandGameSaveSlot(slot,false);
        error=saved?std::string{}:gStorylandSaveStatus;return saved;
    };
    services.deleteGame=[](int slot,std::string& error) {
        if(slot<1||slot>8) {
            error="Select a save slot from one to eight.";
            return false;
        }
        bool removed=false;
        for(const auto& path:{storylandSaveSlotPath(slot),storylandPreviousSaveSlotPath(slot),
            storylandLegacySaveSlotPath(slot)}) {
            std::error_code fileError;
            const bool exists=std::filesystem::exists(path,fileError);
            if(!fileError&&!exists)continue;
            if(!fileError&&!std::filesystem::is_regular_file(path,fileError)) {
                error="The selected save path is not a regular file.";
                return false;
            }
            if(fileError) {
                error="Cannot inspect the saved game: "+fileError.message();
                return false;
            }
            if(std::filesystem::remove(path,fileError))removed=true;
            if(fileError) {
                error="Cannot delete the saved game: "+fileError.message();
                return false;
            }
        }
        error=removed?std::string{}:"The selected save slot is already empty.";
        return removed;
    };
    services.finishSaveMenu=[]() {
        gPlayTestMissionEnvironment.saveMenuRequested=false;
        gPlayTestMissionEnvironment.saveGameFinished=true;
    };
    services.drawSettings=[]() {
        drawStorylandShaderSelection();
        int aspect=int(gStorylandDisplaySettings.aspectRatio);
        ImGui::TextUnformatted("Aspect ratio");
        ImGui::SetNextItemWidth(-1);
        if(ImGui::Combo("##AspectRatio",&aspect,StorylandAspectRatioNames,5)) {
            gStorylandDisplaySettings.aspectRatio=StorylandAspectRatio(aspect);
            gStorylandDisplaySettings.save(storylandUserFilesDirectory(gPlayTestMissionVm.vcs),gStorylandDisplaySettingsError);
        }
        ImGui::TextWrapped("The view fits inside the window. Black bars preserve the selected shape.");
        bool graphicsChanged=ImGui::Checkbox("Trails",&gStorylandDisplaySettings.trails);
        graphicsChanged|=drawStorylandAdvancedGraphicsOptions();
        if(graphicsChanged)gStorylandDisplaySettings.save(storylandUserFilesDirectory(gPlayTestMissionVm.vcs),
            gStorylandDisplaySettingsError);
        int audioMode=int(StorylandAudioMixer::currentAudioMode());
        if(ImGui::Combo("Audio Mode",&audioMode,"PC (Enhanced)\0PS2\0PSP\0"))
            storylandSelectAudioMode(storylandAudioModeFromInt(audioMode));
        bool changed=ImGui::SliderFloat("Music volume",&gStorylandMusicVolume,0,1,"%.2f");
        changed|=ImGui::SliderFloat("Sound volume",&gStorylandSfxVolume,0,1,"%.2f");
        changed|=ImGui::Checkbox("HUD",&gPlayTestRuntime.hud);
        changed|=ImGui::Checkbox("Subtitles",&gPlayTestMissionEnvironment.missionAudioSubtitleEnabled);
        changed|=ImGui::Checkbox("Invert look",&gStorylandInvertLook);
        if(changed)storylandRetailFrontendPreferences(true);
        if(!gStorylandDisplaySettingsError.empty())ImGui::TextWrapped("%s",gStorylandDisplaySettingsError.c_str());
    };
    return services;
}

static void storylandDrawHostGameMenu() {
    if(gPlayTestActive&&gPlayTestMissionEnvironment.saveMenuRequested)gPlayTestRuntime.pauseMenu=true;
    if((!gPlayTestActive&&!gStorylandFrontendBootPending)||
        !gPlayTestRuntime.pauseMenu||gPlayTestOpeningMovies.active()) {
        gStorylandGameMenu.close();
        return;
    }
    if(gPlayTestMissionEnvironment.saveMenuRequested) {
        gPlayTestMissionEnvironment.saveMenuRequested=false;
        gStorylandGameMenu.showSave(true);
    } else if(!gStorylandGameMenu.open()) {
        storylandRetailFrontendPreferences(false);
        if(gStorylandFrontend)gStorylandGameMenu.showMain();
        else gStorylandGameMenu.showPause();
    }
    ImGui::BeginDisabled(!storylandLcsInputReady());
    gStorylandGameMenu.draw(storylandWindowsMenuServices());
    ImGui::EndDisabled();
}
