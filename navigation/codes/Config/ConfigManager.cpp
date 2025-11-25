// ConfigManager.cpp
#include "ConfigManager.h"
#include "Utils/Logger.h"
#include "Utils/FileUtils.h"
#include <fstream>

/**
 * @file ConfigManager.cpp
 * @brief Implementation of configuration loading, saving and conversion helpers.
 *
 * This file implements the ConfigManager singleton which loads application
 * settings (AppConfig) and renderer settings (RenderConfig) from JSON files.
 * Helper functions convert between JSON and struct representations.
 *
 * Notes:
 * - All IO errors are logged and cause the manager to return failure.
 */


/**
 * Singleton accessor for ConfigManager instance.
 * @return Reference to the static ConfigManager instance.
 */
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}


/**
 * Attempts to load all configuration files.
 * @return True if all configs loaded successfully, false otherwise.
 */
bool ConfigManager::loadAllConfigs() {
    bool success = true;
    
    // Load application configuration
    if (!loadAppConfig()) {
        Logger::error("Failed to load app config");
        success = false;
    }
    
    // Load render configuration
    if (!loadRenderConfig()) {
        Logger::error("Failed to load render config");
        success = false;
    }
    
    return success;
}


/**
 * Loads application configuration from JSON file.
 * Creates default config if file doesn't exist.
 * @return True if loading or creating default was successful, false otherwise.
 */
bool ConfigManager::loadAppConfig() {
    try {
        // Construct full path to app config file
        std::string configPath = configBasePath + "app_config.json";
        std::ifstream file(configPath);
        
        // Create default config if file doesn't exist
        if (!file.is_open()) {
            Logger::warn("App config file not found, creating default");
            return saveAppConfig();
        }
        
        // Parse JSON from file
        json j;
        file >> j;  // Parse contents in the file into a `json` instance
        // Convert JSON to AppConfig structure
        jsonToAppConfig(j, appConfig);
        
        Logger::info("App config loaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Error loading app config: " + std::string(e.what()));
        return false;
    }
}


/**
 * Loads render configuration from JSON file.
 * Creates default config if file doesn't exist.
 * @return True if loading or creating default was successful, false otherwise.
 */
bool ConfigManager::loadRenderConfig() {
    try {
        // Construct full path to render config file
        std::string configPath = configBasePath + "render_config.json";
        std::ifstream file(configPath);
        
        // Create default config if file doesn't exist
        if (!file.is_open()) {
            Logger::warn("Render config file not found, creating default");
            return saveRenderConfig();
        }
        
        // Parse JSON from file
        json j;
        file >> j;
        // Convert JSON to RenderConfig structure
        jsonToRenderConfig(j, renderConfig);
        
        Logger::info("Render config loaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Error loading render config: " + std::string(e.what()));
        return false;
    }
}


/**
 * Saves all configuration files to disk.
 * @return True if all configs saved successfully, false otherwise.
 */
bool ConfigManager::saveAllConfigs() {
    bool success = true;
    
    // Save application configuration
    if (!saveAppConfig()) {
        Logger::error("Failed to save app config");
        success = false;
    }
    
    // Save render configuration
    if (!saveRenderConfig()) {
        Logger::error("Failed to save render config");
        success = false;
    }
    
    return success;
}


/**
 * Saves application configuration to JSON file.
 * @return True if save was successful, false otherwise.
 */
bool ConfigManager::saveAppConfig() {
    try {
        // Construct full path to app config file
        std::string configPath = configBasePath + "app_config.json";
        std::ofstream file(configPath);
        
        // Check if file opened successfully
        if (!file.is_open()) {
            Logger::error("Cannot open app config file for writing: " + configPath);
            return false;
        }
        
        // Convert AppConfig to JSON
        json j;
        appConfigToJson(appConfig, j);
        // Write JSON to file with 4-space indentation
        file << j.dump(4);
        
        Logger::info("App config saved successfully");
        return true;

    } catch (const std::exception& e) {
        Logger::error("Error saving app config: " + std::string(e.what()));
        return false;
    }
}


