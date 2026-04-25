module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

export module zep.common.arena;

export template <typename T>
class Arena {
  private:
    std::vector<T*> pointers;

  public:
    Arena() = default;

    ~Arena() {
        for (auto* pointer : pointers) {
            delete pointer;
        }
    }

    template <typename U, typename... Args>
        requires(std::is_base_of_v<T, U>)
    [[nodiscard]] U* create(Args&&... args) {
        auto* pointer = new U(std::forward<Args>(args)...);
        pointers.push_back(pointer);

        return pointer;
    }
};
