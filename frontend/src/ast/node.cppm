module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.node;

import zep.common.source.span;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.common.arena;
import zep.frontend.token;

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
            Field,
            EnumVariant,
            StructLiteralField,
            WhenPatternField,
            WhenPattern,
            WhenArm,
            Attribute,

            NumberLiteral,
            FloatLiteral,
            StringLiteral,
            CharLiteral,
            BooleanLiteral,
            NullLiteral,
            IdentifierExpression,
            BinaryExpression,
            UnaryExpression,
            CoerceExpression,
            CallExpression,
            IndexExpression,
            MemberExpression,
            QualifiedAccessExpression,
            AssignExpression,
            StructLiteralExpression,
            EnumVariantExpression,
            IfExpression,
            WhenExpression,
            ClosureExpression,
            BlockExpression,
            BuiltinCall,

            BlockStatement,
            ExpressionStatement,
            ReturnStatement,
            WhileStatement,
            ForStatement,
            DeferStatement,
            InterfaceDeclaration,
            StructDeclaration,
            EnumDeclaration,
            VarDeclaration,
            FunctionDeclaration,
            ExternFunctionDeclaration,
            ExternVarDeclaration,
            ImportStatement,
            TypeAliasDeclaration,
            ArrayLiteralExpression,
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

export class Intrinsic {
  public:
    enum class Type : std::uint8_t { InlineAssembly };
};

export class CallTarget {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Direct, Indirect, Interface, Intrinsic };
    };

    const Kind::Type kind;

    explicit CallTarget(Kind::Type kind) : kind(kind) {}
    virtual ~CallTarget() = default;

    template <typename T>
    const T* as() const {
        return kind == T::static_kind ? static_cast<const T*>(this) : nullptr;
    }
};

export class DirectCallTarget final : public CallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Direct;
    const FunctionSymbol* function_symbol;

    explicit DirectCallTarget(const FunctionSymbol& function)
        : CallTarget(static_kind), function_symbol(&function) {}
};

export class IndirectCallTarget final : public CallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Indirect;
    const FunctionType* function_type;

    explicit IndirectCallTarget(const FunctionType& function_type)
        : CallTarget(static_kind), function_type(&function_type) {}
};

export class InterfaceCallTarget final : public CallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Interface;
    const FunctionSymbol* method_symbol;
    std::size_t slot;

    InterfaceCallTarget(const FunctionSymbol& method, std::size_t slot)
        : CallTarget(static_kind), method_symbol(&method), slot(slot) {}
};

export class IntrinsicCallTarget final : public CallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Intrinsic;
    Intrinsic::Type intrinsic;

    explicit IntrinsicCallTarget(Intrinsic::Type intrinsic)
        : CallTarget(static_kind), intrinsic(intrinsic) {}
};

export class ResolvedCall {
  public:
    std::unique_ptr<CallTarget> target;

    explicit ResolvedCall(std::unique_ptr<CallTarget> target) : target(std::move(target)) {}
};

export class Expression : public Node {
  protected:
    explicit Expression(Kind::Type kind, Span span)
        : Node(kind, span), type(nullptr), scope(nullptr), compile_time_value(std::nullopt) {}

  public:
    const Type* type;
    Scope* scope;
    std::optional<CompileTimeValue> compile_time_value;
};

export class Statement : public Node {
  protected:
    explicit Statement(Kind::Type kind, Span span) : Node(kind, span), type(nullptr) {}

  public:
    const Type* type;
};

export class IdentifierExpression;
export class Attribute;
export class GenericArgument;

