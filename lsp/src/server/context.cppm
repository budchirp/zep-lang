module;

export module zep.lsp.server.context;

import zep.lsp.analysis;
import zep.lsp.document.store;
import zep.lsp.protocol.transport;

export class ServerContext {
  private:
  public:
    DocumentStore& documents;
    AnalysisService& analysis;
    Transport& transport;

    bool is_shutdown = false;
    bool is_exit = false;
    int exit_code = 0;

    ServerContext(DocumentStore& documents, AnalysisService& analysis, Transport& transport)
        : documents(documents), analysis(analysis), transport(transport) {}
};
