module;

export module zep.common.context;

import zep.common.source;
import zep.common.logger;
import zep.common.logger.diagnostic;

export class Context {
  public:
    const Source& source;
    Logger logger;
    DiagnosticList diagnostics;

    explicit Context(const Source& source)
        : source(source), logger(source) {}
};
