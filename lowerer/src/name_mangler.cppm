module;

#include <memory>
#include <string>
#include <vector>

export module zep.lowerer.name_mangler;

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

  public:
    static std::string encode_type(const std::shared_ptr<Type>& type) {
        if (type == nullptr) {
            return "void";
        }

        return sanitize(type->to_string());
    }

    static std::string mangle_function_if_needed(
        const std::string& name, const std::vector<std::shared_ptr<Type>>& generic_type_arguments,
        const std::vector<std::shared_ptr<Type>>& parameter_types,
        const std::shared_ptr<Type>& return_type, bool is_variadic, bool has_overloads) {
        if (generic_type_arguments.empty() && !has_overloads) {
            return name;
        }
        return mangle_function(name, generic_type_arguments, parameter_types, return_type,
                               is_variadic);
    }

    static std::string
    mangle_function(const std::string& name,
                    const std::vector<std::shared_ptr<Type>>& generic_type_arguments,
                    const std::vector<std::shared_ptr<Type>>& parameter_types,
                    const std::shared_ptr<Type>& return_type, bool is_variadic) {
        std::string result = name;

        if (!generic_type_arguments.empty()) {
            result += "_G";
            for (const auto& type : generic_type_arguments) {
                result += "_" + encode_type(type);
            }
        }

        result += "_P";
        for (const auto& type : parameter_types) {
            result += "_" + encode_type(type);
        }

        result += "_R_" + encode_type(return_type);

        if (is_variadic) {
            result += "_VA";
        }

        return result;
    }

    static std::string
    mangle_struct(const std::string& name,
                  const std::vector<std::shared_ptr<Type>>& generic_type_arguments) {
        if (generic_type_arguments.empty()) {
            return name;
        }

        std::string result = name + "_G";
        for (const auto& type : generic_type_arguments) {
            result += "_" + encode_type(type);
        }

        return result;
    }
};
