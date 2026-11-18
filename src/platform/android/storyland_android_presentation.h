#pragma once
#include "src/platform/android/storyland_android_layout.h"
#include "src/platform/android/storyland_android_presentation_data.h"
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>

class StorylandAndroidPresentation {
public:
    enum class View { None, Intro, About, Thanks };

    void showIntro(uint32_t seed) {
        view = View::Intro;
        elapsed = 0;
        quoteSeed = seed;
    }

    void showThanks() {
        view = View::Thanks;
        elapsed = 0;
        credit = 0;
        paused = false;
    }

    void showAbout() { view = View::About; }
    void close() { view = View::None; }
    bool active() const { return view != View::None; }
    bool intro() const { return view == View::Intro; }
    int musicTrack() const { return view == View::Intro ? 1 : view == View::Thanks ? 2 : 0; }

    void advance(float seconds) {
        if (!std::isfinite(seconds) || seconds < 0 || !active()) return;
        if (view == View::Thanks && paused) return;
        elapsed += seconds;
        if (view == View::Intro && elapsed >= 5.5f) close();
        if (view == View::Thanks && elapsed >= 5.4f) {
            elapsed = std::fmod(elapsed,5.4f);
            credit = (credit + 1) % std::size(storylandAndroidCredits);
        }
    }

    void draw(const StorylandAndroidLayout& layout, ImFont* pixelFont, const std::string& musicError) {
        if (!active()) return;
        const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::SetNextWindowPos(layout.origin);
        ImGui::SetNextWindowSize(layout.size);
        ImGui::SetNextWindowScroll({0,0});
        ImGui::Begin("Storyland presentation",nullptr,flags);
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 end{layout.origin.x+layout.size.x,layout.origin.y+layout.size.y};
        draw->AddRectFilled(layout.origin,end,IM_COL32(8,8,24,255));
        draw->PushClipRect(layout.origin,end,true);
        if (view == View::Intro) drawIntro(draw,layout);
        else if (view == View::Thanks) drawThanks(draw,layout,pixelFont);
        else drawAbout(draw,layout,pixelFont);
        draw->PopClipRect();

        const float unit = layout.unit;
        ImGui::SetCursorPos({12*unit,layout.size.y-56*unit});
        if (view == View::Intro) {
            ImGui::SetCursorPosX(layout.size.x-142*unit);
            if (storylandTouchButton("Skip intro",unit,130)) close();
        } else if (view == View::Thanks) {
            if (storylandTouchButton("<",unit,44)) changeCredit(-1);
            ImGui::SameLine();
            if (storylandTouchButton(">",unit,44)) changeCredit(1);
            ImGui::SameLine();
            if (storylandTouchButton(paused?"Resume":"Pause",unit,84)) paused=!paused;
            ImGui::SameLine();
            if (storylandTouchButton("Close",unit,72)) close();
        } else {
            if (storylandTouchButton("Special Thanks",unit)) showThanks();
            ImGui::SameLine();
            if (storylandTouchButton("Close",unit,72)) close();
        }
        if (!musicError.empty() && musicTrack()) {
            const float width = layout.size.x-24*unit;
            const float size = 13*unit;
            const auto extent = ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,width,musicError.c_str());
            const ImVec2 origin{layout.origin.x+12*unit,layout.origin.y+layout.size.y-64*unit-extent.y};
            draw->AddRectFilled(origin,{origin.x+width,origin.y+extent.y},IM_COL32(8,8,24,245));
            draw->AddText(ImGui::GetFont(),size,origin,IM_COL32(255,180,180,255),musicError.c_str(),nullptr,width);
        }
        ImGui::End();
    }

