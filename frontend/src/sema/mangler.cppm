module;

#include <memory>
#include <string>
#include <vector>

export module zep.frontend.sema.mangler;

import zep.frontend.sema.type;

export class NameMangler {
  private:
    static std::string encode(const Type* type) {
        if (type == nullptr) {
            return "1V";
        }

        auto name = type->to_string();
        return std::to_string(name.size()) + name;
    }

    static std::string join(const std::vector<const Type*>& types) {
        std::string result;

        for (std::size_t index = 0; index < types.size(); ++index) {
            if (index > 0) {
                result += '_';
            }

            result += encode(types[index]);
        }

        return result;
    }

  public:
    static std::string mangle(const std::string& name, const std::vector<const Type*>& types) {
        if (types.empty()) {
            return name;
        }

        return name + "$" + join(types);
    }
};
