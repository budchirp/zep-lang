export module zep.lsp.handlers;

import zep.lsp.dispatcher;
import zep.lsp.handlers.completion;
import zep.lsp.handlers.documents;
import zep.lsp.handlers.hover;
import zep.lsp.handlers.lifecycle;
import zep.lsp.handlers.navigation;
import zep.lsp.handlers.signature;
import zep.lsp.handlers.symbols;
import zep.lsp.handlers.tokens;
import zep.lsp.protocol;
import zep.lsp.session;

export void register_handlers(Dispatcher& dispatcher, Session& session, ProtocolCodec& protocol) {
    register_lifecycle(dispatcher, session);
    register_documents(dispatcher, session, protocol);
    register_hover(dispatcher, session, protocol);
    register_completion(dispatcher, session, protocol);
    register_tokens(dispatcher, session, protocol);
    register_navigation(dispatcher, session, protocol);
    register_symbols(dispatcher, session, protocol);
    register_signature(dispatcher, session, protocol);
}
