#include "TimeManager.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

TimeManager::TimeManager() : accumulator(0.0f), hour(8), minute(0) {
    // Start at 8:00 AM
}

void TimeManager::update(float dt) {
    accumulator += dt;

    // You mentioned you changed this to 1 sec = 2 mins.
    // I will use a variable here so it works with your change.
    const float SECONDS_PER_GAME_MINUTE = 0.5f; // 1 real sec = 2 game mins

    if (accumulator >= SECONDS_PER_GAME_MINUTE) {
        accumulator -= SECONDS_PER_GAME_MINUTE;
        minute++;
        
        if (minute >= 60) {
            minute = 0;
            hour++;
            if (hour >= 24) {
                hour = 0;
            }
        }
    }
}

std::string TimeManager::getFormattedTime() const {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    return std::string(buffer);
}

float TimeManager::getDaylightFactor() const {
    // Convert current time to a single float value (0.0 to 24.0)
    float time = hour + (minute / 60.0f);

    // === CONFIGURATION ===
    float sunriseStart = 5.0f;  // 5:00 AM
    float sunriseEnd   = 9.0f;  // 9:00 AM
    float sunsetStart  = 17.0f; // 5:00 PM
    float sunsetEnd    = 21.0f; // 9:00 PM
    
    float minBrightness = 0.3f; // Darkest night
    float maxBrightness = 1.0f; // Brightest day
    // =====================

    // 1. NIGHT (Before sunrise or After sunset end)
    if (time < sunriseStart || time >= sunsetEnd) {
        return minBrightness;
    }
    
    // 2. SUNRISE (Gradual brightening)
    if (time >= sunriseStart && time < sunriseEnd) {
        float t = (time - sunriseStart) / (sunriseEnd - sunriseStart);
        // Smoothstep formula for nicer transition (optional, linear is fine too)
        return minBrightness + (maxBrightness - minBrightness) * t;
    }

    // 3. DAY (Bright)
    if (time >= sunriseEnd && time < sunsetStart) {
        return maxBrightness;
    }

    // 4. SUNSET (Gradual darkening)
    if (time >= sunsetStart && time < sunsetEnd) {
        float t = (time - sunsetStart) / (sunsetEnd - sunsetStart);
        // Inverse the transition (1.0 -> 0.3)
        return maxBrightness - (maxBrightness - minBrightness) * t;
    }

    return minBrightness;
}