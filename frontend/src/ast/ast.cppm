module;

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

export module zep.frontend.ast;

import zep.common.position;
import zep.common.logger;
import zep.sema.type;

export enum class Visibility : std::uint8_t { Public, Private };

static std::string visibility_string(Visibility visibility) {
    return visibility == Visibility::Public ? "public" : "private";
}

export enum class StorageKind : std::uint8_t { Const, Var, VarMut };

static std::string storage_kind_string(StorageKind kind) {
    switch (kind) {
    case StorageKind::Const:
        return "const";
    case StorageKind::Var:
        return "var";
    case StorageKind::VarMut:
        return "var mut";
    }
    return "unknown";
}

export template <typename T>
class Visitor;

export class Node {
  private:
    Position position;

  public:
    explicit Node(Position position) : position(position) {}
    virtual ~Node() = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = default;
    Node& operator=(Node&&) = default;

    virtual void dump(int depth, bool with_indent = true, bool trailing_newline = true) const = 0;

    Position get_position() const { return position; }
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
    std::shared_ptr<Type> type;

    TypeExpression(Position position, std::shared_ptr<Type> type)
        : Node(position), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "TypeExpression(type: ";
        if (type) {
            type->dump(depth, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
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
    std::string name;
    std::unique_ptr<TypeExpression> constraint;

    GenericParameter(Position position, std::string name,
                     std::unique_ptr<TypeExpression> constraint)
        : Node(position), name(std::move(name)), constraint(std::move(constraint)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "GenericParameter(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "constraint: ";
        if (constraint) {
            constraint->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }

    const std::string& get_name() const { return name; }
    TypeExpression* get_constraint() const { return constraint.get(); }
};

export class GenericArgument : public Node {
  public:
    std::string name;
    std::unique_ptr<TypeExpression> type;

    GenericArgument(Position position, std::string name, std::unique_ptr<TypeExpression> type)
        : Node(position), name(std::move(name)), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "GenericArgument(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type) {
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Parameter : public Node {
  public:
    bool is_variadic;

    std::string name;
    std::unique_ptr<TypeExpression> type;

    Parameter(Position position, bool is_variadic, std::string name,
              std::unique_ptr<TypeExpression> type)
        : Node(position), is_variadic(is_variadic), name(std::move(name)), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "Parameter(\n";

        print_indent(depth + 1);
        std::cout << "is_variadic: " << (is_variadic ? "true" : "false") << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type) {
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Argument : public Node {
  public:
    std::string name;
    std::unique_ptr<Expression> value;

    Argument(Position position, std::string name, std::unique_ptr<Expression> value)
        : Node(position), name(std::move(name)), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "Argument(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "value: ";
        value->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FunctionPrototype : public Node {
  public:
    std::string name;

    std::vector<std::unique_ptr<GenericParameter>> generic_parameters;
    std::vector<std::unique_ptr<Parameter>> parameters;

    std::unique_ptr<TypeExpression> return_type;

    FunctionPrototype(Position position, std::string name,
                      std::vector<std::unique_ptr<GenericParameter>> generic_parameters,
                      std::vector<std::unique_ptr<Parameter>> parameters,
                      std::unique_ptr<TypeExpression> return_type)
        : Node(position), name(std::move(name)), generic_parameters(std::move(generic_parameters)),
          parameters(std::move(parameters)), return_type(std::move(return_type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "FunctionPrototype(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "generic_parameters: [";
        if (generic_parameters.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                generic_parameters[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < generic_parameters.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "parameters: [";
        if (parameters.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < parameters.size(); ++i) {
                parameters[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < parameters.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "return_type: ";
        if (return_type) {
            return_type->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructField : public Node {
  public:
    Visibility visibility;

    std::string name;
    std::unique_ptr<TypeExpression> type;

    StructField(Position position, Visibility visibility, std::string name,
                std::unique_ptr<TypeExpression> type)
        : Node(position), visibility(visibility), name(std::move(name)), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StructField(\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type) {
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructLiteralField : public Node {
  public:
    std::string name;
    std::unique_ptr<Expression> value;

    StructLiteralField(Position position, std::string name, std::unique_ptr<Expression> value)
        : Node(position), name(std::move(name)), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StructLiteralField(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "value: ";
        value->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class NumberLiteral : public Expression {
  public:
    std::string value;

    NumberLiteral(Position position, std::string value)
        : Expression(position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "NumberLiteral(value: \"" << value << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FloatLiteral : public Expression {
  public:
    std::string value;

    FloatLiteral(Position position, std::string value)
        : Expression(position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "FloatLiteral(value: \"" << value << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StringLiteral : public Expression {
  public:
    std::string value;

    StringLiteral(Position position, std::string value)
        : Expression(position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StringLiteral(value: \"" << value << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BooleanLiteral : public Expression {
  public:
    bool value;

    BooleanLiteral(Position position, bool value) : Expression(position), value(value) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "BooleanLiteral(value: " << (value ? "true" : "false") << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IdentifierExpression : public Expression {
  public:
    std::string name;

    IdentifierExpression(Position position, std::string name)
        : Expression(position), name(std::move(name)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "IdentifierExpression(name: \"" << name << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BinaryExpression : public Expression {
  public:
    enum class BinaryOperator : std::uint8_t {
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

  private:
    static std::string operator_string(BinaryOperator op) {
        switch (op) {
        case BinaryOperator::Plus:
            return "+";
        case BinaryOperator::Minus:
            return "-";
        case BinaryOperator::Asterisk:
            return "*";
        case BinaryOperator::Divide:
            return "/";
        case BinaryOperator::Modulo:
            return "%";
        case BinaryOperator::Equals:
            return "==";
        case BinaryOperator::NotEquals:
            return "!=";
        case BinaryOperator::LessThan:
            return "<";
        case BinaryOperator::GreaterThan:
            return ">";
        case BinaryOperator::LessEqual:
            return "<=";
        case BinaryOperator::GreaterEqual:
            return ">=";
        case BinaryOperator::And:
            return "&&";
        case BinaryOperator::Or:
            return "||";
        case BinaryOperator::As:
            return "as";
        case BinaryOperator::Is:
            return "is";
        }
        return "?";
    }

  public:
    std::unique_ptr<Expression> left;
    BinaryOperator op;
    std::unique_ptr<Expression> right;

    BinaryExpression(Position position, std::unique_ptr<Expression> left, BinaryOperator op,
                     std::unique_ptr<Expression> right)
        : Expression(position), left(std::move(left)), op(op), right(std::move(right)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "BinaryExpression(\n";

        print_indent(depth + 1);
        std::cout << "left: ";
        left->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "op: " << operator_string(op) << ",\n";

        print_indent(depth + 1);
        std::cout << "right: ";
        right->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class UnaryExpression : public Expression {
  public:
    enum class UnaryOperator : std::uint8_t {
        Plus,
        Minus,
        Not,
        Dereference,
        AddressOf,
    };

  private:
    static std::string operator_string(UnaryOperator op) {
        switch (op) {
        case UnaryOperator::Plus:
            return "+";
        case UnaryOperator::Minus:
            return "-";
        case UnaryOperator::Not:
            return "!";
        case UnaryOperator::Dereference:
            return "*";
        case UnaryOperator::AddressOf:
            return "&";
        }
        return "?";
    }

  public:
    UnaryOperator op;
    std::unique_ptr<Expression> operand;

    UnaryExpression(Position position, UnaryOperator op, std::unique_ptr<Expression> operand)
        : Expression(position), op(op), operand(std::move(operand)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "UnaryExpression(\n";

        print_indent(depth + 1);
        std::cout << "op: " << operator_string(op) << ",\n";

        print_indent(depth + 1);
        std::cout << "operand: ";
        operand->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class CallExpression : public Expression {
  public:
    std::unique_ptr<Expression> callee;

    std::vector<std::unique_ptr<GenericArgument>> generic_arguments;

    std::vector<std::unique_ptr<Argument>> arguments;

    CallExpression(Position position, std::unique_ptr<Expression> callee,
                   std::vector<std::unique_ptr<GenericArgument>> generic_arguments,
                   std::vector<std::unique_ptr<Argument>> arguments)
        : Expression(position), callee(std::move(callee)),
          generic_arguments(std::move(generic_arguments)), arguments(std::move(arguments)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "CallExpression(\n";

        print_indent(depth + 1);
        std::cout << "callee: ";
        callee->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "generic_arguments: [";
        if (generic_arguments.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                generic_arguments[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < generic_arguments.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "arguments: [";
        if (arguments.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                arguments[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < arguments.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IndexExpression : public Expression {
  public:
    std::unique_ptr<Expression> value;
    std::unique_ptr<Expression> index;

    IndexExpression(Position position, std::unique_ptr<Expression> value,
                    std::unique_ptr<Expression> index)
        : Expression(position), value(std::move(value)), index(std::move(index)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "IndexExpression(\n";

        print_indent(depth + 1);
        std::cout << "value: ";
        value->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "index: ";
        index->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class MemberExpression : public Expression {
  public:
    std::unique_ptr<Expression> value;
    std::string member;

    MemberExpression(Position position, std::unique_ptr<Expression> value, std::string member)
        : Expression(position), value(std::move(value)), member(std::move(member)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "MemberExpression(\n";

        print_indent(depth + 1);
        std::cout << "value: ";
        value->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "member: \"" << member << "\"\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class AssignExpression : public Expression {
  public:
    std::unique_ptr<Expression> target;
    std::unique_ptr<Expression> value;

    AssignExpression(Position position, std::unique_ptr<Expression> target,
                     std::unique_ptr<Expression> value)
        : Expression(position), target(std::move(target)), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "AssignExpression(\n";

        print_indent(depth + 1);
        std::cout << "target: ";
        target->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "value: ";
        value->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructLiteralExpression : public Expression {
  public:
    std::unique_ptr<IdentifierExpression> name;

    std::vector<std::unique_ptr<GenericArgument>> generic_arguments;

    std::vector<std::unique_ptr<StructLiteralField>> fields;

    StructLiteralExpression(Position position, std::unique_ptr<IdentifierExpression> name,
                            std::vector<std::unique_ptr<GenericArgument>> generic_arguments,
                            std::vector<std::unique_ptr<StructLiteralField>> fields)
        : Expression(position), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StructLiteralExpression(\n";

        print_indent(depth + 1);
        std::cout << "name: ";
        name->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "generic_arguments: [";
        if (generic_arguments.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                generic_arguments[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < generic_arguments.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "fields: [";
        if (fields.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < fields.size(); ++i) {
                fields[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < fields.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BlockStatement : public Statement {
  public:
    std::vector<std::unique_ptr<Statement>> statements;

    BlockStatement(Position position, std::vector<std::unique_ptr<Statement>> statements)
        : Statement(position), statements(std::move(statements)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "BlockStatement(statements: [";
        if (statements.empty()) {
            std::cout << "])";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < statements.size(); ++i) {
                statements[i]->dump(depth + 1, true, false);
                std::cout << (i + 1 < statements.size() ? ",\n" : "\n");
            }

            print_indent(depth);
            std::cout << "])";
        }

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExpressionStatement : public Statement {
  public:
    std::unique_ptr<Expression> expression;

    explicit ExpressionStatement(std::unique_ptr<Expression> expression)
        : Statement(expression->get_position()), expression(std::move(expression)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "ExpressionStatement(expression: ";
        expression->dump(depth, false, false);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IfExpression : public Expression {
  public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> then_branch;
    std::unique_ptr<Statement> else_branch;

    IfExpression(Position position, std::unique_ptr<Expression> condition,
                 std::unique_ptr<Statement> then_branch, std::unique_ptr<Statement> else_branch)
        : Expression(position), condition(std::move(condition)),
          then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "IfStatement(\n";

        print_indent(depth + 1);
        std::cout << "condition: ";
        condition->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "then_branch: ";
        then_branch->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "else_branch: ";
        if (else_branch) {
            else_branch->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ReturnStatement : public Statement {
  public:
    std::unique_ptr<Expression> value;

    ReturnStatement(Position position, std::unique_ptr<Expression> value)
        : Statement(position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "ReturnStatement(value: ";
        if (value) {
            value->dump(depth, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructDeclaration : public Statement {
  public:
    Visibility visibility;

    std::string name;

    std::vector<std::unique_ptr<GenericParameter>> generic_parameters;

    std::vector<std::unique_ptr<StructField>> fields;

    StructDeclaration(Position position, Visibility visibility, std::string name,
                      std::vector<std::unique_ptr<GenericParameter>> generic_parameters,
                      std::vector<std::unique_ptr<StructField>> fields)
        : Statement(position), visibility(visibility), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StructDeclaration(\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "generic_parameters: [";
        if (generic_parameters.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                generic_parameters[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < generic_parameters.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "fields: [";
        if (fields.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < fields.size(); ++i) {
                fields[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < fields.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class VarDeclaration : public Statement {
  public:
    Visibility visibility;
    StorageKind storage_kind;

    std::string name;
    std::unique_ptr<TypeExpression> type;
    std::unique_ptr<Expression> initializer;

    VarDeclaration(Position position, Visibility visibility, StorageKind storage_kind,
                   std::string name, std::unique_ptr<TypeExpression> type,
                   std::unique_ptr<Expression> initializer)
        : Statement(position), visibility(visibility), storage_kind(storage_kind),
          name(std::move(name)), type(std::move(type)), initializer(std::move(initializer)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "VarDeclaration(\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "storage_kind: " << storage_kind_string(storage_kind) << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "initializer: ";
        if (initializer) {
            initializer->dump(depth + 1, false, false);
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
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FunctionDeclaration : public Statement {
  public:
    Visibility visibility;

    std::unique_ptr<FunctionPrototype> prototype;
    std::unique_ptr<BlockStatement> body;

    FunctionDeclaration(Position position, Visibility visibility,
                        std::unique_ptr<FunctionPrototype> prototype,
                        std::unique_ptr<BlockStatement> body)
        : Statement(position), visibility(visibility), prototype(std::move(prototype)),
          body(std::move(body)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "FunctionDeclaration(\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "prototype: ";
        prototype->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "body: ";
        body->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExternFunctionDeclaration : public Statement {
  public:
    Visibility visibility;

    std::unique_ptr<FunctionPrototype> prototype;

    ExternFunctionDeclaration(Position position, Visibility visibility,
                              std::unique_ptr<FunctionPrototype> prototype)
        : Statement(position), visibility(visibility), prototype(std::move(prototype)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "ExternFunctionDeclaration(\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "prototype: ";
        prototype->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExternVarDeclaration : public Statement {
  public:
    Visibility visibility;

    std::string name;
    std::unique_ptr<TypeExpression> type;

    ExternVarDeclaration(Position position, Visibility visibility, std::string name,
                         std::unique_ptr<TypeExpression> type)
        : Statement(position), visibility(visibility), name(std::move(name)),
          type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "ExternVarDeclaration(\n";

        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        type->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ImportStatement : public Statement {
  public:
    std::vector<std::unique_ptr<IdentifierExpression>> path;

    ImportStatement(Position position, std::vector<std::unique_ptr<IdentifierExpression>> path)
        : Statement(position), path(std::move(path)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "ImportStatement(path: [";
        if (path.empty()) {
            std::cout << "])";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < path.size(); ++i) {
                path[i]->dump(depth + 1, true, false);
                std::cout << (i + 1 < path.size() ? ",\n" : "\n");
            }

            print_indent(depth);
            std::cout << "])";
        }

        if (trailing_newline) {
            std::cout << "\n";
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
