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

export module zep.frontend.arena;

import zep.frontend.ast;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.type;

export template <typename T>
class Arena {
  private:
    std::vector<T*> pointers;

  public:
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

export using NodeArena = Arena<Node>;
export using ScopeArena = Arena<Scope>;
export using SymbolArena = Arena<Symbol>;
export using TypeArena = Arena<Type>;
