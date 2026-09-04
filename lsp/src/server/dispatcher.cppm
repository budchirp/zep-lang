module;

#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>

export module zep.lsp.server.dispatcher;

import zep.lsp.protocol.message;

export class Dispatcher {
  public:
    using RequestHandler = std::function<nlohmann::json(const nlohmann::json&)>;
    using NotificationHandler = std::function<void(const nlohmann::json&)>;

  private:
    std::unordered_map<std::string, RequestHandler> request_handlers;
    std::unordered_map<std::string, NotificationHandler> notification_handlers;

  public:
    Dispatcher() = default;

    void register_request(std::string method, RequestHandler handler) {
        request_handlers.insert_or_assign(std::move(method), std::move(handler));
    }

    void register_notification(std::string method, NotificationHandler handler) {
        notification_handlers.insert_or_assign(std::move(method), std::move(handler));
    }

    bool has_request_handler(const std::string& method) const {
        return request_handlers.contains(method);
    }

    bool has_notification_handler(const std::string& method) const {
        return notification_handlers.contains(method);
    }

    nlohmann::json dispatch_request(const Request& request) const {
        auto iterator = request_handlers.find(request.method);
        if (iterator == request_handlers.end()) {
            ErrorResponse error(
                request.id,
                JsonRpcError(static_cast<std::int32_t>(JsonRpcErrorCode::Type::MethodNotFound),
                             "Method not found: " + request.method));
            return error.to_json();
        }

        try {
            auto result = iterator->second(request.params);
            Response response(request.id, std::move(result));
            return response.to_json();
        } catch (const std::exception& exception) {
            ErrorResponse error(request.id, JsonRpcError(static_cast<std::int32_t>(
                                                             JsonRpcErrorCode::Type::InternalError),
                                                         exception.what()));
            return error.to_json();
        }
    }

    void dispatch_notification(const Notification& notification) const {
        auto iterator = notification_handlers.find(notification.method);
        if (iterator == notification_handlers.end()) {
            return;
        }

        try {
            iterator->second(notification.params);
        } catch (...) {}
    }
};
