#include "SimulationConfig.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace phyz {
namespace {

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string stripComment(const std::string& line) {
    bool inQuote = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            inQuote = !inQuote;
        }
        if (!inQuote && (ch == '#' || ch == ';')) {
            return line.substr(0, i);
        }
    }
    return line;
}

std::optional<double> parseDouble(const std::string& text) {
    try {
        std::size_t consumed = 0;
        const double value = std::stod(trim(text), &consumed);
        if (trim(text.substr(consumed)).empty()) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::vector<double> parseNumberList(std::string text) {
    for (char& ch : text) {
        if (ch == ',' || ch == ';') {
            ch = ' ';
        }
    }

    std::vector<double> values;
    std::istringstream stream(text);
    double value = 0.0;
    while (stream >> value) {
        values.push_back(value);
    }
    return values;
}

std::optional<Vec3> parseVec3(const std::string& text) {
    const std::vector<double> values = parseNumberList(text);
    if (values.size() < 3) {
        return std::nullopt;
    }
    return Vec3{values[0], values[1], values[2]};
}

std::optional<Color> parseColor(const std::string& text) {
    const std::vector<double> values = parseNumberList(text);
    if (values.size() < 3) {
        return std::nullopt;
    }
    return Color{
        static_cast<float>(clamp(values[0], 0.0, 1.0)),
        static_cast<float>(clamp(values[1], 0.0, 1.0)),
        static_cast<float>(clamp(values[2], 0.0, 1.0)),
        static_cast<float>(values.size() >= 4 ? clamp(values[3], 0.0, 1.0) : 1.0),
    };
}

std::string unquote(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool parseBodyKey(const std::string& fullKey, int& bodyIndex, std::string& field) {
    std::string key = lower(fullKey);
    std::replace(key.begin(), key.end(), '_', '.');

    if (key.rfind("body.", 0) == 0) {
        const std::size_t secondDot = key.find('.', 5);
        if (secondDot == std::string::npos) {
            return false;
        }
        const std::string indexText = key.substr(5, secondDot - 5);
        if (!std::all_of(indexText.begin(), indexText.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
            return false;
        }
        bodyIndex = std::stoi(indexText);
        field = key.substr(secondDot + 1);
        return true;
    }

    if (key.rfind("body", 0) == 0 && key.size() > 4 && std::isdigit(static_cast<unsigned char>(key[4])) != 0) {
        std::size_t pos = 4;
        while (pos < key.size() && std::isdigit(static_cast<unsigned char>(key[pos])) != 0) {
            ++pos;
        }
        if (pos >= key.size() || key[pos] != '.') {
            return false;
        }
        bodyIndex = std::stoi(key.substr(4, pos - 4));
        field = key.substr(pos + 1);
        return true;
    }

    return false;
}

void ensureBodySlot(InitialConditionConfig& config, int index) {
    if (index < 0) {
        return;
    }
    if (static_cast<int>(config.bodies.size()) <= index) {
        config.bodies.resize(static_cast<std::size_t>(index + 1));
    }
}

void applyBodyField(InitialConditionConfig& config, int index, const std::string& field, const std::string& value) {
    ensureBodySlot(config, index);
    if (index < 0 || index >= static_cast<int>(config.bodies.size())) {
        return;
    }

    BodyInitialConfig& body = config.bodies[static_cast<std::size_t>(index)];
    const std::string normalizedField = lower(field);

    if (normalizedField == "name") {
        body.name = unquote(value);
        config.enabled = true;
    } else if (normalizedField == "mass") {
        if (const auto parsed = parseDouble(value)) {
            body.mass = std::max(0.001, *parsed);
            config.enabled = true;
        }
    } else if (normalizedField == "radius") {
        if (const auto parsed = parseDouble(value)) {
            body.radius = std::max(0.002, *parsed);
            config.enabled = true;
        }
    } else if (normalizedField == "physical.radius" || normalizedField == "physicalradius" || normalizedField == "collision.radius") {
        if (const auto parsed = parseDouble(value)) {
            body.physicalRadius = std::max(1.0e-5, *parsed);
            config.enabled = true;
        }
    } else if (normalizedField == "color" || normalizedField == "colour") {
        if (const auto parsed = parseColor(value)) {
            body.color = *parsed;
            config.enabled = true;
        }
    } else if (normalizedField == "position" || normalizedField == "pos") {
        if (const auto parsed = parseVec3(value)) {
            body.position = *parsed;
            config.enabled = true;
        }
    } else if (normalizedField == "velocity" || normalizedField == "vel") {
        if (const auto parsed = parseVec3(value)) {
            body.velocity = *parsed;
            config.enabled = true;
        }
    }
}

} // namespace

InitialConditionConfig loadInitialConditionConfig(const std::string& path) {
    InitialConditionConfig config;

    std::ifstream file(path);
    if (!file) {
        return config;
    }

    config.sourcePath = path;
    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(stripComment(line));
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        std::string key = lower(trim(line.substr(0, equals)));
        const std::string value = trim(line.substr(equals + 1));
        if (!section.empty() && section.rfind("body", 0) == 0 && key.find("body") != 0) {
            key = section + "." + key;
        }

        if (key == "body.count" || key == "body_count" || key == "bodies" || key == "count") {
            if (const auto parsed = parseDouble(value)) {
                config.bodyCount = std::clamp(static_cast<int>(std::round(*parsed)), 1, 64);
                config.enabled = true;
            }
            continue;
        }

        if (key == "physics.dt" || key == "physics_dt" || key == "dt" || key == "time.step") {
            if (const auto parsed = parseDouble(value)) {
                config.physicsDt = std::clamp(*parsed, 1.0e-7, 0.02);
                config.enabled = true;
            }
            continue;
        }

        if (key == "camera.distance" || key == "camera_distance") {
            if (const auto parsed = parseDouble(value)) {
                config.cameraDistance = std::clamp(*parsed, 1.5, 120.0);
                config.enabled = true;
            }
            continue;
        }

        if (key == "softening" || key == "physics.softening") {
            if (const auto parsed = parseDouble(value)) {
                config.softening = std::clamp(*parsed, 1.0e-5, 0.25);
                config.enabled = true;
            }
            continue;
        }

        int bodyIndex = -1;
        std::string bodyField;
        if (parseBodyKey(key, bodyIndex, bodyField)) {
            applyBodyField(config, bodyIndex, bodyField, value);
        }
    }

    if (config.enabled) {
        config.bodyCount = std::clamp(config.bodyCount, 1, 64);
        if (static_cast<int>(config.bodies.size()) > config.bodyCount) {
            config.bodyCount = static_cast<int>(config.bodies.size());
        }
        config.bodies.resize(static_cast<std::size_t>(config.bodyCount));
    }

    return config;
}

InitialConditionConfig loadInitialConditionConfigFromDefaultLocations() {
    InitialConditionConfig config = loadInitialConditionConfig("phyzbox.ini");
    if (config.enabled) {
        return config;
    }
    return loadInitialConditionConfig("..\\phyzbox.ini");
}

} // namespace phyz
