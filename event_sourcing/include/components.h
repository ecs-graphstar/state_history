#pragma once

class StateHistory;
void register_all_components(StateHistory& history);

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

struct Armor {
    int defense;
    float durability;
};

struct Zen {
    float current;
    float maximum;
};

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