/**
 * Saves render configuration to JSON file.
 * @return True if save was successful, false otherwise.
 */
bool ConfigManager::saveRenderConfig() {
    try {
        // Construct full path to render config file
        std::string configPath = configBasePath + "render_config.json";
        std::ofstream file(configPath);
        
        // Check if file opened successfully
        if (!file.is_open()) {
            Logger::error("Cannot open render config file for writing: " + configPath);
            return false;
        }
        
        // Convert RenderConfig to JSON
        json j;
        renderConfigToJson(renderConfig, j);
        // Write JSON to file with 4-space indentation
        file << j.dump(4);
        
        Logger::info("Render config saved successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Error saving render config: " + std::string(e.what()));
        return false;
    }
}


/**
 * Converts JSON object to AppConfig structure.
 * @param j JSON object containing app configuration
 * @param config AppConfig structure to populate with data
 */
void ConfigManager::jsonToAppConfig(const json& j, AppConfig& config) {
    // Parse window settings
    if (j.contains("window")) {
        const auto& window = j["window"];
        if (window.contains("width")) config.window.width = window["width"];
        if (window.contains("height")) config.window.height = window["height"];
        if (window.contains("title")) config.window.title = window["title"];
        if (window.contains("fullscreen")) config.window.fullscreen = window["fullscreen"];
        if (window.contains("resizable")) config.window.resizable = window["resizable"];
    }
    
    // Parse path settings
    if (j.contains("paths")) {
        const auto& paths = j["paths"];
        if (paths.contains("mapsDirectory")) {
            config.paths.mapsDirectory = paths["mapsDirectory"];
        }
        if (paths.contains("tilesDirectory")) {
            config.paths.tilesDirectory = paths["tilesDirectory"];
        }
        if (paths.contains("configDirectory")) {
            config.paths.configDirectory = paths["configDirectory"];
        }
        if (paths.contains("defaultMap")) {
            config.paths.defaultMap = paths["defaultMap"];
        }
    }
    
    // Parse logging settings
    if (j.contains("logging")) {
        const auto& logging = j["logging"];
        if (logging.contains("level")) config.logging.level = logging["level"];
        if (logging.contains("fileLogging")) {
            config.logging.fileLogging = logging["fileLogging"];
        }
        if (logging.contains("logFile")) config.logging.logFile = logging["logFile"];
    }
    
    // Parse color settings
    if (j.contains("colors")) {
        const auto& colors = j["colors"];
        if (colors.contains("background")) config.colors.background = colors["background"];
        if (colors.contains("text")) config.colors.text = colors["text"];
        if (colors.contains("highlight")) config.colors.highlight = colors["highlight"];
        if (colors.contains("warning")) config.colors.warning = colors["warning"];
        if (colors.contains("error")) config.colors.error = colors["error"];
    }
    
    // Parse performance settings
    if (j.contains("performance")) {
        const auto& performance = j["performance"];
        if (performance.contains("targetFPS")) config.performance.targetFPS = performance["targetFPS"];
        if (performance.contains("vsync")) config.performance.vsync = performance["vsync"];
        if (performance.contains("textureFilter")) config.performance.textureFilter = performance["textureFilter"];
    }

    // Parse map display settings
    if (j.contains("mapDisplay") && j["mapDisplay"].is_object()) {
        const auto& mapDisplay = j["mapDisplay"];
        if (mapDisplay.contains("tilesWidth") && mapDisplay["tilesWidth"].is_number()) {
            config.mapDisplay.tilesWidth = mapDisplay["tilesWidth"];
        }
        if (mapDisplay.contains("tilesHeight") && mapDisplay["tilesHeight"].is_number()) {
            config.mapDisplay.tilesHeight = mapDisplay["tilesHeight"];
        }
        if (mapDisplay.contains("defaultZoom") && mapDisplay["defaultZoom"].is_number()) {
            config.mapDisplay.defaultZoom = mapDisplay["defaultZoom"];
        }
        if (mapDisplay.contains("minZoom") && mapDisplay["minZoom"].is_number()) {
            config.mapDisplay.minZoom = mapDisplay["minZoom"];
        }
        if (mapDisplay.contains("maxZoom") && mapDisplay["maxZoom"].is_number()) {
            config.mapDisplay.maxZoom = mapDisplay["maxZoom"];
        }
    }

    // Parse UI settings (map button)
    if (j.contains("ui") && j["ui"].is_object()) {
        const auto& ui = j["ui"];
        if (ui.contains("mapButton") && ui["mapButton"].is_object()) {
            const auto& mb = ui["mapButton"];
            if (mb.contains("enabled")) config.mapButton.enabled = mb["enabled"];
            if (mb.contains("label")) config.mapButton.label = mb["label"];
            if (mb.contains("x")) config.mapButton.x = mb["x"];
            if (mb.contains("y")) config.mapButton.y = mb["y"];
            if (mb.contains("width")) config.mapButton.width = mb["width"];
            if (mb.contains("height")) config.mapButton.height = mb["height"];
            if (mb.contains("bgColor")) config.mapButton.bgColor = mb["bgColor"];
            if (mb.contains("hoverColor")) config.mapButton.hoverColor = mb["hoverColor"];
            if (mb.contains("textColor")) config.mapButton.textColor = mb["textColor"];
            if (mb.contains("fontSize")) config.mapButton.fontSize = mb["fontSize"];
            if (mb.contains("anchorRight")) config.mapButton.anchorRight = mb["anchorRight"];
        }
    }
}


