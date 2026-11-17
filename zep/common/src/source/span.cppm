module;

export module zep.common.source.span;

import zep.common.source.position;

export class Span {
  public:
    Position start;
    Position end;

    Span() = default;

    Span(Position start, Position end) : start(start), end(end) {}

    bool contains(Position position) const {
        if (start.line == 0 || position.line < start.line || position.line > end.line) {
            return false;
        }

        if (position.line == start.line && position.column < start.column) {
            return false;
        }

        return position.line != end.line || position.column <= end.column;
    }

    static Span from_position(Position position) { return Span(position, position); }
};
