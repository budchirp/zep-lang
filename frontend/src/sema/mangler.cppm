module;

#include <string>
#include <vector>

export module zep.frontend.sema.mangler;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.sema.type.type_helper;
import zep.frontend.arena.type;

export class NameMangler {
  private:
    static std::string encode_type(const TypeArena& arena, TypeId id) {
        if (!id.is_valid()) {
            return "1V";
        }

        std::string name = TypeHelper(arena).type_to_string(id);
        return std::to_string(name.size()) + name;
    }

    static std::string join_type_suffix(const TypeArena& arena,
                                        const std::vector<TypeId>& types) {
        std::string result;

        for (std::size_t index = 0; index < types.size(); ++index) {
            if (index > 0) {
                result += '_';
            }
            result += encode_type(arena, types[index]);
        }

        return result;
    }

  public:
    static std::string mangle(const std::string& name, const TypeArena& arena,
                              const std::vector<TypeId>& types) {
        if (types.empty()) {
            return name;
        }

        return name + "$" + join_type_suffix(arena, types);
    }
};
