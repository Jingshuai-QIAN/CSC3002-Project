// Prevent multiple inclusions of this header file
#pragma once

#include <string>
#include <vector>
#include <sstream>

/*
 * File: StringUtils.h
 * Description: Lightweight string helper utilities used throughout the codebase.
 *
 * The functions here are deliberately simple and avoid dependencies on
 * additional libraries. They operate on std::string and return new strings.
 */
class StringUtils {
public:
    /**
     * @brief Splits a string into tokens based on a delimiter character.
     * 
     * @param str The input string to split.
     * @param delimiter The character used to separate tokens.
     * @return std::vector<std::string> A vector containing the split tokens.
     */
    static std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        
        // Extract tokens separated by delimiter
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        
        return tokens;
    }
    
    /**
     * @brief Checks if a string starts with the specified prefix.
     * 
     * @param str The string to check.
     * @param prefix The prefix to look for.
     * 
     * @return true if the string starts with the prefix.
     * @return false if the string does not start with the prefix.
     */
    static bool startsWith(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() && 
               str.compare(0, prefix.size(), prefix) == 0;
    }
    
    /**
     * @brief Checks if a string ends with the specified suffix.
     * 
     * @param str The string to check.
     * @param suffix The suffix to look for.
     * 
     * @return true if the string ends with the suffix.
     * @return false if the string does not end with the suffix.
     */
    static bool endsWith(const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() && 
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
    
    /**
     * @brief Removes whitespace characters from both ends of a string.
     * 
     * @param str The string to trim.
     * @return std::string The trimmed string with no leading/trailing whitespace.
     */
    static std::string trim(const std::string& str) {
        // Find first non-whitespace character
        size_t start = str.find_first_not_of(" \t\n\r");
        // Find last non-whitespace character
        size_t end = str.find_last_not_of(" \t\n\r");
        
        // If string contains only whitespace, return empty string
        if (start == std::string::npos) {
            return "";
        }
        
        // Return substring without leading/trailing whitespace
        return str.substr(start, end - start + 1);
    }
};