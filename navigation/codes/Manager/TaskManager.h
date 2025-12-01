#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <functional>

struct Task {
    std::string id;          // Unique ID (e.g., "eat_breakfast")
    std::string description; // Display text (e.g., "Eat Breakfast at Canteen")
    int expReward;
    int energyReward;
    bool isCompleted;
};

class TaskManager {
public:
    TaskManager() : currentExp(0), currentEnergy(100) {}

    // Add a task to the list
    void addTask(const std::string& id, const std::string& desc, int exp, int energy) {
        tasks.push_back({id, desc, exp, energy, false});
    }

    // Call this when an event happens (e.g., eating)
    void completeTask(const std::string& id) {
        for (auto& task : tasks) {
            if (task.id == id && !task.isCompleted) {
                task.isCompleted = true;
                
                // Apply Rewards
                currentExp += task.expReward;
                currentEnergy += task.energyReward;
                if(currentEnergy > 100) currentEnergy = 100;

                std::cout << "[Quest] Completed: " << task.description << std::endl;
                std::cout << "        Rewards: Exp+" << task.expReward << ", Energy+" << task.energyReward << std::endl;
            }
        }
    }

    // Getters for UI
    const std::vector<Task>& getTasks() const { return tasks; }
    int getExp() const { return currentExp; }
    int getEnergy() const { return currentEnergy; }

    // Manual energy modification (e.g., walking costs energy)
    void modifyEnergy(int amount) {
        currentEnergy += amount;
        if (currentEnergy < 0) currentEnergy = 0;
        if (currentEnergy > 100) currentEnergy = 100;
    }

private:
    std::vector<Task> tasks;
    int currentExp;
    int currentEnergy;
};