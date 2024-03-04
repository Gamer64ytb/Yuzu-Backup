// SPDX-FileCopyrightText: 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <mutex>
#include <string>

#include <fmt/format.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif
#include <httplib.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "common/logging/log.h"
#include "web_service/web_backend.h"
#include "web_service/web_result.h"

namespace WebService {

constexpr std::array<const char, 1> API_VERSION{'1'};

constexpr std::size_t TIMEOUT_SECONDS = 30;

struct Client::Impl {
    Impl(std::string host_, std::string username_, std::string token_)
        : host{std::move(host_)}, username{std::move(username_)}, token{std::move(token_)} {
        std::scoped_lock lock{jwt_cache.mutex};
        if (this->username == jwt_cache.username && this->token == jwt_cache.token) {
            jwt = jwt_cache.jwt;
        }

        // Normalize host expression
        if (!this->host.empty() && this->host.back() == '/') {
            static_cast<void>(this->host.pop_back());
        }
    }

    /// A generic function handles POST, GET and DELETE request together
    WebResult GenericRequest(const std::string& method, const std::string& path,
                             const std::string& data, bool allow_anonymous,
                             const std::string& accept) {
        if (jwt.empty()) {
            UpdateJWT();
        }

        if (jwt.empty() && !allow_anonymous) {
            LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
            return WebResult{WebResult::Code::CredentialsMissing, "Credentials needed", ""};
        }

        auto result = GenericRequest(method, path, data, accept, jwt);
        if (result.result_string == "401") {
            // Try again with new JWT
            UpdateJWT();
            result = GenericRequest(method, path, data, accept, jwt);
        }

        return result;
    }

    /**
     * A generic function with explicit authentication method specified
     * JWT is used if the jwt parameter is not empty
     * username + token is used if jwt is empty but username and token are
     * not empty anonymous if all of jwt, username and token are empty
     */
    WebResult GenericRequest(const std::string& method, const std::string& path,
                             const std::string& data, const std::string& accept,
                             const std::string& jwt_ = "", const std::string& username_ = "",
                             const std::string& token_ = "") {
        if (cli == nullptr) {
            cli = std::make_unique<httplib::Client>(host.c_str());
            cli->set_connection_timeout(TIMEOUT_SECONDS);
            cli->set_read_timeout(TIMEOUT_SECONDS);
            cli->set_write_timeout(TIMEOUT_SECONDS);
        }
        if (!cli->is_valid()) {
            LOG_ERROR(WebService, "Invalid URL {}", host + path);
            return WebResult{WebResult::Code::InvalidURL, "Invalid URL", ""};
        }

        httplib::Headers params;
        if (!jwt_.empty()) {
            params = {
                {std::string("Authorization"), fmt::format("Bearer {}", jwt_)},
            };
        } else if (!username_.empty()) {
            params = {
                {std::string("x-username"), username_},
                {std::string("x-token"), token_},
            };
        }

        params.emplace(std::string("api-version"),
                       std::string(API_VERSION.begin(), API_VERSION.end()));
        if (method != "GET") {
            params.emplace(std::string("Content-Type"), std::string("application/json"));
        }

        httplib::Request request;
        request.method = method;
        request.path = path;
        request.headers = params;
        request.body = data;

        httplib::Result result = cli->send(request);

        if (!result) {
            LOG_ERROR(WebService, "{} to {} returned null", method, host + path);
            return WebResult{WebResult::Code::LibError, "Null response", ""};
        }

        httplib::Response response = result.value();

        if (response.status >= 400) {
            LOG_ERROR(WebService, "{} to {} returned error status code: {}", method, host + path,
                      response.status);
            return WebResult{WebResult::Code::HttpError, std::to_string(response.status), ""};
        }

        auto content_type = response.headers.find("content-type");

        if (content_type == response.headers.end()) {
            LOG_ERROR(WebService, "{} to {} returned no content", method, host + path);
            return WebResult{WebResult::Code::WrongContent, "", ""};
        }

        if (content_type->second.find(accept) == std::string::npos) {
            LOG_ERROR(WebService, "{} to {} returned wrong content: {}", method, host + path,
                      content_type->second);
            return WebResult{WebResult::Code::WrongContent, "Wrong content", ""};
        }
        return WebResult{WebResult::Code::Success, "", response.body};
    }

    // Retrieve a new JWT from given username and token
    void UpdateJWT() {
        if (username.empty() || token.empty()) {
            return;
        }

        auto result = GenericRequest("POST", "/jwt/internal", "", "text/html", "", username, token);
        if (result.result_code != WebResult::Code::Success) {
            LOG_ERROR(WebService, "UpdateJWT failed");
        } else {
            std::scoped_lock lock{jwt_cache.mutex};
            jwt_cache.username = username;
            jwt_cache.token = token;
            jwt_cache.jwt = jwt = result.returned_data;
        }
    }

    std::string host;
    std::string username;
    std::string token;
    std::string jwt;
    std::unique_ptr<httplib::Client> cli;

    struct JWTCache {
        std::mutex mutex;
        std::string username;
        std::string token;
        std::string jwt;
    };
    static inline JWTCache jwt_cache;
};

Client::Client(std::string host, std::string username, std::string token)
    : impl{std::make_unique<Impl>(std::move(host), std::move(username), std::move(token))} {}

Client::~Client() = default;

WebResult Client::PostJson(const std::string& path, const std::string& data, bool allow_anonymous) {
    return impl->GenericRequest("POST", path, data, allow_anonymous, "application/json");
}

WebResult Client::GetJson(const std::string& path, bool allow_anonymous) {
    return impl->GenericRequest("GET", path, "", allow_anonymous, "application/json");
}

WebResult Client::DeleteJson(const std::string& path, const std::string& data,
                             bool allow_anonymous) {
    return impl->GenericRequest("DELETE", path, data, allow_anonymous, "application/json");
}

WebResult Client::GetPlain(const std::string& path, bool allow_anonymous) {
    return impl->GenericRequest("GET", path, "", allow_anonymous, "text/plain");
}

WebResult Client::GetImage(const std::string& path, bool allow_anonymous) {
    return impl->GenericRequest("GET", path, "", allow_anonymous, "image/png");
}

WebResult Client::GetExternalJWT(const std::string& audience) {
    return impl->GenericRequest("POST", fmt::format("/jwt/external/{}", audience), "", false,
                                "text/html");
}

} // namespace WebService
