// Prevent multiple inclusions of this header file
#pragma once

#include <string>
#include <iostream>

/**
 * @file Logger.h
 * @brief Minimal logging helpers used across the project.
 *
 * Provides simple static functions for printing informational, warning,
 * error and debug messages to the console. This small facility is intended
 * for development and debugging; it is not a full-featured logging framework.
 *
 * Notes:
 * - Debug logging is enabled by defining the DEBUG macro at compile time.
 */
class Logger {
public:
    /**
     * @brief Logs an informational message to standard output.
     * 
     * @param message The message to log.
     */
    static void info(const std::string& message) {
        std::cout << "[INFO] " << message << std::endl;
    }
    
    /**
     * @brief Logs an error message to standard error stream.
     * 
     * @param message The error message to log.
     */
    static void error(const std::string& message) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
    
    /**
     * @brief Logs a warning message to standard output.
     * 
     * @param message The warning message to log.
     */
    static void warn(const std::string& message) {
        std::cout << "[WARN] " << message << std::endl;
    }
    
    /**
     * @brief Logs a debug message to standard output only in debug builds.
     * 
     * This method only outputs messages when the DEBUG preprocessor macro is defined,
     * preventing debug logs from appearing in release builds.
     * 
     * @param message The debug message to log.
     */
    static void debug(const std::string& message) {
// Only compile debug logging code if DEBUG macro is defined
#ifdef DEBUG
        std::cout << "[DEBUG] " << message << std::endl;
#endif
    }
};