export class TypeExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::TypeExpression;

    const Type* source_type;
    std::vector<Expression*> array_sizes;
    std::vector<GenericArgument*> generic_arguments;
    TypeExpression* element;
    std::vector<TypeExpression*> parameters;
    TypeExpression* return_type;

    TypeExpression(Span span, const Type* type)
        : Expression(static_kind, span), source_type(type), array_sizes(), generic_arguments(),
          element(nullptr), parameters(), return_type(nullptr) {
        this->type = type;
    }

    TypeExpression(Span span, const Type* type, std::vector<Expression*> array_sizes)
        : TypeExpression(span, type) {
        this->array_sizes = std::move(array_sizes);
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

    GenericParameterType::Kind::Type kind;
    std::string name;
    TypeExpression* constraint;
    TypeExpression* value_type;

    GenericParameter(Span span, std::string name, TypeExpression* constraint)
        : Node(static_kind, span), kind(GenericParameterType::Kind::Type::Type),
          name(std::move(name)), constraint(constraint), value_type(nullptr) {}

    GenericParameter(Span span, GenericParameterType::Kind::Type kind, std::string name,
                     TypeExpression* value_type)
        : Node(static_kind, span), kind(kind), name(std::move(name)), constraint(nullptr),
          value_type(value_type) {}

    bool is_const() const { return kind == GenericParameterType::Kind::Type::Const; }

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
    Expression* value;
    std::optional<ConstBinding> const_binding;

    GenericArgument(Span span, std::string name, TypeExpression* type_expression)
        : Node(static_kind, span), name(std::move(name)), type(type_expression), value(nullptr),
          const_binding(std::nullopt) {}

    GenericArgument(Span span, std::string name, Expression* value)
        : Node(static_kind, span), name(std::move(name)), type(nullptr), value(value),
          const_binding(std::nullopt) {}

    bool is_const() const { return value != nullptr || const_binding.has_value(); }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Parameter : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::Parameter;

    std::string name;
    TypeExpression* type;

    std::vector<Attribute*> attributes;

    Parameter(Span span, std::string name, TypeExpression* type_expression,
              std::vector<Attribute*> attributes = {})
        : Node(static_kind, span), name(std::move(name)), type(type_expression),
          attributes(std::move(attributes)) {}

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

    class ReceiverKind {
      public:
        enum class Type : std::uint8_t { None, Borrow, MutBorrow };
    };

    bool is_variadic;
    ReceiverKind::Type receiver_kind;
    std::string name;

    std::vector<GenericParameter*> generic_parameters;
    std::vector<Parameter*> parameters;

    TypeExpression* return_type;

    FunctionPrototype(Span span, bool is_variadic, std::string name,
                      std::vector<GenericParameter*> generic_parameters,
                      std::vector<Parameter*> parameters, TypeExpression* return_type)
        : Node(static_kind, span), is_variadic(is_variadic),
          receiver_kind(ReceiverKind::Type::None), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), parameters(std::move(parameters)),
          return_type(return_type) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Field : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::Field;

    Visibility::Type visibility;

    std::vector<Attribute*> attributes;
    std::string name;
    TypeExpression* type;
    Expression* default_value;

    Field(Span span, Visibility::Type visibility, std::vector<Attribute*> attributes,
          std::string name, TypeExpression* type_expression, Expression* default_value = nullptr)
        : Node(static_kind, span), visibility(visibility), attributes(std::move(attributes)),
          name(std::move(name)), type(type_expression), default_value(default_value) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class EnumVariant : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::EnumVariant;

    std::vector<Attribute*> attributes;
    std::string name;
    std::vector<Field*> fields;
    Expression* value_expression;

    EnumVariant(Span span, std::vector<Attribute*> attributes, std::string name,
                std::vector<Field*> fields, Expression* value_expression = nullptr)
        : Node(static_kind, span), attributes(std::move(attributes)), name(std::move(name)),
          fields(std::move(fields)), value_expression(value_expression) {}

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

