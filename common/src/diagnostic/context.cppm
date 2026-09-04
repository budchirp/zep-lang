module;

export module zep.common.context;

import zep.common.source;
import zep.common.logger;
import zep.common.diagnostic.collection;

export class Context {
  public:
    const Source& source;
    Logger logger;
    Diagnostics diagnostics;

    explicit Context(const Source& source) : source(source), logger(source), diagnostics(&source) {}
};