/**
 * Converts AppConfig structure to JSON object.
 * @param config AppConfig structure to convert
 * @param j JSON object to populate with config data
 */
void ConfigManager::appConfigToJson(const AppConfig& config, json& j) {
    // Convert window settings to JSON
    j["window"] = {
        {"width", config.window.width},
        {"height", config.window.height},
        {"title", config.window.title},
        {"fullscreen", config.window.fullscreen},
        {"resizable", config.window.resizable}
    };
    
    // Convert path settings to JSON
    j["paths"] = {
        {"mapsDirectory", config.paths.mapsDirectory},
        {"tilesDirectory", config.paths.tilesDirectory},
        {"configDirectory", config.paths.configDirectory},
        {"defaultMap", config.paths.defaultMap}
    };
    
    // Convert logging settings to JSON
    j["logging"] = {
        {"level", config.logging.level},
        {"fileLogging", config.logging.fileLogging},
        {"logFile", config.logging.logFile}
    };
    
    // Convert color settings to JSON
    j["colors"] = {
        {"background", config.colors.background},
        {"text", config.colors.text},
        {"highlight", config.colors.highlight},
        {"warning", config.colors.warning},
        {"error", config.colors.error}
    };
    
    // Convert performance settings to JSON
    j["performance"] = {
        {"targetFPS", config.performance.targetFPS},
        {"vsync", config.performance.vsync},
        {"textureFilter", config.performance.textureFilter}
    };

    // Add map display settings
    j["mapDisplay"]["tilesWidth"] = config.mapDisplay.tilesWidth;
    j["mapDisplay"]["tilesHeight"] = config.mapDisplay.tilesHeight;
    j["mapDisplay"]["defaultZoom"] = config.mapDisplay.defaultZoom;
    j["mapDisplay"]["minZoom"] = config.mapDisplay.minZoom;
    j["mapDisplay"]["maxZoom"] = config.mapDisplay.maxZoom;

    // Add UI settings (map button)
    j["ui"]["mapButton"] = {
        {"enabled", config.mapButton.enabled},
        {"label", config.mapButton.label},
        {"x", config.mapButton.x},
        {"y", config.mapButton.y},
        {"width", config.mapButton.width},
        {"height", config.mapButton.height},
        {"bgColor", config.mapButton.bgColor},
        {"hoverColor", config.mapButton.hoverColor},
        {"textColor", config.mapButton.textColor},
        {"fontSize", config.mapButton.fontSize},
        {"anchorRight", config.mapButton.anchorRight}
    };
}


