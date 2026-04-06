#include "components.h"
#include "state_history.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <flecs.h>

using namespace std::chrono;

int main() {
    spdlog::set_level(spdlog::level::off); 
    flecs::world ecs;

    // Fixed simulation step for deterministic branching
    const float SIM_DELTA = 1.0f / 60.0f; 
    TimelineTree tree(&ecs, 10, true);
    register_all_components(tree);
    tree.setup_observers();

    // PHYSICS SYSTEM: Standard Newtonian Integration
    ecs.system<Position, const Velocity>()
        .each([](flecs::iter& it, size_t i, Position& p, const Velocity& v) {
            p.x += v.x * it.delta_time();
            p.y += v.y * it.delta_time();
        });

    // 1. INITIALIZE THE SWARM (100 Entities)
    std::vector<flecs::entity> swarm;
    for (int i = 0; i < 1000; ++i) {
        auto e = ecs.entity() // Unnamed for speed, or use BFO naming
            .set<Position>({(float)i, 0.0f})
            .set<Velocity>({0.0f, 0.0f})
            .set<Mass>({1.0f});
        tree.track_entity(e);
        swarm.push_back(e);
    }

    tree.capture_state(); // Frame 0: The "Static" swarm

    const int NUM_TIMELINES = 1000;
    std::vector<TimelineNode*> branches;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> vel_rand(-10.0f, 10.0f);
    std::uniform_int_distribution<int> entity_rand(0, 99);

    auto start_bench = high_resolution_clock::now();

    // 2. THE BRANCHING PHASE
    for (int i = 0; i < NUM_TIMELINES; ++i) {
        auto* branch = tree.branch(tree.root(), 0);
        tree.roll_to(branch, 0);

        // Perturb only 3 "Key Entities" to simulate an intervention
        for (int j = 0; j < 3; ++j) {
            int target_idx = entity_rand(rng);
            swarm[target_idx].set<Velocity>({vel_rand(rng), vel_rand(rng)});
        }

        // Simulate 60 ticks (1 second of world time)
        for(int f = 0; f < 60; ++f) {
            ecs.progress(SIM_DELTA);
            tree.capture_state();
        }
        branches.push_back(branch);
    }

    auto end_bench = high_resolution_clock::now();
    auto total_ms = duration_cast<milliseconds>(end_bench - start_bench).count();

    // 3. ANALYSIS
    std::cout << "=== Pauphos Peach Swarm Profiling ===\n";
    std::cout << "Entities per World: 100\n";
    std::cout << "Total Entity States Processed: " << NUM_TIMELINES * 60 * 100 << "\n";
    std::cout << "Real Execution Time: " << total_ms << "ms\n";
    std::cout << "Speedup Factor: " << (double)(NUM_TIMELINES * 1000) / total_ms << "x\n";
    std::cout << "=====================================\n";

    return 0;
}