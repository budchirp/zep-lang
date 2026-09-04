module;

#include <cstdint>
#include <string>

export module zep.frontend.sema.kind;

export class Linkage {
  private:
  public:
    enum class Type : std::uint8_t { External, Internal, LinkOnceODR };

    static std::string to_string(Type linkage) {
        switch (linkage) {
        case Type::External:
            return "external";
        case Type::Internal:
            return "internal";
        case Type::LinkOnceODR:
            return "linkonce_odr";
        }

        return "unknown";
    }
};

export class Abi {
  private:
  public:
    enum class Type : std::uint8_t { Language, C };
};

export class Visibility {
  private:
  public:
    enum class Type : std::uint8_t { Public, Private };

    static std::string to_string(Type visibility) {
        return visibility == Type::Public ? "public" : "private";
    }
};

export class StorageKind {
  private:
  public:
    enum class Type : std::uint8_t { Var, VarMut };

    static std::string to_string(Type kind) {
        switch (kind) {
        case Type::Var:
            return "var";
        case Type::VarMut:
            return "var mut";
        }

        return "unknown";
    }
};

export class BinaryOperator {
  public:
    enum class Type : std::uint8_t {
        Plus,
        Minus,
        Asterisk,
        Divide,
        Modulo,
        Equals,
        NotEquals,
        LessThan,
        GreaterThan,
        LessEqual,
        GreaterEqual,
        And,
        Or,
        As,
        Is,
    };

    static std::string to_string(Type op) {
        static const std::string values[] = {
            "+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "as", "is",
        };
        return values[static_cast<std::size_t>(op)];
    }
};

export class UnaryOperator {
  public:
    enum class Type : std::uint8_t {
        Plus,
        Minus,
        Not,
        Dereference,
        AddressOf,
        AddressOfMut,
    };

    static std::string to_string(Type op) {
        static const std::string values[] = {"+", "-", "!", "*", "&", "&mut"};
        return values[static_cast<std::size_t>(op)];
    }
};

export class Coercion {
  public:
    enum class Type : std::uint8_t { None, BaseSlice, InterfaceValue };
};
