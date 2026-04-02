#include "components.h"
#include "state_history.h"

void register_all_components(StateHistory& history) {
    history.register_component<Position>();
    history.register_component<Velocity>();
    history.register_component<Health>();
    history.register_component<Damage>();
    history.register_component<Armor>();
    history.register_component<Zen>();
    history.register_component<AttackPower>();
    history.register_component<MovementSpeed>();
    history.register_component<Experience>();
}

void register_all_components(TimelineTree& tree) {
    tree.register_component<Position>();
    tree.register_component<Velocity>();
    tree.register_component<Health>();
    tree.register_component<Damage>();
    tree.register_component<Armor>();
    tree.register_component<Zen>();
    tree.register_component<AttackPower>();
    tree.register_component<MovementSpeed>();
    tree.register_component<Experience>();
}