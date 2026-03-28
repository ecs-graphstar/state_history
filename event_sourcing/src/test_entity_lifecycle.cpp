// Component definitions must be included before state_history.h
#include "components.h"
#include "state_history.h"
#include <cassert>
#include <iostream>

// Test utilities
void assert_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ASSERTION FAILED: " << message << "\n";
        std::exit(1);
    }
}

void assert_entity_exists(flecs::world& ecs, const char* name, bool should_exist) {
    auto e = ecs.lookup(name);
    bool exists = e.is_valid() && e.is_alive();

    if (should_exist && !exists) {
        std::cerr << "Entity '" << name << "' should exist but doesn't\n";
        std::exit(1);
    } else if (!should_exist && exists) {
        std::cerr << "Entity '" << name << "' should not exist but does\n";
        std::exit(1);
    }
}

int main() {
    std::cout << "=== Entity Lifecycle Test Harness ===\n\n";

    flecs::world ecs;
    StateHistory history(&ecs, 5, false);  // Keyframe every 5 frames, no compression
    register_all_components(history);
    history.setup_observers();

    std::cout << "Test 1: Entity Creation\n";
    std::cout << "------------------------\n";

    history.capture_state();  // Frame 0 (keyframe) - no entities

    std::cout << "Frame 1: Creating entity e1\n";
    auto e1 = ecs.entity("e1");
    history.track_entity(e1);
    e1.set<Position>({10.0, 20.0});
    ecs.progress();
    history.capture_state();  // Frame 1

    assert_entity_exists(ecs, "e1", true);
    assert_true(e1.has<Position>(), "e1 should have Position");
    std::cout << "  ✓ Entity e1 created and tracked\n";

    std::cout << "\nTest 2: Multiple Entity Creation\n";
    std::cout << "----------------------------------\n";

    std::cout << "Frame 2: Creating entities e2 and e3\n";
    auto e2 = ecs.entity("e2");
    history.track_entity(e2);
    e2.set<Position>({30.0, 40.0});
    e2.set<Velocity>({1.0, 2.0});

    auto e3 = ecs.entity("e3");
    history.track_entity(e3);
    e3.set<Health>({100.0});

    ecs.progress();
    history.capture_state();  // Frame 2

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e2", true);
    assert_entity_exists(ecs, "e3", true);
    std::cout << "  ✓ Multiple entities created\n";

    std::cout << "\nTest 3: Entity Destruction\n";
    std::cout << "----------------------------\n";

    std::cout << "Frame 3: Destroying entity e2\n";
    history.untrack_entity(e2);
    e2.destruct();
    ecs.progress();
    history.capture_state();  // Frame 3

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e2", false);
    assert_entity_exists(ecs, "e3", true);
    std::cout << "  ✓ Entity e2 destroyed\n";

    std::cout << "\nTest 4: More Operations\n";
    std::cout << "------------------------\n";

    std::cout << "Frame 4: Modifying e1, creating e4\n";
    e1.set<Position>({50.0, 60.0});

    auto e4 = ecs.entity("e4");
    history.track_entity(e4);
    e4.set<Position>({100.0, 200.0});
    e4.set<Velocity>({5.0, 10.0});

    ecs.progress();
    history.capture_state();  // Frame 4

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e3", true);
    assert_entity_exists(ecs, "e4", true);
    std::cout << "  ✓ e1 modified, e4 created\n";

    std::cout << "\nTest 5: Keyframe with Entity State\n";
    std::cout << "------------------------------------\n";

    std::cout << "Frame 5: Keyframe should capture all entities\n";
    e1.set<Velocity>({2.0, 3.0});
    ecs.progress();
    history.capture_state();  // Frame 5 (keyframe)

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e3", true);
    assert_entity_exists(ecs, "e4", true);
    std::cout << "  ✓ Keyframe captured with entities\n";

    std::cout << "\nTest 6: More Entity Operations\n";
    std::cout << "--------------------------------\n";

    std::cout << "Frame 6: Destroying e3, creating e5\n";
    history.untrack_entity(e3);
    e3.destruct();

    auto e5 = ecs.entity("e5");
    history.track_entity(e5);
    e5.set<Health>({75.0});

    ecs.progress();
    history.capture_state();  // Frame 6

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e3", false);
    assert_entity_exists(ecs, "e4", true);
    assert_entity_exists(ecs, "e5", true);
    std::cout << "  ✓ e3 destroyed, e5 created\n";

    std::cout << "\nTest 7: Rollback to Frame 3\n";
    std::cout << "-----------------------------\n";
    std::cout << "Expected state: e1, e2 destroyed, e3 exists, no e4/e5\n";

    history.rollback_to(3);

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e2", false);  // e2 was destroyed at frame 3
    assert_entity_exists(ecs, "e3", true);
    assert_entity_exists(ecs, "e4", false);  // e4 created at frame 4
    assert_entity_exists(ecs, "e5", false);  // e5 created at frame 6

    e1 = ecs.lookup("e1");
    const Position* pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 10.0 && pos->y == 20.0, "e1 position at frame 3");

    std::cout << "  ✓ Rollback to frame 3 successful!\n";
    std::cout << "    e1 exists with Position(10, 20)\n";
    std::cout << "    e2 is destroyed\n";
    std::cout << "    e3 exists\n";
    std::cout << "    e4 doesn't exist yet\n";
    std::cout << "    e5 doesn't exist yet\n";

    std::cout << "\nTest 8: Rollback to Frame 1\n";
    std::cout << "-----------------------------\n";
    std::cout << "Expected state: Only e1 exists\n";

    history.rollback_to(1);

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e2", false);
    assert_entity_exists(ecs, "e3", false);
    assert_entity_exists(ecs, "e4", false);
    assert_entity_exists(ecs, "e5", false);

    std::cout << "  ✓ Rollback to frame 1 successful!\n";
    std::cout << "    Only e1 exists\n";

    std::cout << "\nTest 9: Roll Forward to Frame 4\n";
    std::cout << "---------------------------------\n";
    std::cout << "Expected state: e1, e3, e4 exist (e2 destroyed)\n";

    history.roll_forward(4);

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e2", false);
    assert_entity_exists(ecs, "e3", true);
    assert_entity_exists(ecs, "e4", true);
    assert_entity_exists(ecs, "e5", false);

    e1 = ecs.lookup("e1");
    pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 50.0 && pos->y == 60.0, "e1 position at frame 4");

    std::cout << "  ✓ Roll forward to frame 4 successful!\n";
    std::cout << "    e1, e3, e4 exist\n";
    std::cout << "    e2 is destroyed\n";
    std::cout << "    e5 doesn't exist yet\n";

    std::cout << "\nTest 10: Roll Forward to Frame 6\n";
    std::cout << "----------------------------------\n";
    std::cout << "Expected state: e1, e4, e5 exist (e2, e3 destroyed)\n";

    history.roll_forward(6);

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e2", false);
    assert_entity_exists(ecs, "e3", false);
    assert_entity_exists(ecs, "e4", true);
    assert_entity_exists(ecs, "e5", true);

    std::cout << "  ✓ Roll forward to frame 6 successful!\n";
    std::cout << "    e1, e4, e5 exist\n";
    std::cout << "    e2, e3 are destroyed\n";

    std::cout << "\nTest 11: Rollback to Frame 0\n";
    std::cout << "------------------------------\n";
    std::cout << "Expected state: No entities\n";

    history.rollback_to(0);

    assert_entity_exists(ecs, "e1", false);
    assert_entity_exists(ecs, "e2", false);
    assert_entity_exists(ecs, "e3", false);
    assert_entity_exists(ecs, "e4", false);
    assert_entity_exists(ecs, "e5", false);

    std::cout << "  ✓ Rollback to frame 0 successful!\n";
    std::cout << "    No entities exist\n";

    std::cout << "\nTest 12: Roll Forward to Frame 2\n";
    std::cout << "----------------------------------\n";
    std::cout << "Expected state: e1, e2, e3 exist\n";

    history.roll_forward(2);

    assert_entity_exists(ecs, "e1", true);
    assert_entity_exists(ecs, "e2", true);
    assert_entity_exists(ecs, "e3", true);
    assert_entity_exists(ecs, "e4", false);
    assert_entity_exists(ecs, "e5", false);

    e1 = ecs.lookup("e1");
    auto e2_check = ecs.lookup("e2");
    auto e3_check = ecs.lookup("e3");

    assert_true(e1.has<Position>(), "e1 has Position");
    assert_true(e2_check.has<Position>(), "e2 has Position");
    assert_true(e2_check.has<Velocity>(), "e2 has Velocity");
    assert_true(e3_check.has<Health>(), "e3 has Health");

    std::cout << "  ✓ Roll forward to frame 2 successful!\n";
    std::cout << "    e1, e2, e3 exist with correct components\n";

    std::cout << "\n=== All Tests Passed! ===\n";
    std::cout << "\nEntity Lifecycle Features Verified:\n";
    std::cout << "  ✓ Entity creation tracked\n";
    std::cout << "  ✓ Entity destruction tracked\n";
    std::cout << "  ✓ Entity state preserved in keyframes\n";
    std::cout << "  ✓ Rollback destroys entities created after target frame\n";
    std::cout << "  ✓ Rollback recreates entities destroyed after target frame\n";
    std::cout << "  ✓ Roll forward creates entities\n";
    std::cout << "  ✓ Roll forward destroys entities\n";
    std::cout << "  ✓ Multiple rollback/roll forward cycles\n";

    history.print_stats();

    return 0;
}
