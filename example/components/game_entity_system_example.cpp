/*
 * game_entity_system_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Game Entity System Example
Demonstrates Entity-Component-System (ECS) pattern using the Atom component
framework. Shows how to create game entities with different components like
position, health, rendering, and AI behavior.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <random>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/components/lifecycle.hpp"

using namespace atom::components;

/**
 * @brief Position component for game entities
 */
class PositionComponent : public Component {
public:
    explicit PositionComponent(const std::string& name) : Component(name) {
        std::cout << "PositionComponent '" << name << "' created" << std::endl;
        
        // Initialize position variables
        addVariable<double>("x", 0.0);
        addVariable<double>("y", 0.0);
        addVariable<double>("z", 0.0);
        
        // Add movement commands
        def("move", [this](double dx, double dy, double dz) {
            auto x = getVariable<double>("x");
            auto y = getVariable<double>("y");
            auto z = getVariable<double>("z");
            
            if (x && y && z) {
                setValue("x", x->get() + dx);
                setValue("y", y->get() + dy);
                setValue("z", z->get() + dz);
                std::cout << "  Moved to (" << x->get() + dx << ", " 
                          << y->get() + dy << ", " << z->get() + dz << ")" << std::endl;
            }
        });
        
        def("setPosition", [this](double newX, double newY, double newZ) {
            setValue("x", newX);
            setValue("y", newY);
            setValue("z", newZ);
            std::cout << "  Position set to (" << newX << ", " << newY << ", " << newZ << ")" << std::endl;
        });
        
        def("getDistance", [this](double targetX, double targetY, double targetZ) -> double {
            auto x = getVariable<double>("x");
            auto y = getVariable<double>("y");
            auto z = getVariable<double>("z");
            
            if (x && y && z) {
                double dx = x->get() - targetX;
                double dy = y->get() - targetY;
                double dz = z->get() - targetZ;
                return std::sqrt(dx*dx + dy*dy + dz*dz);
            }
            return 0.0;
        });
    }
};

/**
 * @brief Health component for game entities
 */
class HealthComponent : public Component {
public:
    explicit HealthComponent(const std::string& name) : Component(name) {
        std::cout << "HealthComponent '" << name << "' created" << std::endl;
        
        // Initialize health variables
        addVariable<int>("health", 100);
        addVariable<int>("maxHealth", 100);
        addVariable<bool>("alive", true);
        
        // Add health management commands
        def("takeDamage", [this](int damage) {
            auto health = getVariable<int>("health");
            if (health) {
                int newHealth = std::max(0, health->get() - damage);
                setValue("health", newHealth);
                setValue("alive", newHealth > 0);
                std::cout << "  Took " << damage << " damage, health: " << newHealth << std::endl;
                
                if (newHealth <= 0) {
                    std::cout << "  Entity died!" << std::endl;
                }
            }
        });
        
        def("heal", [this](int amount) {
            auto health = getVariable<int>("health");
            auto maxHealth = getVariable<int>("maxHealth");
            if (health && maxHealth) {
                int newHealth = std::min(maxHealth->get(), health->get() + amount);
                setValue("health", newHealth);
                std::cout << "  Healed " << amount << " points, health: " << newHealth << std::endl;
            }
        });
        
        def("isAlive", [this]() -> bool {
            auto alive = getVariable<bool>("alive");
            return alive ? alive->get() : false;
        });
    }
};

/**
 * @brief AI Behavior component for game entities
 */
class AIComponent : public Component {
public:
    explicit AIComponent(const std::string& name) : Component(name) {
        std::cout << "AIComponent '" << name << "' created" << std::endl;
        
        // Initialize AI variables
        addVariable<std::string>("behavior", "idle");
        addVariable<double>("aggroRange", 10.0);
        addVariable<double>("patrolRadius", 5.0);
        addVariable<int>("intelligence", 50);
        
        // Add AI behavior commands
        def("setBehavior", [this](const std::string& behavior) {
            setValue("behavior", behavior);
            std::cout << "  AI behavior set to: " << behavior << std::endl;
        });
        
        def("update", [this]() {
            auto behavior = getVariable<std::string>("behavior");
            if (behavior) {
                std::cout << "  AI updating with behavior: " << behavior->get() << std::endl;
                
                // Simple behavior simulation
                if (behavior->get() == "patrol") {
                    std::cout << "    Patrolling area..." << std::endl;
                } else if (behavior->get() == "chase") {
                    std::cout << "    Chasing target..." << std::endl;
                } else if (behavior->get() == "attack") {
                    std::cout << "    Attacking target!" << std::endl;
                } else {
                    std::cout << "    Idling..." << std::endl;
                }
            }
        });
    }
};

/**
 * @brief Game Entity that combines multiple components
 */
class GameEntity {
private:
    std::string entityId_;
    std::vector<std::shared_ptr<Component>> components_;
    
public:
    explicit GameEntity(const std::string& id) : entityId_(id) {
        std::cout << "GameEntity '" << id << "' created" << std::endl;
    }
    
