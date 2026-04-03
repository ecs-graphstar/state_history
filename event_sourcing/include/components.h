#pragma once

#include "serialize.h"

class StateHistory;
void register_all_components(StateHistory& history);

struct Position {
    double x, y;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_double(stream, x);
        serialize_double(stream, y);
        return true;
    }
};

struct SpawnID {
    uint64_t id;
};

struct Velocity {
    double x, y;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_double(stream, x);
        serialize_double(stream, y);
        return true;
    }
};

struct Health {
    float value;
    float max_value;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_float(stream, value);
        serialize_float(stream, max_value);
        return true;
    }
};

struct Damage {
    float amount;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_float(stream, amount);
        return true;
    }
};

struct Armor {
    int defense;
    float durability;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_int(stream, defense, -1000000, 1000000); // Approximate min/max values needed
        serialize_float(stream, durability);
        return true;
    }
};

struct Zen {
    float current;
    float maximum;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_float(stream, current);
        serialize_float(stream, maximum);
        return true;
    }
};

struct AttackPower {
    float damage;
    float range;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_float(stream, damage);
        serialize_float(stream, range);
        return true;
    }
};

struct MovementSpeed {
    float speed;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_float(stream, speed);
        return true;
    }
};

struct Experience {
    int level;
    int xp;

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_int(stream, level, 0, 1000000);
        serialize_int(stream, xp, 0, 2000000000); // Using standard int size range here safely
        return true;
    }
};