#include "TimeManager.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

TimeManager::TimeManager() : accumulator(0.0f), hour(8), minute(0) {
    // Start at 8:00 AM
}

void TimeManager::update(float dt) {
    accumulator += dt;

    // 1 real sec = 2 game mins
    const float SECONDS_PER_GAME_MINUTE = 0.5f; 

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
    float time = hour + (minute / 60.0f);

    float sunriseStart = 5.0f;  
    float sunriseEnd   = 9.0f;  
    float sunsetStart  = 17.0f; 
    float sunsetEnd    = 21.0f; 
    
    // === UPDATE: Increased minimum brightness from 0.3 to 0.5 ===
    float minBrightness = 0.5f; // Slightly brighter night
    float maxBrightness = 1.0f; 
    // ============================================================

    if (time < sunriseStart || time >= sunsetEnd) {
        return minBrightness;
    }
    
    if (time >= sunriseStart && time < sunriseEnd) {
        float t = (time - sunriseStart) / (sunriseEnd - sunriseStart);
        return minBrightness + (maxBrightness - minBrightness) * t;
    }

    if (time >= sunriseEnd && time < sunsetStart) {
        return maxBrightness;
    }

    if (time >= sunsetStart && time < sunsetEnd) {
        float t = (time - sunsetStart) / (sunsetEnd - sunsetStart);
        return maxBrightness - (maxBrightness - minBrightness) * t;
    }

    return minBrightness;
}
