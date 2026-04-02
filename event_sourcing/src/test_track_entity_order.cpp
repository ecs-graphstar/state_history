// Regression test for relationship tracking bug
// Bug: Dynamic relationships added BEFORE track_entity() weren't being captured
// Fix: track_entity() now snapshots existing relationships when entity is first tracked

#include "components.h"
#include "state_history.h"
#include <iostream>

struct Attacking {};

int main() {
    flecs::world ecs;
    StateHistory history(&ecs, 100, true);
    register_all_components(history);
    history.setup_observers();

    auto DynamicRel = ecs.entity("DynamicRel");
    auto target = ecs.entity("Target");
    history.track_entity(target);

    history.capture_state();  // Frame 0

    std::cout << "=== Testing relationship tracking with different orderings ===\n\n";

    // Case 1: Track entity BEFORE adding relationship (always worked)
    std::cout << "Case 1: Track entity BEFORE adding relationship\n";
    auto entity1 = ecs.entity("Entity1");
    history.track_entity(entity1);  // Track first
    entity1.add(DynamicRel, target);  // Then add relationship

    history.capture_state();  // Frame 1
    std::cout << "  ✓ Relationship added and will be tracked\n\n";

    // Case 2: Track entity AFTER adding relationship (this was the bug)
    std::cout << "Case 2: Track entity AFTER adding relationship (BUG CASE)\n";
    auto entity2 = ecs.entity("Entity2");
    entity2.add(DynamicRel, target);  // Add relationship first
    history.track_entity(entity2);  // Then track

    history.capture_state();  // Frame 2
    std::cout << "  ✓ Relationship was added before tracking, but should still be captured by fix\n\n";

    // Verify both relationships exist
    std::cout << "=== Verifying relationships exist ===\n";
    std::cout << "Entity1 has relationship: " << (entity1.has(DynamicRel, target) ? "YES" : "NO") << "\n";
    std::cout << "Entity2 has relationship: " << (entity2.has(DynamicRel, target) ? "YES" : "NO") << "\n\n";

    // Test rollback to ensure both are restored
    std::cout << "=== Testing rollback/restore ===\n";
    entity1.remove(DynamicRel, target);
    entity2.remove(DynamicRel, target);
    std::cout << "Removed both relationships\n";

    history.rollback_to(2);
    std::cout << "Rolled back to frame 2\n\n";

    auto e1 = ecs.lookup("Entity1");
    auto e2 = ecs.lookup("Entity2");
    auto tgt = ecs.lookup("Target");

    bool e1_has = e1.has(DynamicRel, tgt);
    bool e2_has = e2.has(DynamicRel, tgt);

    std::cout << "After rollback:\n";
    std::cout << "  Entity1 has relationship: " << (e1_has ? "YES ✓" : "NO ✗") << "\n";
    std::cout << "  Entity2 has relationship: " << (e2_has ? "YES ✓" : "NO ✗") << "\n\n";

    // Report results
    if (e1_has && e2_has) {
        std::cout << "=== SUCCESS: Bug is FIXED ===\n";
        std::cout << "Both relationships correctly tracked and restored,\n";
        std::cout << "regardless of track_entity() call timing.\n";
        history.recording_enabled = false;
        return 0;
    } else {
        std::cout << "=== FAILURE: Bug still present ===\n";
        if (!e1_has) std::cout << "ERROR: Entity1 relationship not restored\n";
        if (!e2_has) std::cout << "ERROR: Entity2 relationship not restored (track after add)\n";
        history.recording_enabled = false;
        return 1;
    }
}
