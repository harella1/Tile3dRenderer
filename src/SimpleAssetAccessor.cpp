#include "SimpleAssetAccessor.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <CesiumAsync/AsyncSystem.h>
#include <iostream>
#include <thread>
#include <regex>
#include <cstring>

namespace CesiumRaylib {

SimpleAssetAccessor::SimpleAssetAccessor() {
}

SimpleAssetAccessor::~SimpleAssetAccessor() {
}

struct ParsedUrl {
    std::string protocol;
    std::string host;
    int port;
    std::string path;
};

static ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl result;
    // Basic regex for parsing URL
    std::regex url_regex(R"(^(([^:/?#]+):)?//([^/?#]*)(?::(\d+))?([^?#]*)(\?([^#]*))?(#(.*))?)");
    std::smatch url_match_result;
    if (std::regex_match(url, url_match_result, url_regex)) {
        result.protocol = url_match_result[2];
        result.host = url_match_result[3];
        std::string port_str = url_match_result[4];
        if (port_str.empty()) {
            result.port = (result.protocol == "https") ? 443 : 80;
        } else {
            result.port = std::stoi(port_str);
        }
        result.path = url_match_result[5];
        if (url_match_result[6].matched) {
             result.path += url_match_result[6];
        }
    }
    return result;
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> SimpleAssetAccessor::get(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::string& url,
    const std::vector<THeader>& headers)
{
    return request(asyncSystem, "GET", url, headers, {});
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> SimpleAssetAccessor::request(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::string& verb,
    const std::string& url,
    const std::vector<THeader>& headers,
    const std::span<const std::byte>& contentPayload)
{
    // Capture necessary data. Note: copying headers and payload.
    // contentPayload is a span, so we must copy the data to a vector.
    std::vector<std::byte> payloadVec(contentPayload.begin(), contentPayload.end());

    return asyncSystem.runInWorkerThread([verb, url, headers, payload = std::move(payloadVec)]() -> std::shared_ptr<CesiumAsync::IAssetRequest> {
        ParsedUrl parsed = parseUrl(url);
        std::string baseUrl = parsed.protocol + "://" + parsed.host + ":" + std::to_string(parsed.port);

        httplib::Client client(baseUrl);
        client.enable_server_certificate_verification(true);
        client.set_follow_location(true); // Essential for GitHub raw and many other CDNs
        client.set_connection_timeout(5, 0); // 5 seconds timeout
        client.set_read_timeout(10, 0); // 10 seconds timeout

        httplib::Headers httpHeaders;
        bool hasUserAgent = false;
        for (const auto& header : headers) {
            httpHeaders.emplace(header.first, header.second);
            if (header.first == "User-Agent" || header.first == "user-agent") {
                hasUserAgent = true;
            }
        }

        // Many servers (like GitHub raw) reject requests without a User-Agent
        if (!hasUserAgent) {
            httpHeaders.emplace("User-Agent", "CesiumRaylibViewer/0.1.0");
        }

        // Ensure we don't get gzip compressed data since we don't have zlib linked in httplib
        httpHeaders.emplace("Accept-Encoding", "identity");

        httplib::Result res;
        if (verb == "GET") {
            res = client.Get(parsed.path, httpHeaders);
        } else if (verb == "POST") {
            res = client.Post(parsed.path, httpHeaders, reinterpret_cast<const char*>(payload.data()), payload.size(), "application/octet-stream");
        } else {
             // Fallback
             return nullptr;
        }

        if (res) {
            if (res->status >= 400) {
                std::cerr << "[SimpleAssetAccessor] HTTP Error: " << res->status << " for URL: " << url << std::endl;
            } else {
                std::cout << "[SimpleAssetAccessor] Success 200 for URL: " << url << "\n";
                // DEBUG: Print the first 100 characters of the body to see what we actually received
                std::string debugBody = res->body.substr(0, std::min<size_t>(100, res->body.size()));
                std::cout << "Body preview: " << debugBody << std::endl;
            }

            std::vector<std::byte> data;
            data.resize(res->body.size());
            std::memcpy(data.data(), res->body.data(), res->body.size());

            CesiumAsync::HttpHeaders responseHeaders;
            for (const auto& h : res->headers) {
                responseHeaders[h.first] = h.second;
            }

            auto response = std::make_shared<SimpleAssetResponse>(
                static_cast<uint16_t>(res->status),
                res->get_header_value("Content-Type"),
                responseHeaders,
                std::move(data)
            );

            return std::make_shared<SimpleAssetRequest>(verb, url, CesiumAsync::HttpHeaders{}, response);
        } else {
            std::cerr << "[SimpleAssetAccessor] HTTP Request Failed! Error: " << httplib::to_string(res.error()) << " for URL: " << url << std::endl;
            // Return a 500 error response so Cesium Native handles it properly instead of crashing
            auto response = std::make_shared<SimpleAssetResponse>(
                static_cast<uint16_t>(500),
                "",
                CesiumAsync::HttpHeaders{},
                std::vector<std::byte>()
            );
            return std::make_shared<SimpleAssetRequest>(verb, url, CesiumAsync::HttpHeaders{}, response);
        }
    });
}

void SimpleAssetAccessor::tick() noexcept {
}

SimpleAssetRequest::SimpleAssetRequest(
    const std::string& method,
    const std::string& url,
    const CesiumAsync::HttpHeaders& headers,
    std::shared_ptr<CesiumAsync::IAssetResponse> response
) : _method(method), _url(url), _headers(headers), _response(std::move(response)) {}

const std::string& SimpleAssetRequest::method() const { return _method; }
const std::string& SimpleAssetRequest::url() const { return _url; }
const CesiumAsync::HttpHeaders& SimpleAssetRequest::headers() const { return _headers; }
const CesiumAsync::IAssetResponse* SimpleAssetRequest::response() const { return _response.get(); }

SimpleAssetResponse::SimpleAssetResponse(
    uint16_t statusCode,
    const std::string& contentType,
    const CesiumAsync::HttpHeaders& headers,
    std::vector<std::byte> data
) : _statusCode(statusCode), _contentType(contentType), _headers(headers), _data(std::move(data)) {}

uint16_t SimpleAssetResponse::statusCode() const { return _statusCode; }
std::string SimpleAssetResponse::contentType() const { return _contentType; }
const CesiumAsync::HttpHeaders& SimpleAssetResponse::headers() const { return _headers; }
std::span<const std::byte> SimpleAssetResponse::data() const { return _data; }

}
