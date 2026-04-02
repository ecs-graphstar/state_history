#include "components.h"
#include "state_history.h"
#include <cassert>
#include <iostream>
#include <map>

struct Likes {};

void assert_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ASSERTION FAILED: " << message << "\n";
        std::exit(1);
    }
}

struct WorldSnapshot {
    std::map<std::string, Position> positions;
    std::map<std::string, Velocity> velocities;
    std::map<std::string, std::vector<std::string>> relationships;
};

WorldSnapshot capture_world(flecs::world& ecs) {
    WorldSnapshot snap;
    ecs.query_builder<Position>().build().each([&](flecs::entity e, Position& p) {
        const char* n = e.name();
        if (n) snap.positions[std::string(n)] = p;
    });
    ecs.query_builder<Velocity>().build().each([&](flecs::entity e, Velocity& v) {
        const char* n = e.name();
        if (n) snap.velocities[std::string(n)] = v;
    });
    auto likes_id = ecs.component<Likes>().id();
    ecs.query_builder<>().with(likes_id, flecs::Wildcard).build().each([&](flecs::entity e) {
        const char* n = e.name();
        if (!n) return;
        std::string name(n);
        e.each([&](flecs::id id) {
            if (id.is_pair() && id.first().id() == likes_id) {
                auto tgt = id.second();
                const char* tn = tgt.name();
                if (tn) {
                    snap.relationships[name].push_back(std::string(tn));
                }
            }
        });
    });
    return snap;
}

bool compare_snapshots(const WorldSnapshot& a, const WorldSnapshot& b, int frame) {
    bool ok = true;
    if (a.positions.size() != b.positions.size()) {
        std::cerr << "Frame " << frame << ": position count mismatch: "
                  << a.positions.size() << " vs " << b.positions.size() << "\n";
        ok = false;
    }
    for (auto& [name, pa] : a.positions) {
        auto it = b.positions.find(name);
        if (it == b.positions.end()) {
            std::cerr << "Frame " << frame << ": entity " << name << " missing in step_forward result\n";
            ok = false;
        } else if (pa.x != it->second.x || pa.y != it->second.y) {
            std::cerr << "Frame " << frame << ": " << name << " position mismatch: ("
                      << pa.x << "," << pa.y << ") vs (" << it->second.x << "," << it->second.y << ")\n";
            ok = false;
        }
    }
    for (auto& [name, va] : a.velocities) {
        auto it = b.velocities.find(name);
        if (it == b.velocities.end()) {
            std::cerr << "Frame " << frame << ": entity " << name << " velocity missing in step_forward\n";
            ok = false;
        } else if (va.x != it->second.x || va.y != it->second.y) {
            std::cerr << "Frame " << frame << ": " << name << " velocity mismatch\n";
            ok = false;
        }
    }
    for (auto& [name, rels_a] : a.relationships) {
        auto it = b.relationships.find(name);
        if (it == b.relationships.end()) {
            std::cerr << "Frame " << frame << ": entity " << name << " relationships missing in step_forward\n";
            ok = false;
        } else {
            auto sorted_a = rels_a;
            auto sorted_b = it->second;
            std::sort(sorted_a.begin(), sorted_a.end());
            std::sort(sorted_b.begin(), sorted_b.end());
            if (sorted_a != sorted_b) {
                std::cerr << "Frame " << frame << ": " << name << " relationship mismatch\n";
                ok = false;
            }
        }
    }
    return ok;
}

