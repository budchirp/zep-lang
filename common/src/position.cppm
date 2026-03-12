module;

#include <cstddef>

export module zep.common.position;

export class Position {
  public:
    std::size_t line;
    std::size_t column;

    Position(std::size_t line, std::size_t column) : line(line), column(column) {}

    void next_line() {
        ++line;
        column = 0;
    }

    void increment_column() { ++column; }
};
