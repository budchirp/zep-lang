module;

export module zep.common.context;

import zep.common.source;
import zep.common.diagnostic.collection;

export class Context {
  public:
    const Source& source;
    Diagnostics diagnostics;

    explicit Context(const Source& source) : source(source), diagnostics(&source) {}
};
