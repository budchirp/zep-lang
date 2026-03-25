module;

#include <memory>
#include <string>
#include <vector>

export module zep.lowerer.mangler;

import zep.sema.type;

export class NameMangler {
  private:
    static std::string sanitize(const std::string& input) {
        std::string result;
        result.reserve(input.size());

        for (char c : input) {
            switch (c) {
            case '*':
                result += 'P';
                break;
            case ' ':
                result += '_';
                break;
            case '[':
                result += '_';
                break;
            case ']':
                result += '_';
                break;
            case '<':
                result += '_';
                break;
            case '>':
                result += '_';
                break;
            case ',':
                result += '_';
                break;
            default:
                result += c;
                break;
            }
        }

        return result;
    }

    static std::string encode_type(const std::shared_ptr<Type>& type) {
        if (type == nullptr) {
            return "void";
        }

        return sanitize(type->to_string());
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
