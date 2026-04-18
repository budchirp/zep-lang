module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.frontend.ast;

import zep.common.position;
import zep.common.span;
import zep.frontend.sema.type;
import zep.frontend.sema.kinds;

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
    explicit Node(Kind::Type kind, Span span) : kind(kind), span(span) {}

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = default;
    Node& operator=(Node&&) = default;

  public:
    Kind::Type kind;
    Span span;

    virtual ~Node() = default;

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

    TypeExpression(Span span, std::shared_ptr<Type> type)
        : Node(static_kind, span), type(std::move(type)) {}

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

    GenericParameter(Span span, std::string name,
                     std::unique_ptr<TypeExpression> constraint)
        : Node(static_kind, span), name(std::move(name)), constraint(std::move(constraint)) {}

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

    GenericArgument(Span span, std::string name, std::unique_ptr<TypeExpression> type)
        : Node(static_kind, span), name(std::move(name)), type(std::move(type)) {}

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

    Parameter(Span span, bool is_variadic, std::string name,
              std::unique_ptr<TypeExpression> type)
        : Node(static_kind, span), is_variadic(is_variadic), name(std::move(name)),
          type(std::move(type)) {}

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

    Argument(Span span, std::string name, std::unique_ptr<Expression> value)
        : Node(static_kind, span), name(std::move(name)), value(std::move(value)) {}

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

    FunctionPrototype(Span span, std::string name,
                      std::vector<std::unique_ptr<GenericParameter>> generic_parameters,
                      std::vector<std::unique_ptr<Parameter>> parameters,
                      std::unique_ptr<TypeExpression> return_type)
        : Node(static_kind, span), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), parameters(std::move(parameters)),
          return_type(std::move(return_type)) {}

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

    StructField(Span span, Visibility::Type visibility, std::string name,
                std::unique_ptr<TypeExpression> type)
        : Node(static_kind, span), visibility(visibility), name(std::move(name)),
          type(std::move(type)) {}

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

    StructLiteralField(Span span, std::string name, std::unique_ptr<Expression> value)
        : Node(static_kind, span), name(std::move(name)), value(std::move(value)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class NumberLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::NumberLiteral;

    std::string value;

    NumberLiteral(Span span, std::string value)
        : Expression(static_kind, span), value(std::move(value)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FloatLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::FloatLiteral;

    std::string value;

    FloatLiteral(Span span, std::string value)
        : Expression(static_kind, span), value(std::move(value)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StringLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::StringLiteral;

    std::string value;

    StringLiteral(Span span, std::string value)
        : Expression(static_kind, span), value(std::move(value)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BooleanLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::BooleanLiteral;

    bool value;

    BooleanLiteral(Span span, bool value)
        : Expression(static_kind, span), value(value) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IdentifierExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::IdentifierExpression;

    std::string name;

    IdentifierExpression(Span span, std::string name)
        : Expression(static_kind, span), name(std::move(name)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BinaryExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::BinaryExpression;

    class Operator {
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

        static std::string to_string(Type op) {
            switch (op) {
            case Type::Plus:
                return "+";
            case Type::Minus:
                return "-";
            case Type::Asterisk:
                return "*";
            case Type::Divide:
                return "/";
            case Type::Modulo:
                return "%";
            case Type::Equals:
                return "==";
            case Type::NotEquals:
                return "!=";
            case Type::LessThan:
                return "<";
            case Type::GreaterThan:
                return ">";
            case Type::LessEqual:
                return "<=";
            case Type::GreaterEqual:
                return ">=";
            case Type::And:
                return "&&";
            case Type::Or:
                return "||";
            case Type::As:
                return "as";
            case Type::Is:
                return "is";
            }
            return "?";
        }
    };

    std::unique_ptr<Expression> left;
    Operator::Type op;
    std::unique_ptr<Expression> right;

    BinaryExpression(Span span, std::unique_ptr<Expression> left, Operator::Type op,
                     std::unique_ptr<Expression> right)
        : Expression(static_kind, span), left(std::move(left)), op(op),
          right(std::move(right)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class UnaryExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::UnaryExpression;

    class Operator {
      public:
        enum class Type : std::uint8_t {
            Plus,
            Minus,
            Not,
            Dereference,
            AddressOf,
        };

        static std::string to_string(Type op) {
            switch (op) {
            case Type::Plus:
                return "+";
            case Type::Minus:
                return "-";
            case Type::Not:
                return "!";
            case Type::Dereference:
                return "*";
            case Type::AddressOf:
                return "&";
            }
            return "?";
        }
    };

    Operator::Type op;
    std::unique_ptr<Expression> operand;

    UnaryExpression(Span span, Operator::Type op, std::unique_ptr<Expression> operand)
        : Expression(static_kind, span), op(op), operand(std::move(operand)) {}

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

    CallExpression(Span span, std::unique_ptr<Expression> callee,
                   std::vector<std::unique_ptr<GenericArgument>> generic_arguments,
                   std::vector<std::unique_ptr<Argument>> arguments)
        : Expression(static_kind, span), callee(std::move(callee)),
          generic_arguments(std::move(generic_arguments)), arguments(std::move(arguments)) {}

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

    IndexExpression(Span span, std::unique_ptr<Expression> value,
                    std::unique_ptr<Expression> index)
        : Expression(static_kind, span), value(std::move(value)), index(std::move(index)) {}

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

    MemberExpression(Span span, std::unique_ptr<Expression> value, std::string member)
        : Expression(static_kind, span), value(std::move(value)), member(std::move(member)) {}

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

    AssignExpression(Span span, std::unique_ptr<Expression> target,
                     std::unique_ptr<Expression> value)
        : Expression(static_kind, span), target(std::move(target)), value(std::move(value)) {}

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

    StructLiteralExpression(Span span, std::unique_ptr<IdentifierExpression> name,
                            std::vector<std::unique_ptr<GenericArgument>> generic_arguments,
                            std::vector<std::unique_ptr<StructLiteralField>> fields)
        : Expression(static_kind, span), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)), fields(std::move(fields)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BlockStatement : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::BlockStatement;

    std::vector<std::unique_ptr<Statement>> statements;

    BlockStatement(Span span, std::vector<std::unique_ptr<Statement>> statements)
        : Statement(static_kind, span), statements(std::move(statements)) {}

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
        : Statement(static_kind, expression->span), expression(std::move(expression)) {}

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

    IfExpression(Span span, std::unique_ptr<Expression> condition,
                 std::unique_ptr<Statement> then_branch, std::unique_ptr<Statement> else_branch)
        : Expression(static_kind, span), condition(std::move(condition)),
          then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ReturnStatement : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ReturnStatement;

    std::unique_ptr<Expression> value;

    ReturnStatement(Span span, std::unique_ptr<Expression> value)
        : Statement(static_kind, span), value(std::move(value)) {}

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

    StructDeclaration(Span span, Visibility::Type visibility, std::string name,
                      std::vector<std::unique_ptr<GenericParameter>> generic_parameters,
                      std::vector<std::unique_ptr<StructField>> fields)
        : Statement(static_kind, span), visibility(visibility), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), fields(std::move(fields)) {}

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

    VarDeclaration(Span span, Visibility::Type visibility, StorageKind::Type storage_kind,
                   std::string name, std::unique_ptr<TypeExpression> type,
                   std::unique_ptr<Expression> initializer)
        : Statement(static_kind, span), visibility(visibility), storage_kind(storage_kind),
          name(std::move(name)), type(std::move(type)), initializer(std::move(initializer)) {}

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

    FunctionDeclaration(Span span, Visibility::Type visibility,
                        std::unique_ptr<FunctionPrototype> prototype,
                        std::unique_ptr<BlockStatement> body)
        : Statement(static_kind, span), visibility(visibility), prototype(std::move(prototype)),
          body(std::move(body)) {}

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

    ExternFunctionDeclaration(Span span, Visibility::Type visibility,
                              std::unique_ptr<FunctionPrototype> prototype)
        : Statement(static_kind, span), visibility(visibility),
          prototype(std::move(prototype)) {}

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

    ExternVarDeclaration(Span span, Visibility::Type visibility, std::string name,
                         std::unique_ptr<TypeExpression> type)
        : Statement(static_kind, span), visibility(visibility), name(std::move(name)),
          type(std::move(type)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ImportStatement : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ImportStatement;

    std::vector<std::unique_ptr<IdentifierExpression>> path;

    ImportStatement(Span span, std::vector<std::unique_ptr<IdentifierExpression>> path)
        : Statement(static_kind, span), path(std::move(path)) {}

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

    void visit_expression(Expression& expression) {
        if (auto* n = expression.as<NumberLiteral>()) { visit(*n); return; }
        if (auto* n = expression.as<FloatLiteral>()) { visit(*n); return; }
        if (auto* n = expression.as<StringLiteral>()) { visit(*n); return; }
        if (auto* n = expression.as<BooleanLiteral>()) { visit(*n); return; }
        if (auto* n = expression.as<IdentifierExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<BinaryExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<UnaryExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<CallExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<IndexExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<MemberExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<AssignExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<StructLiteralExpression>()) { visit(*n); return; }
        if (auto* n = expression.as<IfExpression>()) { visit(*n); return; }
    }

    void visit_statement(Statement& statement) {
        if (auto* n = statement.as<BlockStatement>()) { visit(*n); return; }
        if (auto* n = statement.as<ExpressionStatement>()) { visit(*n); return; }
        if (auto* n = statement.as<ReturnStatement>()) { visit(*n); return; }
        if (auto* n = statement.as<StructDeclaration>()) { visit(*n); return; }
        if (auto* n = statement.as<VarDeclaration>()) { visit(*n); return; }
        if (auto* n = statement.as<FunctionDeclaration>()) { visit(*n); return; }
        if (auto* n = statement.as<ExternFunctionDeclaration>()) { visit(*n); return; }
        if (auto* n = statement.as<ExternVarDeclaration>()) { visit(*n); return; }
        if (auto* n = statement.as<ImportStatement>()) { visit(*n); return; }
    }

    void visit_node(Node& node) {
        if (auto* n = node.as<TypeExpression>()) { visit(*n); return; }
        if (auto* n = node.as<GenericParameter>()) { visit(*n); return; }
        if (auto* n = node.as<GenericArgument>()) { visit(*n); return; }
        if (auto* n = node.as<Parameter>()) { visit(*n); return; }
        if (auto* n = node.as<Argument>()) { visit(*n); return; }
        if (auto* n = node.as<FunctionPrototype>()) { visit(*n); return; }
        if (auto* n = node.as<StructField>()) { visit(*n); return; }
        if (auto* n = node.as<StructLiteralField>()) { visit(*n); return; }

        if (auto* n = node.as<NumberLiteral>()) { visit(*n); return; }
        if (auto* n = node.as<FloatLiteral>()) { visit(*n); return; }
        if (auto* n = node.as<StringLiteral>()) { visit(*n); return; }
        if (auto* n = node.as<BooleanLiteral>()) { visit(*n); return; }
        if (auto* n = node.as<IdentifierExpression>()) { visit(*n); return; }
        if (auto* n = node.as<BinaryExpression>()) { visit(*n); return; }
        if (auto* n = node.as<UnaryExpression>()) { visit(*n); return; }
        if (auto* n = node.as<CallExpression>()) { visit(*n); return; }
        if (auto* n = node.as<IndexExpression>()) { visit(*n); return; }
        if (auto* n = node.as<MemberExpression>()) { visit(*n); return; }
        if (auto* n = node.as<AssignExpression>()) { visit(*n); return; }
        if (auto* n = node.as<StructLiteralExpression>()) { visit(*n); return; }
        if (auto* n = node.as<IfExpression>()) { visit(*n); return; }

        if (auto* n = node.as<BlockStatement>()) { visit(*n); return; }
        if (auto* n = node.as<ExpressionStatement>()) { visit(*n); return; }
        if (auto* n = node.as<ReturnStatement>()) { visit(*n); return; }
        if (auto* n = node.as<StructDeclaration>()) { visit(*n); return; }
        if (auto* n = node.as<VarDeclaration>()) { visit(*n); return; }
        if (auto* n = node.as<FunctionDeclaration>()) { visit(*n); return; }
        if (auto* n = node.as<ExternFunctionDeclaration>()) { visit(*n); return; }
        if (auto* n = node.as<ExternVarDeclaration>()) { visit(*n); return; }
        if (auto* n = node.as<ImportStatement>()) { visit(*n); return; }
    }
};
