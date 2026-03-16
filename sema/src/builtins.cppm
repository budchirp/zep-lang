module;

#include <memory>
#include <string>
#include <unordered_map>

export module zep.sema.builtins;

import zep.sema.scope;
import zep.sema.type;

export class Builtins {
  private:
    std::unordered_map<std::string, std::shared_ptr<Type>> types;

  public:
    Builtins() {
        types["void"] = std::make_shared<VoidType>();
        types["string"] = std::make_shared<StringType>();
        types["bool"] = std::make_shared<BooleanType>();
        types["any"] = std::make_shared<AnyType>();
        types["unknown"] = std::make_shared<UnknownType>();
        for (int size : {8, 16, 32, 64}) {
            types["i" + std::to_string(size)] =
                std::make_shared<IntegerType>(false, static_cast<std::uint8_t>(size));
            types["u" + std::to_string(size)] =
                std::make_shared<IntegerType>(true, static_cast<std::uint8_t>(size));
        }
        for (int size : {32, 64}) {
            types["f" + std::to_string(size)] =
                std::make_shared<FloatType>(static_cast<std::uint8_t>(size));
        }
    }

    std::shared_ptr<Type> lookup_type(const std::string& name) const {
        auto iterator = types.find(name);
        if (iterator != types.end()) {
            return iterator->second;
        }
        return nullptr;
    }

    void register_into(Scope& scope) const {
        for (const auto& [name, type] : types) {
            scope.define_type(name, type);
        }
    }
};