    template<typename T>
    void addComponent(const std::string& componentName) {
        auto& registry = Registry::instance();
        auto component = registry.createComponent<T>(entityId_ + "_" + componentName);
        components_.push_back(component);
        std::cout << "  Added " << componentName << " to entity " << entityId_ << std::endl;
    }
    
    template<typename T>
    std::shared_ptr<T> getComponent() {
        for (auto& comp : components_) {
            if (auto typed = std::dynamic_pointer_cast<T>(comp)) {
                return typed;
            }
        }
        return nullptr;
    }
    
    void update() {
        std::cout << "Updating entity: " << entityId_ << std::endl;
        for (auto& comp : components_) {
            if (comp->hasCommand("update")) {
                comp->executeCommand("update", {});
            }
        }
    }
    
    const std::string& getId() const { return entityId_; }
};

/**
 * @brief Simple Game World that manages entities
 */
class GameWorld {
private:
    std::vector<std::unique_ptr<GameEntity>> entities_;
    std::mt19937 rng_;
    
public:
    GameWorld() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
        std::cout << "GameWorld created" << std::endl;
    }
    
    GameEntity* createEntity(const std::string& id) {
        auto entity = std::make_unique<GameEntity>(id);
        GameEntity* ptr = entity.get();
        entities_.push_back(std::move(entity));
        return ptr;
    }
    
    void simulateGameLoop(int iterations) {
        std::cout << "\n=== Starting Game Simulation ===" << std::endl;
        
        for (int i = 0; i < iterations; ++i) {
            std::cout << "\n--- Game Tick " << (i + 1) << " ---" << std::endl;
            
            // Update all entities
            for (auto& entity : entities_) {
                entity->update();
            }
            
            // Simulate some random events
            simulateRandomEvents();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    
private:
    void simulateRandomEvents() {
        if (entities_.empty()) return;
        
        std::uniform_int_distribution<> entityDist(0, entities_.size() - 1);
        std::uniform_int_distribution<> eventDist(0, 3);
        
        auto& entity = entities_[entityDist(rng_)];
        int event = eventDist(rng_);
        
        switch (event) {
            case 0: // Random movement
                if (auto pos = entity->getComponent<PositionComponent>()) {
                    std::uniform_real_distribution<> moveDist(-2.0, 2.0);
                    pos->executeCommand("move", {std::to_string(moveDist(rng_)), 
                                               std::to_string(moveDist(rng_)), "0"});
                }
                break;
            case 1: // Random damage
                if (auto health = entity->getComponent<HealthComponent>()) {
                    std::uniform_int_distribution<> damageDist(5, 20);
                    health->executeCommand("takeDamage", {std::to_string(damageDist(rng_))});
                }
                break;
            case 2: // Random healing
                if (auto health = entity->getComponent<HealthComponent>()) {
                    std::uniform_int_distribution<> healDist(10, 25);
                    health->executeCommand("heal", {std::to_string(healDist(rng_))});
                }
                break;
            case 3: // Change AI behavior
                if (auto ai = entity->getComponent<AIComponent>()) {
                    std::vector<std::string> behaviors = {"idle", "patrol", "chase", "attack"};
                    std::uniform_int_distribution<> behaviorDist(0, behaviors.size() - 1);
                    ai->executeCommand("setBehavior", {behaviors[behaviorDist(rng_)]});
                }
                break;
        }
    }
};

int main() {
    std::cout << "=== Atom Component Game Entity System Example ===" << std::endl;
    
    try {
        // Create game world
        GameWorld world;
        
        // Create player entity
        auto* player = world.createEntity("Player");
        player->addComponent<PositionComponent>("Position");
        player->addComponent<HealthComponent>("Health");
        
        // Create NPC entities
        auto* npc1 = world.createEntity("Guard");
        npc1->addComponent<PositionComponent>("Position");
        npc1->addComponent<HealthComponent>("Health");
        npc1->addComponent<AIComponent>("AI");
        
        auto* npc2 = world.createEntity("Merchant");
        npc2->addComponent<PositionComponent>("Position");
        npc2->addComponent<HealthComponent>("Health");
        npc2->addComponent<AIComponent>("AI");
        
        // Set initial positions
        if (auto pos = player->getComponent<PositionComponent>()) {
            pos->executeCommand("setPosition", {"0", "0", "0"});
        }
        
        if (auto pos = npc1->getComponent<PositionComponent>()) {
            pos->executeCommand("setPosition", {"10", "5", "0"});
        }
        
        if (auto pos = npc2->getComponent<PositionComponent>()) {
            pos->executeCommand("setPosition", {"-5", "8", "0"});
        }
        
        // Run game simulation
        world.simulateGameLoop(5);
        
        std::cout << "\n=== Game Entity System Example Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
