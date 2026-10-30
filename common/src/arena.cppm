module;

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

export module zep.common.arena;

export template <typename T>
class Arena {
  private:
    std::vector<std::unique_ptr<T>> pointers;

  public:
    Arena() = default;

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&&) noexcept = default;

    Arena& operator=(Arena&& other) noexcept {
        if (this != &other) {
            pointers = std::move(other.pointers);
        }
        return *this;
    }

    template <typename U, typename... Args>
        requires(std::is_base_of_v<T, U>)
    [[nodiscard]] U* create(Args&&... args) {
        auto pointer = std::make_unique<U>(std::forward<Args>(args)...);
        auto* result = pointer.get();
        pointers.push_back(std::move(pointer));

        return result;
    }
};
