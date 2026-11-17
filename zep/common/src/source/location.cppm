module;

#include <memory>
#include <utility>

export module zep.common.source.location;

import zep.common.source;
import zep.common.source.span;

export class SourceLocation {
  private:
    std::shared_ptr<const Source> owner;

  public:
    const Source* source;
    Span span;

    SourceLocation(const Source* source, Span span) : owner(), source(source), span(span) {}

    SourceLocation(std::shared_ptr<const Source> source, Span span)
        : owner(std::move(source)), source(owner.get()), span(span) {}
};
