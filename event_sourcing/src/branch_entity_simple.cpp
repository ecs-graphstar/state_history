// Simple entity lifecycle test
#include "components.h"
#include "state_history.h"
#include <iostream>
#include <flecs.h>

int main() {
    std::cout << "=== Simple Branching History Test ===\n\n";

    flecs::world* ecs = new flecs::world();
    StateHistory* root_history = new StateHistory(ecs, 5, false);
    register_all_components(*root_history);
    root_history->setup_observers();
    TimelineNode root_timeline = {root_history, 0, nullptr, {}, 0};
    
    TimelineTree tree(&root_timeline);

    std::cout << "Capturing frame 0 (no entities)...\n";
    root_timeline.history->capture_state();

    std::cout << "Creating entity e1...\n";
    auto e1 = root_timeline.history->world->entity("e1");
    root_timeline.history->track_entity(e1);
    e1.set<Position>({10.0, 20.0});
    // root_timeline.history->world->progress();

    std::cout << "Capturing frame 1...\n";
    root_timeline.history->capture_state();

    e1.set<Position>({20.0, 30.0});
    std::cout << "Capturing frame 2...\n";
    root_timeline.history->capture_state();

    // Now say we want to create a branch off of root_timeline
    // starting at the 1st frame..
    // We use the same ECS world for now
    StateHistory* branch_history = new StateHistory(ecs, 5, false);
    branch_history->tracked_entities = root_history->tracked_entities;
    branch_history->entity_names = root_history->entity_names;
    branch_history->tracked_component_ids = root_history->tracked_component_ids;
    branch_history->tracked_component_sizes = root_history->tracked_component_sizes;
    register_all_components(*branch_history);
    branch_history->setup_observers();
    TimelineNode* divergent_timeline = tree.create_branch(&root_timeline, 1);

    // root_timeline.history->rollback_to(1);
    tree.roll_to(divergent_timeline, 0);

    e1 = ecs->lookup("e1");
    std::cout << e1.ensure<Position>().x << std::endl;

    e1.set<Position>({100.0, 200.0});
    std::cout << e1.ensure<Position>().x << std::endl;
    std::cout << "Capturing frame 2 in a divergent timeline...\n";
    divergent_timeline->history->capture_state();

    tree.roll_to(&root_timeline, 1);
    e1 = ecs->lookup("e1");
    std::cout << e1.ensure<Position>().x << std::endl;
    // TODO: We need to figure out how StateHistory can be expanded efficiently 

    tree.roll_to(divergent_timeline, 1);
    e1 = ecs->lookup("e1");
    std::cout << e1.ensure<Position>().x << std::endl;

    return 0;
}
