module;

#include <utility>
#include <vector>

export module zep.frontend.arena.scope;

import zep.frontend.sema.scope;

export class ScopeArena {
  private:
    std::vector<Scope*> scopes;

  public:
    ScopeArena() = default;

    ScopeArena(const ScopeArena&) = delete;
    ScopeArena& operator=(const ScopeArena&) = delete;
    ScopeArena(ScopeArena&&) = delete;
    ScopeArena& operator=(ScopeArena&&) = delete;

    ~ScopeArena() {
        for (Scope* scope : scopes) {
            delete scope;
        }
    }

    template <typename... Args>
    Scope* create(Args&&... args) {
        Scope* pointer = new Scope(std::forward<Args>(args)...);
        scopes.push_back(pointer);
        return pointer;
    }
};
