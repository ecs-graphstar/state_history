// Simple entity lifecycle test
#include "components.h"
#include "state_history.h"
#include <iostream>

int main() {
    std::cout << "=== Simple Entity Test ===\n\n";

    flecs::world ecs;
    StateHistory history(&ecs, 5, false);
    history.setup_observers();

    std::cout << "Capturing frame 0 (no entities)...\n";
    history.capture_state();

    std::cout << "Creating entity e1...\n";
    auto e1 = ecs.entity("e1");
    history.track_entity(e1);
    e1.set<Position>({10.0, 20.0});
    ecs.progress();

    std::cout << "Capturing frame 1...\n";
    history.capture_state();

    std::cout << "e1 ID: " << e1.id() << ", name: " << e1.name() << "\n";
    std::cout << "e1 is alive: " << e1.is_alive() << "\n";

    std::cout << "\nTrying rollback to frame 0 (entity should be destroyed)...\n";
    history.rollback_to(0);

    std::cout << "After rollback:\n";
    auto e1_check = ecs.lookup("e1");
    std::cout << "e1_check is valid: " << e1_check.is_valid() << "\n";
    if (e1_check.is_valid()) {
        std::cout << "e1_check is alive: " << e1_check.is_alive() << "\n";
    }

    if (e1_check.is_valid() && e1_check.is_alive()) {
        std::cout << "ERROR: e1 should not exist after rollback to frame 0!\n";
        return 1;
    }

    std::cout << "\n=== Test Complete ===\n";
    return 0;
}
