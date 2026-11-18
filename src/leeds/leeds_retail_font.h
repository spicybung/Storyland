#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Native glyph records contain left and right horizontal bearings.
struct StorylandRetailFontGlyph {
    int16_t bearing=0,advance=0,trailing=0;
    uint16_t height=0,code=0,u=0,v=0;
    uint16_t atlasWidth=0,atlasHeight=0;
    int rasterWidth() const { return int(bearing)+int(advance)+int(trailing); }
};
struct StorylandRetailFont {
    uint16_t width=0,height=0;
    float horizontalScale=1.0f;
    bool uppercaseGlyphs=false;
    std::map<uint16_t,StorylandRetailFontGlyph> glyphs,specialGlyphs;
};

inline const StorylandRetailFontGlyph* storylandRetailTextGlyph(
    const StorylandRetailFont& font,uint16_t code,bool special=false) {
    const auto& glyphs=special?font.specialGlyphs:font.glyphs;
    if(font.uppercaseGlyphs&&!special&&((code>='a'&&code<='z')||
        (code>=0xE0&&code<=0xFE&&code!=0xF7)))code=uint16_t(code-32);
    const auto found=glyphs.find(code);
    return found==glyphs.end()?nullptr:&found->second;
}

inline std::map<std::string,StorylandRetailFont> storylandDecodeRetailFonts(const std::vector<uint8_t>& data,bool vcs) {
    std::map<std::string,StorylandRetailFont> fonts;
    const auto word=[&](size_t offset)->uint32_t {
        if(offset>data.size()||data.size()-offset<4)return 0;
        return uint32_t(data[offset])|(uint32_t(data[offset+1])<<8)|(uint32_t(data[offset+2])<<16)|(uint32_t(data[offset+3])<<24);
    };
    const auto half=[&](size_t offset)->uint16_t {
        if(offset>data.size()||data.size()-offset<2)return 0;
        return uint16_t(data[offset])|(uint16_t(data[offset+1])<<8);
    };
    if(!vcs)return fonts; // LCS uses fixed atlas cells and executable width tables.
    const uint32_t table=word(0xF4);
    const char* names[]={"fancytext","gametext","bigtext","pagertext"};
    if(!table||uint64_t(table)+16>data.size())return fonts;
    for(unsigned index=0;index<4;++index) {
        const uint32_t row=word(size_t(table)+index*4);
        if(!row||uint64_t(row)+16>data.size())continue;
        const uint16_t count=half(row),buttons=half(row+2);
        const uint32_t glyphs=word(row+8),special=word(row+12);
        StorylandRetailFont font;font.width=half(row+4);font.height=half(row+6);
        font.uppercaseGlyphs=index==2;
        if(!count||count>2048||buttons>128||!font.width||font.width>256||!font.height||font.height>256||
            !glyphs||uint64_t(glyphs)+uint64_t(count)*14>data.size())continue;
        const auto decode=[&](uint32_t pointer,uint16_t number,auto& destination) {
            if(!pointer||uint64_t(pointer)+uint64_t(number)*14>data.size())return;
            for(uint16_t glyph=0;glyph<number;++glyph) {
                const size_t entry=size_t(pointer)+size_t(glyph)*14;
                StorylandRetailFontGlyph value{int16_t(half(entry)),int16_t(half(entry+2)),int16_t(half(entry+4)),
                    half(entry+6),half(entry+8),half(entry+10),half(entry+12)};
                if(value.bearing>=-256&&value.bearing<=256&&value.trailing>=-256&&value.trailing<=256&&
                    value.advance>=0&&value.advance<=256&&
                    ((value.code==32&&value.rasterWidth()>=0&&value.height<=256)||
                     (value.rasterWidth()>0&&value.rasterWidth()<=512&&value.height>0&&value.height<=256))&&value.u<2048&&value.v<2048) {
                    static constexpr uint16_t windows1252[32]={
                        0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,
                        0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,
                        0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
                        0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178
                    };
                    if(vcs&&value.code>=0x80&&value.code<=0x9F)value.code=windows1252[value.code-0x80];
                    destination.emplace(value.code,value);
                }
            }
        };
        decode(glyphs,count,font.glyphs);decode(special,buttons,font.specialGlyphs);
        if(font.glyphs.count(32)&&font.glyphs.size()>32)fonts.emplace(names[index],std::move(font));
    }
    return fonts;
}

