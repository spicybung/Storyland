#include "storyland_data.h"
#include "storyland_atomic_io.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <set>

namespace {
std::string upper(std::string value) {
    for (char& c : value) c = char(std::toupper(static_cast<unsigned char>(c)));
    return value;
}
bool command(const std::string& value) {
    static const std::set<std::string> names = {"IDE", "IPL", "COLFILE", "MODELFILE", "TEXDICTION", "SPLASH", "IMG", "CDIMAGE", "HIERFILE"};
    return names.count(upper(value)) != 0;
}
bool separator(char c) { return c == ' ' || c == '\t' || c == ','; }
std::vector<StorylandDataField> tokenize(const std::string& text, size_t begin) {
    std::vector<StorylandDataField> result;
    size_t cursor = begin;
    while (cursor < text.size()) {
        while (cursor < text.size() && separator(text[cursor])) ++cursor;
        if (cursor == text.size() || text[cursor] == '#' || text[cursor] == ';' || text.compare(cursor, 2, "//") == 0) break;
        size_t start = cursor;
        if (text[cursor] == '"') {
            ++cursor;
            while (cursor < text.size() && text[cursor] != '"') ++cursor;
            if (cursor < text.size()) ++cursor;
        } else {
            while (cursor < text.size() && !separator(text[cursor]) && text[cursor] != '#' && text[cursor] != ';') ++cursor;
        }
        result.push_back({"Value " + std::to_string(result.size() + 1), text.substr(start, cursor - start), start, cursor - start});
    }
    return result;
}
float readFloat(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t bits = uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
        uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
void writeWord(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (i * 8));
}
std::string floatText(float value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return stream.str();
}
bool number(const std::string& text, double& value) {
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    if (!(stream >> value) || !std::isfinite(value)) return false;
    stream >> std::ws;
    return stream.eof();
}
}

