#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <algorithm> // for std::clamp

struct Task {
    std::string id;          
    std::string description; 
    int expReward;
    int energyReward; 
    bool isCompleted;
};

class TaskManager {
public:
    // Start with 100.0 float Energy. Max Exp set to 100 for level 1.
    TaskManager() : currentExp(0), maxExp(100), currentEnergy(100.0f) {} 

    void addTask(const std::string& id, const std::string& desc, int exp, int energy) {
        tasks.push_back({id, desc, exp, energy, false});
    }

    void completeTask(const std::string& id) {
        for (auto& task : tasks) {
            if (task.id == id && !task.isCompleted) {
                task.isCompleted = true;
                
                // Apply Task Rewards
                currentExp += task.expReward;
                modifyEnergy(static_cast<float>(task.energyReward));

                std::cout << "[Quest] Completed: " << task.description << std::endl;
            }
        }
    }

    void modifyEnergy(float amount) {
        currentEnergy += amount;
        // Clamp between 0 and 100
        if (currentEnergy > 100.0f) currentEnergy = 100.0f;
        if (currentEnergy < 0.0f) currentEnergy = 0.0f;
    }

    const std::vector<Task>& getTasks() const { return tasks; }
    int getExp() const { return currentExp; }
    int getMaxExp() const { return maxExp; } // New Getter
    
    // Return int for UI display, but keep internal float precision
    int getEnergy() const { return static_cast<int>(currentEnergy); }

private:
    std::vector<Task> tasks;
    int currentExp;
    int maxExp; // New variable to track progress
    float currentEnergy;
};
