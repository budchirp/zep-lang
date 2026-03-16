module;

#include <iostream>
#include <memory>
#include <string>

export module zep.lowerer.symbols;

import zep.common.logger;
import zep.lowerer.types;
import zep.sema.kinds;

export enum class Linkage : std::uint8_t { External, Internal };

export std::string linkage_string(Linkage linkage) {
    return linkage == Linkage::External ? "external" : "internal";
}

export class LoweredParameter {
  public:
    std::string name;
    std::shared_ptr<LoweredType> type;

    LoweredParameter(std::string name, std::shared_ptr<LoweredType> type)
        : name(std::move(name)), type(std::move(type)) {}

    void dump(int depth) const {
        print_indent(depth);
        std::cout << "Parameter(name=\"" << name << "\", type="
                  << (type != nullptr ? type->to_string() : "null") << ")\n";
    }
};

export class LoweredSymbol {
  public:
    enum class Kind : std::uint8_t { Var, Function, Type };

  protected:
    LoweredSymbol(Kind kind, std::string name, Linkage linkage, Visibility visibility,
                  std::shared_ptr<LoweredType> type)
        : kind(kind), name(std::move(name)), linkage(linkage), visibility(visibility),
          type(std::move(type)) {}

  public:
    Kind kind;
    std::string name;
    Linkage linkage;
    Visibility visibility;
    std::shared_ptr<LoweredType> type;

    virtual ~LoweredSymbol() = default;

    virtual void dump(int depth) const {
        print_indent(depth);
        std::cout << "Symbol(kind=" << static_cast<int>(kind) << ", name=\"" << name
                  << "\", linkage=" << linkage_string(linkage)
                  << ", visibility=" << visibility_string(visibility)
                  << ", type=" << (type != nullptr ? type->to_string() : "null") << ")\n";
    }

    template <typename T>
    T* as() {
        if (kind == T::static_kind) {
            return static_cast<T*>(this);
        }
        return nullptr;
    }

    template <typename T>
    const T* as() const {
        if (kind == T::static_kind) {
            return static_cast<const T*>(this);
        }
        return nullptr;
    }
};

export class LoweredTypeSymbol : public LoweredSymbol {
  public:
    static constexpr Kind static_kind = Kind::Type;

    LoweredTypeSymbol(std::string name, Linkage linkage, Visibility visibility,
                      std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "TypeSymbol(name=\"" << name << "\", linkage=" << linkage_string(linkage)
                  << ", visibility=" << visibility_string(visibility)
                  << ", type=" << (type != nullptr ? type->to_string() : "null") << ")\n";
    }
};

export class LoweredFunctionSymbol : public LoweredSymbol {
  public:
    static constexpr Kind static_kind = Kind::Function;

    LoweredFunctionSymbol(std::string name, Linkage linkage, Visibility visibility,
                          std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "FunctionSymbol(name=\"" << name << "\", linkage=" << linkage_string(linkage)
                  << ", visibility=" << visibility_string(visibility)
                  << ", type=" << (type != nullptr ? type->to_string() : "null") << ")\n";
    }
};

export class LoweredVarSymbol : public LoweredSymbol {
  public:
    static constexpr Kind static_kind = Kind::Var;

    LoweredVarSymbol(std::string name, Linkage linkage, Visibility visibility,
                     std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "VarSymbol(name=\"" << name << "\", linkage=" << linkage_string(linkage)
                  << ", visibility=" << visibility_string(visibility)
                  << ", type=" << (type != nullptr ? type->to_string() : "null") << ")\n";
    }
};
