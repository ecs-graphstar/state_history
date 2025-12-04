#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <variant>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <nanovg.h>
#include <nanovg_gl.h>

// ECS Components
struct Position {
    float x, y;
};

struct Velocity {
    float dx, dy;
};

struct Bounds {
    float minX, maxX, minY, maxY;
};

struct RectRenderable {
    float width, height;
    uint32_t color;
};

struct TextRenderable {
    std::string text;
    std::string fontFace;
    float fontSize;
    uint32_t color;
    int alignment;
};

struct ImageRenderable {
    int imageHandle;
    float width, height;
    float alpha;
};

struct ZIndex {
    int layer;
};

struct Window {
    GLFWwindow* handle;
    int width, height;
};

struct Graphics {
    NVGcontext* vg;
};

enum class RenderType {
    Rectangle,
    Text,
    Image
};

struct RenderCommand {
    Position pos;
    std::variant<RectRenderable, TextRenderable, ImageRenderable> renderData;
    RenderType type;
    int zIndex;

    bool operator<(const RenderCommand& other) const {
        return zIndex < other.zIndex;
    }
};

struct RenderQueue {
    std::vector<RenderCommand> commands;

    void clear() {
        commands.clear();
    }

    void addRectCommand(const Position& pos, const RectRenderable& renderable, int zIndex) {
        commands.push_back({pos, renderable, RenderType::Rectangle, zIndex});
    }

    void addTextCommand(const Position& pos, const TextRenderable& renderable, int zIndex) {
        commands.push_back({pos, renderable, RenderType::Text, zIndex});
    }

    void addImageCommand(const Position& pos, const ImageRenderable& renderable, int zIndex) {
        commands.push_back({pos, renderable, RenderType::Image, zIndex});
    }

    void sort() {
        std::sort(commands.begin(), commands.end());
    }
};

