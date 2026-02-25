#include "ApiServer.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace CesiumRaylib {

ApiServer::ApiServer(SharedState& sharedState) : _sharedState(sharedState) {
    _server = std::make_unique<httplib::Server>();
}

ApiServer::~ApiServer() {
    stop();
}

void ApiServer::start(int port) {
    if (_running) return;
    _running = true;

    _server->Post("/render", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);

            RenderRequest request;
            request.latitude = j.value("lat", 37.7749);
            request.longitude = j.value("lon", -122.4194);
            request.height = j.value("height", 500.0);
            request.heading = j.value("heading", 0.0);
            request.pitch = j.value("pitch", -45.0);
            request.roll = j.value("roll", 0.0);
            request.fov = j.value("fov", 60.0);
            request.width = j.value("width", 800);
            request.height = j.value("height", 600);

            std::unique_lock<std::mutex> lock(_sharedState.mutex);
            _sharedState.cv.wait(lock, [this]() { return !_sharedState.hasRequest; });

            _sharedState.request = request;
            _sharedState.hasRequest = true;
            _sharedState.resultReady = false;

            _sharedState.cv.notify_one(); // Notify main thread? Wait, notify_one notifies one waiter.
            // Main thread waits? No, main thread polls in loop.
            // So notify is useless if polling.
            // But if main thread waits on CV (e.g. strict render loop driven by requests), then useful.
            // The prompt says "interactive window". So main loop runs continuously.
            // Polling is fine.

            _sharedState.cv.wait(lock, [this]() { return _sharedState.resultReady; });

            res.set_content(reinterpret_cast<const char*>(_sharedState.resultImage.data()), _sharedState.resultImage.size(), "image/png");

            _sharedState.resultReady = false;
            // hasRequest was cleared by main thread.

        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        }
    });

    _serverThread = std::thread([this, port]() {
        std::cout << "API Server listening on port " << port << std::endl;
        _server->listen("0.0.0.0", port);
    });
}

void ApiServer::stop() {
    if (_running) {
        _running = false;
        if (_server) _server->stop();
        if (_serverThread.joinable()) {
             _serverThread.join();
        }
    }
}

}