inline StorylandRetailFont storylandLcsRetailFont(unsigned style,uint16_t atlasWidth,uint16_t atlasHeight) {
    StorylandRetailFont font;
    if(style>2||atlasWidth<16||atlasHeight<128)return font;
    // LCS PS2 SLUS_214.23 font widths at file offsets 0x28E148, 0x28E48E and 0x28E7D4.
    static constexpr int16_t advances[3][192]={
        {
            19,10,12,28,26,32,32,6,12,12,16,28,12,14,12,16,
            26,12,26,24,28,26,26,26,28,26,12,12,26,26,28,20,
            32,30,24,26,26,22,22,28,26,10,20,24,22,30,24,28,
            22,28,24,26,24,24,30,36,28,28,22,14,16,12,26,22,
            10,30,24,26,26,22,22,28,26,10,20,24,22,30,24,28,
            22,28,24,26,24,24,30,36,28,28,22,14,16,12,26,22,
            30,30,30,30,30,30,30,26,22,22,22,22,12,12,14,14,
            30,24,28,28,28,28,28,26,28,24,24,24,24,28,26,24,
            30,30,30,30,30,30,30,26,22,22,22,22,12,12,14,14,
            30,24,28,28,28,28,28,26,28,24,24,24,24,28,26,26,
            28,12,20,14,12,32,12,12,22,20,14,32,12,32,26,10,
            20,26,22,28,8,18,14,30,18,20,30,16,6,20,20,36,
        },
        {
            8,10,18,22,26,28,26,10,12,12,18,22,10,14,8,26,
            24,18,22,22,24,22,22,22,22,22,10,10,20,20,22,22,
            30,24,22,24,24,20,18,26,24,10,20,24,20,28,24,24,
            24,28,24,22,24,22,24,28,24,22,24,12,16,12,20,20,
            12,20,18,19,20,20,14,20,18,9,10,16,8,26,18,20,
            18,20,18,20,16,18,18,26,18,18,18,14,8,14,18,-2,
            26,26,26,26,26,26,30,24,20,20,20,20,12,12,14,14,
            28,26,26,24,24,24,24,18,28,24,24,24,24,26,24,20,
            20,20,20,20,20,20,30,20,20,20,20,20,12,12,16,14,
            20,18,20,20,20,20,20,20,22,18,18,18,18,20,20,20,
            24,10,16,16,10,30,10,10,16,16,16,30,10,30,26,8,
            20,26,22,26,8,24,14,28,14,18,28,16,12,16,22,30,
        },
        {
            17,10,2,2,20,2,24,10,15,15,2,2,9,2,10,24,
            20,19,20,20,22,20,20,20,20,20,10,20,20,20,20,20,
            17,20,20,10,20,20,15,30,20,20,20,20,20,20,22,20,
            21,32,20,20,20,20,20,20,2,20,2,30,20,20,20,20,
            2,10,10,10,10,2,20,20,20,20,2,20,2,2,20,20,
            20,20,2,2,22,32,20,10,11,11,20,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        },
    };
    static constexpr uint16_t tail[32]={
        0x20AC,0x201A,0x201E,0x2C6,0x2039,0x152,0x2018,0x2019,
        0x201C,0x201D,0x2DC,0x2122,0x203A,0x153,0x178,0xA1,
        0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,
        0xAA,0xAB,0xAE,0xB0,0xB9,0xBB,0xBF,0x7F
    };
    font.width=32;font.height=style==2?32:40;
    const uint16_t cellWidth=atlasWidth/16,cellHeight=uint16_t(unsigned(atlasHeight)*5/(style==2?32:64));
    for(unsigned index=0;index<(style==2?96u:192u);++index) {
        uint16_t code=uint16_t(index+32);
        if(code>=0x80&&code<=0xBF)code+=0x40;
        else if(code>=0xC0)code=tail[code-0xC0];
        if(code==0x7F)continue;
        const int16_t advance=advances[style][index];
        font.glyphs.emplace(code,StorylandRetailFontGlyph{0,advance,int16_t(style==2?0:32-advance),font.height,code,
            uint16_t((index%16)*cellWidth),uint16_t((index/16)*cellHeight),uint16_t(style==2?unsigned(cellWidth)*advance/32:cellWidth),cellHeight});
    }
    return font;
}

inline StorylandRetailFont storylandLcsHudWantedFont(uint16_t width,uint16_t height) {
    auto font=storylandLcsRetailFont(2,width,height);
    // The native font3 atlas carries the wanted star in row 5, column 5.
    const auto star=font.glyphs.find('u');
    if(star==font.glyphs.end())return {};
    auto glyph=star->second;
    glyph.code='*';
    font.glyphs.clear();
    font.glyphs.emplace('*',glyph);
    return font;
}

inline StorylandRetailFont storylandVcsHudNumberFont(uint16_t width,uint16_t height,bool money) {
    StorylandRetailFont font;
    if(width<8||height<64)return font;
    // Native 0x31EF08: eight columns, row pitch 17/64, digits 0..9 or 11..20.
    font.width=11;font.height=10;font.horizontalScale=17.0f/21.0f;
    const uint16_t cellWidth=width/8,cellHeight=uint16_t(unsigned(height)*17/64);
    const auto add=[&](uint16_t code,unsigned index,int16_t advance,int16_t rasterWidth) {
        font.glyphs.emplace(code,StorylandRetailFontGlyph{0,advance,int16_t(rasterWidth-advance),10,code,
            uint16_t((index%8)*cellWidth),uint16_t((index/8)*cellHeight),cellWidth,cellHeight});
    };
    for(unsigned digit=0;digit<10;++digit)add(uint16_t('0'+digit),digit+(money?11:0),9,11);
    add(money?'$':':',money?21:10,money?9:5,11);
    add('*',22,12,14);
    return font;
}