void error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
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

    GLFWwindow* window = glfwCreateWindow(800, 600, "Flecs ECS Demo", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, 800, 600);

    NVGcontext* vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (vg == NULL) {
        std::cerr << "Failed to initialize NanoVG" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Create flecs world
    flecs::world world;

    // Register components
    world.component<Position>();
    world.component<Velocity>();
    world.component<Bounds>();
    world.component<RectRenderable>();
    world.component<TextRenderable>();
    world.component<ImageRenderable>();
    world.component<ZIndex>();
    world.component<Window>();
    world.component<Graphics>().add(flecs::Singleton);
    world.component<RenderQueue>();

    // Create singleton entities for global resources
    auto windowEntity = world.entity("Window")
        .set<Window>({window, 800, 600});

    auto graphicsEntity = world.entity("Graphics")
        .set<Graphics>({vg});

    auto renderQueueEntity = world.entity("RenderQueue")
        .set<RenderQueue>({});

    // Create multiple entities with different z-indices for testing
    auto square1 = world.entity("Square1")
        .set<Position>({100.0f, 275.0f})
        .set<Velocity>({200.0f, 0.0f})
        .set<Bounds>({50.0f, 750.0f - 50.0f, 0.0f, 600.0f})
        .set<RectRenderable>({50.0f, 50.0f, 0xFFC000FF})
        .set<ZIndex>({1});

    auto square2 = world.entity("Square2")
        .set<Position>({150.0f, 300.0f})
        .set<Velocity>({-150.0f, 100.0f})
        .set<Bounds>({30.0f, 770.0f - 30.0f, 0.0f, 570.0f})
        .set<RectRenderable>({30.0f, 30.0f, 0xFF0000FF})
        .set<ZIndex>({3});

    auto square3 = world.entity("Square3")
        .set<Position>({200.0f, 250.0f})
        .set<Velocity>({100.0f, -50.0f})
        .set<Bounds>({40.0f, 760.0f - 40.0f, 0.0f, 560.0f})
        .set<RectRenderable>({40.0f, 40.0f, 0x00FF00FF})
        .set<ZIndex>({2});

    // Create text entities with different z-indices
    auto text1 = world.entity("Text1")
        .set<Position>({400.0f, 100.0f})
        .set<TextRenderable>({"Behind boxes", "ATARISTOCRAT", 24.0f, 0xFFFFFFFF, NVG_ALIGN_CENTER})
        .set<ZIndex>({0});

    auto text2 = world.entity("Text2")
        .set<Position>({400.0f, 500.0f})
        .set<TextRenderable>({"In front of boxes", "ATARISTOCRAT", 20.0f, 0xFFFF00FF, NVG_ALIGN_CENTER})
        .set<ZIndex>({5});

    auto text3 = world.entity("Text3")
        .set<Position>({400.0f, 300.0f})
        .set<TextRenderable>({"Between boxes", "ATARISTOCRAT", 18.0f, 0xFF00FFFF, NVG_ALIGN_CENTER})
        .set<ZIndex>({2});

    // Movement system
    auto movementSystem = world.system<Position, Velocity, Bounds>()
        .each([](flecs::iter& it, size_t i, Position& pos, Velocity& vel, Bounds& bounds) {
            float deltaTime = it.delta_system_time();

            pos.x += vel.dx * deltaTime;
            pos.y += vel.dy * deltaTime;

            // Bounce off boundaries
            if (pos.x <= bounds.minX || pos.x >= bounds.maxX) {
                vel.dx *= -1.0f;
                pos.x = std::max(bounds.minX, std::min(bounds.maxX, pos.x));
            }
            if (pos.y <= bounds.minY || pos.y >= bounds.maxY) {
                vel.dy *= -1.0f;
                pos.y = std::max(bounds.minY, std::min(bounds.maxY, pos.y));
            }
        });

    // Render queue collection system for rectangles
    auto rectQueueSystem = world.system<Position, RectRenderable, ZIndex>()
        .each([&](flecs::entity e, Position& pos, RectRenderable& renderable, ZIndex& zIndex) {
            RenderQueue& queue = world.ensure<RenderQueue>();
            queue.addRectCommand(pos, renderable, zIndex.layer);
        });

    // Render queue collection system for text
    auto textQueueSystem = world.system<Position, TextRenderable, ZIndex>()
        .each([&](flecs::entity e, Position& pos, TextRenderable& renderable, ZIndex& zIndex) {
            RenderQueue& queue = world.ensure<RenderQueue>();
            queue.addTextCommand(pos, renderable, zIndex.layer);
        });

    // Render queue collection system for images
    auto imageQueueSystem = world.system<Position, ImageRenderable, ZIndex>()
        .each([&](flecs::entity e, Position& pos, ImageRenderable& renderable, ZIndex& zIndex) {
            RenderQueue& queue = world.ensure<RenderQueue>();
            queue.addImageCommand(pos, renderable, zIndex.layer);
        });

    // Render execution system - sorts and renders all queued commands
    auto renderExecutionSystem = world.system<RenderQueue, Graphics>()
        .each([&](flecs::entity e, RenderQueue& queue, Graphics& graphics) {
            queue.sort();

            for (const auto& cmd : queue.commands) {
                switch (cmd.type) {
                    case RenderType::Rectangle: {
                        const auto& rect = std::get<RectRenderable>(cmd.renderData);
                        nvgBeginPath(graphics.vg);
                        nvgRect(graphics.vg, cmd.pos.x, cmd.pos.y, rect.width, rect.height);

                        uint8_t r = (rect.color >> 24) & 0xFF;
                        uint8_t g = (rect.color >> 16) & 0xFF;
                        uint8_t b = (rect.color >> 8) & 0xFF;

                        nvgFillColor(graphics.vg, nvgRGB(r, g, b));
                        nvgFill(graphics.vg);
                        break;
                    }
                    case RenderType::Text: {
                        const auto& text = std::get<TextRenderable>(cmd.renderData);
                        nvgFontSize(graphics.vg, text.fontSize);
                        nvgFontFace(graphics.vg, text.fontFace.c_str());
                        nvgTextAlign(graphics.vg, text.alignment);

                        uint8_t r = (text.color >> 24) & 0xFF;
                        uint8_t g = (text.color >> 16) & 0xFF;
                        uint8_t b = (text.color >> 8) & 0xFF;

                        nvgFillColor(graphics.vg, nvgRGB(r, g, b));
                        nvgText(graphics.vg, cmd.pos.x, cmd.pos.y, text.text.c_str(), nullptr);
                        break;
                    }
                    case RenderType::Image: {
                        const auto& image = std::get<ImageRenderable>(cmd.renderData);
                        if (image.imageHandle != -1) {
                            NVGpaint imgPaint = nvgImagePattern(graphics.vg, cmd.pos.x, cmd.pos.y,
                                                              image.width, image.height, 0.0f,
                                                              image.imageHandle, image.alpha);
                            nvgBeginPath(graphics.vg);
                            nvgRect(graphics.vg, cmd.pos.x, cmd.pos.y, image.width, image.height);
                            nvgFillPaint(graphics.vg, imgPaint);
                            nvgFill(graphics.vg);
                        }
                        break;
                    }
                }
            }

            queue.clear();
        });

    int fontHandle = nvgCreateFont(vg, "ATARISTOCRAT", "../assets/ATARISTOCRAT.ttf");
    int cogImageHandle = nvgCreateImage(vg, "../assets/cog_theory.png", 0);

    if (cogImageHandle == -1) {
        std::cerr << "Failed to load cog_theory.png" << std::endl;
    }

    // Create cog theory book image entity
    auto cogBookImage = world.entity("CogBookImage")
        .set<Position>({0.0f, 0.0f})
        .set<ImageRenderable>({cogImageHandle, 165.0f, 224.0f, 1.0f})
        .set<ZIndex>({1});
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        int winWidth, winHeight;
        glfwGetFramebufferSize(window, &winWidth, &winHeight);
        float pxRatio = (float)winWidth / (float)800;

        glViewport(0, 0, winWidth, winHeight);
        glClearColor(0.1f, 0.2f, 0.2f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Update window size in ECS
        windowEntity.set<Window>({window, winWidth, winHeight});

        nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

        // Run ECS systems (this will render all entities through the queue system)
        world.progress();

        nvgEndFrame(vg);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    nvgDeleteGL2(vg);
    glfwTerminate();
    return 0;
}
