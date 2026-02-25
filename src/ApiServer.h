#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>
#include <optional>
#include <memory>

// Forward declaration to avoid including httplib.h in header
namespace httplib { class Server; }

namespace CesiumRaylib {

struct RenderRequest {
    double latitude;
    double longitude;
    double height;
    double heading;
    double pitch;
    double roll;
    double fov;
    int width;
    int height;
};

struct SharedState {
    std::mutex mutex;
    std::condition_variable cv;
    bool hasRequest = false;
    RenderRequest request;
    std::vector<unsigned char> resultImage; // PNG data
    bool resultReady = false;
};

class ApiServer {
public:
    ApiServer(SharedState& sharedState);
    ~ApiServer();

    void start(int port);
    void stop();

private:
    SharedState& _sharedState;
    std::thread _serverThread;
    std::atomic<bool> _running = false;
    std::unique_ptr<httplib::Server> _server;
};

}