private:
    View view = View::None;
    float elapsed = 0;
    size_t credit = 0;
    uint32_t quoteSeed = 0;
    bool paused = false;

    static uint32_t hash(uint32_t value) {
        value ^= value >> 16;
        value *= 0x7FEB352Du;
        value ^= value >> 15;
        value *= 0x846CA68Bu;
        return value ^ (value >> 16);
    }

    static ImU32 colour565(float red, float green, float blue, float alpha=1) {
        return IM_COL32((std::clamp(int(red),0,255)>>3)<<3,
            (std::clamp(int(green),0,255)>>2)<<2,
            (std::clamp(int(blue),0,255)>>3)<<3,int(std::clamp(alpha,0.f,1.f)*255));
    }

    static void centeredText(ImDrawList* draw, ImFont* font, float size, float centerX,
        float y, float width, ImU32 colour, const char* text) {
        const auto extent = font->CalcTextSizeA(size,FLT_MAX,width,text);
        draw->AddText(font,size,{std::round(centerX-extent.x*.5f),std::round(y)},colour,text,nullptr,width);
    }

    static float pixelSize(ImFont* font, const char* text, float target, float width) {
        const float naturalWidth = font->CalcTextSizeA(13,FLT_MAX,0,text).x;
        const float scale = std::max(1.f,std::floor(std::min(target/13,width/std::max(1.f,naturalWidth))));
        return 13*scale;
    }

    void changeCredit(int direction) {
        const int count = int(std::size(storylandAndroidCredits));
        credit = size_t((int(credit)+direction+count)%count);
        elapsed = 0;
    }

    void drawIntro(ImDrawList* draw, const StorylandAndroidLayout& layout) const {
        const float unit = layout.unit;
        const auto origin = layout.origin;
        const auto size = layout.size;
        const float center = origin.x+size.x*.5f;
        float palette = std::clamp((elapsed-.2f)/1.6f,0.f,1.f);
        palette = palette*palette*(3-2*palette);
        draw->AddRectFilledMultiColor(origin,{origin.x+size.x,origin.y+size.y},
            IM_COL32(0,4,14,255),IM_COL32(12,4,22,255),IM_COL32(12,4,22,255),IM_COL32(0,4,14,255));
        const char glyphs[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ<>[]{}+=*#:/\\|";
        const float cell = 14*unit;
        const int columns = int(std::ceil(size.x/cell));
        for (int column=0;column<columns;++column) {
            const uint32_t seed = hash(uint32_t(column)*0x9E3779B9u+quoteSeed);
            const int trail = 12+int(seed%19);
            const float travel = size.y+cell*trail;
            const float speed = (90+float((seed>>8)%90))*unit;
            const float head = std::fmod(float(seed%10000)*.0001f*travel+elapsed*speed,travel);
            for (int row=trail;row>=0;--row) {
                const float y = origin.y+head-float(row)*cell;
                if (y<origin.y-cell || y>origin.y+size.y) continue;
                const uint32_t random = hash(seed+uint32_t(row)*101+uint32_t(elapsed*8)*37);
                const char text[] = {glyphs[random%(sizeof(glyphs)-1)],0};
                const float fade = 1-float(row)/float(trail+1);
                const float strength = row==0?1.f:.08f+.7f*fade*fade;
                const ImU32 colour = row==0?IM_COL32(245,245,255,240):
                    IM_COL32(int(30+225*palette),int(132-16*palette),int(255-59*palette),int(210*strength));
                draw->AddText(ImGui::GetFont(),13*unit,{origin.x+column*cell,y},colour,text);
            }
        }

        const char* quote = storylandAndroidQuotes[hash(quoteSeed)%std::size(storylandAndroidQuotes)];
        const float quoteWidth = std::min(size.x-56*unit,760*unit);
        const auto quoteSize = ImGui::GetFont()->CalcTextSizeA(17*unit,FLT_MAX,quoteWidth,quote);
        const float quoteTop = origin.y+18*unit;
        draw->AddRectFilled({center-quoteWidth*.5f-12*unit,quoteTop-8*unit},
            {center+quoteWidth*.5f+12*unit,quoteTop+quoteSize.y+8*unit},IM_COL32(5,8,17,245),6*unit);
        centeredText(draw,ImGui::GetFont(),17*unit,center,quoteTop,quoteWidth,IM_COL32(245,248,255,255),quote);

        size_t columnsInLogo = 0, rowColumns = 0, rows = 1;
        for (char32_t glyph : storylandAndroidStudioLogo) {
            if (glyph == U'\n') { columnsInLogo=std::max(columnsInLogo,rowColumns);rowColumns=0;++rows; }
            else if (glyph) ++rowColumns;
        }
        columnsInLogo=std::max(columnsInLogo,rowColumns);
        const float top = quoteTop+quoteSize.y+18*unit;
        const float bottom = origin.y+size.y-126*unit;
        const float cellWidth = std::max(.5f,std::min(size.x*.92f/float(columnsInLogo),
            std::max(10.f,bottom-top)/(float(rows)*2)));
        const float cellHeight = cellWidth*2;
        const ImVec2 logoOrigin{center-float(columnsInLogo)*cellWidth*.5f,
            top+std::max(0.f,(bottom-top-float(rows)*cellHeight)*.5f)};
        const float reveal = std::clamp((elapsed-.2f)/.55f,0.f,1.f);
        size_t column = 0, row = 0;
        for (char32_t glyph : storylandAndroidStudioLogo) {
            if (glyph == U'\n') {column=0;++row;continue;}
            if (glyph && glyph != U' ') {
                ImVec2 low{logoOrigin.x+column*cellWidth,logoOrigin.y+row*cellHeight};
                ImVec2 high{low.x+cellWidth,low.y+cellHeight};
                float brightness = 1;
                if (glyph==U'▄') low.y+=cellHeight*.5f;
                if (glyph==U'▀') high.y-=cellHeight*.5f;
                if (glyph==U'▐') low.x+=cellWidth*.5f;
                if (glyph==U'▌') high.x-=cellWidth*.5f;
                if (glyph==U'░') brightness=.32f;
                if (glyph==U'▒') brightness=.58f;
                if (glyph==U'▓') brightness=.8f;
                draw->AddRectFilled(low,high,colour565(48+207*palette,152-24*palette,255-51*palette,reveal*brightness));
            }
            ++column;
        }
        const float captionTop = origin.y+size.y-126*unit;
        const float titleAlpha = std::clamp((elapsed-.65f)*3,0.f,1.f);
        centeredText(draw,ImGui::GetFont(),14*unit,center,captionTop,size.x-24*unit,
            IM_COL32(255,180,220,int(255*titleAlpha)),"REIGNS STUDIOS PRESENTS");
        centeredText(draw,ImGui::GetFont(),32*unit,center,captionTop+22*unit,size.x-24*unit,
            IM_COL32(255,220,240,int(255*titleAlpha)),"STORYLAND");
        draw->AddRect(origin,{origin.x+size.x,origin.y+size.y},
            colour565(32+223*palette,112+16*palette,224-20*palette),8*unit,0,2*unit);
        if (elapsed>5.2f) draw->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},
            IM_COL32(0,0,0,int(std::clamp((elapsed-5.2f)/.3f,0.f,1.f)*255)));
    }

    void drawThanks(ImDrawList* draw, const StorylandAndroidLayout& layout, ImFont* pixelFont) const {
        const float unit = layout.unit;
        const auto origin = layout.origin;
        const auto size = layout.size;
        const float center = origin.x+size.x*.5f;
        centeredText(draw,pixelFont,pixelSize(pixelFont,"SPECIAL THANKS",24*unit,size.x-24*unit),center,
            origin.y+12*unit,size.x-24*unit,colour565(255,224,96),"SPECIAL THANKS");
        centeredText(draw,ImGui::GetFont(),13*unit,center,origin.y+44*unit,size.x-32*unit,
            colour565(176,192,224),"(I'm not affiliated with some of these peeps)");
        const auto& entry = storylandAndroidCredits[credit];
        const float width = size.x-40*unit;
        const float nameSize = pixelSize(pixelFont,entry.name,30*unit,width-16*unit);
        const float detailWidth = std::min(size.x-40*unit,680*unit);
        const auto detailSize = ImGui::GetFont()->CalcTextSizeA(16*unit,FLT_MAX,detailWidth,entry.contribution);
        const float availableTop = origin.y+78*unit;
        const float availableBottom = origin.y+size.y-66*unit;
        const float textHeight = nameSize+10*unit+detailSize.y;
        const float settledY = availableTop+std::max(0.f,(availableBottom-availableTop-textHeight)*.45f);
        const float travel = std::clamp(elapsed/1.65f,0.f,1.f);
        const float eased = 1-std::pow(1-travel,3.f);
        const float nameY = availableBottom+(settledY-availableBottom)*eased;
        const float visibility = elapsed<=3.05f?1.f:std::clamp(1-(elapsed-3.05f)/1.1f,0.f,1.f);
        const float pulse = .5f+.5f*std::sin(elapsed*5.2f+float(credit));
        const bool ruby = std::strcmp(entry.name,"Ruby")==0;
        const float time = (float(credit)*5.4f+elapsed)*3;
        const auto nameColour = colour565(ruby?255:160+95*std::sin(time),
            ruby?24+48*pulse:160+95*std::sin(time+2.0943951f),
            ruby?40+48*pulse:160+95*std::sin(time+4.1887902f),visibility);
        draw->PushClipRect({origin.x,availableTop},{origin.x+size.x,availableBottom},true);
        centeredText(draw,pixelFont,nameSize,center,nameY,width,nameColour,entry.name);
        centeredText(draw,ImGui::GetFont(),16*unit,center,nameY+nameSize+10*unit,
            detailWidth,colour565(244,224,255,visibility),entry.contribution);
        if (std::strcmp(entry.name,"Rockstar Leeds")==0) {
            constexpr unsigned char heart[] = {0x66,0xff,0xff,0x7e,0x3c,0x18};
            const auto nameExtent = pixelFont->CalcTextSizeA(nameSize,FLT_MAX,0,entry.name);
            const float pixel = std::max(1.f,std::floor(unit*1.5f));
            const float x = center+nameExtent.x*.5f+4*unit;
            for(int row=0;row<6;++row)for(int column=0;column<8;++column)if(heart[row]&(0x80>>column))
                draw->AddRectFilled({x+column*pixel,nameY+nameSize*.35f+row*pixel},
                    {x+(column+1)*pixel,nameY+nameSize*.35f+(row+1)*pixel},colour565(248,36,56,visibility));
        }
        drawFireworks(draw,layout);
        draw->PopClipRect();
        const std::string count = std::to_string(credit+1)+" / "+std::to_string(std::size(storylandAndroidCredits));
        centeredText(draw,pixelFont,13*std::max(1.f,std::floor(unit)),origin.x+size.x-40*unit,
            origin.y+size.y-42*unit,64*unit,colour565(176,192,224),count.c_str());
    }

    void drawFireworks(ImDrawList* draw, const StorylandAndroidLayout& layout) const {
        constexpr float circle = 6.283185307f;
        constexpr float colours[][3] = {{255,80,80},{80,255,112},{96,144,255},{255,224,80},{208,96,255},{80,240,255}};
        const float scale = std::min(layout.size.x/480,layout.size.y/270);
        for(int burst=0;burst<3;++burst) {
            const float time = elapsed-2.84f-float(burst)*.25f;
            const ImVec2 origin{layout.origin.x+layout.size.x*(.30f+float(burst)*.19f),
                layout.origin.y+layout.size.y*(.35f+float(burst%2)*.11f)};
            if(time>=-.46f && time<0) {
                const float ascent=1+time/.46f;
                const float y=layout.origin.y+layout.size.y*.82f+
                    (origin.y-layout.origin.y-layout.size.y*.82f)*ascent*ascent;
                draw->AddRectFilled({origin.x-scale,y-8*scale},{origin.x+scale,y},colour565(255,224,80));
            }
            if(time<0 || time>=2.1f)continue;
            const float fade=1-time/2.1f;
            for(int particle=0;particle<64;++particle) {
                const uint32_t seed=hash(uint32_t(particle+1)*0x9E3779B9u+uint32_t(credit)*131+uint32_t(burst)*317);
                if(fade<float((seed>>19)&255)/255 && ((seed>>24)+uint32_t(time*18))%6)continue;
                const float angle=circle*(float(particle)+float(seed&255)/1024)/64;
                const float speed=31+float((seed>>16)%49);
                const float ring=particle%4==0?.7f:1;
                const float distance=speed*time*ring*(1-std::min(time*.12f,.23f));
                const float x=std::round(origin.x/scale+std::cos(angle)*distance)*scale;
                const float y=std::round(origin.y/scale+std::sin(angle)*distance+12*time*time)*scale;
                const float radius=(fade>.75f&&particle%8==0?3:fade>.3f?2:1)*scale;
                const auto& colour=colours[(seed>>7)%6];
                draw->AddRectFilled({x-radius,y-radius},{x+radius,y+radius},colour565(colour[0],colour[1],colour[2],fade));
            }
        }
    }

    static void drawAbout(ImDrawList* draw, const StorylandAndroidLayout& layout, ImFont* pixelFont) {
        const float unit=layout.unit;
        const float center=layout.origin.x+layout.size.x*.5f;
        const float width=layout.size.x-32*unit;
        centeredText(draw,pixelFont,pixelSize(pixelFont,"STORYLAND",32*unit,width),center,
            layout.origin.y+24*unit,width,colour565(255,128,204),"STORYLAND");
        centeredText(draw,ImGui::GetFont(),18*unit,center,layout.origin.y+76*unit,width,
            colour565(244,224,255),STORYLAND_VERSION "\nReigns Studios\nAuthor: spicybung");
        centeredText(draw,ImGui::GetFont(),15*unit,center,layout.origin.y+152*unit,width,
            colour565(176,192,224),"LCS / VCS resource editing and playback\nGNU General Public License, version 3");
    }
};