int main() {
    spdlog::set_level(spdlog::level::off);

    std::cout << "=== step_forward vs roll_forward Equivalence Test ===\n\n";

    const int KEYFRAME_INTERVAL = 5;
    const int TOTAL_FRAMES = 30;

    flecs::world ecs;
    StateHistory history(&ecs, KEYFRAME_INTERVAL, true);
    history.setup_observers();
    register_all_components(history);
    ecs.component<Likes>();

    // Create entities with C API to avoid flecs 4 set<> template issues
    auto alice = ecs.entity("Alice");
    auto bob = ecs.entity("Bob");
    auto carol = ecs.entity("Carol");

    Position pa = {0, 0}, pb = {10, 10}, pc = {5, 5};
    Velocity va = {1, 2}, vb = {-1, 0}, vc = {0, 1};

    ecs_set_id(ecs.c_ptr(), alice.id(), ecs.component<Position>().id(), sizeof(Position), &pa);
    ecs_set_id(ecs.c_ptr(), alice.id(), ecs.component<Velocity>().id(), sizeof(Velocity), &va);
    ecs_set_id(ecs.c_ptr(), bob.id(), ecs.component<Position>().id(), sizeof(Position), &pb);
    ecs_set_id(ecs.c_ptr(), bob.id(), ecs.component<Velocity>().id(), sizeof(Velocity), &vb);
    ecs_set_id(ecs.c_ptr(), carol.id(), ecs.component<Position>().id(), sizeof(Position), &pc);
    ecs_set_id(ecs.c_ptr(), carol.id(), ecs.component<Velocity>().id(), sizeof(Velocity), &vc);

    history.track_entity(alice);
    history.track_entity(bob);
    history.track_entity(carol);

    auto pos_id = ecs.component<Position>().id();
    auto vel_id = ecs.component<Velocity>().id();

    for (int i = 0; i < TOTAL_FRAMES; i++) {
        // Update positions based on velocities
        {
            auto* p = static_cast<Position*>(ecs_ensure_id(ecs.c_ptr(), alice.id(), pos_id, sizeof(Position)));
            auto* v = static_cast<const Velocity*>(ecs_get_id(ecs.c_ptr(), alice.id(), vel_id));
            p->x += v->x; p->y += v->y;
            ecs_modified_id(ecs.c_ptr(), alice.id(), pos_id);
        }
        {
            auto* p = static_cast<Position*>(ecs_ensure_id(ecs.c_ptr(), bob.id(), pos_id, sizeof(Position)));
            auto* v = static_cast<const Velocity*>(ecs_get_id(ecs.c_ptr(), bob.id(), vel_id));
            p->x += v->x; p->y += v->y;
            ecs_modified_id(ecs.c_ptr(), bob.id(), pos_id);
        }
        {
            auto* p = static_cast<Position*>(ecs_ensure_id(ecs.c_ptr(), carol.id(), pos_id, sizeof(Position)));
            auto* v = static_cast<const Velocity*>(ecs_get_id(ecs.c_ptr(), carol.id(), vel_id));
            p->x += v->x; p->y += v->y;
            ecs_modified_id(ecs.c_ptr(), carol.id(), pos_id);
        }

        // Add/remove relationships at specific frames
        if (i == 3) alice.add<Likes>(bob);
        if (i == 7) bob.add<Likes>(carol);
        if (i == 12) alice.remove<Likes>(bob);
        if (i == 15) alice.add<Likes>(carol);
        if (i == 20) carol.add<Likes>(alice);
        if (i == 25) bob.remove<Likes>(carol);

        // Change velocity mid-simulation
        if (i == 10) {
            Velocity new_v = {-2, 1};
            ecs_set_id(ecs.c_ptr(), alice.id(), vel_id, sizeof(Velocity), &new_v);
        }
        if (i == 18) {
            Velocity new_v = {3, -1};
            ecs_set_id(ecs.c_ptr(), bob.id(), vel_id, sizeof(Velocity), &new_v);
        }

        history.capture_state();
    }

    std::cout << "Recorded " << TOTAL_FRAMES << " frames (keyframe interval: " << KEYFRAME_INTERVAL << ")\n\n";

    // Phase 1: Collect ground truth using roll_forward
    std::cout << "Phase 1: Collecting ground truth via roll_forward...\n";
    std::vector<WorldSnapshot> ground_truth;
    ground_truth.reserve(TOTAL_FRAMES);

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        if (frame == 0) {
            history.rollback_to(0);
        } else {
            history.roll_forward(frame);
        }
        ground_truth.push_back(capture_world(ecs));
    }
    std::cout << "  Collected " << ground_truth.size() << " snapshots\n";

    // Phase 2: Verify step_forward produces identical results
    std::cout << "Phase 2: Verifying step_forward equivalence...\n";
    history.rollback_to(0);

    int mismatches = 0;
    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        if (frame > 0) {
            history.step_forward();
        }
        auto snap = capture_world(ecs);
        if (!compare_snapshots(ground_truth[frame], snap, frame)) {
            mismatches++;
        }
    }

    if (mismatches == 0) {
        std::cout << "  All " << TOTAL_FRAMES << " frames match!\n\n";
    } else {
        std::cerr << "  " << mismatches << " frames had mismatches!\n";
        return 1;
    }

    // Phase 3: Second sequential scan (verify repeatability)
    std::cout << "Phase 3: Second sequential scan...\n";
    history.rollback_to(0);

    for (int frame = 0; frame < TOTAL_FRAMES; frame++) {
        if (frame > 0) {
            history.step_forward();
        }
        auto snap = capture_world(ecs);
        if (!compare_snapshots(ground_truth[frame], snap, frame)) {
            mismatches++;
        }
    }

    if (mismatches == 0) {
        std::cout << "  Second scan matches!\n\n";
    } else {
        std::cerr << "  " << mismatches << " frames had mismatches on second scan!\n";
        return 1;
    }

    // Phase 4: Verify normal operation after step_forward scan
    std::cout << "Phase 4: Verify rollback_to still works after scan...\n";
    history.rollback_to(10);
    auto snap_10 = capture_world(ecs);
    if (!compare_snapshots(ground_truth[10], snap_10, 10)) {
        std::cerr << "  rollback_to(10) failed after step_forward scan!\n";
        return 1;
    }
    std::cout << "  rollback_to(10) correct after scan\n\n";

    std::cout << "=== All Tests Passed! ===\n\n";
    std::cout << "step_forward Features Verified:\n";
    std::cout << "  - Produces identical world state as roll_forward for all frames\n";
    std::cout << "  - Correctly handles keyframe boundaries (every " << KEYFRAME_INTERVAL << " frames)\n";
    std::cout << "  - Handles component modifications (XOR diffs)\n";
    std::cout << "  - Handles relationship add/remove across frames\n";
    std::cout << "  - Repeatable across multiple sequential scans\n";
    std::cout << "  - Normal rollback_to works correctly after scan\n";

    return 0;
}
