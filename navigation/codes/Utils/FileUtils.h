// Prevent multiple inclusions of this header file
#pragma once

#include <string>

/**
 * @file FileUtils.h
 * @brief Small helpers for manipulating file paths and extracting components.
 *
 * This header contains only a couple of convenience functions used by the
 * configuration and resource loading code. The functions operate on strings
 * and do not perform filesystem IO.
 */
class FileUtils {
public:
    /**
     * @brief Extracts the file extension from a file path.
     * 
     * @param filepath The full path to the file.
     * @return std::string The file extension without the dot, or empty string if no extension found.
     */
    static std::string getFileExtension(const std::string& filepath) {
        // Find the last occurrence of dot in the file path
        size_t dotPos = filepath.find_last_of(".");

        // If dot is found and it's not at the beginning/end
        if (dotPos != std::string::npos) {
            // Return substring after the dot (the extension)
            return filepath.substr(dotPos + 1);
        }

        // Return empty string if no extension found
        return "";
    }
    
    /**
     * @brief Extracts the file name from a full file path.
     * 
     * @param filepath The full path to the file.
     * @return std::string The file name with extension, or original path if no directory separators found.
     */
    static std::string getFileName(const std::string& filepath) {
        // Find the last occurrence of directory separator (both Unix '/' and Windows '\')
        size_t slashPos = filepath.find_last_of("/\\");

        // If directory separator is found
        if (slashPos != std::string::npos) {
            // Return substring after the last separator (the file name)
            return filepath.substr(slashPos + 1);
        }
        
        // Return original path if no directory separators found (already just file name)
        return filepath;
    }
};