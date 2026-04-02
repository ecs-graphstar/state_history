// Component definitions must be included before state_history.h
#include "components.h"
#include "state_history.h"
#include <random>
#include <cmath>
#include <iostream>

struct Attacking {};

int main(int, char *[]) {
    spdlog::set_level(spdlog::level::debug);
    flecs::world ecs;

    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<> dis(0.0, 1.0);

    // Use a small keyframe interval for testing
    StateHistory history(&ecs, 100, true);
    register_all_components(history);
    history.setup_observers();

    // Define tag components
    auto Dead = ecs.entity("Dead");
    auto Fleeing = ecs.entity("Fleeing");
    auto InCombat = ecs.entity("InCombat");
    auto Soldier = ecs.entity("Soldier");
    auto Enemy = ecs.entity("Enemy");
    
    // Define relationship types
    auto AlliedWith = ecs.entity("AlliedWith");
    auto Following = ecs.entity("Following");
    auto Target = ecs.entity("Target");
    auto AfraidOf = ecs.entity("AfraidOf");
    auto TestRel = ecs.entity("TestRel");

    // System: Apply damage in combat
    ecs.system<Health, const AttackPower>("CombatSystem")
        .with<Attacking>(flecs::Wildcard)
        .without(Dead)
        .each([&](flecs::iter& it, size_t i, Health& h, const AttackPower& atk) {
            auto attacker = it.entity(i);

            // Find who we're attacking
            attacker.each<Attacking>([&](flecs::entity target) {
                if (target.has<Health>() && !target.has(Dead)) {
                    auto& target_health = target.ensure<Health>();
                    target_health.value -= atk.damage * 0.1f;  // Deal 10% damage per frame

                    if (target_health.value <= 0) {
                        target_health.value = 0;
                        target.add(Dead);

                        // Remove all attacking relationships when target dies
                        auto rel_query = ecs.query_builder()
                            .with<Attacking>(target)
                            .build();
                        rel_query.each([&](flecs::entity e) {
                            e.remove<Attacking>(target);
                        });

                        std::cout << "  [DEATH] " << target.name() << " was killed by " << attacker.name() << "!\n";
                    }
                }
            });
        });

    // System: Movement
    ecs.system<Position, const Velocity>("MovementSystem")
        .without(Dead)
        .each([](flecs::entity e, Position& p, const Velocity& v) {
            p.x += v.x;
            p.y += v.y;
        });

    // System: Flee when health is low
    ecs.system<Health, Velocity>("FleeSystem")
        .without(Dead)
        .without(Fleeing)
        .each([&](flecs::entity e, Health& h, Velocity& v) {
            if (h.value < h.max_value * 0.3f) {
                e.add(Fleeing);
                // Increase movement speed when fleeing
                v.x *= 1.5;
                v.y *= 1.5;

                // Stop attacking when fleeing
                auto rel_query = ecs.query_builder()
                    .with<Attacking>(flecs::Wildcard)
                    .build();
                rel_query.each([&](flecs::entity attacker) {
                    if (attacker == e) {
                        attacker.each<Attacking>([&](flecs::entity target) {
                            attacker.remove<Attacking>(target);
                            std::cout << "  [FLEE] " << e.name() << " is fleeing!\n";
                        });
                    }
                });
            }
        });

    std::cout << "=== COMBAT SIMULATION ===\n\n";
    std::cout << "Creating combatants...\n";

    // Create soldier squad
    auto leader = ecs.entity("Leader");
    leader.add(Soldier);
    leader.set<Position>({0, 0});
    leader.set<Velocity>({1.0, 0.5});
    leader.set<Health>({100.0f, 100.0f});
    leader.set<AttackPower>({15.0f, 10.0f});
    leader.set<MovementSpeed>({2.0f});
    leader.set<Experience>({5, 0});
    history.track_entity(leader);

    auto soldier1 = ecs.entity("Soldier1");
    soldier1.add(Soldier);
    soldier1.set<Position>({-5, -5});
    soldier1.set<Velocity>({0.8, 0.4});
    soldier1.set<Health>({80.0f, 80.0f});
    soldier1.set<AttackPower>({12.0f, 8.0f});
    soldier1.set<MovementSpeed>({1.8f});
    soldier1.set<Experience>({3, 50});
    soldier1.add(Following, leader);
    soldier1.add(AlliedWith, leader);
    history.track_entity(soldier1);

    auto soldier2 = ecs.entity("Soldier2");
    soldier2.add(Soldier);
    soldier2.set<Position>({5, -5});
    soldier2.set<Velocity>({0.9, 0.3});
    soldier2.set<Health>({70.0f, 70.0f});
    soldier2.set<AttackPower>({10.0f, 7.0f});
    soldier2.set<MovementSpeed>({2.1f});
    soldier2.set<Experience>({2, 120});
    soldier2.add(Following, leader);
    soldier2.add(AlliedWith, leader);
    soldier2.add(AlliedWith, soldier1);
    history.track_entity(soldier2);

    // Create enemy forces
    auto enemy1 = ecs.entity("Orc1");
    enemy1.add(Enemy);
    enemy1.set<Position>({50, 50});
    enemy1.set<Velocity>({-1.2, -0.6});
    enemy1.set<Health>({90.0f, 90.0f});
    enemy1.set<AttackPower>({18.0f, 9.0f});
    enemy1.set<MovementSpeed>({1.5f});
    enemy1.set<Armor>({5, 100.0f});
    history.track_entity(enemy1);

    auto enemy2 = ecs.entity("Orc2");
    enemy2.add(Enemy);
    enemy2.set<Position>({60, 45});
    enemy2.set<Velocity>({-1.0, -0.5});
    enemy2.set<Health>({85.0f, 85.0f});
    enemy2.set<AttackPower>({16.0f, 8.5f});
    enemy2.set<MovementSpeed>({1.6f});
    enemy2.set<Armor>({4, 100.0f});
    enemy2.add(AlliedWith, enemy1);
    history.track_entity(enemy2);

    auto enemy3 = ecs.entity("Goblin1");
    enemy3.add(Enemy);
    enemy3.set<Position>({55, 40});
    enemy3.set<Velocity>({-0.7, -0.8});
    enemy3.set<Health>({50.0f, 50.0f});
    enemy3.set<AttackPower>({8.0f, 6.0f});
    enemy3.set<MovementSpeed>({2.5f});
    history.track_entity(enemy3);

    auto enemy4 = ecs.entity("Goblin2");
    enemy4.add(Enemy);
    enemy4.set<Position>({58, 38});
    enemy4.set<Velocity>({-0.6, -0.9});
    enemy4.set<Health>({45.0f, 45.0f});
    enemy4.set<AttackPower>({7.0f, 5.5f});
    enemy4.set<MovementSpeed>({2.6f});
    enemy4.add(Following, enemy3);
    history.track_entity(enemy4);

    std::cout << "\nInitial state:\n";
    std::cout << "Soldiers:\n";
    ecs.each<Position>([&](flecs::entity e, Position& p) {
        if (e.has(Soldier)) {
            const Health* h = e.try_get<Health>();
            std::cout << "  " << e.name() << " at (" << p.x << ", " << p.y << ")";
            if (h) std::cout << " HP: " << h->value;
            std::cout << "\n";
        }
    });
    std::cout << "Enemies:\n";
    ecs.each<Position>([&](flecs::entity e, Position& p) {
        if (e.has(Enemy)) {
            const Health* h = e.try_get<Health>();
            std::cout << "  " << e.name() << " at (" << p.x << ", " << p.y << ")";
            if (h) std::cout << " HP: " << h->value;
            std::cout << "\n";
        }
    });

    std::cout << "\n=== Starting Combat Simulation ===\n";

    // Capture initial state (frame 0)
    history.capture_state();

    // Establish initial combat engagement
    leader.add<Attacking>(enemy1);
    soldier1.add<Attacking>(enemy2);
    soldier2.add<Attacking>(enemy3);
    enemy1.add<Attacking>(leader);
    enemy2.add<Attacking>(soldier1);
    enemy3.add<Attacking>(soldier2);
    enemy4.add<Attacking>(soldier2);

    std::cout << "Combat initiated!\n";

    for (size_t i = 1; i <= 20; i++)
    {
        std::cout << "\n--- Frame " << i << " ---\n";

        ecs.progress(0.1f);  // Advance time

        // Dynamic relationship changes
        if (i == 5) {
            // Goblin2 switches target to Leader
            std::cout << "  [STRATEGY] Goblin2 switches target to Leader!\n";
            if (enemy4.is_alive()) {
                enemy4.remove<Attacking>(soldier2);
                enemy4.add<Attacking>(leader);
            }
        }

        if (i == 12)
        {
            leader.add(AfraidOf, ecs.lookup("Troll1"));
        }

        if (i == 10) {
            // New enemy arrives
            auto enemy5 = ecs.entity("Troll1");
            enemy5.add(TestRel, leader);
            history.track_entity(enemy5);
            enemy5.add(Enemy);
            enemy5.set<Position>({70, 50});
            enemy5.set<Velocity>({-1.5, -0.7});
            enemy5.set<Health>({150.0f, 150.0f});
            enemy5.set<AttackPower>({25.0f, 12.0f});
            enemy5.set<MovementSpeed>({1.2f});
            enemy5.set<Armor>({10, 150.0f});
            // TODO: add(Attacking, leader) // entity rel doesn't get registered to state history if it's added on same state as entity
            enemy5.add<Attacking>(leader);
            std::cout << "  [REINFORCEMENTS] Troll1 enters the battle!\n";
        }
        if (i == 11)
        {
            // ecs.lookup("Troll1").add<Attacking>(leader);
        }

        if (i == 996) {
            std::cout << "  [TIME REWIND] Rolling back to frame 8...\n";
            history.rollback_to(15);
            std::cout << "  Combat state restored to frame 8!\n";
            history.roll_forward(995);
            // Relationships should be restored automatically from snapshots
        }

        history.capture_state();

        // Status report
        int alive_soldiers = 0, alive_enemies = 0;
        ecs.each<Health>([&](flecs::entity e, Health& h) {
            if (!e.has(Dead)) {
                if (e.has(Soldier)) alive_soldiers++;
                if (e.has(Enemy)) alive_enemies++;
            }
        });

        std::cout << "  Soldiers alive: " << alive_soldiers
                  << " | Enemies alive: " << alive_enemies << "\n";

        // Sample some entities
        if (leader.is_alive() && !leader.has(Dead)) {
            const Health* h = leader.try_get<Health>();
            const Position* p = leader.try_get<Position>();
            if (h && p) {
                std::cout << "  Leader: HP " << h->value << "/" << h->max_value
                          << " at (" << p->x << ", " << p->y << ")";
                if (leader.has(Fleeing)) std::cout << " [FLEEING]";
                std::cout << "\n";
            }
        }

        if (enemy1.is_alive() && !enemy1.has(Dead)) {
            const Health* h = enemy1.try_get<Health>();
            const Position* p = enemy1.try_get<Position>();
            if (h && p) {
                std::cout << "  Orc1: HP " << h->value << "/" << h->max_value
                          << " at (" << p->x << ", " << p->y << ")";
                if (enemy1.has(Fleeing)) std::cout << " [FLEEING]";
                std::cout << "\n";
            }
        }
    }

    std::cout << "\n=== Combat Complete ===\n";
    history.print_stats();

    // Disable recording before destruction to prevent observer callbacks during cleanup
    history.recording_enabled = false;

    return 0;
}
