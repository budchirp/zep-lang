module;

#include <cstdint>
#include <utility>
#include <vector>

export module zep.frontend.arena.type;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;

export class TypeArena {
  private:
    std::vector<Type*> types;

  public:
    TypeArena() {
        TypeId unknown_id(0);
        types.push_back(new UnknownType(unknown_id));
    }

    TypeArena(const TypeArena&) = delete;
    TypeArena& operator=(const TypeArena&) = delete;
    TypeArena(TypeArena&&) = delete;
    TypeArena& operator=(TypeArena&&) = delete;

    ~TypeArena() {
        for (Type* type : types) {
            delete type;
        }
    }

    template <typename T, typename... Args>
    TypeId create(Args&&... args) {
        TypeId id(static_cast<std::uint32_t>(types.size()));
        Type* pointer = new T(id, std::forward<Args>(args)...);
        types.push_back(pointer);
        return id;
    }

    const Type* get(TypeId id) const { return types[id.raw()]; }

    Type* get(TypeId id) { return types[id.raw()]; }
};
