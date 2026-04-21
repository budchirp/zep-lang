module;

#include <utility>
#include <vector>

export module zep.frontend.arena.symbol;

import zep.frontend.sema.symbol;

export class SymbolArena {
  private:
    std::vector<Symbol*> symbols;

  public:
    SymbolArena() = default;

    SymbolArena(const SymbolArena&) = delete;
    SymbolArena& operator=(const SymbolArena&) = delete;
    SymbolArena(SymbolArena&&) = delete;
    SymbolArena& operator=(SymbolArena&&) = delete;

    ~SymbolArena() {
        for (Symbol* symbol : symbols) {
            delete symbol;
        }
    }

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        T* pointer = new T(std::forward<Args>(args)...);
        symbols.push_back(pointer);
        return pointer;
    }
};
