module;

#include <cstddef>

export module zep.common.hash;

export class Hash {
  public:
    static std::size_t combine(std::size_t seed, std::size_t value) {
        return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }
};
