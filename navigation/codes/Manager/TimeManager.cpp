#include "TimeManager.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <vector>

TimeManager::TimeManager() 
    : accumulator(0.0f), hour(8), minute(0), 
      month(9), day(1), weekday(0) { // Starts 9/1 (Monday)
}

void TimeManager::update(float dt) {
    accumulator += dt;

    if (accumulator >= SECONDS_PER_GAME_MINUTE) {
        accumulator -= SECONDS_PER_GAME_MINUTE;
        minute++;
        
        if (minute >= 60) {
            minute = 0;
            hour++;
            if (hour >= 24) {
                hour = 0;
                
                // === New Date Logic ===
                weekday = (weekday + 1) % 7;
                day++;
                
                // Days in months (Index 0 is unused, 1=Jan, etc.)
                // 9=Sep(30), 10=Oct(31), 11=Nov(30), 12=Dec(31)
                static const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                
                if (day > daysInMonth[month]) {
                    day = 1;
                    month++;
                    if (month > 12) month = 1;
                }
            }
        }
    }
}

std::string TimeManager::getFormattedTime() const {
    static const char* wdayStr[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    
    char buffer[32];
    // Format: "Mon 9/01 08:00"
    std::snprintf(buffer, sizeof(buffer), "%s %d/%02d %02d:%02d", 
                  wdayStr[weekday], month, day, hour, minute);
    return std::string(buffer);
}

float TimeManager::getDaylightFactor() const {
    float time = hour + (minute / 60.0f);

    float sunriseStart = 5.0f;
    float sunriseEnd   = 9.0f;
    float sunsetStart  = 17.0f;
    float sunsetEnd    = 21.0f;
    
    float minBrightness = 0.3f;
    float maxBrightness = 1.0f;

    if (time < sunriseStart || time >= sunsetEnd) return minBrightness;
    
    if (time >= sunriseStart && time < sunriseEnd) {
        float t = (time - sunriseStart) / (sunriseEnd - sunriseStart);
        return minBrightness + (maxBrightness - minBrightness) * t;
    }

    if (time >= sunriseEnd && time < sunsetStart) return maxBrightness;

    if (time >= sunsetStart && time < sunsetEnd) {
        float t = (time - sunsetStart) / (sunsetEnd - sunsetStart);
        return maxBrightness - (maxBrightness - minBrightness) * t;
    }

    return minBrightness;
}

void TimeManager::addHours(int hours) {
    hour += hours;
    
    // Handle day overflow
    while (hour >= 24) {
        hour -= 24;
        weekday = (weekday + 1) % 7;
        day++;
        
        // Days in months (Index 0 is unused, 1=Jan, etc.)
        static const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        
        if (day > daysInMonth[month]) {
            day = 1;
            month++;
            if (month > 12) month = 1;
        }
    }
}
