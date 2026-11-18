#include "src/storyland/storyland_play_test_hud.h"
#include "src/storyland/storyland_pc_assets.h"
#include "src/storyland/storyland_hud_names.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace {
std::string pcArtworkName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char value) {
        return char(std::tolower(value));
    });
    return name;
}

StorylandRetailFont pcAtlasFont(const RgbaImage& image, bool fancy) {
    StorylandRetailFont font;
    if (image.width < 256 || image.height < 512 || image.width % 16 || image.height % 64 ||
        image.rgba.size() != size_t(image.width) * size_t(image.height) * 4) return font;
    const unsigned cellWidth = unsigned(image.width) / 16;
    const unsigned cellHeight = unsigned(image.height) * 5 / 64;
    font.width = 32;
    font.height = 40;
    const auto add = [&](uint16_t code, unsigned cell) {
        const unsigned left = (cell % 16) * cellWidth;
        const unsigned top = (cell / 16) * cellHeight;
        if (top + cellHeight > unsigned(image.height)) return;
        unsigned first = cellWidth, last = 0;
        for (unsigned y = top; y < top + cellHeight; ++y) {
            for (unsigned x = 0; x < cellWidth; ++x) {
                if (!image.rgba[(size_t(y) * image.width + left + x) * 4 + 3]) continue;
                first = std::min(first, x);
                last = std::max(last, x + 1);
            }
        }
        if (first >= last) return;
        const auto width = int16_t(std::max(1u, ((last - first) * 32 + cellWidth / 2) / cellWidth));
        const int16_t spacing = fancy ? 1 : 2;
        font.glyphs[code] = {0, int16_t(width + spacing), int16_t(-spacing), 40, code,
            uint16_t(left + first), uint16_t(top), uint16_t(last - first), uint16_t(cellHeight)};
    };
    font.glyphs[' '] = {0, 10, 0, 40, ' ', 0, 0, 0, 0};
    for (uint16_t code = 33; code <= 122; ++code) add(code, code - 32);
    add('^', 91);
    add(0xB0, 92);
    add(0xDF, 93);
    add(0xFE, 94);
    add(0x2122, 95);
    static constexpr uint16_t extended[] = {
        0x104, 0x105, 0x118, 0x119, 0x106, 0x141, 0x143, 0xD3,
        0x15A, 0x179, 0x17B, 0x107, 0x142, 0x144, 0xF3, 0x15B,
        0x17A, 0x17C, 0xC0, 0xC8, 0xCC, 0xD2, 0xD9, 0xE0,
        0xE8, 0xEC, 0xF2, 0xF9, 0xC3, 0xD5, 0xE3, 0xF5,
        0xC1, 0xDA, 0xC9, 0xCD, 0xC7, 0xE7, 0x178, 0xFF,
        0xD0, 0xE1, 0xFA, 0xE9, 0xED, 0xD1, 0xF1, 0xBF,
        0xC2, 0xCA, 0xD4, 0xDB, 0xE2, 0xEA, 0xF4, 0xFB,
        0xC6, 0x152, 0xE6, 0x153, 0xC4, 0xCF, 0xD6, 0xDC,
        0xE4, 0xEF, 0xF6, 0xFC
    };
    for (unsigned index = 0; index < std::size(extended); ++index) {
        if (fancy && index < 18) continue;
        add(extended[index], 96 + index);
    }
    if (fancy) {
        add(0xA3, 101);
        add(0xCE, 108);
    }
    return font;
}

StorylandRetailFont pcNumberFont(const RgbaImage& image) {
    StorylandRetailFont font;
    if (image.width < 256 || image.height < 512 || image.width % 16 || image.height % 64)
        return font;
    const auto cellWidth = uint16_t(image.width / 16);
    const auto cellHeight = uint16_t(unsigned(image.height) * 5 / 64);
    font.width = 11;
    font.height = 10;
    font.horizontalScale = 17.0f / 21.0f;
    const auto add = [&](uint16_t code, unsigned column, int16_t advance, int16_t width) {
        font.glyphs[code] = {0, advance, int16_t(width - advance), 10, code,
            uint16_t(column * cellWidth), uint16_t(11 * cellHeight), cellWidth, cellHeight};
    };
    for (unsigned digit = 0; digit < 10; ++digit) add(uint16_t('0' + digit), digit, 9, 11);
    add(':', 10, 5, 11);
    add('$', 11, 9, 11);
    add('*', 13, 12, 14);
    add('+', 15, 9, 11);
    return font;
}
}

void StorylandRetailHudAssets::setPcMode(bool enabled) {
    if (pcMode == enabled) return;
    for (const auto& role : pcSpriteBindings) {
        const auto source = sprites.find((enabled ? "pc:" : "retail:") + role);
        if (source != sprites.end()) sprites[role] = source->second;
        else sprites.erase(role);
    }
    for (const auto& role : pcFontBindings) {
        const auto source = fonts.find((enabled ? "pc:" : "retail:") + role);
        if (source != fonts.end()) fonts[role] = source->second;
        else fonts.erase(role);
    }
    pcMode = enabled;
    ++generation;
}

