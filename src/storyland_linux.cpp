#include "storyland_archive.h"
#include "storyland_dtz.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

struct Area {
    std::string name;
    std::filesystem::path path;
    std::unique_ptr<StorylandArchiveBrowser> archive;
    std::string error;
};

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return value;
}

std::vector<std::filesystem::path> searchFolders(const std::filesystem::path& input) {
    std::filesystem::path root = input.parent_path();
    if (root.empty()) root = ".";
    std::vector<std::filesystem::path> folders{root};
    const std::wstring folderName = lower(root.filename().wstring());
    if ((folderName == L"ps2" || folderName == L"models" || folderName == L"data") &&
        root.has_parent_path()) folders.push_back(root.parent_path());

    size_t start = 0;
    for (int depth = 0; depth < 3 && start < folders.size(); ++depth) {
        const size_t end = folders.size();
        for (size_t i = start; i < end && folders.size() < 256; ++i) {
            std::error_code ec;
            for (std::filesystem::directory_iterator it(folders[i], ec), last;
                 !ec && it != last && folders.size() < 256; it.increment(ec)) {
                if (it->is_directory(ec)) folders.push_back(it->path());
            }
        }
        start = end;
    }
    return folders;
}

void findAreas(const std::filesystem::path& input, std::array<Area, 3>& areas) {
    const auto folders = searchFolders(input);
    for (Area& area : areas) {
        for (const auto& folder : folders) {
            std::error_code ec;
            for (std::filesystem::directory_iterator it(folder, ec), last;
                 !ec && it != last; it.increment(ec)) {
                const auto candidate = it->path();
                if (!it->is_regular_file(ec) || lower(candidate.extension().wstring()) != L".lvz" ||
                    lower(candidate.stem().wstring()) != lower(std::wstring(area.name.begin(), area.name.end()))) continue;
                auto archive = std::make_unique<StorylandArchiveBrowser>();
                std::string error;
                if (archive->loadLvzWithCompanionImg(candidate.wstring(), error)) {
                    area.path = candidate;
                    area.archive = std::move(archive);
                    area.error.clear();
                    break;
                }
                area.error = error;
            }
            if (area.archive) break;
        }
    }
}

std::string escapeHtml(const std::string& text) {
    std::string escaped;
    for (const char c : text) {
        switch (c) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '\"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped += c; break;
        }
    }
    return escaped;
}

struct Bounds {
    double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
    bool valid = false;

    void include(double x, double y, double radius = 0.0) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        if (!std::isfinite(radius)) radius = 0.0;
        radius = std::clamp(radius, 0.0, 500.0);
        if (!valid) {
            minX = maxX = x;
            minY = maxY = y;
            valid = true;
        }
        minX = std::min(minX, x - radius);
        maxX = std::max(maxX, x + radius);
        minY = std::min(minY, y - radius);
        maxY = std::max(maxY, y + radius);
    }
};

void writeHeader(std::ostream& out, const std::string& title, const Bounds& bounds) {
    const double width = std::max(1.0, bounds.maxX - bounds.minX);
    const double height = std::max(1.0, bounds.maxY - bounds.minY);
    const double margin = std::max(width, height) * 0.04;
    out << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>" << escapeHtml(title) << " — Storyland Linux</title>"
           "<style>body{margin:0;background:#1b1d26;color:#f0f0f5;font:14px Segoe UI,Arial,sans-serif}"
           "header{background:linear-gradient(#ee3793,#a71664);padding:12px 18px;font-size:18px}"
           "nav{padding:10px 18px;background:#292c38;display:flex;gap:20px;align-items:center;flex-wrap:wrap}"
           "label{cursor:pointer}main{height:calc(100vh - 104px);overflow:hidden;background:#273849}"
           "svg{width:100%;height:100%;touch-action:none;cursor:grab}svg:active{cursor:grabbing}"
           "#areas polygon{stroke:none}#lights circle{stroke:#fff;stroke-width:.15}"
           "footer{position:absolute;right:15px;bottom:12px;background:#1b1d26d8;padding:7px;border-radius:5px}"
           "</style></head><body><header>Storyland · " << escapeHtml(title)
        << "</header><nav><label><input type=\"checkbox\" id=\"showAreas\" checked> Area meshes</label>"
           "<label><input type=\"checkbox\" id=\"showLights\" checked> 2DFX lights</label>"
           "<span id=\"summary\"></span></nav><main><svg id=\"view\" xmlns=\"http://www.w3.org/2000/svg\" "
           "viewBox=\"" << bounds.minX - margin << ' ' << -bounds.maxY - margin << ' '
        << width + 2 * margin << ' ' << height + 2 * margin
        << "\" preserveAspectRatio=\"xMidYMid meet\">"
           "<g id=\"areas\">";
}

struct Result {
    size_t areaCount = 0;
    size_t meshInstances = 0;
    size_t triangles = 0;
    size_t lights = 0;
};

