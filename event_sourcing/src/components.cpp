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