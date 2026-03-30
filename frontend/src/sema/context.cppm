module;

export module zep.frontend.sema.context;

import zep.common.source;
import zep.common.logger;
import zep.frontend.sema.env;
import zep.common.logger.diagnostic;

export class Context {
  public:
    const Source& source;
    Logger logger;

    Env env;
    DiagnosticList diagnostics;

    explicit Context(const Source& source) : source(source), logger(source) {}
};
