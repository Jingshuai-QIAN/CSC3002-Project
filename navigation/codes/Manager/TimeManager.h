#pragma once
#include <string>

class TimeManager {
public:
    TimeManager();

    // Updates time based on delta time
    void update(float dt);

    // Returns formatted string "Mon 9/1 08:00"
    std::string getFormattedTime() const;
    
    // Returns 0.0 (Darkest Night) to 1.0 (Full Daylight)
    float getDaylightFactor() const; 

    int getHour() const { return hour; }

private:
    float accumulator;
    int hour;
    int minute;
    
    // === Date System Variables ===
    int month;
    int day;
    int weekday; // 0=Mon, 1=Tue, ... 6=Sun
    
    // 1 real second = 2 game minutes
    const float SECONDS_PER_GAME_MINUTE = 0.5f; 
};
