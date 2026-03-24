module;

#include <iostream>
#include <memory>
#include <string>

export module zep.lowerer.sema.scope.symbol;

import zep.common.logger;
import zep.lowerer.sema.types;
import zep.sema.kinds;

export class LoweredParameter {
  public:
    std::string name;
    std::shared_ptr<LoweredType> type;

    LoweredParameter(std::string name, std::shared_ptr<LoweredType> type)
        : name(std::move(name)), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "Parameter(\n";

        print_indent(depth + 1);
        std::cout << "is_variadic: false,\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
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

    virtual void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "Symbol(\n";

        print_indent(depth + 1);
        std::cout << "kind: ";
        switch (kind) {
        case Kind::Var:
            std::cout << "var";
            break;
        case Kind::Function:
            std::cout << "function";
            break;
        case Kind::Type:
            std::cout << "type";
            break;
        }
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "linkage: " << linkage_string(linkage) << ",\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
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
    static constexpr Kind static_kind = Kind::Type;

    LoweredTypeSymbol(std::string name, Linkage linkage, Visibility visibility,
                      std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "TypeSymbol(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "linkage: " << linkage_string(linkage) << ",\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredFunctionSymbol : public LoweredSymbol {
  public:
    static constexpr Kind static_kind = Kind::Function;

    LoweredFunctionSymbol(std::string name, Linkage linkage, Visibility visibility,
                          std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "FunctionSymbol(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "linkage: " << linkage_string(linkage) << ",\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredVarSymbol : public LoweredSymbol {
  public:
    static constexpr Kind static_kind = Kind::Var;

    LoweredVarSymbol(std::string name, Linkage linkage, Visibility visibility,
                     std::shared_ptr<LoweredType> type)
        : LoweredSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "VarSymbol(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "linkage: " << linkage_string(linkage) << ",\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type != nullptr) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};