Result writeAreaMeshes(std::ostream& out, const std::array<Area, 3>& areas) {
    Result result;
    out << std::fixed << std::setprecision(2);
    for (size_t areaIndex = 0; areaIndex < areas.size(); ++areaIndex) {
        const auto& area = areas[areaIndex];
        if (!area.archive) continue;
        ++result.areaCount;
        const auto& browser = *area.archive;
        std::map<uint64_t, const StorylandWorldMesh*> meshes;
        for (const auto& mesh : browser.worldMeshes())
            meshes.emplace((uint64_t(mesh.sectorIndex) << 32) | mesh.resourceIndex, &mesh);
        for (const auto& placement : browser.placements()) {
            auto found = meshes.find((uint64_t(placement.sectorIndex) << 32) | placement.resourceIndex);
            if (found == meshes.end()) continue;
            const auto& mesh = *found->second;
            if (mesh.triangles.empty()) continue;
            ++result.meshInstances;
            for (const auto& tri : mesh.triangles) {
                if (tri.a >= mesh.vertices.size() || tri.b >= mesh.vertices.size() ||
                    tri.c >= mesh.vertices.size()) continue;
                const uint32_t seed = tri.textureId == UINT32_MAX ? placement.resourceIndex : tri.textureId;
                const int red = 75 + int((seed * 37u + 31u) & 0x7fu);
                const int green = 75 + int((seed * 67u + 91u) & 0x7fu);
                const int blue = 75 + int((seed * 97u + 17u) & 0x7fu);
                out << "<polygon fill=\"rgb(" << red << ',' << green << ',' << blue
                    << ")\" points=\"";
                bool finite = true;
                std::ostringstream points;
                points << std::fixed << std::setprecision(2);
                for (uint32_t index : {tri.a, tri.b, tri.c}) {
                    const auto& vertex = mesh.vertices[index];
                    const double x = placement.matrix[0] * vertex.x + placement.matrix[4] * vertex.y +
                                     placement.matrix[8] * vertex.z + placement.matrix[12];
                    const double y = placement.matrix[1] * vertex.x + placement.matrix[5] * vertex.y +
                                     placement.matrix[9] * vertex.z + placement.matrix[13];
                    finite = finite && std::isfinite(x) && std::isfinite(y);
                    points << x << ',' << -y << ' ';
                }
                if (finite) {
                    out << points.str() << "\"/>\n";
                    ++result.triangles;
                } else {
                    out << "\"/>\n";
                }
            }
        }
    }
    return result;
}

void writeLights(std::ostream& out, const StorylandDtzArchive& dtz, const Bounds& bounds, Result& result) {
    const auto& effects = dtz.leeds2dfxEffects();
    const double span = std::max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY);
    const double radius = std::max(0.5, span * 0.0015);
    out << "</g><g id=\"lights\">" << std::fixed << std::setprecision(2);
    for (const auto& light : dtz.leeds2dfxWorldInstances()) {
        if (!light.valid || light.effectIndex >= effects.size()) continue;
        const auto& effect = effects[light.effectIndex];
        if (!effect.isLight || !effect.valid || !std::isfinite(light.worldX) ||
            !std::isfinite(light.worldY)) continue;
        out << "<circle cx=\"" << light.worldX << "\" cy=\"" << -light.worldY
            << "\" r=\"" << radius << "\" fill=\"rgb("
            << int(effect.red) << ',' << int(effect.green) << ',' << int(effect.blue)
            << ")\" fill-opacity=\".88\"/>\n";
        ++result.lights;
    }
}

