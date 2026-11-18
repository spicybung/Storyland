#pragma once
#include "imgui.h"
#include "src/platform/android/storyland_android_layout.h"
#include "src/platform/android/storyland_android_scene_view.h"
#include "src/leeds/leeds_archive.h"
#include "src/leeds/leeds_img_stream.h"
#include "src/leeds/leeds_dtz.h"
#include "src/leeds/leeds_model.h"
#include "src/leeds/leeds_texture.h"
#include <GLES3/gl3.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <numeric>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>

class StorylandAndroidResources {
public:
    bool standalone() const { return standaloneDocument || archiveMode != ArchiveMode::None || loadingArchive.valid(); }
    bool modified() const { return changed || archiveDirty; }
    const std::string& status() const { return message; }
    bool busy() const { return saving.valid() || loadingArchive.valid(); }
    int importToken() const { return generation; }
    void releasePreview() {
        if (previewTexture) glDeleteTextures(1,&previewTexture);
        if (galleryTexture) glDeleteTextures(1,&galleryTexture);
        galleryTexture=0;galleryTextureIndex=-1;
        previewTexture = 0;
        scene.release();
        activeSceneId = -3;
    }
    void reset() {
        releasePreview();
        pendingSelection=-1; inspectRequested=false; returnRequested=false;
        standaloneDocument=false; archiveMode=ArchiveMode::None; archiveDirty=false;
        imageArchive.reset(); streamArchive.reset(); standaloneDtz.reset(); archiveTitle.clear();
        browseStream = true;
        partialBytes=false; worldView=true;
        selected = -1; offset = 0; archiveByteOffset = 0; archiveByteImg = false; ++generation; bytes.clear(); changed = false;
        filter.Clear(); message.clear();
        std::snprintf(archiveJump,sizeof(archiveJump),"0");
        textureArchive = LeedsTextureArchive{}; model = StorylandModelFile{}; image = {};
        textureReady = modelReady = false;
    }
    void openArchive(const std::filesystem::path& primary, const std::filesystem::path& companion) {
        if (busy()) { message = "A file is already opening or saving."; return; }
        reset();
        archiveTitle = primary.filename().string();
        message = "Opening files...";
        loadingArchive = std::async(std::launch::async, [primary, companion]() {
            OpenedArchive opened;
            try {
                const std::string extension = lower(primary.extension().string());
                const std::string companionExtension = lower(companion.extension().string());
                const bool imageCompanion = companionExtension == ".img";
                const bool dtzCompanion = companionExtension == ".dtz" || companionExtension == ".bin";
                const bool dirCompanion = companionExtension == ".dir";

                if (extension == ".dtz" || extension == ".bin") {
                    opened.dtz = std::make_unique<StorylandDtzArchive>();
                    if (!opened.dtz->loadFromFile(primary.wstring(), opened.error, false)) {
                        opened.dtz.reset();
                    } else if (imageCompanion) {
                        opened.stream = std::make_unique<StorylandImgStream>();
                        const auto named = rangesFromDtz(*opened.dtz);
                        if (!opened.stream->open(companion, named, {}, opened.error)) opened.stream.reset();
                    }
                } else if (extension == ".img") {
                    const auto stem = lower(primary.stem().string());
                    const bool storiesImg = stem == "gta3ps2" || stem == "gta3psp" ||
                        stem == "gta3psphr" || stem.find("gta3ps") == 0;
                    std::error_code ec;
                    const auto bytes = std::filesystem::file_size(primary, ec);
                    const bool largeImg = !ec && bytes > 192ull * 1024ull * 1024ull;
                    if ((storiesImg || dtzCompanion || largeImg) && companionExtension != ".lvz") {
                        auto dtzPath = dtzCompanion ? companion : findGameDtzNearImg(primary);
                        if (!dtzPath.empty()) {
                            opened.dtz = std::make_unique<StorylandDtzArchive>();
                            std::string dtzError;
                            if (!opened.dtz->loadFromFile(dtzPath.wstring(), dtzError, false))
                                opened.dtz.reset();
                        }
                        opened.stream = std::make_unique<StorylandImgStream>();
                        const auto named = opened.dtz ? rangesFromDtz(*opened.dtz) : std::vector<StorylandImgNamedRange>{};
                        const std::filesystem::path directory = dirCompanion ? companion : std::filesystem::path{};
                        if (!opened.stream->open(primary, named, directory, opened.error)) opened.stream.reset();
                    } else {
                        opened.image = std::make_unique<StorylandArchiveBrowser>();
                        if (!opened.image->loadImgFromFile(primary.wstring(), opened.error)) opened.image.reset();
                    }
                } else if (extension == ".dir" && imageCompanion) {
                    opened.stream = std::make_unique<StorylandImgStream>();
                    if (!opened.stream->open(companion, {}, primary, opened.error)) opened.stream.reset();
                } else if (extension == ".lvz" || extension == ".dir" || extension == ".zmg") {
                    opened.image = std::make_unique<StorylandArchiveBrowser>();
                    const bool success = extension == ".lvz" ?
                        opened.image->loadLvzWithCompanionImg(primary.wstring(), opened.error) :
                        extension == ".dir" ?
                            opened.image->loadDirWithCompanionImg(primary.wstring(), opened.error) :
                            opened.image->loadZmgFromFile(primary.wstring(), opened.error);
                    if (!success) opened.image.reset();
                } else {
                    opened.error = "Unsupported file type.";
                }
            } catch (const std::bad_alloc&) {
                opened.error = "Not enough memory to open this file.";
            } catch (const std::exception& failure) { opened.error = failure.what(); }
            return opened;
        });
    }
    void poll(const std::function<void(const std::filesystem::path&)>& exportFolder) {
        if (loadingArchive.valid() && loadingArchive.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto opened = loadingArchive.get();
            imageArchive = std::move(opened.image);
            streamArchive = std::move(opened.stream);
            standaloneDtz = std::move(opened.dtz);
            browseStream = bool(streamArchive);
            archiveMode = imageArchive ? ArchiveMode::Image : streamArchive ? ArchiveMode::Stream :
                standaloneDtz ? ArchiveMode::Dtz : ArchiveMode::None;
            if (archiveMode == ArchiveMode::None) {
                message = opened.error.empty() ? "Can't open this file." : opened.error;
            }
            else if (imageArchive) message = std::to_string(imageArchive->entries().size())+" files, "+
                std::to_string(imageArchive->worldMeshes().size())+" meshes, "+
                std::to_string(imageArchive->directTextures().size())+" textures.";
            else if (streamArchive) message = streamArchive->description();
            else message = std::to_string(standaloneDtz->dirEntries().size())+" files.";
        }
        if (!saving.valid() || saving.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        auto result = saving.get();
        message = result.error;
        if (!result.directory.empty()) {
            if (result.archiveCopy) { archiveDirty = false; changed = false; }
            message = "Saved copy. Choose where to export it.";
            exportFolder(result.directory);
        }
    }
    void importFile(const std::filesystem::path& path, int token, const std::string& name) {
        try {
            if (busy() || (token!=-1&&(token != generation || (selected < 0&&!standaloneDocument)))) throw std::runtime_error("Selection changed. Choose the file again.");
            std::ifstream file(path,std::ios::binary|std::ios::ate);
            const auto size = file ? file.tellg() : std::streampos(-1);
            if (size <= 0 || size > std::streampos(128LL*1024*1024)) throw std::runtime_error("File is too large. Open its LVZ, IMG or DTZ instead.");
            std::vector<uint8_t> replacement(static_cast<size_t>(size));
            file.seekg(0);
            if (!file.read(reinterpret_cast<char*>(replacement.data()),std::streamsize(replacement.size())))
                throw std::runtime_error("Couldn't read the whole file.");
            if(token==-1) {
                reset();standaloneDocument=true;selected=-2;
                resourceName=name.empty()?path.filename().string():name;
                originalSize=replacement.size();
            }
            bytes = std::move(replacement); changed = token!=-1; partialBytes = false; offset = 0;
            parsePreview(); message = token == -1 ? "File opened." : "File replaced. Save or export your changes.";
        } catch (const std::exception& error) { message = error.what(); }
        std::error_code ignored; std::filesystem::remove(path,ignored);
    }
    void draw(const StorylandDtzArchive& gameArchive, bool playing, const std::filesystem::path& documents,
        const StorylandAndroidLayout& layout, const std::function<void(int)>& chooseReplacement) {
        const float unit = layout.unit;
        if (loadingArchive.valid()) {
            ImGui::TextUnformatted("Opening files...");
            return;
        }
        const StorylandDtzArchive& archive = standaloneDtz ? *standaloneDtz : gameArchive;
        if (streamArchive && standaloneDtz && !standaloneDocument) {
            if (ImGui::SmallButton(browseStream ? "IMG files *" : "IMG files")) setBrowseStream(true);
            ImGui::SameLine();
            if (ImGui::SmallButton(browseStream ? "GAME.DTZ" : "GAME.DTZ *")) setBrowseStream(false);
            ImGui::Separator();
        }
        if (standaloneDocument) {
            if (!gameArchive.dirEntries().empty()) {
                if (returnRequested) { ImGui::OpenPopup("Return to files"); returnRequested=false; }
                bool returned=false;
                if (ImGui::BeginPopupModal("Return to files",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped(changed?"Discard the current resource edits?":"Return to the imported game archive?");
                    if (storylandTouchButton("Cancel",unit)) ImGui::CloseCurrentPopup();
                    if (storylandTouchButton("Back to game files",unit)) {reset();returned=true;ImGui::CloseCurrentPopup();}
                    ImGui::EndPopup();
                }
                if (returned) return;
            }
            drawInspector(archive,playing,documents,unit,chooseReplacement,!layout.splitResources);
            return;
        }
        if (layout.splitResources) {
            ImGui::BeginChild("Resource browser",{layout.resourceWidth,0},ImGuiChildFlags_Borders);
            drawList(archive,unit);
            ImGui::EndChild(); ImGui::SameLine();
            ImGui::BeginChild("Resource inspector",{0,0},ImGuiChildFlags_Borders);
            drawInspector(archive,playing,documents,unit,chooseReplacement);
            ImGui::EndChild();
        } else {
            drawInspector(archive,playing,documents,unit,chooseReplacement,true);
        }
    }
private:
    enum class ArchiveMode { None, Dtz, Image, Stream };
    struct OpenedArchive {
        std::unique_ptr<StorylandArchiveBrowser> image;
        std::unique_ptr<StorylandImgStream> stream;
        std::unique_ptr<StorylandDtzArchive> dtz;
        std::string error;
    };
    static std::string lower(std::string text) {
        std::transform(text.begin(),text.end(),text.begin(),[](unsigned char value){return char(std::tolower(value));});
        return text;
    }
    static std::vector<StorylandImgNamedRange> rangesFromDtz(const StorylandDtzArchive& dtz) {
        std::vector<StorylandImgNamedRange> result;
        result.reserve(dtz.dirEntries().size());
        for (const auto& entry : dtz.dirEntries()) {
            if (entry.sectorCount == 0) continue;
            result.push_back({entry.name, entry.startSector, entry.sectorCount});
        }
        return result;
    }
    static std::filesystem::path findGameDtzNearImg(const std::filesystem::path& img) {
        const std::filesystem::path folder = img.parent_path();
        const auto stem = lower(img.stem().string());
        std::vector<std::filesystem::path> folders{folder};
        if (stem == "gta3ps2") {
            if (lower(folder.filename().string()) == "models") folders.push_back(folder.parent_path());
            else folders.push_back(folder / "MODELS");
        }
        for (const auto& location : folders) {
            std::error_code ec;
            for (const auto& file : std::filesystem::directory_iterator(location, ec)) {
                if (ec) break;
                if (!file.is_regular_file(ec)) continue;
                const auto name = lower(file.path().filename().string());
                if (name == "game.dtz" || name == "game.bin") return file.path();
            }
        }
        return {};
    }
    bool browsingStream() const { return streamArchive && browseStream; }
    void setBrowseStream(bool requested) {
        if (browseStream == requested) return;
        browseStream = requested;
        selected = -1; bytes.clear(); resourceName.clear(); partialBytes = false;
        changed = false; filter.Clear(); offset = 0; archiveByteOffset = 0;
        releasePreview();
    }
    size_t listedCount(const StorylandDtzArchive& archive) const {
        return browsingStream() ? streamArchive->entries().size() :
            imageArchive ? imageArchive->entries().size() : archive.dirEntries().size();
    }
    std::string listedName(const StorylandDtzArchive& archive, size_t index) const {
        return browsingStream() ? streamArchive->entries()[index].name :
            imageArchive ? imageArchive->entries()[index].name : archive.dirEntries()[index].name;
    }
    uint64_t listedSize(const StorylandDtzArchive& archive, size_t index) const {
        return browsingStream() ? streamArchive->entries()[index].byteSize :
            imageArchive ? imageArchive->entries()[index].byteSize : archive.dirEntries()[index].byteLength;
    }
    ArchiveMode archiveMode = ArchiveMode::None;
    std::unique_ptr<StorylandArchiveBrowser> imageArchive;
    std::unique_ptr<StorylandImgStream> streamArchive;
    std::unique_ptr<StorylandDtzArchive> standaloneDtz;
    bool browseStream = true;
    std::future<OpenedArchive> loadingArchive;
    StorylandAndroidSceneView scene;
    std::string archiveTitle;
    int activeSceneId = -3;
    bool worldView = true, partialBytes = false, archiveDirty = false, archiveByteImg = false;
    uint64_t archiveByteOffset = 0;
    char archiveJump[24] = "0";
    int category = 0;
    struct Result { std::filesystem::path directory; std::string error; bool archiveCopy = false; };
    int pendingSelection = -1;
    bool inspectRequested = false, standaloneDocument = false, returnRequested = false;
    int selected = -1, offset = 0, generation = 1, textureIndex = 0, vertexIndex = 0;
    bool changed = false, textureReady = false, modelReady = false, previewStale = false;
    bool requestedHexScroll = false;
    std::vector<uint8_t> bytes;
    std::string resourceName, message, previewMessage;
    ImGuiTextFilter filter;
    std::future<Result> saving;
    LeedsTextureArchive textureArchive;
    StorylandModelFile model;
    RgbaImage image;
    GLuint previewTexture = 0;
    GLuint galleryTexture = 0;
    int galleryTextureIndex = -1, galleryIndex=0;
    char rename[64]{};
    float yaw = .6f, pitch = .3f, zoom = 1;
    size_t originalSize = 0;

    void parsePreview() {
        releasePreview(); previewStale = false; textureReady = modelReady = false; image = {};
        textureArchive = LeedsTextureArchive{}; model = StorylandModelFile{};
        std::string ignored; previewMessage.clear(); textureIndex = vertexIndex = 0;
        const auto path = std::filesystem::u8path(resourceName).wstring();
        auto extension = std::filesystem::u8path(resourceName).extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
        if (extension == ".xtx" || extension == ".chk" || extension == ".tex" ||
            extension == ".txd" || extension == ".gim" || extension == ".dds")
            textureReady = textureArchive.loadFromMemory(bytes,LeedsPlatform::Auto,ignored,path) && !textureArchive.textures().empty();
        if (!textureReady && (extension == ".mdl" || extension == ".dff" || extension == ".chk"))
            modelReady = model.loadFromMemory(bytes,path,ignored) && !model.previewPoints().empty();
        if (textureReady) decodeTexture();
    }
    void decodeTexture() {
        releasePreview(); image = {};
        if (textureArchive.decodeTexture(size_t(textureIndex),image,previewMessage)) {
            std::snprintf(rename,sizeof(rename),"%s",textureArchive.textures()[size_t(textureIndex)].name.c_str());
        }
    }
    void select(const StorylandDtzArchive& archive, size_t index) {
        selected = int(index); inspectRequested = true; ++generation; offset = 0; changed = false; bytes.clear(); message.clear();
        releasePreview(); partialBytes=false; previewStale = false; textureReady = modelReady = false; image = {};
        if (browsingStream()) {
            const auto& entry = streamArchive->entries()[index];
            resourceName = entry.name;
            originalSize = size_t(std::min<uint64_t>(entry.byteSize, SIZE_MAX));
            if (streamArchive->readEntry(index, 16u * 1024u * 1024u, bytes, partialBytes, message)) {
                parsePreview();
                if (partialBytes) message = "Large file: showing the first 16 MiB. Full file available for export.";
            }
        } else if (imageArchive) {
            const auto& entry=imageArchive->entries()[index];
            resourceName=entry.name; originalSize=size_t(entry.byteSize);
            if (originalSize <= 128*1024*1024) {
                if (imageArchive->extractEntryBytes(index,bytes,message)) parsePreview();
            } else {
                const auto& source=imageArchive->imgBytes();
                if (entry.byteOffset < source.size()) {
                    const size_t length=std::min<size_t>(4096,source.size()-size_t(entry.byteOffset));
                    bytes.assign(source.begin()+size_t(entry.byteOffset),source.begin()+size_t(entry.byteOffset)+length);
                    partialBytes=true;
                    message="File is too large to edit. Showing the first 4 KiB.";
                }
            }
        } else {
            resourceName=archive.dirEntries()[index].name;
            originalSize=size_t(archive.dirEntries()[index].byteLength);
            if (originalSize<=128*1024*1024) {
                if (archive.extractDirEntryBytes(index,bytes,message)) parsePreview();
            } else message="File is too large to edit (128 MiB limit).";
        }
        activeSceneId=-3;
    }
    void drawList(const StorylandDtzArchive& archive, float unit, bool heading=true) {
        const size_t total=listedCount(archive);
        if (heading) {
            ImGui::TextUnformatted(browsingStream() ? "gta3PS IMG" : imageArchive ? "LVZ / IMG" : standaloneDtz ? "DTZ" : "FILES");
            if (!archiveTitle.empty()) ImGui::TextDisabled("%s",archiveTitle.c_str());
        }
        ImGui::SetNextItemWidth(-1);
        filter.Draw("Find file",-1);
        const char* categories[]={"All files","Models","Textures","World / sectors","Other"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##Resource type",&category,categories,5);
        std::vector<size_t> matches;
        matches.reserve(total);
        for (size_t index=0;index<total;++index) {
            const auto name=listedName(archive,index);
            if (!filter.PassFilter(name.c_str())) continue;
            const std::string extension=lower(std::filesystem::u8path(name).extension().string());
            const bool model=extension==".mdl"||extension==".dff"||extension==".chk";
            const bool texture=extension==".xtx"||extension==".chk"||extension==".tex"||extension==".txd"||extension==".gim";
            const bool world=extension==".lvz"||extension==".img"||extension==".ipl"||extension==".col"||extension==".col2"||extension==".wbn"||extension==".area"||extension==".wrld"||
                (imageArchive && imageArchive->entries()[index].usesLvzChunkHeader);
            if ((category==1 && !model) || (category==2 && !texture) ||
                (category==3 && !world) || (category==4 && (model||texture||world))) continue;
            matches.push_back(index);
        }
        ImGui::TextDisabled("%zu of %zu files",matches.size(),total);
        const float height=std::max(40.f,ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("Archive files",{0,height},ImGuiChildFlags_None);
        ImGuiListClipper clip;
        clip.Begin(int(matches.size()),45*unit+ImGui::GetStyle().ItemSpacing.y);
        while (clip.Step()) for (int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
            const size_t index=matches[size_t(row)];
            const std::string name=listedName(archive,index);
            const uint64_t storedSize=listedSize(archive,index);
            ImGui::PushID(int(index));
            const bool wasSelected=selected==int(index);
            if (ImGui::Selectable(name.c_str(),wasSelected,ImGuiSelectableFlags_AllowDoubleClick,{0,45*unit})) {
                if (changed && !wasSelected) pendingSelection=int(index);
                else if (!wasSelected) select(archive,index);
                else inspectRequested=true;
            }
            const auto location=ImGui::GetItemRectMin();
            const std::string information=storedSize >= 1048576 ?
                std::to_string(storedSize/1048576) + " MiB" :
                storedSize>=1024 ? std::to_string(storedSize/1024) + " KiB" :
                std::to_string(storedSize)+" bytes";
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),12*unit,
                {location.x+9*unit,location.y+26*unit},IM_COL32(173,160,185,255),information.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%zu bytes",name.c_str(),storedSize);
            ImGui::PopID();
        }
        ImGui::EndChild();
        if(pendingSelection>=0) ImGui::OpenPopup("Unsaved file");
        ImGui::SetNextWindowSize({std::min(360*unit,ImGui::GetIO().DisplaySize.x-24*unit),0},ImGuiCond_Appearing);
        if(ImGui::BeginPopupModal("Unsaved file",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("This file has unsaved changes.");
            if(storylandTouchButton("Keep editing",unit)){pendingSelection=-1;ImGui::CloseCurrentPopup();}
            if(storylandTouchButton("Discard and open",unit)) {const int next=pendingSelection;pendingSelection=-1;select(archive,size_t(next));ImGui::CloseCurrentPopup();}
            ImGui::EndPopup();
        }
    }
    void drawInspector(const StorylandDtzArchive& archive, bool playing, const std::filesystem::path& documents,
        float unit, const std::function<void(int)>& chooseReplacement, bool compact=false) {
        const bool validSelection=standaloneDocument||
            (selected>=0 && size_t(selected)<listedCount(archive));
        if(compact&&!validSelection&&!imageArchive&&!browsingStream()){drawList(archive,unit,false);return;}
        if(!compact&&!validSelection&&!imageArchive&&!browsingStream()) {
            ImGui::TextUnformatted("FILES");
            ImGui::TextUnformatted("Select a file to view it."); return;
        }
        if(!compact)ImGui::Text("%s%s",resourceName.c_str(),changed?" *":"");
        if (ImGui::BeginTabBar("Inspector views",ImGuiTabBarFlags_FittingPolicyScroll)) {
            if(compact&&!standaloneDocument&&ImGui::BeginTabItem("Files")){drawList(archive,unit,false);ImGui::EndTabItem();}
            if(imageArchive&&ImGui::BeginTabItem("World")) {
                ImGui::BeginChild("World preview",{0,0});
                if (ImGui::SmallButton("Whole area")) {worldView=true;activeSceneId=-3;}
                if (validSelection) {ImGui::SameLine();if (ImGui::SmallButton("Selected file")) {worldView=false;activeSceneId=-3;}}
                const int sceneId=worldView||!validSelection?-1:int(imageArchive->entries()[size_t(selected)].index);
                if (activeSceneId!=sceneId) {scene.buildArchive(*imageArchive,sceneId);activeSceneId=sceneId;}
                ImGui::TextDisabled("%zu placements, %zu meshes, %zu textures",imageArchive->placements().size(),
                    imageArchive->worldMeshes().size(),imageArchive->directTextures().size());
                scene.draw(unit);
                ImGui::EndChild();ImGui::EndTabItem();
            }
            if (imageArchive && !imageArchive->directTextures().empty() && ImGui::BeginTabItem("Textures")) {
                ImGui::BeginChild("Textures gallery",{0,0});
                drawGallery(unit);
                ImGui::EndChild();ImGui::EndTabItem();
            }
            if(!standaloneDocument && ImGui::BeginTabItem("Archive bytes")) {
                ImGui::BeginChild("Archive hex data",{0,0});
                drawArchiveBytes(archive,unit);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if(!validSelection){
                if(imageArchive && imageArchive->hasLvzContext()) {
                    ImGui::BeginDisabled(playing || busy());
                    if (storylandTouchButton("Export LVZ + IMG pair",unit)) saveImageArchive(documents);
                    ImGui::EndDisabled();
                }
                ImGui::EndTabBar();return;
            }
            if (ImGui::BeginTabItem("Preview",nullptr,compact&&inspectRequested?ImGuiTabItemFlags_SetSelected:0)) {inspectRequested=false;
                ImGui::BeginChild("Preview body",{0,0});
                drawPreview(unit,compact); ImGui::EndChild(); ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Edit")) {
                ImGui::BeginChild("Edit body",{0,0});
                if(busy())ImGui::TextUnformatted("Saving copy...");
                if(imageArchive && imageArchive->hasLvzContext()) {
                    ImGui::BeginDisabled(playing || busy());
                    if (storylandTouchButton("Save LVZ + IMG copy",unit)) saveImageArchive(documents);
                    ImGui::EndDisabled();
                    if(archiveDirty) ImGui::TextDisabled("Changes haven't been saved.");
                }
                if (playing)ImGui::TextUnformatted("Stop Play to edit files.");
                if (browsingStream()) {
                    if (storylandTouchButton("Export file", unit) && !busy()) saveStreamEntry(documents);
                    ImGui::TextDisabled("IMG contents are read-only. Export a file to edit it on PC.");
                }
                ImGui::BeginDisabled(playing||busy()||bytes.empty()||partialBytes||browsingStream());
                if (storylandTouchButton("Replace from file",unit)) chooseReplacement(generation);
                if(!standaloneDocument&&!imageArchive&&storylandTouchButton("Rebuild DTZ + IMG copy",unit)) save(archive,documents,true);
                if (storylandTouchButton("Export resource",unit)) save(archive,documents,false);
                if (imageArchive && imageArchive->hasLvzContext() && storylandTouchButton("Apply to working LVZ + IMG",unit))
                    applyImageEdits();
                if (!standaloneDocument&&storylandTouchButton("Revert edits",unit)) select(archive,size_t(selected));
                if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
                ImGui::Separator();
                ImGui::TextWrapped(standaloneDocument ? "Export saves a copy." : imageArchive ?
                    "Apply your changes, then save an LVZ / IMG copy." :
                    "Rebuild saves a new DTZ / IMG copy.");
                if (!bytes.empty()) {
                    ImGui::SetNextItemWidth(std::min(200*unit,ImGui::GetContentRegionAvail().x));
                    ImGui::InputInt("Offset",&offset,1,16,ImGuiInputTextFlags_CharsHexadecimal);
                    offset = std::clamp(offset,0,int(bytes.size()-1));
                    int value = bytes[size_t(offset)];
                    ImGui::SetNextItemWidth(std::min(200*unit,ImGui::GetContentRegionAvail().x));
                    if (ImGui::InputInt("Byte",&value,1,16,ImGuiInputTextFlags_CharsHexadecimal)) {
                        bytes[size_t(offset)] = uint8_t(std::clamp(value,0,255)); changed = true;previewStale=true;
                    }
                }
                if(previewStale)parsePreview();
                if (textureReady && ImGui::CollapsingHeader("Texture name")) {
                    ImGui::SetNextItemWidth(-1); ImGui::InputText("##Texture name",rename,sizeof(rename));
                    if (storylandTouchButton("Apply texture name",unit)) {
                        if (textureArchive.renameTexture(size_t(textureIndex),rename,message)) {bytes=textureArchive.rawBytes();changed=true;}
                    }
                }
                if (modelReady && !model.previewPrelights().empty() && ImGui::CollapsingHeader("Vertex colour")) {
                    ImGui::SetNextItemWidth(std::min(200*unit,ImGui::GetContentRegionAvail().x));
                    ImGui::InputInt("Vertex",&vertexIndex); vertexIndex=std::clamp(vertexIndex,0,int(model.previewPrelights().size()-1));
                    const auto& colour=model.previewPrelights()[size_t(vertexIndex)];
                    float rgba[]={colour.red/255.f,colour.green/255.f,colour.blue/255.f,colour.alpha/255.f};
                    if (ImGui::ColorEdit4("Colour",rgba)) {
                        if (model.setPreviewPrelightColor(size_t(vertexIndex),uint8_t(rgba[0]*255),uint8_t(rgba[1]*255),uint8_t(rgba[2]*255),uint8_t(rgba[3]*255),message)) {
                            bytes=model.rawBytes();changed=true;
                        }
                    }
                }
                ImGui::EndDisabled(); ImGui::EndChild(); ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Bytes")) { drawHex(unit); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Details")) {
                ImGui::BeginChild("Properties",{0,0});
                ImGui::TextWrapped("%s%s",resourceName.c_str(),changed?" *":"");
                ImGui::Text("Working size: %zu bytes",bytes.size());
                if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
                if(standaloneDocument&&!archive.dirEntries().empty()&&storylandTouchButton("Game resources",unit))returnRequested=true;
                if (partialBytes) ImGui::TextDisabled("Showing header sample only; editing disabled.");
                if (browsingStream()) {
                    const auto& entry = streamArchive->entries()[size_t(selected)];
                    ImGui::Text("Sector: %u", entry.startSector);
                    ImGui::Text("Sectors: %u", entry.sectorCount);
                    ImGui::Text("IMG offset: 0x%llX", static_cast<unsigned long long>(entry.byteOffset));
                    ImGui::Text("Stored: %llu bytes", static_cast<unsigned long long>(entry.byteSize));
                    ImGui::TextUnformatted(entry.hasOriginalName ? "Name from GAME.DTZ / DIR" : "Name based on sector signature");
                } else if (imageArchive) {
                    const auto& entry=imageArchive->entries()[size_t(selected)];
                    ImGui::Text("Sector: %u",entry.startSector);
                    ImGui::Text("Sectors: %u",entry.sectorCount);
                    ImGui::Text("IMG offset: 0x%llX",static_cast<unsigned long long>(entry.byteOffset));
                    ImGui::Text("Stored: %llu bytes",static_cast<unsigned long long>(entry.byteSize));
                    ImGui::Text("Resource ID: %u",entry.index);
                } else if(!standaloneDocument) {const auto& entry=archive.dirEntries()[size_t(selected)];
                    ImGui::Text("Sector: %u",entry.startSector); ImGui::Text("Sectors: %u",entry.sectorCount);
                    ImGui::Text("Offset: 0x%llX",static_cast<unsigned long long>(entry.byteOffset));
                    ImGui::Text("Stored: %llu bytes",static_cast<unsigned long long>(entry.byteLength));
                    ImGui::TextWrapped("Type: %s",entry.detectedExtension.c_str());}
                if (modelReady) { ImGui::Text("Vertices: %zu",model.previewPoints().size()); ImGui::Text("Triangles: %zu",model.previewTriangles().size()); }
                if (textureReady) ImGui::Text("Textures: %zu",textureArchive.textures().size());
                ImGui::EndChild(); ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    void drawGallery(float unit) {
        if (!imageArchive || imageArchive->directTextures().empty()) return;
        const auto& textures=imageArchive->directTextures();
        const int count=int(textures.size());
        if (ImGui::SmallButton("Previous")) galleryIndex=(galleryIndex+count-1)%count;
        ImGui::SameLine();
        if (ImGui::SmallButton("Next")) galleryIndex=(galleryIndex+1)%count;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::min(135*unit,ImGui::GetContentRegionAvail().x));
        ImGui::InputInt("Index",&galleryIndex);
        galleryIndex=std::clamp(galleryIndex,0,count-1);
        const auto& texture=textures[size_t(galleryIndex)];
        ImGui::TextWrapped("%s",texture.name.c_str());
        ImGui::TextDisabled("%d x %d - %d bpp - material %d - %d / %d",texture.width,texture.height,texture.bpp,
            texture.materialId,galleryIndex+1,count);
        if (galleryTextureIndex!=galleryIndex) {
            if (galleryTexture) glDeleteTextures(1,&galleryTexture);
            galleryTexture=0;
            galleryTextureIndex=galleryIndex;
            if (!texture.rgba.empty() && texture.width>0 && texture.height>0) {
                GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
                if (texture.width<=maximum && texture.height<=maximum) {
                    glGenTextures(1,&galleryTexture);
                    glBindTexture(GL_TEXTURE_2D,galleryTexture);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture.width,texture.height,0,GL_RGBA,GL_UNSIGNED_BYTE,texture.rgba.data());
                    glBindTexture(GL_TEXTURE_2D,0);
                }
            }
        }
        if (!galleryTexture) {ImGui::TextWrapped("Can't preview this texture.");return;}
        const ImVec2 available=ImGui::GetContentRegionAvail();
        const float scale=std::min(available.x/float(texture.width),std::max(1.f,available.y)/float(texture.height));
        const ImVec2 size{std::max(1.f,texture.width*scale),std::max(1.f,texture.height*scale)};
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(available.x-size.x)*.5f));
        ImGui::Image((ImTextureID)(intptr_t)galleryTexture,size);
    }
    void drawArchiveBytes(const StorylandDtzArchive& archive, float unit) {
        if (browsingStream()) {drawStreamBytes(unit);return;}
        const bool hasLvz = imageArchive && !imageArchive->lvzBytes().empty();
        if (imageArchive && hasLvz) {
            if (ImGui::SmallButton("LVZ header")) {archiveByteImg=false;archiveByteOffset=0;}
            ImGui::SameLine();
            if (ImGui::SmallButton("IMG data")) {archiveByteImg=true;archiveByteOffset=0;}
        }
        const bool useImg = imageArchive && (!hasLvz || archiveByteImg);
        const std::vector<uint8_t>& source = imageArchive ?
            (useImg ? imageArchive->imgBytes() : imageArchive->lvzBytes()) : archive.unpackedBytes();
        if(source.empty()) {
            ImGui::TextWrapped("No bytes available.");
            return;
        }
        ImGui::TextDisabled("%s: %zu bytes",imageArchive ? (useImg ? "IMG" : "LVZ") : "DTZ (decoded)",source.size());
        if (ImGui::SmallButton("- 4 KiB")) {
            archiveByteOffset = archiveByteOffset>=4096 ? archiveByteOffset-4096 : 0;
            std::snprintf(archiveJump,sizeof(archiveJump),"%llX",static_cast<unsigned long long>(archiveByteOffset));
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("+ 4 KiB")) {
            archiveByteOffset = std::min<uint64_t>(source.size()-1,archiveByteOffset+4096);
            std::snprintf(archiveJump,sizeof(archiveJump),"%llX",static_cast<unsigned long long>(archiveByteOffset));
        }
        ImGui::SetNextItemWidth(std::min(155*unit,ImGui::GetContentRegionAvail().x*.52f));
        ImGui::InputText("Hex offset",archiveJump,sizeof(archiveJump),ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::SmallButton("Go to")) archiveByteOffset = std::min<uint64_t>(
            std::strtoull(archiveJump,nullptr,16),source.size()-1);
        const uint64_t page = (archiveByteOffset/4096u)*4096u;
        const uint64_t last = std::min<uint64_t>(page+4096u,source.size());
        if (archiveByteOffset+4<=source.size()) {
            const size_t location=size_t(archiveByteOffset);
            const uint32_t value = uint32_t(source[location]) |
                (uint32_t(source[location+1])<<8u) |
                (uint32_t(source[location+2])<<16u) |
                (uint32_t(source[location+3])<<24u);
            const uint32_t swapped = ((value&0xffu)<<24u)|((value&0xff00u)<<8u)|
                ((value&0xff0000u)>>8u)|((value&0xff000000u)>>24u);
            float decimal = 0; std::memcpy(&decimal,&value,sizeof(decimal));
            ImGui::TextDisabled("0x%llX: LE %08X | BE %08X | f32 %.6g",
                static_cast<unsigned long long>(archiveByteOffset),value,swapped,decimal);
        }
        ImGui::BeginChild("Archive hex page",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
        ImGuiListClipper clip;
        clip.Begin(int((last-page+15)/16),ImGui::GetTextLineHeightWithSpacing());
        while(clip.Step()) for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
            const uint64_t position=page+uint64_t(row)*16u;
            std::ostringstream out;
            out<<std::uppercase<<std::hex<<std::setw(8)<<std::setfill('0')<<position<<"  ";
            for(int col=0;col<16;++col) {
                const uint64_t index=position+uint64_t(col);
                if(index<last) out<<std::setw(2)<<unsigned(source[size_t(index)])<<' ';
                else out<<"   ";
            }
            out<<" |";
            for(int col=0;col<16;++col) {
                const uint64_t index=position+uint64_t(col);
                if(index<last) {
                    const unsigned char ch=source[size_t(index)];
                    out<<char(ch>=32&&ch<127?ch:'.');
                } else out<<' ';
            }
            out<<'|';
            const std::string line = out.str();
            if (ImGui::Selectable(line.c_str(),position<=archiveByteOffset &&
                archiveByteOffset<position+16,0,{0,ImGui::GetTextLineHeight()})) {
                archiveByteOffset=position;
                std::snprintf(archiveJump,sizeof(archiveJump),"%llX",static_cast<unsigned long long>(position));
            }
        }
        ImGui::EndChild();
    }
    void drawHex(float unit) {
        if (bytes.empty()) {ImGui::TextWrapped("No bytes available for this file.");return;}
        static char jump[24]="0";
        ImGui::SetNextItemWidth(std::min(150*unit,ImGui::GetContentRegionAvail().x*.42f));
        ImGui::InputText("Hex offset",jump,sizeof(jump),ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::SmallButton("Go")) {
            const unsigned long long value=std::strtoull(jump,nullptr,16);
            offset=int(std::min<unsigned long long>(value,bytes.size()-1));
            requestedHexScroll=true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%zu bytes%s",bytes.size(),partialBytes?" (sample)":"");
        if (size_t(offset)<bytes.size()) {
            const size_t pos=size_t(offset);
            const uint32_t integer=pos+4<=bytes.size()? uint32_t(bytes[pos])|(uint32_t(bytes[pos+1])<<8)|
                (uint32_t(bytes[pos+2])<<16)|(uint32_t(bytes[pos+3])<<24):0;
            float decimal=0;std::memcpy(&decimal,&integer,sizeof(decimal));
            if (pos+4<=bytes.size()) ImGui::TextDisabled("Selected 0x%zX   U32 LE: %u / 0x%08X   Float: %.6g",pos,integer,integer,decimal);
            else ImGui::TextDisabled("Selected 0x%zX   U8: %u",pos,unsigned(bytes[pos]));
        }
        const int columns=ImGui::GetContentRegionAvail().x>=720*unit?16:8;
        const size_t rowCount=(bytes.size()+size_t(columns)-1)/size_t(columns);
        ImGui::BeginChild("Hexadecimal",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
        ImGuiListClipper clip;clip.Begin(int(rowCount),ImGui::GetTextLineHeightWithSpacing());
        if (requestedHexScroll) {ImGui::SetScrollY(float((size_t(offset)/size_t(columns)))*ImGui::GetTextLineHeightWithSpacing());requestedHexScroll=false;}
        while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
            const size_t begin=size_t(row)*size_t(columns);
            std::ostringstream out;
            out<<std::uppercase<<std::hex<<std::setw(8)<<std::setfill('0')<<begin<<"  ";
            for(int col=0;col<columns;++col) {
                const size_t index=begin+size_t(col);
                if(index<bytes.size())out<<std::setw(2)<<unsigned(bytes[index])<<' ';
                else out<<"   ";
            }
            out<<" |";
            for(int col=0;col<columns;++col) {
                const size_t index=begin+size_t(col);
                if(index<bytes.size()) {
                    const unsigned char ch=bytes[index];
                    out<<char(ch>=32&&ch<127?ch:'.');
                } else out<<' ';
            }
            out<<'|';
            const std::string label=out.str();
            if(ImGui::Selectable(label.c_str(),size_t(offset)/size_t(columns)==size_t(row),0,{0,ImGui::GetTextLineHeight()})) {
                offset=int(begin);
                std::snprintf(jump,sizeof(jump),"%zX",begin);
            }
        }
        ImGui::EndChild();
    }
    void drawPreview(float unit, bool compact) {
        if(previewStale)parsePreview();
        if (textureReady) {
            ImGui::SetNextItemWidth(-1);
            if ((!compact||textureArchive.textures().size()>1)&&ImGui::BeginCombo("##Texture",textureArchive.textures()[size_t(textureIndex)].name.c_str())) {
                for (size_t index=0;index<textureArchive.textures().size();++index)
                    if (ImGui::Selectable(textureArchive.textures()[index].name.c_str(),textureIndex==int(index),0,{0,44*unit})) {textureIndex=int(index);decodeTexture();}
                ImGui::EndCombo();
            }
            if(!compact)ImGui::Text("%d x %d",image.width,image.height);
            if (!image.rgba.empty()) {
                if (!previewTexture) {
                    glGenTextures(1,&previewTexture); glBindTexture(GL_TEXTURE_2D,previewTexture);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
                }
                const auto available=ImGui::GetContentRegionAvail();
                const float fit=std::min(available.x/image.width,std::max(1.f,available.y)/image.height);
                const auto origin=ImGui::GetCursorScreenPos();
                auto* draw=ImGui::GetWindowDrawList();
                const float tile=12*unit;
                for(float y=0;y<available.y;y+=tile)for(float x=0;x<available.x;x+=tile)
                    draw->AddRectFilled({origin.x+x,origin.y+y},{origin.x+std::min(x+tile,available.x),origin.y+std::min(y+tile,available.y)},
                        (int(x/tile)+int(y/tile))%2?IM_COL32(38,34,43,255):IM_COL32(28,25,32,255));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(available.x-image.width*fit)*.5f);
                ImGui::Image(static_cast<ImTextureID>(previewTexture),{image.width*fit,image.height*fit});
            }
            if (!previewMessage.empty()) ImGui::TextWrapped("%s",previewMessage.c_str());
        } else if (modelReady) {
            if (activeSceneId != -2) {scene.buildModel(model);activeSceneId=-2;}
            if (scene.available()) {scene.draw(unit);return;}
            if(!compact) {ImGui::Text("%zu vertices / %zu triangles",model.previewPoints().size(),model.previewTriangles().size());
            if (storylandTouchButton("Reset view",unit)) {yaw=.6f;pitch=.3f;zoom=1;}
            ImGui::SameLine(); if(storylandTouchButton("-",unit))zoom=std::max(.2f,zoom*.8f);
            ImGui::SameLine(); if(storylandTouchButton("+",unit))zoom=std::min(5.f,zoom*1.25f);
            }
            const ImVec2 size={std::max(1.f,ImGui::GetContentRegionAvail().x),std::max(80*unit,ImGui::GetContentRegionAvail().y)};
            const auto origin=ImGui::GetCursorScreenPos(); ImGui::InvisibleButton("Orbit model",size);
            if (ImGui::IsItemActive()&&ImGui::IsMouseDragging(0)) {yaw+=ImGui::GetIO().MouseDelta.x/unit*.01f;pitch=std::clamp(pitch+ImGui::GetIO().MouseDelta.y/unit*.01f,-1.5f,1.5f);}
            auto* draw=ImGui::GetWindowDrawList(); draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
            const auto& points=model.previewPoints(); auto low=points.front(),high=low;
            for(const auto& point:points){low.x=std::min(low.x,point.x);low.y=std::min(low.y,point.y);low.z=std::min(low.z,point.z);high.x=std::max(high.x,point.x);high.y=std::max(high.y,point.y);high.z=std::max(high.z,point.z);}
            const float fit=.75f*std::min(size.x,size.y)*zoom/std::max(.001f,std::hypot(high.x-low.x,high.y-low.y,high.z-low.z));
            const auto project=[&](const StorylandModelPoint& point){const float x=point.x-(low.x+high.x)*.5f,y=point.y-(low.y+high.y)*.5f,z=point.z-(low.z+high.z)*.5f;const float a=x*std::cos(yaw)-y*std::sin(yaw),b=x*std::sin(yaw)+y*std::cos(yaw);return ImVec2(origin.x+size.x*.5f+a*fit,origin.y+size.y*.5f-(z*std::cos(pitch)+b*std::sin(pitch))*fit);};
            size_t shown=0;
            for(const auto& triangle:model.previewTriangles()) {
                if(++shown>12000)break;
                if(triangle.a>=points.size()||triangle.b>=points.size()||triangle.c>=points.size())continue;
                const auto a=project(points[triangle.a]),b=project(points[triangle.b]),c=project(points[triangle.c]);
                draw->AddTriangle(a,b,c,IM_COL32(242,115,184,220),unit);
            }
            draw->PopClipRect();
            if(model.previewTriangles().size()>12000)draw->AddText({origin.x+8*unit,origin.y+8*unit},IM_COL32_WHITE,"Wireframe preview limited to 12,000 triangles");
        } else {
            if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
            ImGui::TextWrapped("No preview for this file. Use World, Bytes or Details.");
        }
    }
    void saveStreamEntry(const std::filesystem::path& documents) {
        if (!browsingStream() || selected < 0 || busy()) return;
        const auto directory = documents / "exports" /
            ("Storyland-" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        const size_t entryIndex = size_t(selected);
        const auto* reader = streamArchive.get();
        saving = std::async(std::launch::async, [reader, directory, entryIndex]() {
            Result result;
            try {
                if (!std::filesystem::create_directories(directory))
                    throw std::runtime_error("Can't create the export folder.");
                const auto file = directory / std::filesystem::path(reader->entries()[entryIndex].name).filename();
                if (!reader->exportEntry(entryIndex, file, result.error))
                    throw std::runtime_error(result.error);
                result.directory = directory;
                result.error.clear();
            } catch (const std::exception& failure) {
                result.error = failure.what();
                std::error_code ignored;
                std::filesystem::remove_all(directory, ignored);
            }
            return result;
        });
        message = "Exporting file...";
    }
    void drawStreamBytes(float unit) {
        if (!streamArchive) return;
        const uint64_t count = streamArchive->byteSize();
        if (!count) {ImGui::TextUnformatted("No IMG data.");return;}
        ImGui::TextDisabled("IMG: %llu bytes | 2048-byte sectors",static_cast<unsigned long long>(count));
        if (ImGui::SmallButton("- 4 KiB")) archiveByteOffset = archiveByteOffset >= 4096 ? archiveByteOffset-4096 : 0;
        ImGui::SameLine();
        if (ImGui::SmallButton("+ 4 KiB")) archiveByteOffset = std::min<uint64_t>(count-1,archiveByteOffset+4096);
        ImGui::SetNextItemWidth(std::min(160*unit, ImGui::GetContentRegionAvail().x*.45f));
        ImGui::InputText("Hex offset",archiveJump,sizeof(archiveJump),ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::SmallButton("Go to")) archiveByteOffset = std::min<uint64_t>(
            std::strtoull(archiveJump,nullptr,16),count-1);
        const uint64_t page = archiveByteOffset & ~uint64_t(4095);
        std::vector<uint8_t> pageBytes;
        std::string error;
        if (!streamArchive->readAt(page,size_t(std::min<uint64_t>(4096,count-page)),pageBytes,error)) {
            ImGui::TextWrapped("%s",error.c_str());return;
        }
        const size_t selectedByte = size_t(archiveByteOffset-page);
        if (selectedByte+4<=pageBytes.size()) {
            const uint32_t le = uint32_t(pageBytes[selectedByte]) |
                (uint32_t(pageBytes[selectedByte+1])<<8u) |
                (uint32_t(pageBytes[selectedByte+2])<<16u) |
                (uint32_t(pageBytes[selectedByte+3])<<24u);
            float real = 0;std::memcpy(&real,&le,sizeof(real));
            const uint32_t be = ((le&0x000000ffu)<<24u)|((le&0x0000ff00u)<<8u)|
                ((le&0x00ff0000u)>>8u)|((le&0xff000000u)>>24u);
            ImGui::TextDisabled("LE %08X  BE %08X  f32 %.6g",le,be,real);
        }
        ImGui::BeginChild("IMG byte page",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
        ImGuiListClipper clip;
        clip.Begin(int((pageBytes.size()+15)/16),ImGui::GetTextLineHeightWithSpacing());
        while (clip.Step()) for (int line=clip.DisplayStart;line<clip.DisplayEnd;++line) {
            const size_t from=size_t(line)*16;
            std::ostringstream out;
            out<<std::uppercase<<std::hex<<std::setw(12)<<std::setfill('0')<<(page+from)<<"  ";
            for (size_t j=0;j<16;++j) {
                if (from+j<pageBytes.size()) out<<std::setw(2)<<unsigned(pageBytes[from+j])<<' ';
                else out<<"   ";
            }
            out<<" |";
            for (size_t j=0;j<16;++j) {
                const unsigned char c=from+j<pageBytes.size()?pageBytes[from+j]:0;
                out<<char(c>=32&&c<127?c:'.');
            }
            out<<'|';
            const auto text=out.str();
            if (ImGui::Selectable(text.c_str(),archiveByteOffset>=page+from&&archiveByteOffset<page+from+16)) {
                archiveByteOffset=page+from;
                std::snprintf(archiveJump,sizeof(archiveJump),"%llX",static_cast<unsigned long long>(archiveByteOffset));
            }
        }
        ImGui::EndChild();
    }
    bool applyImageEdits() {
        if (!imageArchive || !imageArchive->hasLvzContext() || selected < 0 || bytes.empty() || partialBytes) {
            message = "Open a supported LVZ + IMG entry before applying resource changes.";
            return false;
        }
        std::string report, failure;
        if (!imageArchive->replaceEntryBytes(size_t(selected), bytes, report, failure)) {
            message = failure.empty() ? "Can't replace this file." : failure;
            return false;
        }
        archiveDirty = true;
        changed = false;
        activeSceneId = -3;
        scene.clearGeometry();
        message = report.empty() ? "Changes applied. Save a copy to keep them." : report;
        return true;
    }
    void saveImageArchive(const std::filesystem::path& documents) {
        if (!imageArchive || !imageArchive->hasLvzContext() || busy()) {
            message = "Open the matching LVZ and IMG files first.";
            return;
        }
        if (changed && !applyImageEdits()) return;
        auto* working = imageArchive.get();
        const auto lvzName = std::filesystem::path(working->lvzPath()).filename();
        const auto imgName = std::filesystem::path(working->imgPath()).filename();
        const auto directory = documents / "exports" /
            ("Storyland-" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        saving = std::async(std::launch::async, [working, directory, lvzName, imgName]() {
            Result result;
            try {
                if (!std::filesystem::create_directories(directory))
                    throw std::runtime_error("Could not create the LVZ / IMG export directory.");
                const auto outputLvz = directory / (lvzName.empty() ? std::filesystem::path("MAINLA.LVZ") : lvzName);
                const auto outputImg = directory / (imgName.empty() ? std::filesystem::path("MAINLA.IMG") : imgName);
                if (!working->saveLvzImgPair(outputLvz.wstring(),outputImg.wstring(),
                    working->wasCompressedLvzInput(), result.error))
                    throw std::runtime_error(result.error);
                result.archiveCopy = true;
                result.directory = directory;
                result.error.clear();
            } catch(const std::exception& exception) {
                result.error = exception.what();
                std::error_code ignored;
                std::filesystem::remove_all(directory, ignored);
            }
            return result;
        });
        message = "Building LVZ + IMG copy...";
    }
  void save(const StorylandDtzArchive &archive,
            const std::filesystem::path &documents, bool pair) {
    const auto source = archive.sourcePath(),
               companion = archive.companionImgPath();
    const auto name =
        std::filesystem::u8path(resourceName).filename();
    const size_t index = standaloneDocument ? 0 : size_t(selected);
    const auto directory =
        documents / "exports" /
        ("Storyland-" +
         std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count()));
    saving = std::async(std::launch::async, [source, companion, name, index,
                                             directory, pair,
                                             replacement = bytes, shift = bytes.size() != originalSize]() {
      Result result;
      try {
        if (!std::filesystem::create_directories(directory))
          throw std::runtime_error("Cannot create export folder.");
        if (pair) {
          StorylandDtzArchive rebuilt;
          if (!rebuilt.loadFromFile(source, result.error))
            throw std::runtime_error(result.error);
          if (!rebuilt.hasCompanionImg() &&
              !rebuilt.loadCompanionImg(companion, result.error))
            throw std::runtime_error(result.error);
          std::string report;
          if (!rebuilt.replaceDirEntryBytes(index, replacement, shift, report,
                                            result.error))
            throw std::runtime_error(result.error);
          if (!rebuilt.saveToFile((directory / "GAME.DTZ").wstring(), true,
                                  result.error))
            throw std::runtime_error(result.error);
        } else {
          const auto filename = name.empty() || name == "." || name == ".."
                                    ? std::filesystem::path("resource.bin")
                                    : name;
          std::ofstream output(directory / filename, std::ios::binary);
          output.write(reinterpret_cast<const char *>(replacement.data()),
                       std::streamsize(replacement.size()));
          output.close();
          if (!output)
            throw std::runtime_error("Could not write the complete resource.");
        }
        result.directory = directory;
        result.archiveCopy = pair;
        result.error.clear();
      } catch (const std::exception &error) {
        result.error = error.what();
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
      }
      return result;
    });
  }
};
