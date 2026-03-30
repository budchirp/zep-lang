module;

#include <memory>
#include <string>

export module zep.lowerer.sema.scope.symbol;

import zep.common.logger;
import zep.lowerer.sema.type;
import zep.sema.kinds;

export class LoweredParameter {
  public:
    std::string name;
    std::shared_ptr<LoweredType> type;

    LoweredParameter(std::string name, std::shared_ptr<LoweredType> type)
        : name(std::move(name)), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Parameter(\n");

        Logger::print_indent(depth + 1);
        Logger::print("is_variadic: false,\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredSymbol {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Var, Function, Type };
    };

  protected:
    LoweredSymbol(Kind::Type kind, std::string name, Linkage::Type linkage,
                  Visibility::Type visibility, std::shared_ptr<LoweredType> type)
        : kind(kind), name(std::move(name)), linkage(linkage), visibility(visibility),
          type(std::move(type)) {}

  public:
    Kind::Type kind;
    std::string name;
    Linkage::Type linkage;
    Visibility::Type visibility;
    std::shared_ptr<LoweredType> type;

    virtual ~LoweredSymbol() = default;

    virtual void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Symbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("kind: ");
        switch (kind) {
        case Kind::Type::Var:
            Logger::print("var");
            break;
        case Kind::Type::Function:
            Logger::print("function");
            break;
        case Kind::Type::Type:
            Logger::print("type");
            break;
        }
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("linkage: ", Linkage::to_string(linkage), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
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
    static constexpr Kind::Type static_kind = Kind::Type::Type;

    LoweredTypeSymbol(std::string name, Linkage::Type linkage, Visibility::Type visibility,
                      std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("TypeSymbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("linkage: ", Linkage::to_string(linkage), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredFunctionSymbol : public LoweredSymbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    LoweredFunctionSymbol(std::string name, Linkage::Type linkage, Visibility::Type visibility,
                          std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionSymbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("linkage: ", Linkage::to_string(linkage), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredVarSymbol : public LoweredSymbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Var;

    LoweredVarSymbol(std::string name, Linkage::Type linkage, Visibility::Type visibility,
                     std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("VarSymbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("linkage: ", Linkage::to_string(linkage), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};
