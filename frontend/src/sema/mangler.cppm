module;

#include <memory>
#include <string>
#include <vector>

export module zep.frontend.sema.mangler;

import zep.frontend.sema.type;

export class NameMangler {
  private:
    static std::string encode_type(const std::shared_ptr<Type>& type) {
        if (type == nullptr) {
            return "1V";
        }

        auto name = type->to_string();
        return std::to_string(name.size()) + name;
    }

    static std::string join_type_suffix(const std::vector<std::shared_ptr<Type>>& types) {
        std::string result;

        for (std::size_t index = 0; index < types.size(); ++index) {
            if (index > 0) {
                result += '_';
            }
            result += encode_type(types[index]);
        }

        return result;
    }

  public:
    static std::string mangle(const std::string& name,
                              const std::vector<std::shared_ptr<Type>>& types) {
        if (types.empty()) {
            return name;
        }

        return name + "$" + join_type_suffix(types);
    }
};
