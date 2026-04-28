module;

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.node;

import zep.common.position;
import zep.common.span;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.common.arena;

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
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

  public:
    const Kind::Type kind;
    const Span span;

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

export using NodeArena = Arena<Node>;

export class Expression : public Node {
  protected:
    explicit Expression(Kind::Type kind, Span span) : Node(kind, span), type(nullptr) {}

  public:
    const Type* type;
};

export class Statement : public Node {
  protected:
    explicit Statement(Kind::Type kind, Span span) : Node(kind, span), type(nullptr) {}

  public:
    const Type* type;
};

export class TypeExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::TypeExpression;

    TypeExpression(Span span, const Type* type) : Expression(static_kind, span) {
        this->type = type;
    }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class GenericParameter : public Node {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::GenericParameter;

    std::string name;
    TypeExpression* constraint;

    GenericParameter(Span span, std::string name, TypeExpression* constraint)
        : Node(static_kind, span), name(std::move(name)), constraint(constraint) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class GenericArgument : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::GenericArgument;

    std::string name;
    TypeExpression* type;

    GenericArgument(Span span, std::string name, TypeExpression* type_expression)
        : Node(static_kind, span), name(std::move(name)), type(type_expression) {}

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
    TypeExpression* type;

    Parameter(Span span, bool is_variadic, std::string name, TypeExpression* type_expression)
        : Node(static_kind, span), is_variadic(is_variadic), name(std::move(name)),
          type(type_expression) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Argument : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::Argument;

    std::string name;
    Expression* value;

    Argument(Span span, std::string name, Expression* value)
        : Node(static_kind, span), name(std::move(name)), value(value) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FunctionPrototype : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::FunctionPrototype;

    std::string name;

    std::vector<GenericParameter*> generic_parameters;
    std::vector<Parameter*> parameters;

    TypeExpression* return_type;

    FunctionPrototype(Span span, std::string name,
                      std::vector<GenericParameter*> generic_parameters,
                      std::vector<Parameter*> parameters, TypeExpression* return_type)
        : Node(static_kind, span), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), parameters(std::move(parameters)),
          return_type(return_type) {}

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
    TypeExpression* type;

    StructField(Span span, Visibility::Type visibility, std::string name,
                TypeExpression* type_expression)
        : Node(static_kind, span), visibility(visibility), name(std::move(name)),
          type(type_expression) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructLiteralField : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::StructLiteralField;

    std::string name;
    Expression* value;

    StructLiteralField(Span span, std::string name, Expression* value)
        : Node(static_kind, span), name(std::move(name)), value(value) {}

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

    BooleanLiteral(Span span, bool value) : Expression(static_kind, span), value(value) {}

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

    Expression* left;
    Operator::Type op;
    Expression* right;

    BinaryExpression(Span span, Expression* left, Operator::Type op, Expression* right)
        : Expression(static_kind, span), left(left), op(op), right(right) {}

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
    Expression* operand;

    UnaryExpression(Span span, Operator::Type op, Expression* operand)
        : Expression(static_kind, span), op(op), operand(operand) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class CallExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::CallExpression;

    Expression* callee;

    std::vector<GenericArgument*> generic_arguments;

    std::vector<Argument*> arguments;

    CallExpression(Span span, Expression* callee, std::vector<GenericArgument*> generic_arguments,
                   std::vector<Argument*> arguments)
        : Expression(static_kind, span), callee(callee),
          generic_arguments(std::move(generic_arguments)), arguments(std::move(arguments)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IndexExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::IndexExpression;

    Expression* value;
    Expression* index;

    IndexExpression(Span span, Expression* value, Expression* index)
        : Expression(static_kind, span), value(value), index(index) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class MemberExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::MemberExpression;

    Expression* value;
    std::string member;

    MemberExpression(Span span, Expression* value, std::string member)
        : Expression(static_kind, span), value(value), member(std::move(member)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class AssignExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::AssignExpression;

    Expression* target;
    Expression* value;

    AssignExpression(Span span, Expression* target, Expression* value)
        : Expression(static_kind, span), target(target), value(value) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class StructLiteralExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::StructLiteralExpression;

    IdentifierExpression* name;

    std::vector<GenericArgument*> generic_arguments;

    std::vector<StructLiteralField*> fields;

    StructLiteralExpression(Span span, IdentifierExpression* name,
                            std::vector<GenericArgument*> generic_arguments,
                            std::vector<StructLiteralField*> fields)
        : Expression(static_kind, span), name(name),
          generic_arguments(std::move(generic_arguments)), fields(std::move(fields)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BlockStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::BlockStatement;

    std::vector<Statement*> statements;

    BlockStatement(Span span, std::vector<Statement*> statements)
        : Statement(static_kind, span), statements(std::move(statements)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExpressionStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::ExpressionStatement;

    Expression* expression;

    explicit ExpressionStatement(Expression* expression)
        : Statement(static_kind, expression->span), expression(expression) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IfExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::IfExpression;

    Expression* condition;
    Statement* then_branch;
    Statement* else_branch;

    IfExpression(Span span, Expression* condition, Statement* then_branch, Statement* else_branch)
        : Expression(static_kind, span), condition(condition), then_branch(then_branch),
          else_branch(else_branch) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ReturnStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::ReturnStatement;

    Expression* value;

    ReturnStatement(Span span, Expression* value) : Statement(static_kind, span), value(value) {}

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

    std::vector<GenericParameter*> generic_parameters;

    std::vector<StructField*> fields;

    StructDeclaration(Span span, Visibility::Type visibility, std::string name,
                      std::vector<GenericParameter*> generic_parameters,
                      std::vector<StructField*> fields)
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
    TypeExpression* annotation;
    Expression* initializer;

    VarDeclaration(Span span, Visibility::Type visibility, StorageKind::Type storage_kind,
                   std::string name, TypeExpression* annotation, Expression* initializer)
        : Statement(static_kind, span), visibility(visibility), storage_kind(storage_kind),
          name(std::move(name)), annotation(annotation), initializer(initializer) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FunctionDeclaration : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::FunctionDeclaration;

    Visibility::Type visibility;

    FunctionPrototype* prototype;
    BlockStatement* body;

    FunctionDeclaration(Span span, Visibility::Type visibility, FunctionPrototype* prototype,
                        BlockStatement* body)
        : Statement(static_kind, span), visibility(visibility), prototype(prototype), body(body) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExternFunctionDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ExternFunctionDeclaration;

    Visibility::Type visibility;

    FunctionPrototype* prototype;

    ExternFunctionDeclaration(Span span, Visibility::Type visibility, FunctionPrototype* prototype)
        : Statement(static_kind, span), visibility(visibility), prototype(prototype) {}

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
    TypeExpression* annotation;

    ExternVarDeclaration(Span span, Visibility::Type visibility, std::string name,
                         TypeExpression* annotation)
        : Statement(static_kind, span), visibility(visibility), name(std::move(name)),
          annotation(annotation) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ImportStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::ImportStatement;

    std::vector<IdentifierExpression*> path;

    ImportStatement(Span span, std::vector<IdentifierExpression*> path)
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

    T visit_expression(Expression& expression) {
        if (auto* number_literal = expression.as<NumberLiteral>(); number_literal != nullptr) {
            return visit(*number_literal);
        }
        if (auto* float_literal = expression.as<FloatLiteral>(); float_literal != nullptr) {
            return visit(*float_literal);
        }
        if (auto* string_literal = expression.as<StringLiteral>(); string_literal != nullptr) {
            return visit(*string_literal);
        }
        if (auto* boolean_literal = expression.as<BooleanLiteral>(); boolean_literal != nullptr) {
            return visit(*boolean_literal);
        }
        if (auto* identifier = expression.as<IdentifierExpression>(); identifier != nullptr) {
            return visit(*identifier);
        }
        if (auto* binary = expression.as<BinaryExpression>(); binary != nullptr) {
            return visit(*binary);
        }
        if (auto* unary = expression.as<UnaryExpression>(); unary != nullptr) {
            return visit(*unary);
        }
        if (auto* call = expression.as<CallExpression>(); call != nullptr) {
            return visit(*call);
        }
        if (auto* index = expression.as<IndexExpression>(); index != nullptr) {
            return visit(*index);
        }
        if (auto* member = expression.as<MemberExpression>(); member != nullptr) {
            return visit(*member);
        }
        if (auto* assign = expression.as<AssignExpression>(); assign != nullptr) {
            return visit(*assign);
        }
        if (auto* struct_literal = expression.as<StructLiteralExpression>();
            struct_literal != nullptr) {
            return visit(*struct_literal);
        }
        if (auto* if_expression = expression.as<IfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }
        if (auto* type_expression = expression.as<TypeExpression>(); type_expression != nullptr) {
            return visit(*type_expression);
        }

        return T();
    }

    T visit_statement(Statement& statement) {
        if (auto* block = statement.as<BlockStatement>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* expression_statement = statement.as<ExpressionStatement>();
            expression_statement != nullptr) {
            return visit(*expression_statement);
        }
        if (auto* return_statement = statement.as<ReturnStatement>(); return_statement != nullptr) {
            return visit(*return_statement);
        }
        if (auto* struct_declaration = statement.as<StructDeclaration>();
            struct_declaration != nullptr) {
            return visit(*struct_declaration);
        }
        if (auto* var_declaration = statement.as<VarDeclaration>(); var_declaration != nullptr) {
            return visit(*var_declaration);
        }
        if (auto* function_declaration = statement.as<FunctionDeclaration>();
            function_declaration != nullptr) {
            return visit(*function_declaration);
        }
        if (auto* extern_function = statement.as<ExternFunctionDeclaration>();
            extern_function != nullptr) {
            return visit(*extern_function);
        }
        if (auto* extern_var = statement.as<ExternVarDeclaration>(); extern_var != nullptr) {
            return visit(*extern_var);
        }
        if (auto* import = statement.as<ImportStatement>(); import != nullptr) {
            return visit(*import);
        }

        return T();
    }

    T visit_node(Node& node) {
        if (auto* type_expression = node.as<TypeExpression>(); type_expression != nullptr) {
            return visit(*type_expression);
        }
        if (auto* generic_parameter = node.as<GenericParameter>(); generic_parameter != nullptr) {
            return visit(*generic_parameter);
        }
        if (auto* generic_argument = node.as<GenericArgument>(); generic_argument != nullptr) {
            return visit(*generic_argument);
        }
        if (auto* parameter = node.as<Parameter>(); parameter != nullptr) {
            return visit(*parameter);
        }
        if (auto* argument = node.as<Argument>(); argument != nullptr) {
            return visit(*argument);
        }
        if (auto* prototype = node.as<FunctionPrototype>(); prototype != nullptr) {
            return visit(*prototype);
        }
        if (auto* struct_field = node.as<StructField>(); struct_field != nullptr) {
            return visit(*struct_field);
        }
        if (auto* struct_literal_field = node.as<StructLiteralField>();
            struct_literal_field != nullptr) {
            return visit(*struct_literal_field);
        }

        if (auto* number_literal = node.as<NumberLiteral>(); number_literal != nullptr) {
            return visit(*number_literal);
        }
        if (auto* float_literal = node.as<FloatLiteral>(); float_literal != nullptr) {
            return visit(*float_literal);
        }
        if (auto* string_literal = node.as<StringLiteral>(); string_literal != nullptr) {
            return visit(*string_literal);
        }
        if (auto* boolean_literal = node.as<BooleanLiteral>(); boolean_literal != nullptr) {
            return visit(*boolean_literal);
        }
        if (auto* identifier = node.as<IdentifierExpression>(); identifier != nullptr) {
            return visit(*identifier);
        }
        if (auto* binary = node.as<BinaryExpression>(); binary != nullptr) {
            return visit(*binary);
        }
        if (auto* unary = node.as<UnaryExpression>(); unary != nullptr) {
            return visit(*unary);
        }
        if (auto* call = node.as<CallExpression>(); call != nullptr) {
            return visit(*call);
        }
        if (auto* index = node.as<IndexExpression>(); index != nullptr) {
            return visit(*index);
        }
        if (auto* member = node.as<MemberExpression>(); member != nullptr) {
            return visit(*member);
        }
        if (auto* assign = node.as<AssignExpression>(); assign != nullptr) {
            return visit(*assign);
        }
        if (auto* struct_literal = node.as<StructLiteralExpression>(); struct_literal != nullptr) {
            return visit(*struct_literal);
        }
        if (auto* if_expression = node.as<IfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }

        if (auto* block = node.as<BlockStatement>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* expression_statement = node.as<ExpressionStatement>();
            expression_statement != nullptr) {
            return visit(*expression_statement);
        }
        if (auto* return_statement = node.as<ReturnStatement>(); return_statement != nullptr) {
            return visit(*return_statement);
        }
        if (auto* struct_declaration = node.as<StructDeclaration>();
            struct_declaration != nullptr) {
            return visit(*struct_declaration);
        }
        if (auto* var_declaration = node.as<VarDeclaration>(); var_declaration != nullptr) {
            return visit(*var_declaration);
        }
        if (auto* function_declaration = node.as<FunctionDeclaration>();
            function_declaration != nullptr) {
            return visit(*function_declaration);
        }
        if (auto* extern_function = node.as<ExternFunctionDeclaration>();
            extern_function != nullptr) {
            return visit(*extern_function);
        }
        if (auto* extern_var = node.as<ExternVarDeclaration>(); extern_var != nullptr) {
            return visit(*extern_var);
        }
        if (auto* import = node.as<ImportStatement>(); import != nullptr) {
            return visit(*import);
        }

        return T();
    }
};
