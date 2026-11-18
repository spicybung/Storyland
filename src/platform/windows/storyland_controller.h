#pragma once
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

enum class StorylandControllerMode { Current, Pcsx2, Gamepad };

struct StorylandControllerState {
    std::array<int,20> pad{};
    bool down(size_t button) const { return pad[button]!=0; }
    int movementAxis(bool vertical) const {
        const int stick=pad[vertical?1:0];
        const int directional=(int(down(vertical?9:11))-int(down(vertical?8:10)))*127;
        return std::abs(stick)>std::abs(directional)?stick:directional;
    }
    static int axis(int value) {
        constexpr int deadzone=6000;
        const int magnitude=std::abs(value);
        if(magnitude<=deadzone)return 0;
        const int scaled=std::min(128,(magnitude-deadzone)*128/(32767-deadzone));
        return value<0?-scaled:scaled;
    }
    template<class Key> void keyboard(const Key& key) {
        pad={};
        pad[0]=(key('D')?128:0)-(key('A')?128:0);
        pad[1]=(key('S')?128:0)-(key('W')?128:0);
        pad[2]=(key('H')?128:0)-(key('F')?128:0);
        pad[3]=(key('G')?128:0)-(key('T')?128:0);
        const std::array<int,16> keys={'Q','1','E','3',0x26,0x28,0x25,0x27,0x0D,0x08,'J','I','K','L','2','4'};
        for(size_t index=0;index<keys.size();++index)pad[index+4]=key(keys[index])?255:0;
    }
};

