module;

export module zep.checker.context;

import zep.common.source;
import zep.common.logger;
import zep.sema.env;
import zep.checker.diagnostic;

export class Context {
  public:
    const Source& source;
    Logger logger;

    Env env;
    DiagnosticList diagnostics;

    explicit Context(const Source& source) : source(source), logger(source) {}
};
