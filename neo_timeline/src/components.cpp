#include "components.h"
#include "state_history.h"

void register_all_components(StateHistory& history) {
    history.register_component<Position>();
    history.register_component<Velocity>();
    history.register_component<Health>();
    history.register_component<RenderColor>();
    history.register_component<FoodSprite>();
}