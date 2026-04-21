module;

#include <cstddef>

export module zep.common.position;

export class Position {
  private:

  public:
    std::size_t line;
    std::size_t column;

    Position() : line(0), column(0) {}

    Position(std::size_t line, std::size_t column) : line(line), column(column) {}

    void next_line() {
        ++line;
        column = 0;
    }

    void increment_column() { ++column; }
};
