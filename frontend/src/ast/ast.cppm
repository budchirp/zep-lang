module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.frontend.ast;

import zep.common.position;
import zep.common.logger;
import zep.sema.type;
export import zep.sema.kinds;

export template <typename T>
class Visitor;

export class Node {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            TypeExpression,
            GenericParameter,
            GenericArgument,
            Parameter,
            Argument,
            FunctionPrototype,
            StructField,
            StructLiteralField,

            NumberLiteral,
            FloatLiteral,
            StringLiteral,
            BooleanLiteral,
            IdentifierExpression,
            BinaryExpression,
            UnaryExpression,
            CallExpression,
            IndexExpression,
            MemberExpression,
            AssignExpression,
            StructLiteralExpression,
            IfExpression,

            BlockStatement,
            ExpressionStatement,
            ReturnStatement,
            StructDeclaration,
            VarDeclaration,
            FunctionDeclaration,
            ExternFunctionDeclaration,
            ExternVarDeclaration,
            ImportStatement,
        };
    };

  protected:
    explicit Node(Kind::Type kind, Position position) : kind(kind), position(position) {}

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = default;
    Node& operator=(Node&&) = default;

  public:
    Kind::Type kind;
    Position position;

    virtual ~Node() = default;

    virtual void dump(int depth, bool with_indent = true, bool trailing_newline = true) const = 0;

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

export class Expression : public Node {
  private:
    std::shared_ptr<Type> type;

  public:
    using Node::Node;

    void set_type(std::shared_ptr<Type> type) { this->type = std::move(type); }
    std::shared_ptr<Type> get_type() const { return type; }
};

export class Statement : public Node {
  private:
    std::shared_ptr<Type> type;

  public:
    using Node::Node;

    void set_type(std::shared_ptr<Type> type) { this->type = std::move(type); }
    std::shared_ptr<Type> get_type() const { return type; }
};

export class TypeExpression : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::TypeExpression;

    std::shared_ptr<Type> type;

    TypeExpression(Position position, std::shared_ptr<Type> type)
        : Node(static_kind, position), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("TypeExpression(type: ");
        if (type) {
            type->dump(depth, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }

    void set_type(std::shared_ptr<Type> type) { this->type = std::move(type); }
    std::shared_ptr<Type> get_type() const { return type; }
};

