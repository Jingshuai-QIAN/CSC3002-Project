#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <functional>

struct Task {
    std::string id;
    std::string description;
    std::string detailedInstruction; 
    std::string achievementName;     
    int pointsReward;
    int energyReward;
    bool achievementUnlocked; 
};

class TaskManager {
public:
    TaskManager() : currentPoints(0), currentEnergy(100.0f) {} // Energy starts as float 100.0

    void addTask(const std::string& id, const std::string& desc, const std::string& detail, const std::string& achName, int points, int energy) {
        tasks.push_back({id, desc, detail, achName, points, energy, false});
    }

    // Returns achievement name ONLY if it hasn't been unlocked yet
    std::string completeTask(const std::string& id) {
        for (auto& task : tasks) {
            if (task.id == id) {
                // Apply Rewards as float
                currentPoints += task.pointsReward;
                currentEnergy += static_cast<float>(task.energyReward); 
                
                // Cap Energy
                if(currentEnergy > 100.0f) currentEnergy = 100.0f;
                if(currentEnergy < 0.0f) currentEnergy = 0.0f;

                std::cout << "[Task] Completed: " << task.description << std::endl;

                // Check Achievement (One-time)
                if (!task.achievementUnlocked) {
                    task.achievementUnlocked = true;
                    return task.achievementName;
                }
                return ""; 
            }
        }
        return "";
    }

    const std::vector<Task>& getTasks() const { return tasks; }
    
    // Getters
    long long getPoints() const { return currentPoints; } 
    
    // Daily Goal is 500 
    long long getDailyGoal() const { return 500; } 
    
    // Return int for UI display, but keep internal float
    int getEnergy() const { return static_cast<int>(currentEnergy); }
    int getMaxEnergy() const { return 100; }

    // Accept small float values without rounding to 0 
    void modifyEnergy(float amount) {
        currentEnergy += amount;
        if (currentEnergy < 0.0f) currentEnergy = 0.0f;
        if (currentEnergy > 100.0f) currentEnergy = 100.0f;
    }


private:
    std::vector<Task> tasks;
    long long currentPoints; 
    float currentEnergy; // Changed from int to float to support passive decay
};


