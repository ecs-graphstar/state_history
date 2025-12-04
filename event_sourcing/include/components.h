#pragma once

// All component definitions for the ECS system
// These will be automatically detected by generate_state_history.py

struct Position {
    double x, y;
};

struct Velocity {
    double x, y;
};

struct Health {
    float value;
    float max_value;
};

struct Damage {
    float amount;
};

// Example: Adding a new component is as simple as defining a struct here
// CMake will automatically regenerate state_history.h when you run make
struct Armor {
    int defense;
    float durability;
};

struct Zen {
    float current;
    float maximum;
};

// Combat-related components
struct AttackPower {
    float damage;
    float range;
};

struct MovementSpeed {
    float speed;
};

struct Experience {
    int level;
    int xp;
};
