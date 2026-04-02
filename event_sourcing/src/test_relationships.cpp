// Component definitions must be included before state_history.h
#include "components.h"
#include "state_history.h"
#include <cassert>
#include <iostream>

// Define some relationship types
struct Likes {};
struct Knows {};
struct Owns {};
struct DreamsOf {};

// Test utilities
void assert_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ASSERTION FAILED: " << message << "\n";
        std::exit(1);
    }
}

void assert_has_relationship(flecs::entity e, flecs::entity_t rel, flecs::entity target, bool should_have) {
    bool has = e.has(rel, target);

    if (should_have && !has) {
        std::cerr << "Entity " << e.name() << " should have relationship to " << target.name() << " but doesn't\n";
        std::exit(1);
    } else if (!should_have && has) {
        std::cerr << "Entity " << e.name() << " should not have relationship to " << target.name() << " but does\n";
        std::exit(1);
    }
}

int main() {
    std::cout << "=== Relationship Tracking Test ===\n\n";

    flecs::world ecs;
    StateHistory history(&ecs, 5, false);  // Keyframe every 5 frames, no compression
    register_all_components(history);
    history.setup_observers();

    // Register relationship types
    ecs.component<Likes>();
    ecs.component<Knows>();
    ecs.component<Owns>();
    ecs.component<DreamsOf>();

    std::cout << "Test 1: Basic Relationship Tracking\n";
    std::cout << "-------------------------------------\n";

    history.capture_state();  // Frame 0 (keyframe) - no entities

    std::cout << "Frame 1: Creating entities\n";
    auto alice = ecs.entity("alice");
    history.track_entity(alice);
    auto bob = ecs.entity("bob");
    history.track_entity(bob);
    auto item = ecs.entity("item");
    history.track_entity(item);
    alice.add<DreamsOf>(bob);
    
    ecs.progress();
    history.capture_state();  // Frame 1
    
    std::cout << "Frame 2: Adding relationships\n";
    alice.add<Likes>(bob);
    alice.add<Knows>(bob);
    bob.add<Owns>(item);

    ecs.progress();
    history.capture_state();  // Frame 2

    assert_true(alice.has<Likes>(bob), "alice should like bob");
    assert_true(alice.has<Knows>(bob), "alice should know bob");
    assert_true(bob.has<Owns>(item), "bob should own item");
    std::cout << "  ✓ Relationships added\n";

    std::cout << "\nTest 2: Relationship Removal\n";
    std::cout << "-----------------------------\n";

    std::cout << "Frame 3: Removing relationship\n";
    alice.remove<Likes>(bob);

    ecs.progress();
    history.capture_state();  // Frame 3

    assert_true(!alice.has<Likes>(bob), "alice should not like bob anymore");
    assert_true(alice.has<Knows>(bob), "alice should still know bob");
    std::cout << "  ✓ Relationship removed\n";

    std::cout << "\nTest 3: More Relationship Changes\n";
    std::cout << "-----------------------------------\n";

    std::cout << "Frame 4: Adding new relationships\n";
    bob.add<Likes>(alice);
    alice.add<Owns>(item);

    ecs.progress();
    history.capture_state();  // Frame 4

    std::cout << "Frame 5: Keyframe with relationships\n";
    alice.add<Likes>(bob);  // Re-add the relationship

    ecs.progress();
    history.capture_state();  // Frame 5 (keyframe)

    assert_true(alice.has<Likes>(bob), "alice should like bob again");
    assert_true(bob.has<Likes>(alice), "bob should like alice");
    assert_true(alice.has<Owns>(item), "alice should own item");
    std::cout << "  ✓ Keyframe captured with relationships\n";

    std::cout << "\nTest 4: Rollback to Frame 2\n";
    std::cout << "-----------------------------\n";
    std::cout << "Expected: alice likes/knows bob, bob owns item\n";

    history.rollback_to(2);

    alice = ecs.lookup("alice");
    bob = ecs.lookup("bob");
    item = ecs.lookup("item");

    assert_true(alice.has<Likes>(bob), "alice should like bob at frame 2");
    assert_true(alice.has<Knows>(bob), "alice should know bob at frame 2");
    assert_true(bob.has<Owns>(item), "bob should own item at frame 2");
    assert_true(!bob.has<Likes>(alice), "bob should not like alice at frame 2");
    assert_true(!alice.has<Owns>(item), "alice should not own item at frame 2");

    std::cout << "  ✓ Rollback to frame 2 successful!\n";
    std::cout << "    Relationships correctly restored\n";

    std::cout << "\nTest 5: Rollback to Frame 1\n";
    std::cout << "-----------------------------\n";
    std::cout << "Expected: No relationships\n";

    history.rollback_to(1);

    alice = ecs.lookup("alice");
    bob = ecs.lookup("bob");
    item = ecs.lookup("item");

    assert_true(!alice.has<Likes>(bob), "alice should not like bob at frame 1");
    assert_true(!alice.has<Knows>(bob), "alice should not know bob at frame 1");
    assert_true(!bob.has<Owns>(item), "bob should not own item at frame 1");

    std::cout << "  ✓ Rollback to frame 1 successful!\n";
    std::cout << "    All relationships correctly cleared\n";

    std::cout << "\nTest 6: Roll Forward to Frame 3\n";
    std::cout << "---------------------------------\n";

    history.roll_forward(3);

    alice = ecs.lookup("alice");
    bob = ecs.lookup("bob");
    item = ecs.lookup("item");

    assert_true(!alice.has<Likes>(bob), "alice should not like bob at frame 3");
    assert_true(alice.has<Knows>(bob), "alice should know bob at frame 3");
    assert_true(bob.has<Owns>(item), "bob should own item at frame 3");

    std::cout << "  ✓ Roll forward to frame 3 successful!\n";
    std::cout << "    Relationships correctly applied\n";

    std::cout << "\nTest 7: Roll Forward to Frame 5\n";
    std::cout << "---------------------------------\n";

    history.roll_forward(5);

    alice = ecs.lookup("alice");
    bob = ecs.lookup("bob");
    item = ecs.lookup("item");

    assert_true(alice.has<Likes>(bob), "alice should like bob at frame 5");
    assert_true(alice.has<Knows>(bob), "alice should know bob at frame 5");
    assert_true(bob.has<Likes>(alice), "bob should like alice at frame 5");
    assert_true(alice.has<Owns>(item), "alice should own item at frame 5");
    assert_true(bob.has<Owns>(item), "bob should own item at frame 5");

    std::cout << "  ✓ Roll forward to frame 5 successful!\n";
    std::cout << "    All relationships correctly restored from keyframe\n";

    std::cout << "\n=== All Tests Passed! ===\n";
    std::cout << "\nRelationship Tracking Features Verified:\n";
    std::cout << "  ✓ Relationship additions tracked\n";
    std::cout << "  ✓ Relationship removals tracked\n";
    std::cout << "  ✓ Relationships preserved in keyframes\n";
    std::cout << "  ✓ Rollback restores relationships correctly\n";
    std::cout << "  ✓ Roll forward applies relationship changes\n";
    std::cout << "  ✓ Multiple relationships between entities supported\n";

    history.print_stats();

    return 0;
}
