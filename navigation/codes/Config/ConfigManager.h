// Prevent multiple inclusions of this header file
#pragma once

// Standard headers
#include <string>
#include <unordered_map>

// JSON library used by the configuration manager implementation.
#include <nlohmann/json.hpp>

// Alias for convenience in the header; implementation also uses this alias.
using json = nlohmann::json;

/**
 * @file ConfigManager.h
 * @brief Application and renderer configuration definitions and manager.
 *
 * This header defines AppConfig and RenderConfig structures and a singleton
 * ConfigManager that loads/saves configuration from JSON files under a
 * configurable base directory.
 *
 * Notes:
 * - ConfigManager provides convenience functions to resolve full paths for
 *   maps and tiles based on configured directories.
 */


/**
 * Main application configuration structure containing all settings
 * for the navigation system application.
 */
struct AppConfig {
    /**
     * Window-related configuration settings.
     */
    struct Window {
        int width = 800;
        int height = 600;
        std::string title = "Navigation System";
        bool fullscreen = false;
        bool resizable = true;
    } window;

    /**
     * File and directory path configuration.
     */
    struct Paths {
        std::string mapsDirectory = "maps/";
        std::string tilesDirectory = "tiles/";
        std::string configDirectory = "config/";
        std::string defaultMap = "map.tmj";
    } paths;

    /**
     * Logging system configuration.
     */
    struct Logging {
        std::string level = "info";
        bool fileLogging = false;
        std::string logFile = "navigation.log";
    } logging;

    /**
     * Color scheme configuration for UI elements.
     */
    struct Colors {
        std::string background = "#2E3440";
        std::string text = "#D8DEE9";
        std::string highlight = "#88C0D0";
        std::string warning = "#EBCB8B";
        std::string error = "#BF616A";
    } colors;

    /**
     * Performance and rendering optimization settings.
     */
    struct Performance {
        int targetFPS = 60;
        bool vsync = true;
        int textureFilter = 1;
    } performance;

    /**
     * Map display and view configuration.
     */
    struct MapDisplay {
        int tilesWidth = 60;      // Number of tiles to display horizontally
        int tilesHeight = 40;     // Number of tiles to display vertically
        float defaultZoom = 1.0f; // Default zoom level
        float minZoom = 0.5f;     // Minimum zoom level
        float maxZoom = 3.0f;     // Maximum zoom level
    } mapDisplay;
};


/**
 * Rendering-specific configuration structure.
 */
struct RenderConfig {
    /**
     * Clear color settings for the rendering context.
     */
    struct ClearColor {
        float r = 0.1f;  // Red component (0.0-1.0)
        float g = 0.1f;  // Green component (0.0-1.0)
        float b = 0.1f;  // Blue component (0.0-1.0)
        float a = 1.0f;  // Alpha component (0.0-1.0)
    } clearColor;  // Instance of ClearColor configuration
    
    /**
     * Text rendering configuration.
     */
    struct Text {
        std::string fontPath = "fonts/arial.ttf";  // Path to font file
        int fontSize = 16;                         // Base font size in points
        std::string textColor = "#D8DEE9";       // Text filling color
        std::string borderColor = "#88C0D0";     // Text border color
        float borderWidth = 1.0f;                  // Border width
    } text;  // Instance of Text configuration
    
    /**
     * Tile rendering optimization settings.
     */
    struct TileRendering {
        bool enableBlending = true;   // Enable alpha blending for tiles
        bool enableSorting = true;    // Enable depth sorting of tiles
        int batchSize = 1000;         // Number of tiles to render per batch
    } tileRendering;  // Instance of TileRendering configuration
};


/**
 * Manager class for handling various configuration types.
 * Provides loading, saving, and access to both AppConfig and RenderConfig.
 */
class ConfigManager {
public:
    /**
     * Get the singleton instance of ConfigManager.
     * @return Reference to the single ConfigManager instance
     */
    static ConfigManager& getInstance();
    
    /**
     * Load all configuration files from disk.
     * @return True if all configs were loaded successfully, false otherwise
     */
    bool loadAllConfigs();
    
    /**
     * Save all current configuration settings to disk.
     * @return True if all configs were saved successfully, false otherwise
     */
    bool saveAllConfigs();
    
    /**
     * Get the current application configuration.
     * @return Constant reference to the AppConfig object
     */
    const AppConfig& getAppConfig() const { return appConfig; }
    
    /**
     * Get the current rendering configuration.
     * @return Constant reference to the RenderConfig object
     */
    const RenderConfig& getRenderConfig() const { return renderConfig; }
    
    /**
     * Update the application configuration with new settings.
     * @param config New AppConfig object containing updated settings
     */
    void updateAppConfig(const AppConfig& config);
    
    /**
     * Update the rendering configuration with new settings.
     * @param config New RenderConfig object containing updated settings
     */
    void updateRenderConfig(const RenderConfig& config);
    
    /**
     * Get the full path to a map file.
     * @param mapFile Optional specific map file name, uses default if empty
     * @return Full filesystem path to the map file
     */
    std::string getFullMapPath(const std::string& mapFile = "") const;
    
    /**
     * Get the full path to a tile file.
     * @param tileFile Optional specific tile file name
     * @return Full filesystem path to the tile file
     */
    std::string getFullTilesPath(const std::string& tileFile = "") const;
    
private:
    // Private constructor for singleton pattern
    ConfigManager() = default;
    
    /**
     * Load application configuration from JSON file.
     * @return True if loaded successfully, false otherwise
     */
    bool loadAppConfig();
    
    /**
     * Load rendering configuration from JSON file.
     * @return True if loaded successfully, false otherwise
     */
    bool loadRenderConfig();
    
    /**
     * Save application configuration to JSON file.
     * @return True if saved successfully, false otherwise
     */
    bool saveAppConfig();
    
    /**
     * Save rendering configuration to JSON file.
     * @return True if saved successfully, false otherwise
     */
    bool saveRenderConfig();
    
    /**
     * Convert JSON object to AppConfig structure.
     * @param j JSON object containing configuration data
     * @param config AppConfig object to populate with data
     */
    void jsonToAppConfig(const json& j, AppConfig& config);
    
    /**
     * Convert AppConfig structure to JSON object.
     * @param config AppConfig object to convert
     * @param j JSON object to populate with configuration data
     */
    void appConfigToJson(const AppConfig& config, json& j);
    
    /**
     * Convert JSON object to RenderConfig structure.
     * @param j JSON object containing configuration data
     * @param config RenderConfig object to populate with data
     */
    void jsonToRenderConfig(const json& j, RenderConfig& config);
    
    /**
     * Convert RenderConfig structure to JSON object.
     * @param config RenderConfig object to convert
     * @param j JSON object to populate with configuration data
     */
    void renderConfigToJson(const RenderConfig& config, json& j);
    
private:
    AppConfig appConfig;                     // Current application configuration
    RenderConfig renderConfig;               // Current rendering configuration
    std::string configBasePath = "config/";  // Base directory for config files
};