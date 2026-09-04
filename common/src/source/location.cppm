module;

export module zep.common.source.location;

import zep.common.source;
import zep.common.source.span;

export class SourceLocation {
  public:
    const Source* source;
    Span span;

    SourceLocation(const Source& source, Span span) : source(&source), span(span) {}

    SourceLocation(const Source* source, Span span) : source(source), span(span) {}
};
