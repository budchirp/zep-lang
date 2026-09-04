module;

export module zep.common.source.span;

import zep.common.source.position;

export class Span {
  private:
  public:
    Position start;
    Position end;

    Span() = default;

    Span(Position start, Position end) : start(start), end(end) {}

    static Span from_position(Position position) { return Span(position, position); }

    static Span merge(const Span& first, const Span& last) { return Span(first.start, last.end); }
};