void StorylandRetailHudAssets::loadPcAssets(bool vcs, const std::filesystem::path& artworkDirectory) {
    const bool restorePcMode = pcMode;
    setPcMode(false);
    for (auto entry = sprites.begin(); entry != sprites.end();) {
        if (entry->first.rfind("pc:", 0) == 0 || entry->first.rfind("retail:", 0) == 0 ||
            entry->first.rfind("frontend:", 0) == 0)
            entry = sprites.erase(entry);
        else ++entry;
    }
    for (auto entry = fonts.begin(); entry != fonts.end();) {
        if (entry->first.rfind("pc:", 0) == 0 || entry->first.rfind("retail:", 0) == 0 ||
            entry->first.rfind("frontend:", 0) == 0) entry = fonts.erase(entry);
        else ++entry;
    }
    pcSpriteBindings.clear();
    pcFontBindings.clear();
    pcAssetsDiagnostic.clear();
    const auto bindSprite = [&](const std::string& role, const std::shared_ptr<const RgbaImage>& image) {
        if (std::find(pcSpriteBindings.begin(), pcSpriteBindings.end(), role) == pcSpriteBindings.end()) {
            const auto original = sprites.find(role);
            if (original != sprites.end()) sprites["retail:" + role] = original->second;
            pcSpriteBindings.push_back(role);
        }
        sprites["pc:" + role] = image;
    };
    const auto bindFont = [&](const std::string& role, const StorylandRetailFont& font,
            const std::shared_ptr<const RgbaImage>& image) {
        if (std::find(pcFontBindings.begin(), pcFontBindings.end(), role) == pcFontBindings.end()) {
            const auto original = fonts.find(role);
            if (original != fonts.end()) fonts["retail:" + role] = original->second;
            pcFontBindings.push_back(role);
        }
        fonts["pc:" + role] = font;
        bindSprite(role, image);
    };
    for (const auto pack : {StorylandPcTexturePack::Hud, StorylandPcTexturePack::Frontend,
            StorylandPcTexturePack::Fonts, StorylandPcTexturePack::Particles, StorylandPcTexturePack::Buttons}) {
        std::vector<StorylandPcTexture> textures;
        std::string diagnostic;
        storylandReadPcTextures(pack, textures, diagnostic, artworkDirectory);
        if (!diagnostic.empty()) {
            if (!pcAssetsDiagnostic.empty()) pcAssetsDiagnostic += '\n';
            pcAssetsDiagnostic += diagnostic;
        }
        for (auto& texture : textures) {
            const auto name = pcArtworkName(texture.name);
            const auto image = std::make_shared<const RgbaImage>(std::move(texture.image));
            if (pack == StorylandPcTexturePack::Buttons) {
                sprites["pc:button:" + name] = image;
            } else if (pack == StorylandPcTexturePack::Fonts) {
                const auto font = pcAtlasFont(*image, name == "fancytext");
                if (font.glyphs.empty()) continue;
                bindFont(name, font, image);
                bindSprite("fsfonts:" + name, image);
                sprites["frontend:" + name] = image;
                fonts["frontend:" + name] = font;
                const char* lcsName = name == "fancytext" ? "font2" :
                    name == "gametext" ? "font1" : name == "bigtext" ? "font3" : nullptr;
                if (lcsName) {
                    bindFont(lcsName, font, image);
                    bindSprite(std::string("fonts:") + lcsName, image);
                }
                if (name == "bigtext") {
                    const auto numbers = pcNumberFont(*image);
                    bindFont("hudnumbers", numbers, image);
                    bindFont("hudmoney", numbers, image);
                    bindFont("hudwanted", numbers, image);
                }
            } else if (pack == StorylandPcTexturePack::Frontend) {
                sprites["frontend:" + name] = image;
                if (texture.group == "fronten1") {
                    const auto station = name == "radio_tplayer" ? "usertracks" : storylandRadioLogoKey(name);
                    if (!station.empty()) {
                        sprites["frontend:radio:" + station] = image;
                        if (vcs) {
                            const auto native = storylandVcsRadioSpriteName(station);
                            if (!native.empty()) sprites["frontend:" + native] = image;
                            bindSprite("radio:" + station, image);
                            if (!native.empty()) bindSprite(native, image);
                        }
                    }
                }
            } else if (pack == StorylandPcTexturePack::Hud) {
                bindSprite(name, image);
                bindSprite("hud:" + name, image);
                if (texture.group == "weapons") bindSprite("weapon:" + storylandWeaponHudKey(name), image);
                if (name == "baroutline") bindSprite("bar_outline", image);
                if (name == "triangle_64") bindSprite("triangle16", image);
            } else if (pack == StorylandPcTexturePack::Particles) {
                bindSprite("particle:" + name, image);
                // Keep particle rasters scoped: splash and rubbish names must never resolve to frontend art.
                if (name.rfind("splash", 0) != 0) bindSprite(name, image);
            }
        }
    }
    const auto particleAlias = [&](const char* role, const char* name) {
        const auto image = sprites.find(std::string("pc:particle:") + name);
        if (image == sprites.end()) return;
        bindSprite(std::string("particle:") + role, image->second);
        if (std::string(role).rfind("splash", 0) != 0) bindSprite(role, image->second);
    };
    particleAlias("skid", "particleskid");
    particleAlias("coronaring", "coronaringa");
    particleAlias("headlight_single", "headlight1");
    particleAlias("boatwake2", "boatwake1");
    particleAlias("blood", "bloodsplat2");
    particleAlias("smoke1", "smoke");
    particleAlias("smoke2", "smoke");
    particleAlias("smoke3", "smokeii_3");
    particleAlias("firehose", "wgush1");
    particleAlias("waterspray1", "wjet2");
    particleAlias("waterspray2", "wjet4");
    particleAlias("waterspray3", "wjet6");
    for (const char* name : {"gunflash1", "gunflash2", "gunflash3", "gunflash4"})
        particleAlias(name, "muzzleflash");
    for (const char* name : {"flame2", "flame3", "flame4", "flame5"})
        particleAlias(name, "flame1b");
    for (const char* name : {"splash2", "splash3", "splash4", "splash5"})
        particleAlias(name, "splash1");
    for (const char* name : {"carsplash_01", "carsplash_03", "carsplash_04"})
        particleAlias(name, "carsplash_02");
    ++generation;
    setPcMode(restorePcMode);
}