/**
 * Converts JSON object to RenderConfig structure.
 * @param j JSON object containing render configuration
 * @param config RenderConfig structure to populate with data
 */
void ConfigManager::jsonToRenderConfig(const json& j, RenderConfig& config) {
    // Parse clear color settings
    if (j.contains("clearColor")) {
        const auto& clearColor = j["clearColor"];
        if (clearColor.contains("r")) config.clearColor.r = clearColor["r"];
        if (clearColor.contains("g")) config.clearColor.g = clearColor["g"];
        if (clearColor.contains("b")) config.clearColor.b = clearColor["b"];
        if (clearColor.contains("a")) config.clearColor.a = clearColor["a"];
    }
    
    // Parse text rendering settings
    if (j.contains("text")) {
        const auto& text = j["text"];
        if (text.contains("fontPath")) config.text.fontPath = text["fontPath"];
        if (text.contains("fontSize")) config.text.fontSize = text["fontSize"];
        if (text.contains("textColor")) config.text.textColor = text["textColor"];
    }
    
    // Parse tile rendering settings
    if (j.contains("tileRendering")) {
        const auto& tileRendering = j["tileRendering"];
        if (tileRendering.contains("enableBlending")) {
            config.tileRendering.enableBlending = tileRendering["enableBlending"];
        }
        if (tileRendering.contains("enableSorting")) {
            config.tileRendering.enableSorting = tileRendering["enableSorting"];
        }
        if (tileRendering.contains("batchSize")) {
            config.tileRendering.batchSize = tileRendering["batchSize"];
        }
    }
}


/**
 * Converts RenderConfig structure to JSON object.
 * @param config RenderConfig structure to convert
 * @param j JSON object to populate with config data
 */
void ConfigManager::renderConfigToJson(const RenderConfig& config, json& j) {
    // Convert clear color settings to JSON
    j["clearColor"] = {
        {"r", config.clearColor.r},
        {"g", config.clearColor.g},
        {"b", config.clearColor.b},
        {"a", config.clearColor.a}
    };
    
    // Convert text rendering settings to JSON
    j["text"] = {
        {"fontPath", config.text.fontPath},
        {"fontSize", config.text.fontSize},
        {"textColor", config.text.textColor}
    };
    
    // Convert tile rendering settings to JSON
    j["tileRendering"] = {
        {"enableBlending", config.tileRendering.enableBlending},
        {"enableSorting", config.tileRendering.enableSorting},
        {"batchSize", config.tileRendering.batchSize}
    };
}


/**
 * Updates application configuration and saves to disk.
 * @param config New AppConfig to apply
 */
void ConfigManager::updateAppConfig(const AppConfig& config) {
    appConfig = config;
    saveAppConfig();
}


/**
 * Updates render configuration and saves to disk.
 * @param config New RenderConfig to apply
 */
void ConfigManager::updateRenderConfig(const RenderConfig& config) {
    renderConfig = config;
    saveRenderConfig();
}


/**
 * Constructs full path to a map file.
 * @param mapFile Map filename (optional)
 * @return Full path combining maps directory and filename
 */
std::string ConfigManager::getFullMapPath(const std::string& mapFile) const {
    std::string mapsDir = appConfig.paths.mapsDirectory;
    std::string mapName = mapFile.empty() ? appConfig.paths.defaultMap : mapFile;
    
    if (!mapsDir.empty() && mapsDir.back() != '/' && mapsDir.back() != '\\') {
        mapsDir += '/';
    }
    
    if (mapName.find('/') != std::string::npos || mapName.find('\\') != std::string::npos) {
        return mapName;
    }
    
    return mapsDir + mapName;
}


/**
 * Constructs full path to a tile file.
 * @param tileFile Tile filename (optional)
 * @return Full path combining tiles directory and filename
 */
std::string ConfigManager::getFullTilesPath(const std::string& tileFile) const {
    std::string result = appConfig.paths.tilesDirectory;
    if (!tileFile.empty()) {
        result += tileFile;
    }
    return result;
}