export class GenericParameter : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::GenericParameter;

    std::string name;
    std::unique_ptr<TypeExpression> constraint;

    GenericParameter(Position position, std::string name,
                     std::unique_ptr<TypeExpression> constraint)
        : Node(static_kind, position), name(std::move(name)), constraint(std::move(constraint)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericParameter(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("constraint: ");
        if (constraint) {
            constraint->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }

    const std::string& get_name() const { return name; }
    TypeExpression* get_constraint() const { return constraint.get(); }
};

export class GenericArgument : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::GenericArgument;

    std::string name;
    std::unique_ptr<TypeExpression> type;

    GenericArgument(Position position, std::string name, std::unique_ptr<TypeExpression> type)
        : Node(static_kind, position), name(std::move(name)), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericArgument(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type) {
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Parameter : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Parameter;

    bool is_variadic;

    std::string name;
    std::unique_ptr<TypeExpression> type;

    Parameter(Position position, bool is_variadic, std::string name,
              std::unique_ptr<TypeExpression> type)
        : Node(static_kind, position), is_variadic(is_variadic), name(std::move(name)),
          type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Parameter(\n");

        Logger::print_indent(depth + 1);
        Logger::print("is_variadic: ", (is_variadic ? "true" : "false"), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type) {
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Argument : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Argument;

    std::string name;
    std::unique_ptr<Expression> value;

    Argument(Position position, std::string name, std::unique_ptr<Expression> value)
        : Node(static_kind, position), name(std::move(name)), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Argument(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        value->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FunctionPrototype : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::FunctionPrototype;

    std::string name;

    std::vector<std::unique_ptr<GenericParameter>> generic_parameters;
    std::vector<std::unique_ptr<Parameter>> parameters;

    std::unique_ptr<TypeExpression> return_type;

    FunctionPrototype(Position position, std::string name,
                      std::vector<std::unique_ptr<GenericParameter>> generic_parameters,
                      std::vector<std::unique_ptr<Parameter>> parameters,
                      std::unique_ptr<TypeExpression> return_type)
        : Node(static_kind, position), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), parameters(std::move(parameters)),
          return_type(std::move(return_type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionPrototype(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_parameters: [");
        if (generic_parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                generic_parameters[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < generic_parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("parameters: [");
        if (parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < parameters.size(); ++i) {
                parameters[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("return_type: ");
        if (return_type) {
            return_type->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructField : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::StructField;

    Visibility::Type visibility;

    std::string name;
    std::unique_ptr<TypeExpression> type;

    StructField(Position position, Visibility::Type visibility, std::string name,
                std::unique_ptr<TypeExpression> type)
        : Node(static_kind, position), visibility(visibility), name(std::move(name)),
          type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructField(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type) {
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructLiteralField : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::StructLiteralField;

    std::string name;
    std::unique_ptr<Expression> value;

    StructLiteralField(Position position, std::string name, std::unique_ptr<Expression> value)
        : Node(static_kind, position), name(std::move(name)), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructLiteralField(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        value->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class NumberLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::NumberLiteral;

    std::string value;

    NumberLiteral(Position position, std::string value)
        : Expression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("NumberLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FloatLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::FloatLiteral;

    std::string value;

    FloatLiteral(Position position, std::string value)
        : Expression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FloatLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StringLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::StringLiteral;

    std::string value;

    StringLiteral(Position position, std::string value)
        : Expression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StringLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BooleanLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::BooleanLiteral;

    bool value;

    BooleanLiteral(Position position, bool value)
        : Expression(static_kind, position), value(value) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BooleanLiteral(value: ", (value ? "true" : "false"), ")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IdentifierExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::IdentifierExpression;

    std::string name;

    IdentifierExpression(Position position, std::string name)
        : Expression(static_kind, position), name(std::move(name)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IdentifierExpression(name: \"", name, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
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
        Is
    };
};

export class UnaryOperator {
  public:
    enum class Type : std::uint8_t {
        Plus,
        Minus,
        Not,
        Dereference,
        AddressOf,
    };
};

export class BinaryExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::BinaryExpression;

  private:
    static std::string operator_string(BinaryOperator::Type op) {
        switch (op) {
        case BinaryOperator::Type::Plus:
            return "+";
        case BinaryOperator::Type::Minus:
            return "-";
        case BinaryOperator::Type::Asterisk:
            return "*";
        case BinaryOperator::Type::Divide:
            return "/";
        case BinaryOperator::Type::Modulo:
            return "%";
        case BinaryOperator::Type::Equals:
            return "==";
        case BinaryOperator::Type::NotEquals:
            return "!=";
        case BinaryOperator::Type::LessThan:
            return "<";
        case BinaryOperator::Type::GreaterThan:
            return ">";
        case BinaryOperator::Type::LessEqual:
            return "<=";
        case BinaryOperator::Type::GreaterEqual:
            return ">=";
        case BinaryOperator::Type::And:
            return "&&";
        case BinaryOperator::Type::Or:
            return "||";
        case BinaryOperator::Type::As:
            return "as";
        case BinaryOperator::Type::Is:
            return "is";
        }
        return "?";
    }

  public:
    std::unique_ptr<Expression> left;
    BinaryOperator::Type op;
    std::unique_ptr<Expression> right;

    BinaryExpression(Position position, std::unique_ptr<Expression> left, BinaryOperator::Type op,
                     std::unique_ptr<Expression> right)
        : Expression(static_kind, position), left(std::move(left)), op(op),
          right(std::move(right)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BinaryExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("left: ");
        left->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("op: ", operator_string(op), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("right: ");
        right->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class UnaryExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::UnaryExpression;

  private:
    static std::string operator_string(UnaryOperator::Type op) {
        switch (op) {
        case UnaryOperator::Type::Plus:
            return "+";
        case UnaryOperator::Type::Minus:
            return "-";
        case UnaryOperator::Type::Not:
            return "!";
        case UnaryOperator::Type::Dereference:
            return "*";
        case UnaryOperator::Type::AddressOf:
            return "&";
        }
        return "?";
    }

  public:
    UnaryOperator::Type op;
    std::unique_ptr<Expression> operand;

    UnaryExpression(Position position, UnaryOperator::Type op, std::unique_ptr<Expression> operand)
        : Expression(static_kind, position), op(op), operand(std::move(operand)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("UnaryExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("op: ", operator_string(op), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("operand: ");
        operand->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class CallExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::CallExpression;

    std::unique_ptr<Expression> callee;

    std::vector<std::unique_ptr<GenericArgument>> generic_arguments;

    std::vector<std::unique_ptr<Argument>> arguments;

    CallExpression(Position position, std::unique_ptr<Expression> callee,
                   std::vector<std::unique_ptr<GenericArgument>> generic_arguments,
                   std::vector<std::unique_ptr<Argument>> arguments)
        : Expression(static_kind, position), callee(std::move(callee)),
          generic_arguments(std::move(generic_arguments)), arguments(std::move(arguments)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("CallExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("callee: ");
        callee->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_arguments: [");
        if (generic_arguments.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                generic_arguments[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < generic_arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("arguments: [");
        if (arguments.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                arguments[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IndexExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::IndexExpression;

    std::unique_ptr<Expression> value;
    std::unique_ptr<Expression> index;

    IndexExpression(Position position, std::unique_ptr<Expression> value,
                    std::unique_ptr<Expression> index)
        : Expression(static_kind, position), value(std::move(value)), index(std::move(index)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IndexExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        value->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("index: ");
        index->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class MemberExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::MemberExpression;

    std::unique_ptr<Expression> value;
    std::string member;

    MemberExpression(Position position, std::unique_ptr<Expression> value, std::string member)
        : Expression(static_kind, position), value(std::move(value)), member(std::move(member)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("MemberExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        value->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("member: \"", member, "\"\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class AssignExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::AssignExpression;

    std::unique_ptr<Expression> target;
    std::unique_ptr<Expression> value;

    AssignExpression(Position position, std::unique_ptr<Expression> target,
                     std::unique_ptr<Expression> value)
        : Expression(static_kind, position), target(std::move(target)), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("AssignExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("target: ");
        target->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        value->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructLiteralExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::StructLiteralExpression;

    std::unique_ptr<IdentifierExpression> name;

    std::vector<std::unique_ptr<GenericArgument>> generic_arguments;

    std::vector<std::unique_ptr<StructLiteralField>> fields;

    StructLiteralExpression(Position position, std::unique_ptr<IdentifierExpression> name,
                            std::vector<std::unique_ptr<GenericArgument>> generic_arguments,
                            std::vector<std::unique_ptr<StructLiteralField>> fields)
        : Expression(static_kind, position), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructLiteralExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: ");
        name->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_arguments: [");
        if (generic_arguments.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                generic_arguments[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < generic_arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("fields: [");
        if (fields.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < fields.size(); ++i) {
                fields[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < fields.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BlockStatement : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::BlockStatement;

    std::vector<std::unique_ptr<Statement>> statements;

    BlockStatement(Position position, std::vector<std::unique_ptr<Statement>> statements)
        : Statement(static_kind, position), statements(std::move(statements)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BlockStatement(statements: [");
        if (statements.empty()) {
            Logger::print("])");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < statements.size(); ++i) {
                statements[i]->dump(depth + 1, true, false);
                Logger::print((i + 1 < statements.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])");
        }

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExpressionStatement : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ExpressionStatement;

    std::unique_ptr<Expression> expression;

    explicit ExpressionStatement(std::unique_ptr<Expression> expression)
        : Statement(static_kind, expression->position), expression(std::move(expression)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ExpressionStatement(expression: ");
        expression->dump(depth, false, false);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IfExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::IfExpression;

    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> then_branch;
    std::unique_ptr<Statement> else_branch;

    IfExpression(Position position, std::unique_ptr<Expression> condition,
                 std::unique_ptr<Statement> then_branch, std::unique_ptr<Statement> else_branch)
        : Expression(static_kind, position), condition(std::move(condition)),
          then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IfStatement(\n");

        Logger::print_indent(depth + 1);
        Logger::print("condition: ");
        condition->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("then_branch: ");
        then_branch->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("else_branch: ");
        if (else_branch) {
            else_branch->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ReturnStatement : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ReturnStatement;

    std::unique_ptr<Expression> value;

    ReturnStatement(Position position, std::unique_ptr<Expression> value)
        : Statement(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ReturnStatement(value: ");
        if (value) {
            value->dump(depth, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::StructDeclaration;

    Visibility::Type visibility;

    std::string name;

    std::vector<std::unique_ptr<GenericParameter>> generic_parameters;

    std::vector<std::unique_ptr<StructField>> fields;

    StructDeclaration(Position position, Visibility::Type visibility, std::string name,
                      std::vector<std::unique_ptr<GenericParameter>> generic_parameters,
                      std::vector<std::unique_ptr<StructField>> fields)
        : Statement(static_kind, position), visibility(visibility), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_parameters: [");
        if (generic_parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                generic_parameters[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < generic_parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("fields: [");
        if (fields.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < fields.size(); ++i) {
                fields[i]->dump(depth + 2, true, false);
                Logger::print((i + 1 < fields.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class VarDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::VarDeclaration;

    Visibility::Type visibility;
    StorageKind::Type storage_kind;

    std::string name;
    std::unique_ptr<TypeExpression> type;
    std::unique_ptr<Expression> initializer;

    VarDeclaration(Position position, Visibility::Type visibility, StorageKind::Type storage_kind,
                   std::string name, std::unique_ptr<TypeExpression> type,
                   std::unique_ptr<Expression> initializer)
        : Statement(static_kind, position), visibility(visibility), storage_kind(storage_kind),
          name(std::move(name)), type(std::move(type)), initializer(std::move(initializer)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("VarDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("storage_kind: ", StorageKind::to_string(storage_kind), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (type) {
            type->dump(depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("initializer: ");
        if (initializer) {
            initializer->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FunctionDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::FunctionDeclaration;

    Visibility::Type visibility;

    std::unique_ptr<FunctionPrototype> prototype;
    std::unique_ptr<BlockStatement> body;

    FunctionDeclaration(Position position, Visibility::Type visibility,
                        std::unique_ptr<FunctionPrototype> prototype,
                        std::unique_ptr<BlockStatement> body)
        : Statement(static_kind, position), visibility(visibility), prototype(std::move(prototype)),
          body(std::move(body)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("prototype: ");
        prototype->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        body->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExternFunctionDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ExternFunctionDeclaration;

    Visibility::Type visibility;

    std::unique_ptr<FunctionPrototype> prototype;

    ExternFunctionDeclaration(Position position, Visibility::Type visibility,
                              std::unique_ptr<FunctionPrototype> prototype)
        : Statement(static_kind, position), visibility(visibility),
          prototype(std::move(prototype)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ExternFunctionDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("prototype: ");
        prototype->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExternVarDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ExternVarDeclaration;

    Visibility::Type visibility;

    std::string name;
    std::unique_ptr<TypeExpression> type;

    ExternVarDeclaration(Position position, Visibility::Type visibility, std::string name,
                         std::unique_ptr<TypeExpression> type)
        : Statement(static_kind, position), visibility(visibility), name(std::move(name)),
          type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ExternVarDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        type->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ImportStatement : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ImportStatement;

    std::vector<std::unique_ptr<IdentifierExpression>> path;

    ImportStatement(Position position, std::vector<std::unique_ptr<IdentifierExpression>> path)
        : Statement(static_kind, position), path(std::move(path)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ImportStatement(path: [");
        if (path.empty()) {
            Logger::print("])");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < path.size(); ++i) {
                path[i]->dump(depth + 1, true, false);
                Logger::print((i + 1 < path.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])");
        }

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export template <typename T>
class Visitor {
  public:
    virtual ~Visitor() = default;

    virtual T visit(TypeExpression& node) = 0;
    virtual T visit(GenericParameter& node) = 0;
    virtual T visit(GenericArgument& node) = 0;
    virtual T visit(Parameter& node) = 0;
    virtual T visit(Argument& node) = 0;
    virtual T visit(FunctionPrototype& node) = 0;
    virtual T visit(StructField& node) = 0;
    virtual T visit(StructLiteralField& node) = 0;

    virtual T visit(NumberLiteral& node) = 0;
    virtual T visit(FloatLiteral& node) = 0;
    virtual T visit(StringLiteral& node) = 0;
    virtual T visit(BooleanLiteral& node) = 0;
    virtual T visit(IdentifierExpression& node) = 0;
    virtual T visit(BinaryExpression& node) = 0;
    virtual T visit(UnaryExpression& node) = 0;
    virtual T visit(CallExpression& node) = 0;
    virtual T visit(IndexExpression& node) = 0;
    virtual T visit(MemberExpression& node) = 0;
    virtual T visit(AssignExpression& node) = 0;
    virtual T visit(StructLiteralExpression& node) = 0;

    virtual T visit(BlockStatement& node) = 0;
    virtual T visit(ExpressionStatement& node) = 0;
    virtual T visit(IfExpression& node) = 0;
    virtual T visit(ReturnStatement& node) = 0;
    virtual T visit(StructDeclaration& node) = 0;
    virtual T visit(VarDeclaration& node) = 0;
    virtual T visit(FunctionDeclaration& node) = 0;
    virtual T visit(ExternFunctionDeclaration& node) = 0;
    virtual T visit(ExternVarDeclaration& node) = 0;
    virtual T visit(ImportStatement& node) = 0;
};
