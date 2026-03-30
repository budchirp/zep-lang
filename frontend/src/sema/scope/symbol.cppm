module;

#include <memory>
#include <string>

export module zep.frontend.sema.symbol;

import zep.common.logger;
import zep.common.position;
import zep.frontend.sema.kinds;
import zep.frontend.sema.type;

export class Symbol {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Var,
            Function,
            Type,
        };
    };

  protected:
    Symbol(Kind::Type kind, std::string name, Position position, Visibility::Type visibility,
           std::shared_ptr<Type> type)
        : kind(kind), position(position), name(std::move(name)), visibility(visibility),
          type(std::move(type)) {}

  private:
    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;
    Symbol(Symbol&&) = default;
    Symbol& operator=(Symbol&&) = default;

  public:
    Kind::Type kind;

    Position position;

    std::string name;

    Visibility::Type visibility;

    std::shared_ptr<Type> type;

    virtual ~Symbol() = default;

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

export class VarSymbol : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Var;

    StorageKind::Type storage_kind;

    VarSymbol(std::string name, Position position, Visibility::Type visibility,
              StorageKind::Type storage_kind, std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), position, visibility, std::move(type)),
          storage_kind(storage_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("VarSymbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("storage_kind: ", StorageKind::to_string(storage_kind), ",\n");

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

export class FunctionSymbol : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    FunctionSymbol(std::string name, Position position, Visibility::Type visibility,
                   std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), position, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionSymbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

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

export class TypeSymbol : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Type;

    TypeSymbol(std::string name, Position position, Visibility::Type visibility,
               std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), position, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("TypeSymbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

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
