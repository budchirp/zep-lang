module;

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.ast;

import zep.common.position;
import zep.common.span;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;

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

    const Type* type;

    TypeExpression(Span span, const Type* type) : Expression(static_kind, span), type(type) {}

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

    void visit_expression(Expression& expression) {
        NumberLiteral* number_literal = expression.as<NumberLiteral>();
        if (number_literal != nullptr) {
            visit(*number_literal);
            return;
        }
        FloatLiteral* float_literal = expression.as<FloatLiteral>();
        if (float_literal != nullptr) {
            visit(*float_literal);
            return;
        }
        StringLiteral* string_literal = expression.as<StringLiteral>();
        if (string_literal != nullptr) {
            visit(*string_literal);
            return;
        }
        BooleanLiteral* boolean_literal = expression.as<BooleanLiteral>();
        if (boolean_literal != nullptr) {
            visit(*boolean_literal);
            return;
        }
        IdentifierExpression* identifier_expression = expression.as<IdentifierExpression>();
        if (identifier_expression != nullptr) {
            visit(*identifier_expression);
            return;
        }
        BinaryExpression* binary_expression = expression.as<BinaryExpression>();
        if (binary_expression != nullptr) {
            visit(*binary_expression);
            return;
        }
        UnaryExpression* unary_expression = expression.as<UnaryExpression>();
        if (unary_expression != nullptr) {
            visit(*unary_expression);
            return;
        }
        CallExpression* call_expression = expression.as<CallExpression>();
        if (call_expression != nullptr) {
            visit(*call_expression);
            return;
        }
        IndexExpression* index_expression = expression.as<IndexExpression>();
        if (index_expression != nullptr) {
            visit(*index_expression);
            return;
        }
        MemberExpression* member_expression = expression.as<MemberExpression>();
        if (member_expression != nullptr) {
            visit(*member_expression);
            return;
        }
        AssignExpression* assign_expression = expression.as<AssignExpression>();
        if (assign_expression != nullptr) {
            visit(*assign_expression);
            return;
        }
        StructLiteralExpression* struct_literal_expression =
            expression.as<StructLiteralExpression>();
        if (struct_literal_expression != nullptr) {
            visit(*struct_literal_expression);
            return;
        }
        IfExpression* if_expression = expression.as<IfExpression>();
        if (if_expression != nullptr) {
            visit(*if_expression);
            return;
        }
    }

    void visit_statement(Statement& statement) {
        BlockStatement* block_statement = statement.as<BlockStatement>();
        if (block_statement != nullptr) {
            visit(*block_statement);
            return;
        }
        ExpressionStatement* expression_statement = statement.as<ExpressionStatement>();
        if (expression_statement != nullptr) {
            visit(*expression_statement);
            return;
        }
        ReturnStatement* return_statement = statement.as<ReturnStatement>();
        if (return_statement != nullptr) {
            visit(*return_statement);
            return;
        }
        StructDeclaration* struct_declaration = statement.as<StructDeclaration>();
        if (struct_declaration != nullptr) {
            visit(*struct_declaration);
            return;
        }
        VarDeclaration* var_declaration = statement.as<VarDeclaration>();
        if (var_declaration != nullptr) {
            visit(*var_declaration);
            return;
        }
        FunctionDeclaration* function_declaration = statement.as<FunctionDeclaration>();
        if (function_declaration != nullptr) {
            visit(*function_declaration);
            return;
        }
        ExternFunctionDeclaration* extern_function_declaration =
            statement.as<ExternFunctionDeclaration>();
        if (extern_function_declaration != nullptr) {
            visit(*extern_function_declaration);
            return;
        }
        ExternVarDeclaration* extern_var_declaration = statement.as<ExternVarDeclaration>();
        if (extern_var_declaration != nullptr) {
            visit(*extern_var_declaration);
            return;
        }
        ImportStatement* import_statement = statement.as<ImportStatement>();
        if (import_statement != nullptr) {
            visit(*import_statement);
            return;
        }
    }

    void visit_node(Node& node) {
        TypeExpression* type_expression = node.as<TypeExpression>();
        if (type_expression != nullptr) {
            visit(*type_expression);
            return;
        }
        GenericParameter* generic_parameter = node.as<GenericParameter>();
        if (generic_parameter != nullptr) {
            visit(*generic_parameter);
            return;
        }
        GenericArgument* generic_argument = node.as<GenericArgument>();
        if (generic_argument != nullptr) {
            visit(*generic_argument);
            return;
        }
        Parameter* parameter = node.as<Parameter>();
        if (parameter != nullptr) {
            visit(*parameter);
            return;
        }
        Argument* argument = node.as<Argument>();
        if (argument != nullptr) {
            visit(*argument);
            return;
        }
        FunctionPrototype* function_prototype = node.as<FunctionPrototype>();
        if (function_prototype != nullptr) {
            visit(*function_prototype);
            return;
        }
        StructField* struct_field = node.as<StructField>();
        if (struct_field != nullptr) {
            visit(*struct_field);
            return;
        }
        StructLiteralField* struct_literal_field = node.as<StructLiteralField>();
        if (struct_literal_field != nullptr) {
            visit(*struct_literal_field);
            return;
        }

        NumberLiteral* number_literal = node.as<NumberLiteral>();
        if (number_literal != nullptr) {
            visit(*number_literal);
            return;
        }
        FloatLiteral* float_literal = node.as<FloatLiteral>();
        if (float_literal != nullptr) {
            visit(*float_literal);
            return;
        }
        StringLiteral* string_literal = node.as<StringLiteral>();
        if (string_literal != nullptr) {
            visit(*string_literal);
            return;
        }
        BooleanLiteral* boolean_literal = node.as<BooleanLiteral>();
        if (boolean_literal != nullptr) {
            visit(*boolean_literal);
            return;
        }
        IdentifierExpression* identifier_expression = node.as<IdentifierExpression>();
        if (identifier_expression != nullptr) {
            visit(*identifier_expression);
            return;
        }
        BinaryExpression* binary_expression = node.as<BinaryExpression>();
        if (binary_expression != nullptr) {
            visit(*binary_expression);
            return;
        }
        UnaryExpression* unary_expression = node.as<UnaryExpression>();
        if (unary_expression != nullptr) {
            visit(*unary_expression);
            return;
        }
        CallExpression* call_expression = node.as<CallExpression>();
        if (call_expression != nullptr) {
            visit(*call_expression);
            return;
        }
        IndexExpression* index_expression = node.as<IndexExpression>();
        if (index_expression != nullptr) {
            visit(*index_expression);
            return;
        }
        MemberExpression* member_expression = node.as<MemberExpression>();
        if (member_expression != nullptr) {
            visit(*member_expression);
            return;
        }
        AssignExpression* assign_expression = node.as<AssignExpression>();
        if (assign_expression != nullptr) {
            visit(*assign_expression);
            return;
        }
        StructLiteralExpression* struct_literal_expression = node.as<StructLiteralExpression>();
        if (struct_literal_expression != nullptr) {
            visit(*struct_literal_expression);
            return;
        }
        IfExpression* if_expression = node.as<IfExpression>();
        if (if_expression != nullptr) {
            visit(*if_expression);
            return;
        }

        BlockStatement* block_statement = node.as<BlockStatement>();
        if (block_statement != nullptr) {
            visit(*block_statement);
            return;
        }
        ExpressionStatement* expression_statement = node.as<ExpressionStatement>();
        if (expression_statement != nullptr) {
            visit(*expression_statement);
            return;
        }
        ReturnStatement* return_statement = node.as<ReturnStatement>();
        if (return_statement != nullptr) {
            visit(*return_statement);
            return;
        }
        StructDeclaration* struct_declaration = node.as<StructDeclaration>();
        if (struct_declaration != nullptr) {
            visit(*struct_declaration);
            return;
        }
        VarDeclaration* var_declaration = node.as<VarDeclaration>();
        if (var_declaration != nullptr) {
            visit(*var_declaration);
            return;
        }
        FunctionDeclaration* function_declaration = node.as<FunctionDeclaration>();
        if (function_declaration != nullptr) {
            visit(*function_declaration);
            return;
        }
        ExternFunctionDeclaration* extern_function_declaration =
            node.as<ExternFunctionDeclaration>();
        if (extern_function_declaration != nullptr) {
            visit(*extern_function_declaration);
            return;
        }
        ExternVarDeclaration* extern_var_declaration = node.as<ExternVarDeclaration>();
        if (extern_var_declaration != nullptr) {
            visit(*extern_var_declaration);
            return;
        }
        ImportStatement* import_statement = node.as<ImportStatement>();
        if (import_statement != nullptr) {
            visit(*import_statement);
            return;
        }
    }
};
