module;

#include <cstdint>

export module zep.codegen.format;

export class CodegenFormat {
  public:
    enum class Type : std::uint8_t { Object, CSource, Assembly };
};
