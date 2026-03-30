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

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#include <stack>

#include <tradewinds.h>

using namespace tradewinds;

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

// Per-thread stack to ensure zones are closed in the correct order
thread_local std::stack<TracyCZoneCtx> zone_stack;

void trace_push(const char *file, size_t line, const char *name) {
    // ___tracy_alloc_srcloc_name signature:
    // (line, source, sourceSz, function, functionSz, name, nameSz, color)
    uint64_t srcloc = ___tracy_alloc_srcloc_name(
        (uint32_t)line,
        file, strlen(file),       // Source file
        name, strlen(name),       // Function name
        name, strlen(name),       // Zone name
        0                         // Color
    );

    TracyCZoneCtx ctx = ___tracy_emit_zone_begin_alloc(srcloc, 1);
    zone_stack.push(ctx);
}

void trace_pop(const char *file, size_t line, const char *name) {
    if (!zone_stack.empty()) {
        TracyCZoneEnd(zone_stack.top());
        zone_stack.pop();
    }
}

int main(int, char *[]) {

    ecs_os_set_api_defaults();
    ecs_os_api_t os_api = ecs_os_get_api();
    os_api.perf_trace_push_ = trace_push;
    os_api.perf_trace_pop_ = trace_pop;
    ecs_os_set_api(&os_api);

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Snapshot Client", NULL, NULL);
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

    world.import<tradewinds::module>();
    world.set<ZMQContext>({ std::make_shared<zmq::context_t>(2) });

    world.entity("GameSub")
        .set<ZMQClient>({ "tcp://localhost:5555", zmq::socket_type::sub })
        .set<ZMQSubscriber>({ "" }); // Subscribe to everything

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
    auto square1 = world.entity("NetBox")
        .set<Position>({100.0f, 275.0f})
        .set<RectRenderable>({50.0f, 50.0f, 0xFFC000FF})
        .set<ZIndex>({1});

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


    world.system<ZMQClient>("ZMQSubscribeMovement")
        .each([](flecs::entity e, ZMQClient& client) {
            if (client.type == zmq::socket_type::sub) {
                zmq::message_t topic_msg;
                
                // Non-blocking check for the first part (Topic)
                auto res = client.socket->recv(topic_msg, zmq::recv_flags::dontwait);
                
                if (res) {
                    std::string topic = topic_msg.to_string();
                    
                    // If there's more data (the payload), receive it
                    if (topic_msg.more()) {
                        zmq::message_t payload_msg;
                        client.socket->recv(payload_msg, zmq::recv_flags::none);
                        
                        std::cout << "[" << e.name() << "] Received on topic '" 
                                << topic << "': " << payload_msg.to_string() << std::endl; 
                        e.world().lookup("NetBox").set<Position>({std::stoi(payload_msg.to_string()), 250});
                                // TODO: Change pos
                    } else {
                        std::cout << "[" << e.name() << "] Received topic only: " << topic << std::endl;
                    }
                }
            }
        });
    
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
        {
            ZoneScopedN("ECS Progress");
            world.progress();
        }

        nvgEndFrame(vg);

        glfwSwapBuffers(window);
        glfwPollEvents();

        FrameMark;
    }

    nvgDeleteGL2(vg);
    glfwTerminate();
    return 0;
}