void openViewer(const std::filesystem::path& output) {
    const auto absolute = std::filesystem::absolute(output).string();
    const pid_t child = fork();
    if (child == 0) {
        execlp("xdg-open", "xdg-open", absolute.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: StorylandLinux GAME.DTZ|BEACH.LVZ [--output view.html] [--img gta3PS2.img] [--no-open]\n";
        return 2;
    }
    std::filesystem::path input = argv[1];
    std::filesystem::path output;
    std::filesystem::path companion;
    bool open = true;
    for (int i = 2; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--no-open") open = false;
        else if ((option == "--output" || option == "--img") && i + 1 < argc) {
            if (option == "--output") output = argv[++i];
            else companion = argv[++i];
        } else {
            std::cerr << "Unknown option or missing argument: " << option << '\n';
            return 2;
        }
    }
    if (output.empty()) output = input.filename().string() + ".storyland.html";
    const std::wstring extension = lower(input.extension().wstring());
    StorylandDtzArchive dtz;
    std::array<Area, 3> areas{{{"BEACH"}, {"MALL"}, {"MAINLA"}}};
    std::string error;
    if (extension == L".dtz") {
        if (!dtz.loadFromFile(input.wstring(), error)) {
            std::cerr << "GAME.DTZ: " << error << '\n';
            return 1;
        }
        if (!companion.empty() && !dtz.loadCompanionImg(companion.wstring(), error)) {
            std::cerr << "Companion IMG: " << error << '\n';
            return 1;
        }
        findAreas(input, areas);
    } else if (extension == L".lvz") {
        auto area = std::make_unique<StorylandArchiveBrowser>();
        if (!area->loadLvzWithCompanionImg(input.wstring(), error)) {
            std::cerr << "LVZ/IMG: " << error << '\n';
            return 1;
        }
        areas[0].name = input.stem().string();
        areas[0].path = input;
        areas[0].archive = std::move(area);
    } else {
        std::cerr << "This Linux map viewer accepts GAME.DTZ or an area .LVZ with its matching .IMG.\n";
        return 2;
    }

    Bounds bounds;
    for (const auto& light : dtz.leeds2dfxWorldInstances())
        if (light.valid) bounds.include(light.worldX, light.worldY);
    for (const auto& area : areas) {
        if (!area.archive) continue;
        for (const auto& placement : area.archive->placements())
            bounds.include(placement.x, placement.y, placement.boundRadius);
    }
    if (!bounds.valid) {
        bounds.include(-50, -50);
        bounds.include(50, 50);
    }
    std::ofstream html(output, std::ios::binary | std::ios::trunc);
    if (!html) {
        std::cerr << "Could not write " << output << '\n';
        return 1;
    }
    writeHeader(html, input.filename().string(), bounds);
    Result result = writeAreaMeshes(html, areas);
    writeLights(html, dtz, bounds, result);
    // The meshes and lights are separate SVG groups so they can be toggled independently.
    html << "</g></svg></main><footer>W/S: move in/out · A/D: pan · Q/E: up/down · Wheel: zoom · Drag: pan</footer>"
            "<script>const view=document.getElementById('view');const b=view.viewBox.baseVal;"
            "const initial={x:b.x,y:b.y,width:b.width,height:b.height};"
            "let box={...initial},target={...initial};function apply(){view.setAttribute('viewBox',"
            "[box.x,box.y,box.width,box.height].join(' '))}"
            "function point(e){let p=view.createSVGPoint();p.x=e.clientX;p.y=e.clientY;"
            "return p.matrixTransform(view.getScreenCTM().inverse())}"
            "function zoom(s,p){target.x=p.x+(target.x-p.x)*s;target.y=p.y+(target.y-p.y)*s;"
            "target.width*=s;target.height*=s}"
            "view.addEventListener('wheel',e=>{e.preventDefault();zoom(Math.exp(e.deltaY*.001),point(e))},{passive:false});"
            "let drag=null;view.addEventListener('pointerdown',e=>{drag=point(e);view.setPointerCapture(e.pointerId)});"
            "view.addEventListener('pointermove',e=>{if(!drag)return;let p=point(e);"
            "target.x+=drag.x-p.x;target.y+=drag.y-p.y;drag=p});"
            "view.addEventListener('pointerup',()=>drag=null);view.addEventListener('pointercancel',()=>drag=null);"
            "view.addEventListener('dblclick',()=>{target={...initial}});"
            "const keys=new Set();window.addEventListener('keydown',e=>{let k=e.key.toLowerCase();"
            "if(['w','a','s','d','q','e'].includes(k)){keys.add(k);e.preventDefault()}});"
            "window.addEventListener('keyup',e=>keys.delete(e.key.toLowerCase()));"
            "window.addEventListener('blur',()=>keys.clear());"
            "let previous=performance.now();function frame(now){let dt=Math.min(.05,(now-previous)/1000);previous=now;"
            "let horizontal=Number(keys.has('d'))-Number(keys.has('a'));"
            "let vertical=Number(keys.has('e'))-Number(keys.has('q'));"
            "let forward=Number(keys.has('w'))-Number(keys.has('s'));"
            "let scale=Math.exp(-forward*dt*1.25);"
            "target.x+=(target.width-target.width*scale)/2;target.y+=(target.height-target.height*scale)/2;"
            "target.width*=scale;target.height*=scale;"
            "target.x+=horizontal*target.width*dt*.75;target.y+=vertical*target.height*dt*.75;"
            "let alpha=1-Math.exp(-dt*13);for(let name of ['x','y','width','height'])"
            "box[name]+=(target[name]-box[name])*alpha;apply();requestAnimationFrame(frame)}"
            "requestAnimationFrame(frame);"
            "for(const [check,layer] of [['showAreas','areas'],['showLights','lights']])"
            "document.getElementById(check).onchange=e=>document.getElementById(layer).style.display=e.target.checked?'':'none';"
            "</script>\n";
    html << "<script>document.getElementById('summary').textContent='" << result.areaCount
         << " areas · " << result.meshInstances << " mesh placements · "
         << result.triangles << " triangles · " << result.lights << " lights";
    for (const auto& area : areas) {
        html << " | " << escapeHtml(area.name) << ": "
             << (area.archive ? "loaded" : area.error.empty() ? "not found" : escapeHtml(area.error));
    }
    html << "';</script></body></html>\n";
    html.close();
    if (!html) {
        std::cerr << "Could not finish writing " << output << '\n';
        return 1;
    }
    std::cout << "Wrote " << output << " (" << result.areaCount << " areas, "
              << result.meshInstances << " mesh placements, " << result.triangles
              << " triangles, " << result.lights << " world lights).\n";
    for (const auto& area : areas) {
        std::cout << "  " << area.name << ": "
                  << (area.archive ? area.path.string() : area.error.empty() ? "not found" : area.error) << '\n';
    }
    if (open) openViewer(output);
    return 0;
}
