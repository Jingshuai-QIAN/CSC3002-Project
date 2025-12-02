#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <algorithm> // for std::clamp

struct Task {
    std::string id;          
    std::string description;        // Short description for list (e.g., "Eat Food")
    std::string detailedInstruction; // Long text for popup (e.g., "Go to Canteen...")
    std::string achievementName;    // Text for "Achievement Unlocked: XXX"
    int expReward;
    int energyReward; 
    bool isCompleted;
};

class TaskManager {
public:
    // Start with 100.0 float Energy. Max Exp set to 100 for level 1.
    TaskManager() : currentExp(0), maxExp(100), currentEnergy(100.0f) {} 

    // Updated addTask to include detailed instruction and achievement name
    void addTask(const std::string& id, const std::string& desc, const std::string& detail, const std::string& achieveName, int exp, int energy) {
        tasks.push_back({id, desc, detail, achieveName, exp, energy, false});
    }

    // Returns the Achievement Name if task was just completed, otherwise returns empty string
    std::string completeTask(const std::string& id) {
        for (auto& task : tasks) {
            if (task.id == id && !task.isCompleted) {
                task.isCompleted = true;
                
                // Apply Task Rewards
                currentExp += task.expReward;
                modifyEnergy(static_cast<float>(task.energyReward));

                std::cout << "[Quest] Completed: " << task.description << std::endl;
                return task.achievementName; // Return the name for the popup
            }
        }
        return ""; // Task already done or not found
    }

    void modifyEnergy(float amount) {
        currentEnergy += amount;
        // Clamp between 0 and 100
        if (currentEnergy > 100.0f) currentEnergy = 100.0f;
        if (currentEnergy < 0.0f) currentEnergy = 0.0f;
    }

    const std::vector<Task>& getTasks() const { return tasks; }
    int getExp() const { return currentExp; }
    int getMaxExp() const { return maxExp; }
    
    int getEnergy() const { return static_cast<int>(currentEnergy); }
    int getMaxEnergy() const { return 100; }

private:
    std::vector<Task> tasks;
    int currentExp;
    int maxExp; 
    float currentEnergy;
};
