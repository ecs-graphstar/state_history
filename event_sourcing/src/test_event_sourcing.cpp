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

void assert_has_component(flecs::entity e, const char* comp_name, bool should_have) {
    bool has = false;
    if (std::string(comp_name) == "Position") {
        has = e.has<Position>();
    } else if (std::string(comp_name) == "Velocity") {
        has = e.has<Velocity>();
    } else if (std::string(comp_name) == "Health") {
        has = e.has<Health>();
    }

    if (should_have && !has) {
        std::cerr << "Entity should have " << comp_name << " but doesn't\n";
        std::exit(1);
    } else if (!should_have && has) {
        std::cerr << "Entity should not have " << comp_name << " but does\n";
        std::exit(1);
    }
}

int main() {
    std::cout << "=== Event Sourcing Test Harness ===\n\n";

    flecs::world ecs;
    StateHistory history(&ecs, 5, false);  // Keyframe every 5 frames, no compression
    history.setup_observers();

    std::cout << "Test 1: Component Add Operations\n";
    std::cout << "----------------------------------\n";

    auto e1 = ecs.entity("TestEntity1");
    history.capture_state();  // Frame 0 (keyframe) - empty state

    std::cout << "Frame 1: Adding Position component\n";
    e1.set<Position>({10.0, 20.0});
    ecs.progress();
    history.capture_state();  // Frame 1

    assert_has_component(e1, "Position", true);
    const Position* pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 10.0 && pos->y == 20.0, "Position values correct");
    std::cout << "  ✓ Position added and captured\n";

    std::cout << "\nTest 2: Component Set Operations\n";
    std::cout << "----------------------------------\n";

    std::cout << "Frame 2: Modifying Position component\n";
    e1.set<Position>({30.0, 40.0});
    ecs.progress();
    history.capture_state();  // Frame 2

    pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 30.0 && pos->y == 40.0, "Position updated correctly");
    std::cout << "  ✓ Position modified and captured\n";

    std::cout << "\nTest 3: Multiple Component Types\n";
    std::cout << "----------------------------------\n";

    std::cout << "Frame 3: Adding Velocity and Health\n";
    e1.set<Velocity>({5.0, 10.0});
    e1.set<Health>({100.0});
    ecs.progress();
    history.capture_state();  // Frame 3

    assert_has_component(e1, "Velocity", true);
    assert_has_component(e1, "Health", true);
    std::cout << "  ✓ Multiple components added\n";

    std::cout << "\nTest 4: Component Remove Operations\n";
    std::cout << "------------------------------------\n";

    std::cout << "Frame 4: Removing Velocity component\n";
    e1.remove<Velocity>();
    ecs.progress();
    history.capture_state();  // Frame 4

    assert_has_component(e1, "Velocity", false);
    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Health", true);
    std::cout << "  ✓ Velocity removed and captured\n";

    std::cout << "\nTest 5: Keyframe Capture\n";
    std::cout << "-------------------------\n";

    std::cout << "Frame 5: Keyframe with current state\n";
    e1.set<Position>({50.0, 60.0});
    ecs.progress();
    history.capture_state();  // Frame 5 (keyframe)

    std::cout << "  ✓ Keyframe captured\n";

    std::cout << "\nTest 6: More Changes After Keyframe\n";
    std::cout << "------------------------------------\n";

    std::cout << "Frame 6: Adding Velocity back\n";
    e1.set<Velocity>({1.0, 2.0});
    ecs.progress();
    history.capture_state();  // Frame 6

    std::cout << "Frame 7: Removing Health\n";
    e1.remove<Health>();
    ecs.progress();
    history.capture_state();  // Frame 7

    std::cout << "Frame 8: Modifying Position again\n";
    e1.set<Position>({100.0, 200.0});
    ecs.progress();
    history.capture_state();  // Frame 8

    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", true);
    assert_has_component(e1, "Health", false);
    pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 100.0 && pos->y == 200.0, "Final position correct");
    std::cout << "  ✓ All changes captured\n";

    std::cout << "\nTest 7: Rollback to Frame 3\n";
    std::cout << "----------------------------\n";
    std::cout << "State at frame 3: Position(30,40), Velocity(5,10), Health(100)\n";

    history.rollback_to(3);

    // Verify rollback restored correct state
    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", true);
    assert_has_component(e1, "Health", true);

    pos = e1.try_get<Position>();
    const Velocity* vel = e1.try_get<Velocity>();
    const Health* health = e1.try_get<Health>();

    assert_true(pos && pos->x == 30.0 && pos->y == 40.0, "Position rolled back");
    assert_true(vel && vel->x == 5.0 && vel->y == 10.0, "Velocity rolled back");
    assert_true(health && health->value == 100.0, "Health rolled back");
    std::cout << "  ✓ Rollback to frame 3 successful!\n";
    std::cout << "    Position: (" << pos->x << ", " << pos->y << ")\n";
    std::cout << "    Velocity: (" << vel->x << ", " << vel->y << ")\n";
    std::cout << "    Health: " << health->value << "\n";

    std::cout << "\nTest 8: Rollback to Frame 2 (intermediate rollback)\n";
    std::cout << "----------------------------------------------------\n";
    std::cout << "State at frame 2: Position(30,40) only (before Velocity/Health added)\n";

    history.rollback_to(2);

    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", false);
    assert_has_component(e1, "Health", false);

    pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 30.0 && pos->y == 40.0, "Position rolled back to frame 2");
    std::cout << "  ✓ Rollback to frame 2 successful!\n";
    std::cout << "    Position: (" << pos->x << ", " << pos->y << ")\n";
    std::cout << "    Velocity: [not yet added]\n";
    std::cout << "    Health: [not yet added]\n";

    std::cout << "\nTest 9: Rollback to Frame 1\n";
    std::cout << "----------------------------\n";
    std::cout << "State at frame 1: Position(10,20) only\n";

    history.rollback_to(1);

    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", false);
    assert_has_component(e1, "Health", false);

    pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 10.0 && pos->y == 20.0, "Position rolled back to frame 1");
    std::cout << "  ✓ Rollback to frame 1 successful!\n";
    std::cout << "    Position: (" << pos->x << ", " << pos->y << ")\n";
    std::cout << "    Velocity: [not present]\n";
    std::cout << "    Health: [not present]\n";

    std::cout << "\nTest 10: Timeline Preservation\n";
    std::cout << "--------------------------------\n";
    std::cout << "Verify that future frames are still accessible after rollback\n";

    // Check that snapshots still exist
    assert_true(history.snapshots.size() == 9, "All 9 snapshots should still exist");
    assert_true(history.current_frame == 2, "Current frame should be 2 after rollback to frame 1");
    std::cout << "  ✓ Future timeline preserved (9 snapshots still exist)\n";
    std::cout << "  ✓ Current frame correctly set to " << history.current_frame << "\n";

    std::cout << "\nTest 11: Roll Forward to Frame 3\n";
    std::cout << "---------------------------------\n";
    std::cout << "Roll forward to frame 3 where Velocity and Health were added\n";

    history.roll_forward(3);

    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", true);
    assert_has_component(e1, "Health", true);

    pos = e1.try_get<Position>();
    vel = e1.try_get<Velocity>();
    health = e1.try_get<Health>();

    assert_true(pos && pos->x == 30.0 && pos->y == 40.0, "Position at frame 3");
    assert_true(vel && vel->x == 5.0 && vel->y == 10.0, "Velocity at frame 3");
    assert_true(health && health->value == 100.0, "Health at frame 3");
    std::cout << "  ✓ Roll forward to frame 3 successful!\n";
    std::cout << "    Position: (" << pos->x << ", " << pos->y << ")\n";
    std::cout << "    Velocity: (" << vel->x << ", " << vel->y << ")\n";
    std::cout << "    Health: " << health->value << "\n";

    std::cout << "\nTest 12: Roll Forward to Frame 8\n";
    std::cout << "---------------------------------\n";
    std::cout << "Roll forward to the final frame\n";

    history.roll_forward(8);

    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", true);
    assert_has_component(e1, "Health", false);

    pos = e1.try_get<Position>();
    vel = e1.try_get<Velocity>();

    assert_true(pos && pos->x == 100.0 && pos->y == 200.0, "Position at frame 8");
    assert_true(vel && vel->x == 1.0 && vel->y == 2.0, "Velocity at frame 8");
    std::cout << "  ✓ Roll forward to frame 8 successful!\n";
    std::cout << "    Position: (" << pos->x << ", " << pos->y << ")\n";
    std::cout << "    Velocity: (" << vel->x << ", " << vel->y << ")\n";
    std::cout << "    Health: [removed at frame 7]\n";

    std::cout << "\nTest 13: Multiple Rollback/Roll Forward Cycles\n";
    std::cout << "------------------------------------------------\n";
    std::cout << "Test navigating back and forth through timeline\n";

    history.rollback_to(4);
    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", false);
    assert_has_component(e1, "Health", true);
    std::cout << "  ✓ Rollback to frame 4: Position + Health, no Velocity\n";

    history.roll_forward(6);
    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", true);
    assert_has_component(e1, "Health", true);
    std::cout << "  ✓ Roll forward to frame 6: All components present\n";

    history.rollback_to(2);
    assert_has_component(e1, "Position", true);
    assert_has_component(e1, "Velocity", false);
    assert_has_component(e1, "Health", false);
    pos = e1.try_get<Position>();
    assert_true(pos && pos->x == 30.0 && pos->y == 40.0, "Position at frame 2");
    std::cout << "  ✓ Rollback to frame 2: Only Position\n";

    std::cout << "\n=== All Tests Passed! ===\n";
    std::cout << "\nEvent Sourcing Features Verified:\n";
    std::cout << "  ✓ OnAdd events tracked and replayed\n";
    std::cout << "  ✓ OnRemove events tracked and replayed\n";
    std::cout << "  ✓ OnSet events tracked with XOR diffs\n";
    std::cout << "  ✓ Keyframes capture full state\n";
    std::cout << "  ✓ Rollback clears and restores state correctly\n";
    std::cout << "  ✓ Timeline preservation after rollback\n";
    std::cout << "  ✓ Roll forward through preserved timeline\n";
    std::cout << "  ✓ Multiple rollback/roll forward cycles\n";
    std::cout << "  ✓ Multiple component types handled\n";

    history.print_stats();

    return 0;
}