class StorylandController {
    SDL_GameController* controller=nullptr;
    bool initialized=false,initializationAttempted=false;
    Uint32 nextScan=0;
    std::array<bool,SDL_CONTROLLER_BUTTON_MAX+2> previousPhysical{};
    std::array<bool,4> previousAxisMoved{};
    int newlyPressedPhysical=-1;
    int newlyMovedAxis=-1;
    static constexpr int LeftTrigger=SDL_CONTROLLER_BUTTON_MAX;
    static constexpr int RightTrigger=SDL_CONTROLLER_BUTTON_MAX+1;
    static constexpr std::array<int,16> defaultButtons={
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,LeftTrigger,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,RightTrigger,
        SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_LEFTSTICK,SDL_CONTROLLER_BUTTON_RIGHTSTICK
    };
    bool physicalDown(int source) const {
        if(!controller)return false;
        if(source==LeftTrigger)return SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>8000;
        if(source==RightTrigger)return SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>8000;
        return source>=0&&source<SDL_CONTROLLER_BUTTON_MAX&&
            SDL_GameControllerGetButton(controller,SDL_GameControllerButton(source))!=0;
    }
    int physicalPressure(int source) const {
        if(!controller)return 0;
        if(source==LeftTrigger||source==RightTrigger) {
            const auto axis=source==LeftTrigger?SDL_CONTROLLER_AXIS_TRIGGERLEFT:SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
            const int value=std::clamp(int(SDL_GameControllerGetAxis(controller,axis)),0,32767);
            constexpr int deadzone=1024;
            return value<=deadzone?0:(value-deadzone)*255/(32767-deadzone);
        }
        return physicalDown(source)?255:0;
    }
public:
    std::array<int,256> pcKeys{};
    std::array<int,16> controllerButtons=defaultButtons;
    std::array<int,4> controllerAxes{{SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,
        SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY}};
    std::array<bool,4> invertedAxes{};
    StorylandControllerMode mode=StorylandControllerMode::Current;
    StorylandControllerState state;
    std::string status="No controller connected";
    bool startHeld=false;
    bool vibrationEnabled=true;
    StorylandController() {resetPcBindings();}
    void resetPcBindings() {for(size_t index=0;index<pcKeys.size();++index)pcKeys[index]=int(index);}
    void resetControllerBindings() {
        controllerButtons=defaultButtons;
        controllerAxes={{SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,
            SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY}};
        invertedAxes={};
    }
    int pcKey(int logicalKey) const {
        return logicalKey>=0&&logicalKey<int(pcKeys.size())?pcKeys[size_t(logicalKey)]:logicalKey;
    }
    void bindPcKey(int logicalKey,int physicalKey) {
        if(logicalKey<=0||logicalKey>=int(pcKeys.size())||physicalKey<=0||physicalKey>=256)return;
        const int previous=pcKeys[size_t(logicalKey)];
        for(size_t index=0;index<pcKeys.size();++index)
            if(int(index)!=logicalKey&&pcKeys[index]==physicalKey)pcKeys[index]=previous;
        pcKeys[size_t(logicalKey)]=physicalKey;
    }
    void bindControllerButton(size_t logicalButton,int physicalButton) {
        if(logicalButton>=controllerButtons.size()||physicalButton<0||
            physicalButton>=int(previousPhysical.size())||physicalButton==SDL_CONTROLLER_BUTTON_GUIDE)return;
        const int previous=controllerButtons[logicalButton];
        for(size_t index=0;index<controllerButtons.size();++index)
            if(index!=logicalButton&&controllerButtons[index]==physicalButton)controllerButtons[index]=previous;
        controllerButtons[logicalButton]=physicalButton;
    }
    void bindControllerAxis(size_t logicalAxis,int physicalAxis) {
        if(logicalAxis>=controllerAxes.size()||physicalAxis<0||physicalAxis>=4)return;
        const int previous=controllerAxes[logicalAxis];
        for(size_t index=0;index<controllerAxes.size();++index)
            if(index!=logicalAxis&&controllerAxes[index]==physicalAxis)controllerAxes[index]=previous;
        controllerAxes[logicalAxis]=physicalAxis;
    }
    static const char* physicalAxisName(int source) {
        static constexpr const char* names[]={"Left stick X","Left stick Y","Right stick X","Right stick Y"};
        return source>=0&&source<4?names[source]:"Unknown";
    }
    static std::string physicalButtonName(int source) {
        if(source==LeftTrigger)return "Left trigger";
        if(source==RightTrigger)return "Right trigger";
        if(source<0||source>=SDL_CONTROLLER_BUTTON_MAX)return "Unknown";
        switch(source) {
        case SDL_CONTROLLER_BUTTON_A:return "A / Cross";
        case SDL_CONTROLLER_BUTTON_B:return "B / Circle";
        case SDL_CONTROLLER_BUTTON_X:return "X / Square";
        case SDL_CONTROLLER_BUTTON_Y:return "Y / Triangle";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return "LB / L1";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return "RB / R1";
        case SDL_CONTROLLER_BUTTON_DPAD_UP:return "D-pad up";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return "D-pad down";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return "D-pad left";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return "D-pad right";
        case SDL_CONTROLLER_BUTTON_START:return "Start / Options";
        case SDL_CONTROLLER_BUTTON_BACK:return "Back / Share";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:return "Left stick press";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:return "Right stick press";
        default:break;
        }
        const char* name=SDL_GameControllerGetStringForButton(SDL_GameControllerButton(source));
        return name?name:"Unknown";
    }
    int pressedPhysicalInput() const {return newlyPressedPhysical;}
    int movedPhysicalAxis() const {return newlyMovedAxis;}
    bool loadBindings(const std::filesystem::path& directory,std::string& error) {
        resetPcBindings();resetControllerBindings();
        if(directory.empty()){error="Controls directory is unavailable";return false;}
        std::ifstream input(directory/"controls.cfg");
        if(!input) {error.clear();return true;}
        std::string line;
        while(std::getline(input,line)) {
            std::istringstream fields(line);
            std::string kind;int logical=-1,physical=-1;
            if(!(fields>>kind>>logical>>physical))continue;
            if(kind=="pc"&&logical>0&&logical<256&&physical>0&&physical<256)
                pcKeys[size_t(logical)]=physical;
            if(kind=="controller"&&logical>=0&&logical<int(controllerButtons.size())&&
                physical>=0&&physical<int(previousPhysical.size())&&physical!=SDL_CONTROLLER_BUTTON_GUIDE)
                controllerButtons[size_t(logical)]=physical;
            if(kind=="axis"&&logical>=0&&logical<int(controllerAxes.size())&&physical>=0&&physical<4)
                controllerAxes[size_t(logical)]=physical;
            if(kind=="invert"&&logical>=0&&logical<int(invertedAxes.size()))
                invertedAxes[size_t(logical)]=physical!=0;
        }
        if(input.bad()){error="Could not finish reading controls.cfg";return false;}
        error.clear();return true;
    }
    bool saveBindings(const std::filesystem::path& directory,std::string& error) const {
        if(directory.empty()){error="Controls directory is unavailable";return false;}
        std::error_code filesystemError;
        std::filesystem::create_directories(directory,filesystemError);
        if(filesystemError){error=filesystemError.message();return false;}
        std::ofstream output(directory/"controls.cfg",std::ios::trunc);
        if(!output){error="Could not write controls.cfg";return false;}
        output<<"version 1\n";
        for(size_t index=1;index<pcKeys.size();++index)
            if(pcKeys[index]!=int(index))output<<"pc "<<index<<' '<<pcKeys[index]<<'\n';
        for(size_t index=0;index<controllerButtons.size();++index)
            if(controllerButtons[index]!=defaultButtons[index])
                output<<"controller "<<index<<' '<<controllerButtons[index]<<'\n';
        for(size_t index=0;index<controllerAxes.size();++index) {
            if(controllerAxes[index]!=int(index))output<<"axis "<<index<<' '<<controllerAxes[index]<<'\n';
            if(invertedAxes[index])output<<"invert "<<index<<" 1\n";
        }
        output.close();
        if(!output){error="Could not finish writing controls.cfg";return false;}
        error.clear();return true;
    }
    void setVibration(bool enabled) {
        vibrationEnabled=enabled;
        if(!enabled&&controller)SDL_GameControllerRumble(controller,0,0,0);
    }
    bool vibrate(float strength,uint32_t milliseconds) {
        if(!vibrationEnabled||!controller||mode!=StorylandControllerMode::Gamepad)return false;
        const auto motor=Uint16(std::clamp(strength,0.0f,1.0f)*65535);
        return SDL_GameControllerRumble(controller,motor,motor,std::min(milliseconds,5000u))==0;
    }
    ~StorylandController() {
        if(controller){SDL_GameControllerRumble(controller,0,0,0);SDL_GameControllerClose(controller);}
        if(initialized)SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
    void select(StorylandControllerMode value) {if(controller)SDL_GameControllerRumble(controller,0,0,0);mode=value;state.pad={};startHeld=true;initializationAttempted=false;newlyPressedPhysical=-1;}
    void update(bool focused,bool allowCapture=false,bool suppressEvents=true) {
        state.pad={};
        newlyPressedPhysical=-1;
        newlyMovedAxis=-1;
        if(mode!=StorylandControllerMode::Gamepad&&!allowCapture)return;
        if(!initialized) {
            if(initializationAttempted)return;
            initializationAttempted=true;
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5,"1");
            if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)!=0) {status=SDL_GetError();return;}
            if(suppressEvents) {
                SDL_GameControllerEventState(SDL_IGNORE);
                SDL_JoystickEventState(SDL_IGNORE);
            }
            initialized=true;
        }
        SDL_PumpEvents();
        SDL_GameControllerUpdate();
        if(controller&&!SDL_GameControllerGetAttached(controller)) {
            SDL_GameControllerClose(controller);controller=nullptr;status="Controller disconnected";
        }
        const auto now=SDL_GetTicks();
        if(!controller&&Sint32(now-nextScan)>=0) {
            nextScan=now+500;
            for(int index=0;index<SDL_NumJoysticks();++index) {
                if(!SDL_IsGameController(index))continue;
                controller=SDL_GameControllerOpen(index);
                if(controller) {previousPhysical={};previousAxisMoved={};const char* name=SDL_GameControllerName(controller);status=name?name:"Controller connected";break;}
            }
        }
        if(!controller)return;
        if(!focused){SDL_GameControllerRumble(controller,0,0,0);previousPhysical={};previousAxisMoved={};return;}
        for(size_t source=0;source<previousPhysical.size();++source) {
            const bool down=physicalDown(int(source));
            if(down&&!previousPhysical[source]&&int(source)!=SDL_CONTROLLER_BUTTON_GUIDE)
                newlyPressedPhysical=int(source);
            previousPhysical[source]=down;
        }
        for(int source=0;source<4;++source) {
            const bool moved=std::abs(int(SDL_GameControllerGetAxis(controller,SDL_GameControllerAxis(source))))>18000;
            if(moved&&!previousAxisMoved[size_t(source)])newlyMovedAxis=source;
            previousAxisMoved[size_t(source)]=moved;
        }
        for(size_t index=0;index<controllerAxes.size();++index) {
            const int value=StorylandControllerState::axis(SDL_GameControllerGetAxis(controller,
                SDL_GameControllerAxis(controllerAxes[index])));
            state.pad[index]=invertedAxes[index]?-value:value;
        }
        for(size_t index=0;index<controllerButtons.size();++index)
            state.pad[index+4]=physicalPressure(controllerButtons[index]);
    }
    bool actionKey(int key,bool driving) const {
        const auto& p=state.pad;
        switch(key) {
        case 'W':return driving?state.down(16):state.movementAxis(true)<0;
        case 'S':return driving?state.down(14):state.movementAxis(true)>0;
        case 'A':return state.movementAxis(false)<0;
        case 'D':return state.movementAxis(false)>0;
        case 'I':return p[3]<-16;
        case 'K':return p[3]>16;
        case 'J':return p[2]<-16;
        case 'L':return p[2]>16;
        case 'F':return state.down(15);
        case 'G':return state.down(17);
        case 'E':return state.down(6);
        case 'R':return state.down(4);
        case 'C':return state.down(19);
        case 'V':return state.down(13);
        case 'H':return state.down(18);
        case 0x20:return driving?state.down(6):state.down(14);
        case 0x10:return state.down(16);
        case 0x11:return state.down(18);
        case 0xA4:return std::hypot(float(state.movementAxis(false)),
            float(state.movementAxis(true)))<80&&!state.down(16);
        default:return false;
        }
    }
};
