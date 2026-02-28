#pragma once

#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>

#include <vector>
#include <string>
#include <memory>
#include <span>
#include <map>

namespace CesiumRaylib {

class SimpleAssetAccessor : public CesiumAsync::IAssetAccessor {
public:
    SimpleAssetAccessor();
    ~SimpleAssetAccessor() override;

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> get(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& url,
        const std::vector<THeader>& headers) override;

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& verb,
        const std::string& url,
        const std::vector<THeader>& headers,
        const std::span<const std::byte>& contentPayload) override;

    void tick() noexcept override;
};

class SimpleAssetRequest : public CesiumAsync::IAssetRequest {
public:
    SimpleAssetRequest(
        const std::string& method,
        const std::string& url,
        const CesiumAsync::HttpHeaders& headers,
        std::shared_ptr<CesiumAsync::IAssetResponse> response
    );

    const std::string& method() const override;
    const std::string& url() const override;
    const CesiumAsync::HttpHeaders& headers() const override;
    const CesiumAsync::IAssetResponse* response() const override;

private:
    std::string _method;
    std::string _url;
    CesiumAsync::HttpHeaders _headers;
    std::shared_ptr<CesiumAsync::IAssetResponse> _response;
};

class SimpleAssetResponse : public CesiumAsync::IAssetResponse {
public:
    SimpleAssetResponse(
        uint16_t statusCode,
        const std::string& contentType,
        const CesiumAsync::HttpHeaders& headers,
        std::vector<std::byte> data
    );

    uint16_t statusCode() const override;
    std::string contentType() const override;
    const CesiumAsync::HttpHeaders& headers() const override;
    std::span<const std::byte> data() const override;

private:
    uint16_t _statusCode;
    std::string _contentType;
    CesiumAsync::HttpHeaders _headers;
    std::vector<std::byte> _data;
};

}