export class WhenPatternField : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::WhenPatternField;

    std::string field_name;
    std::string binding_name;

    WhenPatternField(Span span, std::string field_name, std::string binding_name)
        : Node(static_kind, span), field_name(std::move(field_name)),
          binding_name(std::move(binding_name)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class WhenPattern : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::WhenPattern;

    class PatternKind {
      public:
        enum class Type : std::uint8_t {
            Expression,
            EnumVariant,
        };
    };

    PatternKind::Type pattern_kind;
    Expression* expression;
    IdentifierExpression* enum_name;
    std::vector<GenericArgument*> generic_arguments;
    std::string variant_name;
    std::vector<WhenPatternField*> fields;

    const EnumType* enum_type;
    const EnumVariantType* variant_type;

    WhenPattern(Span span, Expression* expression)
        : Node(static_kind, span), pattern_kind(PatternKind::Type::Expression),
          expression(expression), enum_name(nullptr), generic_arguments(), variant_name(), fields(),
          enum_type(nullptr), variant_type(nullptr) {}

    WhenPattern(Span span, IdentifierExpression* enum_name,
                std::vector<GenericArgument*> generic_arguments, std::string variant_name,
                std::vector<WhenPatternField*> fields)
        : Node(static_kind, span), pattern_kind(PatternKind::Type::EnumVariant),
          expression(nullptr), enum_name(enum_name),
          generic_arguments(std::move(generic_arguments)), variant_name(std::move(variant_name)),
          fields(std::move(fields)), enum_type(nullptr), variant_type(nullptr) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class Attribute : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::Attribute;

    std::string name;
    std::vector<Expression*> arguments;

    Attribute(Span span, std::string name, std::vector<Expression*> arguments)
        : Node(static_kind, span), name(std::move(name)), arguments(std::move(arguments)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export Attribute* find_attribute(const std::vector<Attribute*>& attributes,
                                 const std::string& name) {
    for (auto* attribute : attributes) {
        if (attribute->name == name) {
            return attribute;
        }
    }

    return nullptr;
}

export class WhenArm : public Node {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::WhenArm;

    bool is_else;
    std::vector<WhenPattern*> patterns;
    Expression* guard;
    Statement* body;

    WhenArm(Span span, bool is_else, std::vector<WhenPattern*> patterns, Expression* guard,
            Statement* body)
        : Node(static_kind, span), is_else(is_else), patterns(std::move(patterns)), guard(guard),
          body(body) {}

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

export class CharLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::CharLiteral;

    std::uint8_t value;

    CharLiteral(Span span, std::uint8_t value) : Expression(static_kind, span), value(value) {}

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

export class NullLiteral : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::NullLiteral;

    NullLiteral(Span span) : Expression(static_kind, span) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class IdentifierExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::IdentifierExpression;

    std::string name;
    const VariableSymbol* var_symbol;
    const FunctionSymbol* function_symbol;
    const void* generic_declaration;
    bool implicit_self_field;

    std::vector<GenericArgument*> generic_arguments;

    IdentifierExpression(Span span, std::string name,
                         std::vector<GenericArgument*> generic_arguments = {})
        : Expression(static_kind, span), name(std::move(name)), var_symbol(nullptr),
          function_symbol(nullptr), generic_declaration(nullptr), implicit_self_field(false),
          generic_arguments(std::move(generic_arguments)) {}

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
            Is,
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

        static std::optional<Type> from_token(Token::Type token_type) {
            Type op;

            switch (token_type) {
            case Token::Type::Plus:
                op = Type::Plus;
                break;
            case Token::Type::Minus:
                op = Type::Minus;
                break;
            case Token::Type::Asterisk:
                op = Type::Asterisk;
                break;
            case Token::Type::Divide:
                op = Type::Divide;
                break;
            case Token::Type::Modulo:
                op = Type::Modulo;
                break;
            case Token::Type::Equals:
                op = Type::Equals;
                break;
            case Token::Type::NotEquals:
                op = Type::NotEquals;
                break;
            case Token::Type::LessThan:
                op = Type::LessThan;
                break;
            case Token::Type::GreaterThan:
                op = Type::GreaterThan;
                break;
            case Token::Type::LessEqual:
                op = Type::LessEqual;
                break;
            case Token::Type::GreaterEqual:
                op = Type::GreaterEqual;
                break;
            case Token::Type::And:
                op = Type::And;
                break;
            case Token::Type::Or:
                op = Type::Or;
                break;
            case Token::Type::As:
                op = Type::As;
                break;
            case Token::Type::Is:
                op = Type::Is;
                break;
            default:
                return std::nullopt;
            }

            return op;
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
            AddressOfMut,
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
            case Type::AddressOfMut:
                return "&mut ";
            }
            return "?";
        }

        static std::optional<Type> from_token(Token::Type type) {
            Type op;

            switch (type) {
            case Token::Type::Minus:
                op = Type::Minus;
                break;
            case Token::Type::Plus:
                op = Type::Plus;
                break;
            case Token::Type::Not:
                op = Type::Not;
                break;
            case Token::Type::Asterisk:
                op = Type::Dereference;
                break;
            case Token::Type::Ampersand:
                op = Type::AddressOf;
                break;
            default:
                return std::nullopt;
            }

            return op;
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

export class CoerceExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::CoerceExpression;

    Expression* value;
    Coercion::Type coercion;
    const Type* source_type;

    CoerceExpression(Span span, Expression* value, const Type* target_type, Coercion::Type coercion,
                     const Type* source_type)
        : Expression(static_kind, span), value(value), coercion(coercion),
          source_type(source_type) {
        type = target_type;
    }

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

    std::unique_ptr<CallTarget> resolved_target;

    CallExpression(Span span, Expression* callee, std::vector<GenericArgument*> generic_arguments,
                   std::vector<Argument*> arguments)
        : Expression(static_kind, span), callee(callee),
          generic_arguments(std::move(generic_arguments)), arguments(std::move(arguments)),
          resolved_target(nullptr) {}

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

export class QualifiedAccessExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::QualifiedAccessExpression;

    std::string parent;
    std::vector<GenericArgument*> parent_generic_arguments;
    std::string member;

    const EnumType* enum_type;
    const EnumVariantType* variant_type;
    std::optional<CompileTimeValue> enum_value;
    const FunctionSymbol* function_symbol;

    QualifiedAccessExpression(Span span, std::string parent,
                              std::vector<GenericArgument*> parent_generic_arguments,
                              std::string member)
        : Expression(static_kind, span), parent(std::move(parent)),
          parent_generic_arguments(std::move(parent_generic_arguments)), member(std::move(member)),
          enum_type(nullptr), variant_type(nullptr), enum_value(std::nullopt),
          function_symbol(nullptr) {}

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

    Expression* name;

    std::vector<GenericArgument*> generic_arguments;

    std::vector<StructLiteralField*> fields;

    StructLiteralExpression(Span span, Expression* name,
                            std::vector<GenericArgument*> generic_arguments,
                            std::vector<StructLiteralField*> fields)
        : Expression(static_kind, span), name(name),
          generic_arguments(std::move(generic_arguments)), fields(std::move(fields)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class EnumVariantExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::EnumVariantExpression;

    IdentifierExpression* enum_name;

    std::vector<GenericArgument*> generic_arguments;

    std::string variant_name;

    bool has_payload;

    std::vector<StructLiteralField*> fields;

    const EnumType* enum_type;
    const EnumVariantType* variant_type;

    EnumVariantExpression(Span span, IdentifierExpression* enum_name,
                          std::vector<GenericArgument*> generic_arguments, std::string variant_name,
                          bool has_payload, std::vector<StructLiteralField*> fields)
        : Expression(static_kind, span), enum_name(enum_name),
          generic_arguments(std::move(generic_arguments)), variant_name(std::move(variant_name)),
          has_payload(has_payload), fields(std::move(fields)), enum_type(nullptr),
          variant_type(nullptr) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BlockStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::BlockStatement;

    std::vector<Statement*> statements;
    Scope* scope;

    BlockStatement(Span span, std::vector<Statement*> statements)
        : Statement(static_kind, span), statements(std::move(statements)), scope(nullptr) {}

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

export class WhenExpression : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::WhenExpression;

    Expression* subject;
    std::vector<WhenArm*> arms;
    bool is_exhaustive;

    WhenExpression(Span span, Expression* subject, std::vector<WhenArm*> arms)
        : Expression(static_kind, span), subject(subject), arms(std::move(arms)),
          is_exhaustive(false) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ClosureExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ClosureExpression;

    std::vector<Parameter*> parameters;
    BlockStatement* body;
    std::vector<std::string> captures;

    ClosureExpression(Span span, std::vector<Parameter*> parameters, BlockStatement* body)
        : Expression(static_kind, span), parameters(std::move(parameters)), body(body) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BlockExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::BlockExpression;

    BlockStatement* body;

    BlockExpression(Span span, BlockStatement* body) : Expression(static_kind, span), body(body) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class BuiltinCall : public Expression {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::BuiltinCall;

    std::string name;
    TypeExpression* type_argument;
    std::vector<Expression*> arguments;

    BuiltinCall(Span span, std::string name, TypeExpression* type_argument,
                std::vector<Expression*> arguments)
        : Expression(static_kind, span), name(std::move(name)), type_argument(type_argument),
          arguments(std::move(arguments)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ArrayLiteralExpression : public Expression {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ArrayLiteralExpression;

    std::vector<Expression*> elements;

    ArrayLiteralExpression(Span span, std::vector<Expression*> elements)
        : Expression(static_kind, span), elements(std::move(elements)) {}

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

export class WhileStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::WhileStatement;

    Expression* condition;
    BlockStatement* body;

    WhileStatement(Span span, Expression* condition, BlockStatement* body)
        : Statement(static_kind, span), condition(condition), body(body) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ForStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::ForStatement;

    Statement* initializer;
    Expression* condition;
    Expression* step;
    BlockStatement* body;

    ForStatement(Span span, Statement* initializer, Expression* condition, Expression* step,
                 BlockStatement* body)
        : Statement(static_kind, span), initializer(initializer), condition(condition), step(step),
          body(body) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class DeferStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::DeferStatement;

    Statement* body;

    DeferStatement(Span span, Statement* body) : Statement(static_kind, span), body(body) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class FunctionDeclaration : public Statement {

  public:
    static constexpr Node::Kind::Type static_kind = Node::Kind::Type::FunctionDeclaration;

    class MemberModifiers {
      public:
        Span span;
        bool is_static;
        bool is_override;
        bool is_mutable_receiver;

        explicit MemberModifiers(Span span)
            : span(span), is_static(false), is_override(false), is_mutable_receiver(false) {}
    };

    Visibility::Type visibility;

    std::vector<Attribute*> attributes;

    bool is_static;
    bool is_override;
    bool is_extension;
    std::string parent;

    FunctionPrototype* prototype;
    BlockStatement* body;

    const FunctionSymbol* function_symbol;

    FunctionDeclaration(Span span, Visibility::Type visibility, std::vector<Attribute*> attributes,
                        bool is_static, bool is_override, bool is_extension, std::string parent,
                        FunctionPrototype* prototype, BlockStatement* body)
        : Statement(static_kind, span), visibility(visibility), attributes(std::move(attributes)),
          is_static(is_static), is_override(is_override), is_extension(is_extension),
          parent(std::move(parent)), prototype(prototype), body(body), function_symbol(nullptr) {}

    FunctionSymbol::Kind::Type kind() const {
        return FunctionSymbol::Kind::classify(parent, is_static, prototype->name);
    }

    bool is_mangled() const {
        auto* attribute = find_attribute(attributes, "mangle");

        if (attribute == nullptr) {
            return true;
        }

        if (attribute->arguments.empty()) {
            return true;
        }

        if (auto* literal = attribute->arguments[0]->as<BooleanLiteral>(); literal != nullptr) {
            return literal->value;
        }

        return true;
    }

    bool is_member() const { return !parent.empty(); }

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class InterfaceDeclaration;
export class StructDeclaration;
export class EnumDeclaration;

export class InterfaceDeclaration : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::InterfaceDeclaration;

    Visibility::Type visibility;

    std::string name;

    std::vector<Attribute*> attributes;

    std::vector<GenericParameter*> generic_parameters;
    std::vector<TypeExpression*> inheritance;

    std::vector<FunctionPrototype*> methods;

    InterfaceDeclaration(Span span, Visibility::Type visibility, std::string name,
                         std::vector<Attribute*> attributes,
                         std::vector<GenericParameter*> generic_parameters,
                         std::vector<TypeExpression*> inheritance,
                         std::vector<FunctionPrototype*> methods)
        : Statement(static_kind, span), visibility(visibility), name(std::move(name)),
          attributes(std::move(attributes)), generic_parameters(std::move(generic_parameters)),
          inheritance(std::move(inheritance)), methods(std::move(methods)) {}

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

    std::vector<Attribute*> attributes;

    std::vector<GenericParameter*> generic_parameters;
    std::vector<TypeExpression*> inheritance;

    std::vector<Field*> fields;
    std::vector<FunctionDeclaration*> methods;
    std::vector<StructDeclaration*> structs;
    std::vector<EnumDeclaration*> enums;

    StructDeclaration(Span span, Visibility::Type visibility, std::string name,
                      std::vector<Attribute*> attributes,
                      std::vector<GenericParameter*> generic_parameters,
                      std::vector<TypeExpression*> inheritance, std::vector<Field*> fields,
                      std::vector<FunctionDeclaration*> methods,
                      std::vector<StructDeclaration*> structs = {},
                      std::vector<EnumDeclaration*> enums = {})
        : Statement(static_kind, span), visibility(visibility), name(std::move(name)),
          attributes(std::move(attributes)), generic_parameters(std::move(generic_parameters)),
          inheritance(std::move(inheritance)), fields(std::move(fields)),
          methods(std::move(methods)), structs(std::move(structs)), enums(std::move(enums)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class EnumDeclaration : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::EnumDeclaration;

    Visibility::Type visibility;

    std::string name;

    std::vector<Attribute*> attributes;

    std::vector<GenericParameter*> generic_parameters;
    TypeExpression* backing_type;

    std::vector<TypeExpression*> inheritance;
    std::vector<EnumVariant*> variants;
    std::vector<FunctionDeclaration*> methods;

    EnumDeclaration(Span span, Visibility::Type visibility, std::string name,
                    std::vector<Attribute*> attributes,
                    std::vector<GenericParameter*> generic_parameters, TypeExpression* backing_type,
                    std::vector<TypeExpression*> inheritance, std::vector<EnumVariant*> variants,
                    std::vector<FunctionDeclaration*> methods = {})
        : Statement(static_kind, span), visibility(visibility), name(std::move(name)),
          attributes(std::move(attributes)), generic_parameters(std::move(generic_parameters)),
          backing_type(backing_type), inheritance(std::move(inheritance)),
          variants(std::move(variants)), methods(std::move(methods)) {}

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
    std::vector<Attribute*> attributes;

    std::string name;
    TypeExpression* annotation;
    Expression* initializer;

    const Type* type;
    const VariableSymbol* variable_symbol;

    VarDeclaration(Span span, Visibility::Type visibility, StorageKind::Type storage_kind,
                   std::vector<Attribute*> attributes, std::string name, TypeExpression* annotation,
                   Expression* initializer)
        : Statement(static_kind, span), visibility(visibility), storage_kind(storage_kind),
          attributes(std::move(attributes)), name(std::move(name)), annotation(annotation),
          initializer(initializer), type(nullptr), variable_symbol(nullptr) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ExternFunctionDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::ExternFunctionDeclaration;

    Visibility::Type visibility;

    std::vector<Attribute*> attributes;

    FunctionPrototype* prototype;

    const FunctionSymbol* function_symbol;

    ExternFunctionDeclaration(Span span, Visibility::Type visibility,
                              std::vector<Attribute*> attributes, FunctionPrototype* prototype)
        : Statement(static_kind, span), visibility(visibility), attributes(std::move(attributes)),
          prototype(prototype), function_symbol(nullptr) {}

    bool is_mangled() const {
        auto* attribute = find_attribute(attributes, "mangle");

        if (attribute == nullptr) {
            return false;
        }

        if (attribute->arguments.empty()) {
            return true;
        }

        if (auto* literal = attribute->arguments[0]->as<BooleanLiteral>(); literal != nullptr) {
            return literal->value;
        }

        return true;
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
    std::vector<Attribute*> attributes;

    std::string name;
    TypeExpression* annotation;

    const Type* type;
    const VariableSymbol* variable_symbol;

    ExternVarDeclaration(Span span, Visibility::Type visibility, std::vector<Attribute*> attributes,
                         std::string name, TypeExpression* annotation)
        : Statement(static_kind, span), visibility(visibility), attributes(std::move(attributes)),
          name(std::move(name)), annotation(annotation), type(nullptr), variable_symbol(nullptr) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class ImportStatement : public Statement {

  public:
    static constexpr Kind::Type static_kind = Kind::Type::ImportStatement;

    std::vector<IdentifierExpression*> path;
    std::string alias;

    ImportStatement(Span span, std::vector<IdentifierExpression*> path, std::string alias)
        : Statement(static_kind, span), path(std::move(path)), alias(std::move(alias)) {}

    template <typename T>
    T accept(Visitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class TypeAliasDeclaration : public Statement {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::TypeAliasDeclaration;

    Visibility::Type visibility;

    std::string name;

    std::vector<GenericParameter*> generic_parameters;

    TypeExpression* target;

    TypeAliasDeclaration(Span span, Visibility::Type visibility, std::string name,
                         std::vector<GenericParameter*> generic_parameters, TypeExpression* target)
        : Statement(static_kind, span), visibility(visibility), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), target(target) {}

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
    virtual T visit(Attribute& node) = 0;
    virtual T visit(GenericParameter& node) = 0;
    virtual T visit(GenericArgument& node) = 0;
    virtual T visit(Parameter& node) = 0;
    virtual T visit(Argument& node) = 0;
    virtual T visit(FunctionPrototype& node) = 0;
    virtual T visit(Field& node) = 0;
    virtual T visit(EnumVariant& node) = 0;
    virtual T visit(StructLiteralField& node) = 0;
    virtual T visit(WhenPatternField& node) = 0;
    virtual T visit(WhenPattern& node) = 0;
    virtual T visit(WhenArm& node) = 0;

    virtual T visit(NumberLiteral& node) = 0;
    virtual T visit(FloatLiteral& node) = 0;
    virtual T visit(StringLiteral& node) = 0;
    virtual T visit(CharLiteral& node) = 0;
    virtual T visit(BooleanLiteral& node) = 0;
    virtual T visit(NullLiteral& node) = 0;
    virtual T visit(IdentifierExpression& node) = 0;
    virtual T visit(BinaryExpression& node) = 0;
    virtual T visit(UnaryExpression& node) = 0;
    virtual T visit(CoerceExpression& node) = 0;
    virtual T visit(CallExpression& node) = 0;
    virtual T visit(IndexExpression& node) = 0;
    virtual T visit(MemberExpression& node) = 0;
    virtual T visit(QualifiedAccessExpression& node) = 0;
    virtual T visit(AssignExpression& node) = 0;
    virtual T visit(StructLiteralExpression& node) = 0;
    virtual T visit(EnumVariantExpression& node) = 0;
    virtual T visit(WhenExpression& node) = 0;
    virtual T visit(ClosureExpression& node) = 0;
    virtual T visit(BlockExpression& node) = 0;
    virtual T visit(BuiltinCall& node) = 0;

    virtual T visit(BlockStatement& node) = 0;
    virtual T visit(ExpressionStatement& node) = 0;
    virtual T visit(IfExpression& node) = 0;
    virtual T visit(ReturnStatement& node) = 0;
    virtual T visit(WhileStatement& node) = 0;
    virtual T visit(ForStatement& node) = 0;
    virtual T visit(DeferStatement& node) = 0;
    virtual T visit(InterfaceDeclaration& node) = 0;
    virtual T visit(StructDeclaration& node) = 0;
    virtual T visit(EnumDeclaration& node) = 0;
    virtual T visit(VarDeclaration& node) = 0;
    virtual T visit(FunctionDeclaration& node) = 0;
    virtual T visit(ExternFunctionDeclaration& node) = 0;
    virtual T visit(ExternVarDeclaration& node) = 0;
    virtual T visit(ImportStatement& node) = 0;
    virtual T visit(TypeAliasDeclaration& node) = 0;
    virtual T visit(ArrayLiteralExpression& node) = 0;

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
        if (auto* char_literal = expression.as<CharLiteral>(); char_literal != nullptr) {
            return visit(*char_literal);
        }
        if (auto* boolean_literal = expression.as<BooleanLiteral>(); boolean_literal != nullptr) {
            return visit(*boolean_literal);
        }
        if (auto* null_literal = expression.as<NullLiteral>(); null_literal != nullptr) {
            return visit(*null_literal);
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
        if (auto* coerce = expression.as<CoerceExpression>(); coerce != nullptr) {
            return visit(*coerce);
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
        if (auto* qualified_access = expression.as<QualifiedAccessExpression>();
            qualified_access != nullptr) {
            return visit(*qualified_access);
        }
        if (auto* assign = expression.as<AssignExpression>(); assign != nullptr) {
            return visit(*assign);
        }
        if (auto* struct_literal = expression.as<StructLiteralExpression>();
            struct_literal != nullptr) {
            return visit(*struct_literal);
        }
        if (auto* enum_variant = expression.as<EnumVariantExpression>(); enum_variant != nullptr) {
            return visit(*enum_variant);
        }
        if (auto* if_expression = expression.as<IfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }
        if (auto* when_expression = expression.as<WhenExpression>(); when_expression != nullptr) {
            return visit(*when_expression);
        }
        if (auto* closure = expression.as<ClosureExpression>(); closure != nullptr) {
            return visit(*closure);
        }
        if (auto* block = expression.as<BlockExpression>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* builtin_call = expression.as<BuiltinCall>(); builtin_call != nullptr) {
            return visit(*builtin_call);
        }
        if (auto* type_expression = expression.as<TypeExpression>(); type_expression != nullptr) {
            return visit(*type_expression);
        }
        if (auto* array_literal = expression.as<ArrayLiteralExpression>();
            array_literal != nullptr) {
            return visit(*array_literal);
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
        if (auto* while_statement = statement.as<WhileStatement>(); while_statement != nullptr) {
            return visit(*while_statement);
        }
        if (auto* for_statement = statement.as<ForStatement>(); for_statement != nullptr) {
            return visit(*for_statement);
        }
        if (auto* defer_statement = statement.as<DeferStatement>(); defer_statement != nullptr) {
            return visit(*defer_statement);
        }
        if (auto* interface_declaration = statement.as<InterfaceDeclaration>();
            interface_declaration != nullptr) {
            return visit(*interface_declaration);
        }
        if (auto* struct_declaration = statement.as<StructDeclaration>();
            struct_declaration != nullptr) {
            return visit(*struct_declaration);
        }
        if (auto* enum_declaration = statement.as<EnumDeclaration>(); enum_declaration != nullptr) {
            return visit(*enum_declaration);
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
        if (auto* type_alias = statement.as<TypeAliasDeclaration>(); type_alias != nullptr) {
            return visit(*type_alias);
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
        if (auto* field = node.as<Field>(); field != nullptr) {
            return visit(*field);
        }
        if (auto* enum_variant = node.as<EnumVariant>(); enum_variant != nullptr) {
            return visit(*enum_variant);
        }
        if (auto* struct_literal_field = node.as<StructLiteralField>();
            struct_literal_field != nullptr) {
            return visit(*struct_literal_field);
        }
        if (auto* when_pattern_field = node.as<WhenPatternField>(); when_pattern_field != nullptr) {
            return visit(*when_pattern_field);
        }
        if (auto* when_pattern = node.as<WhenPattern>(); when_pattern != nullptr) {
            return visit(*when_pattern);
        }
        if (auto* when_arm = node.as<WhenArm>(); when_arm != nullptr) {
            return visit(*when_arm);
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
        if (auto* char_literal = node.as<CharLiteral>(); char_literal != nullptr) {
            return visit(*char_literal);
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
        if (auto* coerce = node.as<CoerceExpression>(); coerce != nullptr) {
            return visit(*coerce);
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
        if (auto* qualified_access = node.as<QualifiedAccessExpression>();
            qualified_access != nullptr) {
            return visit(*qualified_access);
        }
        if (auto* assign = node.as<AssignExpression>(); assign != nullptr) {
            return visit(*assign);
        }
        if (auto* struct_literal = node.as<StructLiteralExpression>(); struct_literal != nullptr) {
            return visit(*struct_literal);
        }
        if (auto* enum_variant = node.as<EnumVariantExpression>(); enum_variant != nullptr) {
            return visit(*enum_variant);
        }
        if (auto* if_expression = node.as<IfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }
        if (auto* when_expression = node.as<WhenExpression>(); when_expression != nullptr) {
            return visit(*when_expression);
        }
        if (auto* closure = node.as<ClosureExpression>(); closure != nullptr) {
            return visit(*closure);
        }
        if (auto* block = node.as<BlockExpression>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* builtin_call = node.as<BuiltinCall>(); builtin_call != nullptr) {
            return visit(*builtin_call);
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
        if (auto* while_statement = node.as<WhileStatement>(); while_statement != nullptr) {
            return visit(*while_statement);
        }
        if (auto* for_statement = node.as<ForStatement>(); for_statement != nullptr) {
            return visit(*for_statement);
        }
        if (auto* defer_statement = node.as<DeferStatement>(); defer_statement != nullptr) {
            return visit(*defer_statement);
        }
        if (auto* interface_declaration = node.as<InterfaceDeclaration>();
            interface_declaration != nullptr) {
            return visit(*interface_declaration);
        }
        if (auto* struct_declaration = node.as<StructDeclaration>();
            struct_declaration != nullptr) {
            return visit(*struct_declaration);
        }
        if (auto* enum_declaration = node.as<EnumDeclaration>(); enum_declaration != nullptr) {
            return visit(*enum_declaration);
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
        if (auto* type_alias = node.as<TypeAliasDeclaration>(); type_alias != nullptr) {
            return visit(*type_alias);
        }

        return T();
    }
};
