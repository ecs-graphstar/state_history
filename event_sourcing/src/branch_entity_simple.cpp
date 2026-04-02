// Simple entity lifecycle test
#include "components.h"
#include "state_history.h"
#include <iostream>
#include <flecs.h>

int main() {
    std::cout << "=== Simple Branching History Test ===\n\n";

    flecs::world ecs;
    TimelineTree tree(&ecs, 5, false);
    register_all_components(tree);
    tree.setup_observers();

    std::cout << "Capturing frame 0 (no entities)...\n";
    tree.capture_state();

    std::cout << "Creating entity e1...\n";
    auto e1 = ecs.entity("e1");
    tree.track_entity(e1);
    e1.set<Position>({10.0, 20.0});

    std::cout << "Capturing frame 1...\n";
    tree.capture_state();

    e1.set<Position>({20.0, 30.0});
    std::cout << "Capturing frame 2...\n";
    tree.capture_state();

    // Create a branch off of root at frame 1 (one line)
    auto* branch = tree.branch(tree.root(), 1);

    // Roll to branch point state (frame 0 = branch point)
    tree.roll_to(branch, 0);

    e1 = ecs.lookup("e1");
    std::cout << e1.ensure<Position>().x << std::endl;

    e1.set<Position>({100.0, 200.0});
    std::cout << e1.ensure<Position>().x << std::endl;
    std::cout << "Capturing frame 2 in a divergent timeline...\n";
    tree.capture_state();

    // Roll back to root at frame 1
    tree.roll_to(tree.root(), 1);
    e1 = ecs.lookup("e1");
    std::cout << e1.ensure<Position>().x << std::endl;

    // Roll to branch at frame 1 (its first captured frame)
    tree.roll_to(branch, 1);
    e1 = ecs.lookup("e1");
    std::cout << e1.ensure<Position>().x << std::endl;

    return 0;
}
