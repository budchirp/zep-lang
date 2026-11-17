module;

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

export module zep.lsp.server;

import zep.lsp.dispatcher;
import zep.lsp.handlers;
import zep.lsp.protocol;
import zep.lsp.session;
import zep.lsp.transport;

export class Server {
  private:
    Transport transport;
    Session session;
    ProtocolCodec protocol;
    Dispatcher dispatcher;

  public:
    Server(std::istream& input, std::ostream& output, std::filesystem::path standard_library)
        : transport(input, output), session(transport, std::move(standard_library)),
          protocol(session) {
        register_handlers(dispatcher, session, protocol);
    }

    int run() {
        while (session.state != Session::State::Exited) {
            auto message = transport.read();
            if (!message.has_value()) {
                if (transport.read_status == Transport::ReadStatus::End) {
                    break;
                }
                auto code = transport.read_status == Transport::ReadStatus::InvalidJson
                                ? -32700
                                : -32600;
                auto message_text = code == -32700 ? "Parse error" : "Invalid request";
                transport.write({{"jsonrpc", "2.0"},
                                 {"id", nullptr},
                                 {"error", {{"code", code}, {"message", message_text}}}});
                continue;
            }

            auto method = message->value("method", std::string());
            if (session.state == Session::State::Created && method != "initialize" &&
                method != "exit") {
                if (message->contains("id")) {
                    transport.write({{"jsonrpc", "2.0"},
                                     {"id", message->at("id")},
                                     {"error", {{"code", -32002},
                                                {"message", "Server not initialized"}}}});
                }
                continue;
            }

            if (session.state == Session::State::Shutdown && method != "exit") {
                if (message->contains("id")) {
                    transport.write({{"jsonrpc", "2.0"},
                                     {"id", message->at("id")},
                                     {"error", {{"code", -32600},
                                                {"message", "Server has shut down"}}}});
                }
                continue;
            }

            auto response = dispatcher.dispatch(*message);
            if (response.has_value()) {
                transport.write(*response);
            }
        }
        return session.exit_code;
    }
};
