module;

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <variant>

export module zep.lsp.protocol.message;

export class JsonRpcId {
  private:
    std::variant<std::monostate, std::int64_t, std::string> value;

  public:
    JsonRpcId() : value(std::monostate{}) {}

    explicit JsonRpcId(std::int64_t number) : value(number) {}

    explicit JsonRpcId(std::string string_value) : value(std::move(string_value)) {}

    bool is_null() const { return std::holds_alternative<std::monostate>(value); }

    bool is_number() const { return std::holds_alternative<std::int64_t>(value); }

    bool is_string() const { return std::holds_alternative<std::string>(value); }

    std::int64_t as_number() const { return std::get<std::int64_t>(value); }

    const std::string& as_string() const { return std::get<std::string>(value); }

    nlohmann::json to_json() const {
        if (is_number()) {
            return as_number();
        }
        if (is_string()) {
            return as_string();
        }

        return nullptr;
    }

    static JsonRpcId from_json(const nlohmann::json& json) {
        if (json.is_number_integer()) {
            return JsonRpcId(json.get<std::int64_t>());
        }
        if (json.is_string()) {
            return JsonRpcId(json.get<std::string>());
        }

        return JsonRpcId();
    }
};

export class JsonRpcErrorCode {
  public:
    enum class Type : std::int32_t {
        ParseError = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603,
        ServerNotInitialized = -32002,
    };
};

export class JsonRpcError {
  private:
  public:
    std::int32_t code;
    std::string message;
    nlohmann::json data;

    explicit JsonRpcError(std::int32_t code, std::string message,
                          nlohmann::json data = nlohmann::json())
        : code(code), message(std::move(message)), data(std::move(data)) {}
};

export class Request {
  private:
  public:
    JsonRpcId id;
    std::string method;
    nlohmann::json params;

    Request(JsonRpcId id, std::string method, nlohmann::json params = nlohmann::json())
        : id(std::move(id)), method(std::move(method)), params(std::move(params)) {}
};

export class Notification {
  private:
  public:
    std::string method;
    nlohmann::json params;

    Notification(std::string method, nlohmann::json params = nlohmann::json())
        : method(std::move(method)), params(std::move(params)) {}

    nlohmann::json to_json() const {
        nlohmann::json payload;
        payload["jsonrpc"] = "2.0";
        payload["method"] = method;
        if (!params.is_null()) {
            payload["params"] = params;
        }

        return payload;
    }
};

export class Response {
  private:
  public:
    JsonRpcId id;
    nlohmann::json result;

    Response(JsonRpcId id, nlohmann::json result) : id(std::move(id)), result(std::move(result)) {}

    nlohmann::json to_json() const {
        nlohmann::json payload;
        payload["jsonrpc"] = "2.0";
        payload["id"] = id.to_json();
        payload["result"] = result;

        return payload;
    }
};

export class ErrorResponse {
  private:
  public:
    JsonRpcId id;
    JsonRpcError error;

    ErrorResponse(JsonRpcId id, JsonRpcError error) : id(std::move(id)), error(std::move(error)) {}

    nlohmann::json to_json() const {
        nlohmann::json payload;
        payload["jsonrpc"] = "2.0";
        payload["id"] = id.to_json();

        nlohmann::json error_object;
        error_object["code"] = error.code;
        error_object["message"] = error.message;
        if (!error.data.is_null()) {
            error_object["data"] = error.data;
        }

        payload["error"] = error_object;
        return payload;
    }
};

export class MessageKind {
  public:
    enum class Type : std::uint8_t {
        Request,
        Notification,
        Response,
        ErrorResponse,
        Invalid,
    };
};