bool StorylandDataFile::loadFromFile(const std::filesystem::path& path, std::string& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { error = "Could not open data file."; return false; }
    const auto length = file.tellg();
    if (length < 0 || length > 64 * 1024 * 1024) { error = "Data file exceeds 64 MiB."; return false; }
    std::vector<uint8_t> data(static_cast<size_t>(length));
    file.seekg(0);
    if (!data.empty() && !file.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size()))) {
        error = "Could not read the complete data file."; return false;
    }
    return loadBytes(data, path, error);
}
bool StorylandDataFile::loadBytes(const std::vector<uint8_t>& data, const std::filesystem::path& path, std::string& error) {
    error.clear();
    StorylandDataFile next;
    next.path_ = path;
    const auto filename = upper(path.filename().string());
    next.manifest_ = filename == "GTA_VC.DAT" || filename == "GTA3.DAT" || filename == "GTA_LCS.DAT" || filename == "GTA_VCS.DAT" || filename == "GTA.DAT";
    if (filename == "WATERPRO.DAT") {
        if (data.size() != 21444) { error = "Unsupported waterpro.dat size; expected the 21444-byte water table."; return false; }
        const uint32_t count = uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
        if (count > 48) { error = "Water count exceeds 48 slots."; return false; }
        next.binaryWater_ = true;
        next.binary_ = data;
        StorylandDataRow header;
        header.section = "Water settings";
        header.text = "Water settings";
        header.fields.push_back({"Active rectangles", std::to_string(count), 0, 0, 0, false});
        next.rows_.push_back(header);
        for (size_t i = 0; i < 48; ++i) {
            StorylandDataRow row;
            row.section = "Water rectangles";
            row.text = "Water " + std::to_string(i) + (i >= count ? " (unused)" : "");
            row.fields.push_back({"Level", floatText(readFloat(data, 4 + i * 4)), 0, 0, 4 + i * 4});
            const char* labels[] = {"Left", "Top", "Right", "Bottom"};
            for (size_t j = 0; j < 4; ++j) row.fields.push_back({labels[j], floatText(readFloat(data, 196 + i * 16 + j * 4)), 0, 0, 196 + i * 16 + j * 4});
            next.rows_.push_back(std::move(row));
        }
        for (size_t dimension : {size_t(64), size_t(128)}) {
            const size_t base = dimension == 64 ? 964 : 5060;
            for (size_t y = 0; y < dimension; ++y) {
                StorylandDataRow row;
                row.section = dimension == 64 ? "64 x 64 grid" : "128 x 128 grid";
                row.text = row.section + " / row " + std::to_string(y);
                for (size_t x = 0; x < dimension; ++x) {
                    const size_t offset = base + y * dimension + x;
                    row.fields.push_back({"Cell " + std::to_string(x), std::to_string(data[offset]), 0, 0, offset, true});
                }
                next.rows_.push_back(std::move(row));
            }
        }
    } else {
        for (uint8_t value : data) {
            if (value == 0 || (value < 32 && value != 9 && value != 10 && value != 13 && value != 26)) {
                error = "This file contains binary data; it is not a supported text table."; return false;
            }
        }
        size_t cursor = 0;
        if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
            next.bom_ = "\xEF\xBB\xBF";
            cursor = 3;
        }
        while (cursor < data.size()) {
            const size_t start = cursor;
            while (cursor < data.size() && data[cursor] != 10 && data[cursor] != 13) ++cursor;
            StorylandDataRow row;
            row.text.assign(data.begin() + start, data.begin() + cursor);
            if (cursor < data.size()) {
                row.ending += char(data[cursor++]);
                if (row.ending == "\r" && cursor < data.size() && data[cursor] == 10) row.ending += char(data[cursor++]);
            }
            next.rows_.push_back(std::move(row));
        }
        next.parseRows();
        if (!next.rows_.empty() && next.rows_[0].fields.size() == 1) {
            double count = 0;
            next.countedPath_ = number(next.rows_[0].fields[0].value, count) && count >= 0 && std::floor(count) == count;
        }
    }
    *this = std::move(next);
    return true;
}
void StorylandDataFile::parseRows() {
    std::string section;
    const auto filename = upper(path_.filename().string());
    for (auto& row : rows_) {
        row.disabled = row.comment = false;
        size_t start = row.text.find_first_not_of(" \t");
        row.fields.clear();
        if (start == std::string::npos) { row.section = section; continue; }
        if (row.text[start] == '#' && manifest_) {
            auto fields = tokenize(row.text, start + 1);
            if (!fields.empty() && command(fields[0].value)) {
                row.disabled = true;
                row.fields = std::move(fields);
            }
        }
        if (row.fields.empty()) {
            if (row.text[start] == '#' || row.text[start] == ';' || row.text.compare(start, 2, "//") == 0) row.comment = true;
            else row.fields = tokenize(row.text, start);
        }
        if (!manifest_ && row.fields.size() == 1) {
            const auto value = upper(row.fields[0].value);
            if (value == "OBJS" || value == "TOBJ" || value == "CARS" || value == "PEDS" || value == "WEAP" || value == "HIER" || value == "TXDP" || value == "PATH" || value == "2DFX" || value == "ZONE" || value == "CULL" || value == "INST" || value == "COL" || value == "CAR") section = value;
            if (value == "END") section.clear();
        }
        row.section = section;
        std::vector<std::string> names;
        if (manifest_ && !row.fields.empty()) {
            const auto type = upper(row.fields[0].value);
            if (type == "COLFILE") names = {"Command", "Level", "Path"};
            else names = {"Command", type == "SPLASH" ? "Image" : "Path"};
        } else if (filename == "WATER.DAT") names = {"Level", "Left", "Bottom", "Right", "Top"};
        else if (filename == "PEDSTATS.DAT") names = {"Name", "Flee distance", "Heading change", "Fear", "Temper", "Lawfulness", "Sexiness", "Attack strength", "Defend weakness", "Flags"};
        else if (filename == "OBJECT.DAT") names = {"Name", "Mass", "Turn mass", "Air resistance", "Elasticity", "Submerged percent", "Uproot limit", "Damage multiplier", "Damage effect", "Collision response", "Camera avoidance"};
        else if (filename == "PED.DAT" && row.fields.size() == 6) names = {"Ped type", "Walk speed", "Run speed", "Fast run speed", "Flee distance", "Heading change"};
        else if (filename == "HANDLING.CFG" && row.fields.size() == 34) names = {"Name", "Mass", "Turn mass", "Drag", "Centre X", "Centre Y", "Centre Z", "Submerged percent", "Traction multiplier", "Traction loss", "Traction bias", "Gears", "Maximum velocity", "Engine acceleration", "Engine inertia", "Drive type", "Engine type", "Brake deceleration", "Brake bias", "ABS", "Steering lock", "Suspension force", "Suspension damping", "High speed damping", "Suspension upper limit", "Suspension lower limit", "Suspension bias", "Anti dive", "Seat offset", "Collision damage", "Value", "Flags", "Front lights", "Rear lights"};
        else if (filename == "CARCOLS.DAT" && section == "COL" && row.fields.size() == 3) names = {"Red", "Green", "Blue"};
        else if (filename == "CARCOLS.DAT" && section == "CAR" && !row.fields.empty()) names = {"Model"};
        else if (section == "ZONE" && row.fields.size() >= 8) names = {"Name", "Type", "Minimum X", "Minimum Y", "Minimum Z", "Maximum X", "Maximum Y", "Maximum Z", "Level"};
        else if (upper(path_.extension().string()) == ".IDE" && section == "PEDS" && row.fields.size() >= 7) names = {"ID", "Model", "Texture", "Ped type", "Stats", "Animation group", "Cars allowed"};
        else if (upper(path_.extension().string()) == ".IDE" && section == "CARS" && row.fields.size() >= 6) names = {"ID", "Model", "Texture", "Type", "Handling", "Game name"};
        else if (upper(path_.extension().string()) == ".IDE" && (section == "OBJS" || section == "TOBJ") && row.fields.size() >= 4) names = {"ID", "Model", "Texture", "Mesh count"};
        else if (filename == "WEAPON.DAT") names = {"Name", "Fire type", "Range", "Firing rate", "Reload", "Ammunition", "Damage", "Speed", "Radius", "Life span", "Spread", "Fire offset X", "Fire offset Y", "Fire offset Z", "Animation", "Loop start", "Loop end", "Fire frame", "Second loop start", "Second loop end", "Second fire frame", "Breakout frame", "Model ID", "Second model ID", "Flags", "Slot"};
        else if ((filename.find("FLIGHT") == 0 || filename.find("TRACKS") == 0 || filename.find("FERRY") == 0 || filename.find("SPATH") == 0) && row.fields.size() == 3) names = {"X", "Y", "Z"};
        for (size_t i = 0; i < row.fields.size() && i < names.size(); ++i) row.fields[i].name = names[i];
    }
}
std::vector<uint8_t> StorylandDataFile::bytes() const {
    if (binaryWater_) return binary_;
    std::string text = bom_;
    for (const auto& row : rows_) text += row.text + row.ending;
    return {text.begin(), text.end()};
}
bool StorylandDataFile::setField(size_t row, size_t field, const std::string& value, std::string& error) {
    error.clear();
    if (row >= rows_.size() || field >= rows_[row].fields.size()) { error = "Select a data field."; return false; }
    auto target = rows_[row].fields[field];
    if (target.value == value) return true;
    if (binaryWater_) {
        double numeric = 0;
        if (!number(value, numeric)) { error = "Enter a finite numeric value."; return false; }
        if (target.byteValue || target.offset == 0) {
            const double maximum = target.byteValue ? 255 : 48;
            if (numeric < 0 || numeric > maximum || std::floor(numeric) != numeric) { error = "Integer value must be between 0 and " + std::to_string(int(maximum)) + "."; return false; }
            if (target.byteValue) binary_[target.offset] = uint8_t(numeric);
            else writeWord(binary_, 0, uint32_t(numeric));
        } else {
            float encoded = float(numeric);
            if (!std::isfinite(encoded)) { error = "Value exceeds the 32-bit float range."; return false; }
            uint32_t bits;
            std::memcpy(&bits, &encoded, 4);
            writeWord(binary_, target.offset, bits);
        }
        rows_[row].fields[field].value = value;
        if (target.offset == 0) for (size_t i = 0; i < 48; ++i) rows_[i + 1].text = "Water " + std::to_string(i) + (i >= size_t(numeric) ? " (unused)" : "");
        dirty_ = true;
        return true;
    }
    if (value.empty() || value.find_first_of("\r\n\0", 0, 3) != std::string::npos) { error = "Enter one nonempty field value."; return false; }
    if (value.find_first_of(",#;\t") != std::string::npos || (value.find(' ') != std::string::npos && (value.front() != '"' || value.back() != '"'))) {
        error = "Values containing spaces must be quoted. Commas, tabs and comment markers cannot occur inside a field."; return false;
    }
    std::string edited = rows_[row].text;
    edited.replace(target.begin, target.length, value);
    return setRow(row, edited, error);
}
bool StorylandDataFile::setRow(size_t row, const std::string& text, std::string& error) {
    error.clear();
    if (binaryWater_) { error = "Edit binary water fields individually."; return false; }
    if (row >= rows_.size()) { error = "Select a data row."; return false; }
    if (text.find('\r') != std::string::npos || text.find('\n') != std::string::npos || text.find('\0') != std::string::npos) { error = "A row cannot contain a line break or NUL byte."; return false; }
    rows_[row].text = text;
    parseRows();
    dirty_ = true;
    return true;
}
void StorylandDataFile::updatePathCount() {
    if (!countedPath_ || rows_.empty() || rows_[0].fields.empty()) return;
    size_t count = 0;
    for (size_t i = 1; i < rows_.size(); ++i) if (rows_[i].fields.size() == 3 && !rows_[i].comment) ++count;
    const auto field = rows_[0].fields[0];
    rows_[0].text.replace(field.begin, field.length, std::to_string(count));
    parseRows();
}
bool StorylandDataFile::insertRow(size_t before, const std::string& text, std::string& error) {
    if (binaryWater_ || before > rows_.size() || (countedPath_ && before == 0)) { error = "Cannot insert a row at this position."; return false; }
    if (text.find_first_of("\r\n\0", 0, 3) != std::string::npos) { error = "Enter a single row."; return false; }
    std::string ending = "\r\n";
    for (const auto& row : rows_) if (!row.ending.empty()) { ending = row.ending; break; }
    if (before > 0 && rows_[before - 1].ending.empty()) rows_[before - 1].ending = ending;
    StorylandDataRow row;
    row.text = text; row.ending = ending;
    rows_.insert(rows_.begin() + before, std::move(row));
    parseRows(); updatePathCount(); dirty_ = true; error.clear(); return true;
}
bool StorylandDataFile::removeRow(size_t row, std::string& error) {
    if (binaryWater_ || row >= rows_.size() || (countedPath_ && row == 0)) { error = "Cannot remove this row."; return false; }
    rows_.erase(rows_.begin() + row); parseRows(); updatePathCount(); dirty_ = true; error.clear(); return true;
}
bool StorylandDataFile::moveRow(size_t row, int direction, std::string& error) {
    const auto other = std::ptrdiff_t(row) + direction;
    if (binaryWater_ || row >= rows_.size() || (direction != -1 && direction != 1) || other < 0 || size_t(other) >= rows_.size() || (countedPath_ && (row == 0 || other == 0))) { error = "Cannot move this row."; return false; }
    // Line terminators belong to positions so a final unterminated row stays separate.
    std::swap(rows_[row].text, rows_[size_t(other)].text);
    parseRows(); dirty_ = true; error.clear(); return true;
}
bool StorylandDataFile::toggleRow(size_t row, std::string& error) {
    if (!manifest_ || row >= rows_.size() || rows_[row].fields.empty() || !command(rows_[row].fields[0].value)) { error = "Select a load-list command."; return false; }
    std::string text = rows_[row].text;
    const auto start = text.find_first_not_of(" \t");
    if (rows_[row].disabled) text.erase(start, 1);
    else text.insert(start, "#");
    return setRow(row, text, error);
}
std::filesystem::path StorylandDataFile::referencedPath(size_t row) const {
    if (!manifest_ || row >= rows_.size()) return {};
    const auto& fields = rows_[row].fields;
    if (fields.empty() || !command(fields[0].value) || upper(fields[0].value) == "SPLASH") return {};
    const size_t index = upper(fields[0].value) == "COLFILE" ? 2 : 1;
    if (fields.size() <= index) return {};
    std::string value = fields[index].value;
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
    std::replace(value.begin(), value.end(), '\\', '/');
    auto base = path_.parent_path();
    if (upper(base.filename().string()) == "DATA") base = base.parent_path();
    auto target = std::filesystem::path(value);
    if (target.is_absolute()) return target.lexically_normal();
    // Match the game's Windows paths when inspecting the same files on Linux.
    for (const auto& component : target) {
        std::error_code ec;
        auto candidate = base / component;
        if (!std::filesystem::exists(candidate, ec)) {
            for (std::filesystem::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec)) {
                if (upper(it->path().filename().string()) == upper(component.string())) { candidate = it->path(); break; }
            }
        }
        base = candidate;
    }
    return base.lexically_normal();
}
std::vector<std::string> StorylandDataFile::warnings() const {
    std::vector<std::string> result;
    if (countedPath_ && !rows_.empty() && !rows_[0].fields.empty()) {
        size_t points = 0;
        for (size_t i = 1; i < rows_.size(); ++i) if (rows_[i].fields.size() == 3 && !rows_[i].comment) {
            ++points;
            for (const auto& field : rows_[i].fields) {
                double value = 0;
                if (!number(field.value, value)) result.push_back("Line " + std::to_string(i + 1) + ": invalid path coordinate");
            }
        }
        double declared = 0;
        if (!number(rows_[0].fields[0].value, declared) || declared != double(points)) result.push_back("Path point count differs from the number of coordinate rows");
    }
    if (manifest_) for (size_t i = 0; i < rows_.size(); ++i) {
        const auto& row = rows_[i];
        if (row.disabled || row.fields.empty() || row.comment) continue;
        const auto type = upper(row.fields[0].value);
        if (!command(type)) result.push_back("Line " + std::to_string(i + 1) + ": unknown command " + type);
        else if ((type == "COLFILE" && row.fields.size() != 3) || (type != "COLFILE" && row.fields.size() != 2)) result.push_back("Line " + std::to_string(i + 1) + ": unexpected field count");
        else {
            const auto reference = referencedPath(i);
            std::error_code ec;
            if (!reference.empty() && !std::filesystem::is_regular_file(reference, ec)) result.push_back("Line " + std::to_string(i + 1) + ": missing " + reference.string());
        }
    }
    return result;
}
std::string StorylandDataFile::summary() const {
    return path_.filename().string() + " | " + std::to_string(rows_.size()) + " rows" + (binaryWater_ ? " | binary water" : manifest_ ? " | load list" : " | text data") + (dirty_ ? " | modified" : "");
}
bool StorylandDataFile::saveToFile(const std::filesystem::path& path, std::string& error) {
    const auto output = bytes();
    StorylandDataFile verification;
    if (!verification.loadBytes(output, path_, error)) return false;
    if (!storylandWriteFilesTransaction({{path, &output}}, error)) return false;
    path_ = path;
    dirty_ = false;
    return true;
}
