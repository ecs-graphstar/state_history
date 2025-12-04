#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <variant>
#include <string>
#include <random>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <nanovg.h>
#include <nanovg_gl.h>
#include "../include/components.h"
#include "../include/state_history.h"

// Timeline UI state
struct TimelineState {
    int total_frames = 0;
    int current_frame = 0;
    bool is_recording = true;
    bool is_playing = false;
    bool mouse_down_on_timeline = false;
    float timeline_y = 500;
    float timeline_height = 80;
};

void error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void draw_timeline(NVGcontext* vg, int width, int height, TimelineState& tl_state, StateHistory& history) {
    float margin = 20;
    float timeline_width = width - 2 * margin;
    float y = tl_state.timeline_y;

    // Grid parameters
    int grid_height = 6;  // 6 tiles high
    // Calculate tile size to fit in available space below y position
    float available_height = height - y - margin;
    float tile_size = available_height / grid_height;

    // Adjust grid width to maintain square tiles
    int grid_width = (int)(timeline_width / tile_size);

    // Calculate actual timeline dimensions
    float timeline_height = grid_height * tile_size;
    tl_state.timeline_height = timeline_height;

    // Draw grey grid
    nvgStrokeColor(vg, nvgRGBA(60, 60, 60, 255));
    nvgStrokeWidth(vg, 1);

    // Vertical grid lines
    for (int i = 0; i <= grid_width; i++) {
        float x = margin + i * tile_size;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, y);
        nvgLineTo(vg, x, y + grid_height * tile_size);
        nvgStroke(vg);
    }

    // Horizontal grid lines
    for (int i = 0; i <= grid_height; i++) {
        float grid_y = y + i * tile_size;
        nvgBeginPath(vg);
        nvgMoveTo(vg, margin, grid_y);
        nvgLineTo(vg, margin + timeline_width, grid_y);
        nvgStroke(vg);
    }

    // Draw white horizontal line vertically centered (at row 3 out of 6)
    float center_y = y + (grid_height / 2.0f) * tile_size;
    nvgBeginPath(vg);
    nvgMoveTo(vg, margin, center_y);
    nvgLineTo(vg, margin + timeline_width, center_y);
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 255));
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);

    // Draw current frame indicator (playhead)
    if (tl_state.total_frames > 0) {
        float x = margin + (tl_state.current_frame * timeline_width / std::max(1, tl_state.total_frames));

        // Draw playhead line (Blender-style blue)
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, y);
        nvgLineTo(vg, x, y + timeline_height);
        nvgStrokeColor(vg, nvgRGBA(64, 156, 255, 255));
        nvgStrokeWidth(vg, 3);
        nvgStroke(vg);

        // Draw playhead triangle
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, y);
        nvgLineTo(vg, x - 6, y - 8);
        nvgLineTo(vg, x + 6, y - 8);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(64, 156, 255, 255));
        nvgFill(vg);

        // Draw frame number above playhead
        char frame_text[32];
        snprintf(frame_text, sizeof(frame_text), "%d", tl_state.current_frame);
        nvgFontSize(vg, 16);
        nvgFontFace(vg, "ATARISTOCRAT");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
        nvgText(vg, x, y - 10, frame_text, NULL);
    }

    // Draw text info
    nvgFontSize(vg, 18);
    nvgFontFace(vg, "sans");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    char info[256];
    snprintf(info, sizeof(info), "Frame: %d / %d", tl_state.current_frame, tl_state.total_frames);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
    nvgText(vg, margin, y - 25, info, NULL);

    const char* status = tl_state.is_recording ? "RECORDING" : (tl_state.is_playing ? "PLAYING" : "PAUSED");
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    nvgFillColor(vg, tl_state.is_recording ? nvgRGBA(255, 50, 50, 255) : nvgRGBA(100, 200, 255, 255));
    nvgText(vg, width - margin, y - 25, status, NULL);
}

bool check_timeline_click(float mouse_x, float mouse_y, int width, TimelineState& tl_state) {
    float margin = 20;
    float timeline_width = width - 2 * margin;
    float y = tl_state.timeline_y;
    float h = tl_state.timeline_height;

    if (mouse_x >= margin && mouse_x <= margin + timeline_width &&
        mouse_y >= y && mouse_y <= y + h) {

        // Calculate which frame was clicked
        float normalized = (mouse_x - margin) / timeline_width;
        int clicked_frame = (int)(normalized * tl_state.total_frames);
        clicked_frame = std::max(0, std::min(clicked_frame, tl_state.total_frames));

        return true;
    }
    return false;
}

