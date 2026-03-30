#include <flecs.h>
#include <tradewinds.h>

using namespace tradewinds;

int main()
{
    flecs::world ecs;
    ecs.import<tradewinds::module>();
    ecs.set<ZMQContext>({ std::make_shared<zmq::context_t>(2) });

    auto pub = ecs.entity("GamePub")
        .set<ZMQServer>({ "tcp://*:5555", zmq::socket_type::pub });

    int frame_count = 0;
    while (ecs.progress()) {
        pub.set<PublishedMessage>({ "news", std::to_string(200 + 100* sin(frame_count*0.01)) });
        ++frame_count;
    }
}