int get_frame_from_mouse(float mouse_x, int width, TimelineState& tl_state) {
    float margin = 20;
    float timeline_width = width - 2 * margin;
    float normalized = (mouse_x - margin) / timeline_width;
    int frame = (int)(normalized * tl_state.total_frames);
    return std::max(0, std::min(frame, tl_state.total_frames));
}

int main(int, char *[]) {
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    int window_width = 1000;
    int window_height = 600;
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "State History Timeline Demo", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    NVGcontext* vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (vg == NULL) {
        std::cerr << "Failed to initialize NanoVG" << std::endl;
        glfwTerminate();
        return -1;
    }
    nvgCreateFont(vg, "ATARISTOCRAT", "../assets/ATARISTOCRAT.ttf");

    // Load food sprites
    std::vector<std::string> food_names = {
        "apple", "banana", "bean", "candy", "carrot", "cherry", "dragonfruit",
        "kiwi", "lemon", "mushroom", "olive", "orange", "papper", "papaya",
        "peach", "pear", "pineapple", "strawberry", "tomato", "watermelon"
    };
    std::vector<int> food_sprites;
    for (const auto& name : food_names) {
        std::string path = "../assets/food/" + name + ".png";
        int img = nvgCreateImage(vg, path.c_str(), 0);
        if (img != -1) {
            food_sprites.push_back(img);
        } else {
            std::cerr << "Failed to load image: " << path << std::endl;
        }
    }
    std::cout << "Loaded " << food_sprites.size() << " food sprites\n";

    // Create flecs world
    flecs::world ecs;

    // Create a relationship tag for proximity connections
    auto NearBy = ecs.entity("NearBy");

    // Initialize state history with 100-frame keyframes, compression enabled
    StateHistory history(&ecs, 100, true);
    history.setup_observers();

    // Timeline state
    TimelineState tl_state;

    // Create entities with random properties
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> pos_x_dist(0.0, 1000.0);
    std::uniform_real_distribution<> pos_y_dist(0.0, 450.0);
    std::uniform_real_distribution<> vel_dist(-100.0, 100.0);
    std::uniform_real_distribution<> radius_dist(3.0, 8.0);
    std::uniform_real_distribution<> health_dist(20.0f, 100.0f);
    std::uniform_int_distribution<> sprite_dist(0, food_sprites.size() - 1);
    std::uniform_real_distribution<> scale_dist(0.5, 1.5);

    for (int i = 0; i < 50; ++i) {
        // Give each entity a unique name for proper state history tracking
        std::string entity_name = "Entity_" + std::to_string(i);
        auto entity = ecs.entity(entity_name.c_str());

        double px = pos_x_dist(gen);
        double py = pos_y_dist(gen);
        double vx = vel_dist(gen);
        double vy = vel_dist(gen);
        float radius = radius_dist(gen);
        float health = health_dist(gen);

        int sprite_idx = sprite_dist(gen);
        float sprite_scale = scale_dist(gen);

        entity.set<Position>({px, py});
        entity.set<Velocity>({vx, vy});
        entity.set<Health>({health, 100.0f});
        entity.set<RenderColor>({0xFF00FF00, radius});  // Start green, will be updated by health system
        entity.set<FoodSprite>({sprite_idx, sprite_scale});
        history.track_entity(entity);
    }

    // Movement system with bouncing
    auto movementSystem = ecs.system<Position, Velocity, RenderColor>()
        .each([&ecs](flecs::iter& it, size_t i, Position& pos, Velocity& vel, RenderColor& color) {
            float dt = it.delta_system_time();

            pos.x += vel.x * dt;
            pos.y += vel.y * dt;

            // Bounce off walls (accounting for radius)
            if (pos.x - color.radius <= 0 || pos.x + color.radius >= 1000) {
                vel.x *= -1.0;
                pos.x = std::max((double)color.radius, std::min(1000.0 - color.radius, pos.x));
            }
            if (pos.y - color.radius <= 0 || pos.y + color.radius >= 450) {
                vel.y *= -1.0;
                pos.y = std::max((double)color.radius, std::min(450.0 - color.radius, pos.y));
            }

            // Notify Flecs that Position was modified (triggers OnSet for state history)
            ecs_modified_id(ecs.c_ptr(), it.entities()[i], ecs.component<Position>().id());
        });

    // Health decay system - gradually reduce health over time
    auto healthDecaySystem = ecs.system<Health>()
        .each([&ecs](flecs::iter& it, size_t i, Health& health) {
            float dt = it.delta_system_time();
            health.value -= 5.0f * dt;  // Lose 5 health per second
            health.value = std::max(0.0f, health.value);
            ecs_modified_id(ecs.c_ptr(), it.entities()[i], ecs.component<Health>().id());
        });

    // Color update system - transition from green → yellow → red based on health
    auto colorUpdateSystem = ecs.system<Health, RenderColor>()
        .each([&ecs](flecs::iter& it, size_t i, Health& health, RenderColor& color) {
            float health_ratio = health.value / health.max_value;

            uint8_t r, g, b;
            if (health_ratio > 0.5f) {
                // Green to Yellow: 100% → 50% health
                float t = (health_ratio - 0.5f) * 2.0f;  // 0 at 50%, 1 at 100%
                r = static_cast<uint8_t>(255 * (1.0f - t));
                g = 255;
                b = 0;
            } else {
                // Yellow to Red: 50% → 0% health
                float t = health_ratio * 2.0f;  // 0 at 0%, 1 at 50%
                r = 255;
                g = static_cast<uint8_t>(255 * t);
                b = 0;
            }

            uint32_t new_color = (r << 24) | (g << 16) | (b << 8) | 0xFF;
            if (color.color != new_color) {
                color.color = new_color;
                ecs_modified_id(ecs.c_ptr(), it.entities()[i], ecs.component<RenderColor>().id());
            }
        });

    // Proximity relationship system - connect nearby entities
    const float proximity_threshold = 60.0f;  // Distance threshold for relationships
    auto proximitySystem = ecs.system<Position>()
        .each([&ecs, &NearBy, proximity_threshold](flecs::iter& it, size_t i, Position& pos1) {
            flecs::entity e1 = it.entity(i);

            // Skip if entity is not alive
            if (!e1.is_alive()) return;

            // Check distance to all other entities with Position
            auto q = ecs.query<Position>();
            q.each([&](flecs::entity e2, Position& pos2) {
                // Skip self
                if (e1 == e2) return;

                // Skip if entity is not alive
                if (!e2.is_alive()) return;

                // Calculate distance
                double dx = pos1.x - pos2.x;
                double dy = pos1.y - pos2.y;
                double dist = std::sqrt(dx * dx + dy * dy);

                // Check if relationship should exist (with safety check)
                bool has_relationship = e1.is_alive() && e2.is_alive() && e1.has(NearBy, e2);

                if (dist < proximity_threshold && !has_relationship) {
                    // Add relationship (only if both entities are alive)
                    if (e1.is_alive() && e2.is_alive()) {
                        e1.add(NearBy, e2);
                    }
                } else if (dist >= proximity_threshold && has_relationship) {
                    // Remove relationship (only if both entities are alive)
                    if (e1.is_alive() && e2.is_alive()) {
                        e1.remove(NearBy, e2);
                    }
                }
            });
        });

    // Capture initial state
    history.capture_state();
    tl_state.total_frames = 0;
    tl_state.current_frame = 0;

    double last_time = glfwGetTime();
    double start_time = glfwGetTime();
    double record_time = 600.0;  // Record for 10 minutes (600 seconds)

    std::cout << "=== State History Timeline Demo ===\n";
    std::cout << "Recording for 10 minutes with dynamic entity creation/destruction...\n";
    std::cout << "\nControls:\n";
    std::cout << "  LEFT/RIGHT Arrow: Step backward/forward one frame\n";
    std::cout << "  SPACEBAR: Play/Pause playback\n";
    std::cout << "  Mouse Click: Click timeline to scrub to frame\n";
    std::cout << "  ESC: Exit\n\n";

    // Mouse state
    double mouse_x, mouse_y;
    bool was_mouse_down = false;

    // Keyboard state for debouncing
    bool was_left_pressed = false;
    bool was_right_pressed = false;
    bool was_space_pressed = false;

    // Playback control
    double playback_timer = 0.0;
    double playback_speed = 1.0 / 60.0;  // Play at 60 FPS

    // Entity lifecycle tracking
    int next_entity_id = 50;  // Start after initial 50 entities
    int frames_since_spawn = 0;
    int frames_since_destroy = 0;

    while (!glfwWindowShouldClose(window)) {
        double current_time = glfwGetTime();
        double elapsed = current_time - start_time;

        // Handle mouse input
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        int mouse_button_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        bool is_mouse_down = (mouse_button_state == GLFW_PRESS);

        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);

        // Handle timeline interaction
        if (is_mouse_down && !was_mouse_down) {
            // Mouse just pressed
            if (check_timeline_click(mouse_x, mouse_y, window_width, tl_state)) {
                tl_state.mouse_down_on_timeline = true;
                tl_state.is_recording = false;
                int target_frame = get_frame_from_mouse(mouse_x, window_width, tl_state);

                std::cout << "Scrubbing to frame " << target_frame << "\n";

                if (target_frame < tl_state.current_frame) {
                    history.rollback_to(target_frame);
                } else if (target_frame > tl_state.current_frame) {
                    history.roll_forward(target_frame);
                }
                tl_state.current_frame = target_frame;
            }
        } else if (is_mouse_down && tl_state.mouse_down_on_timeline) {
            // Mouse held down on timeline - scrub
            int target_frame = get_frame_from_mouse(mouse_x, window_width, tl_state);
            if (target_frame != tl_state.current_frame) {
                if (target_frame < tl_state.current_frame) {
                    history.rollback_to(target_frame);
                } else if (target_frame > tl_state.current_frame) {
                    history.roll_forward(target_frame);
                }
                tl_state.current_frame = target_frame;
            }
        } else if (!is_mouse_down && was_mouse_down) {
            // Mouse released
            tl_state.mouse_down_on_timeline = false;
        }

        was_mouse_down = is_mouse_down;

        // Handle keyboard controls
        int left_key = glfwGetKey(window, GLFW_KEY_LEFT);
        int right_key = glfwGetKey(window, GLFW_KEY_RIGHT);
        int space_key = glfwGetKey(window, GLFW_KEY_SPACE);

        // Arrow keys: frame-by-frame navigation
        if (left_key == GLFW_PRESS && !was_left_pressed) {
            // Step backward one frame
            if (!tl_state.is_recording && tl_state.current_frame > 0) {
                tl_state.is_playing = false;
                int target_frame = tl_state.current_frame - 1;
                history.rollback_to(target_frame);
                tl_state.current_frame = target_frame;
            }
        }
        was_left_pressed = (left_key == GLFW_PRESS);

        if (right_key == GLFW_PRESS && !was_right_pressed) {
            // Step forward one frame
            if (!tl_state.is_recording && tl_state.current_frame < tl_state.total_frames) {
                tl_state.is_playing = false;
                int target_frame = tl_state.current_frame + 1;
                history.roll_forward(target_frame);
                tl_state.current_frame = target_frame;
            }
        }
        was_right_pressed = (right_key == GLFW_PRESS);

        // Spacebar: toggle play/pause
        if (space_key == GLFW_PRESS && !was_space_pressed) {
            if (!tl_state.is_recording) {
                tl_state.is_playing = !tl_state.is_playing;
                playback_timer = 0.0;
            }
        }
        was_space_pressed = (space_key == GLFW_PRESS);

        // Playback mode: automatically step through frames
        if (tl_state.is_playing && !tl_state.is_recording) {
            double dt = current_time - last_time;
            playback_timer += dt;

            while (playback_timer >= playback_speed && tl_state.current_frame < tl_state.total_frames) {
                playback_timer -= playback_speed;
                int target_frame = tl_state.current_frame + 1;
                history.roll_forward(target_frame);
                tl_state.current_frame = target_frame;
            }

            // Stop at end of timeline
            if (tl_state.current_frame >= tl_state.total_frames) {
                tl_state.is_playing = false;
            }
        }

        last_time = current_time;

        // Recording phase
        if (tl_state.is_recording && elapsed < record_time) {
            // Dynamic entity destruction - destroy entities when health reaches 0
            // Do this BEFORE ecs.progress() to avoid systems accessing dying entities
            frames_since_destroy++;
            if (frames_since_destroy >= 10) {  // Check every 10 frames
                frames_since_destroy = 0;
                std::vector<flecs::entity> entities_to_destroy;
                auto q = ecs.query<Health>();
                q.each([&](flecs::entity e, Health& health) {
                    if (health.value <= 0.1f) {  // Dead or nearly dead
                        entities_to_destroy.push_back(e);
                    }
                });

                // Destroy entities - observers will track the Remove operations
                // Use defer to ensure destruction happens at a safe time
                ecs.defer_begin();
                for (auto& e : entities_to_destroy) {
                    if (e.is_alive()) {
                        history.untrack_entity(e);
                        e.destruct();
                    }
                }
                ecs.defer_end();
            }

            // Dynamic entity spawning - every 60 frames (1 second), spawn 2 new entities
            frames_since_spawn++;
            if (frames_since_spawn >= 60) {
                frames_since_spawn = 0;

                for (int i = 0; i < 2; ++i) {
                    std::string entity_name = "Entity_" + std::to_string(next_entity_id++);
                    auto entity = ecs.entity(entity_name.c_str());

                    double px = pos_x_dist(gen);
                    double py = pos_y_dist(gen);
                    double vx = vel_dist(gen);
                    double vy = vel_dist(gen);
                    float radius = radius_dist(gen);
                    float health = health_dist(gen);

                    int sprite_idx = sprite_dist(gen);
                    float sprite_scale = scale_dist(gen);

                    // Track entity first, then add components so observers can record Add operations
                    history.track_entity(entity);
                    entity.set<Position>({px, py});
                    entity.set<Velocity>({vx, vy});
                    entity.set<Health>({health, 100.0f});
                    entity.set<RenderColor>({0xFF00FF00, radius});  // Start green, will be updated by health system
                    entity.set<FoodSprite>({sprite_idx, sprite_scale});
                }
            }

            // Now run the simulation frame
            ecs.progress(1.0f/60.0f);

            // Capture state after everything is done
            history.capture_state();
            tl_state.total_frames = history.snapshots.size() - 1;
            tl_state.current_frame = tl_state.total_frames;
        }
        // Stop recording after 10 minutes
        else if (tl_state.is_recording && elapsed >= record_time) {
            tl_state.is_recording = false;
            std::cout << "\nRecording complete! " << tl_state.total_frames << " frames captured.\n";
            std::cout << "Use arrow keys, spacebar, or click the timeline to navigate!\n";
            history.print_stats();
        }

        // Rendering
        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        float pixel_ratio = (float)fb_width / (float)window_width;
        nvgBeginFrame(vg, window_width, window_height, pixel_ratio);

        // Render relationship lines first (behind sprites)
        nvgStrokeWidth(vg, 1.5f);
        nvgStrokeColor(vg, nvgRGBA(100, 100, 255, 128));  // Semi-transparent blue
        auto pos_query = ecs.query<Position>();
        pos_query.each([&](flecs::entity e1, Position& pos1) {
            // Find all entities this one has NearBy relationships with
            e1.each([&](flecs::id rel_id) {
                if (rel_id.is_pair() && rel_id.first() == NearBy) {
                    // Get the target entity
                    flecs::entity e2 = rel_id.second();
                    if (e2.is_alive() && e2.has<Position>()) {
                        const Position pos2 = e2.get<Position>();
                        // Draw line between the two entities
                        nvgBeginPath(vg);
                        nvgMoveTo(vg, pos1.x, pos1.y);
                        nvgLineTo(vg, pos2.x, pos2.y);
                        nvgStroke(vg);
                    }
                }
            });
        });

        // Render entities as food sprites
        auto q = ecs.query<Position, RenderColor, FoodSprite>();
        q.each([&](flecs::entity e, Position& pos, RenderColor& color, FoodSprite& sprite) {
            if (sprite.sprite_index >= 0 && sprite.sprite_index < food_sprites.size()) {
                int img = food_sprites[sprite.sprite_index];
                int img_w, img_h;
                nvgImageSize(vg, img, &img_w, &img_h);

                // Scale the sprite to fixed 24x24 size
                float sprite_size = 24.0f;
                float scale_w = sprite_size / img_w;
                float scale_h = sprite_size / img_h;

                // Apply health-based tint color
                uint8_t r = (color.color >> 24) & 0xFF;
                uint8_t g = (color.color >> 16) & 0xFF;
                uint8_t b = (color.color >> 8) & 0xFF;

                nvgSave(vg);
                nvgTranslate(vg, pos.x - sprite_size/2, pos.y - sprite_size/2);
                nvgScale(vg, scale_w, scale_h);

                NVGpaint imgPaint = nvgImagePattern(vg, 0, 0, img_w, img_h, 0, img, 1.0f);
                // Tint the sprite based on health
                imgPaint.innerColor = nvgRGBA(r, g, b, 255);

                nvgBeginPath(vg);
                nvgRect(vg, 0, 0, img_w, img_h);
                nvgFillPaint(vg, imgPaint);
                nvgFill(vg);

                nvgRestore(vg);
            }
        });

        // Draw timeline
        draw_timeline(vg, window_width, window_height, tl_state, history);

        nvgEndFrame(vg);

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Handle ESC to close
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    }

    // Disable recording before cleanup
    history.recording_enabled = false;

    nvgDeleteGL2(vg);
    glfwTerminate();
    return 0;
}
