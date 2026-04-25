module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.hir.ir;

import zep.common.position;
import zep.common.span;
import zep.frontend.sema.kind;
import zep.frontend.sema.type;
import zep.common.arena;

export template <typename T>
class HIRVisitor;

export class HIRNodeKind {
  public:
    enum class Type : std::uint8_t {
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
        TypeExpression,
        BlockStatement,
        ExpressionStatement,
        ReturnStatement,
        VarDeclaration,
        FunctionDeclaration,
    };
};

export class HIRNode {
  private:
  protected:
    explicit HIRNode(HIRNodeKind::Type kind, Span span)
        : kind(kind), span(span) {}

    HIRNode(const HIRNode&) = delete;
    HIRNode& operator=(const HIRNode&) = delete;
    HIRNode(HIRNode&&) = default;
    HIRNode& operator=(HIRNode&&) = delete;

  public:
    const HIRNodeKind::Type kind;
    const Span span;

    virtual ~HIRNode() = default;

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

    template <typename T>
    auto accept(HIRVisitor<T>& visitor) -> T;
};

export class HIRExpression : public HIRNode {
  public:
    const Type* type;

    HIRExpression(HIRNodeKind::Type kind, Span span, const Type* type = nullptr)
        : HIRNode(kind, span), type(type) {}
};

export class HIRStatement : public HIRNode {
  public:
    const Type* type;

    HIRStatement(HIRNodeKind::Type kind, Span span, const Type* type = nullptr)
        : HIRNode(kind, span), type(type) {}
};

export class HIRBlockStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BlockStatement;

    std::vector<HIRStatement*> statements;

    explicit HIRBlockStatement(Span span, std::vector<HIRStatement*> statements)
        : HIRStatement(static_kind, span), statements(std::move(statements)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRNumberLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::NumberLiteral;

    std::string value;

    HIRNumberLiteral(Span span, std::string value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(std::move(value)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRFloatLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::FloatLiteral;

    std::string value;

    HIRFloatLiteral(Span span, std::string value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(std::move(value)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRStringLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::StringLiteral;

    std::string value;

    HIRStringLiteral(Span span, std::string value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(std::move(value)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRBooleanLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BooleanLiteral;

    bool value;

    HIRBooleanLiteral(Span span, bool value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(value) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRIdentifierExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IdentifierExpression;

    std::string name;

    HIRIdentifierExpression(Span span, std::string name, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), name(std::move(name)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRBinaryExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BinaryExpression;

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
    };

    HIRExpression* left;
    Operator::Type op;
    HIRExpression* right;

    HIRBinaryExpression(Span span, HIRExpression* left, Operator::Type op,
                        HIRExpression* right, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), left(left), op(op),
          right(right) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRUnaryExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::UnaryExpression;

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
    HIRExpression* operand;

    HIRUnaryExpression(Span span, Operator::Type op, HIRExpression* operand,
                       const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), op(op), operand(operand) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRCallExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::CallExpression;

    HIRExpression* callee;
    std::vector<HIRExpression*> arguments;

    HIRCallExpression(Span span, HIRExpression* callee,
                      std::vector<HIRExpression*> arguments,
                      const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), callee(callee),
          arguments(std::move(arguments)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRIndexExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IndexExpression;

    HIRExpression* object;
    HIRExpression* index;

    HIRIndexExpression(Span span, HIRExpression* object,
                       HIRExpression* index, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), object(object),
          index(index) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRMemberExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::MemberExpression;

    HIRExpression* object;
    std::string member;

    HIRMemberExpression(Span span, HIRExpression* object, std::string member,
                        const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), object(object),
          member(std::move(member)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRAssignExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::AssignExpression;

    HIRExpression* target;
    HIRExpression* value;

    HIRAssignExpression(Span span, HIRExpression* target,
                        HIRExpression* value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), target(target),
          value(value) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRStructLiteralField {
  public:
    std::string name;
    HIRExpression* value;

    HIRStructLiteralField(std::string name, HIRExpression* value)
        : name(std::move(name)), value(value) {}
};

export class HIRStructLiteralExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::StructLiteralExpression;

    std::string name;
    std::vector<HIRStructLiteralField> fields;

    HIRStructLiteralExpression(Span span, std::string name,
                               std::vector<HIRStructLiteralField> fields,
                               const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), name(std::move(name)),
          fields(std::move(fields)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRIfExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IfExpression;

    HIRExpression* condition;
    HIRStatement* then_branch;
    HIRStatement* else_branch;

    HIRIfExpression(Span span, HIRExpression* condition,
                    HIRStatement* then_branch,
                    HIRStatement* else_branch, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), condition(condition),
          then_branch(then_branch), else_branch(else_branch) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRTypeExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::TypeExpression;

    const Type* type_value;

    HIRTypeExpression(Span span, const Type* type_value)
        : HIRExpression(static_kind, span, type_value), type_value(type_value) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRExpressionStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::ExpressionStatement;

    HIRExpression* expression;

    explicit HIRExpressionStatement(Span span, HIRExpression* expression)
        : HIRStatement(static_kind, span, expression != nullptr ? expression->type : nullptr),
          expression(expression) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRReturnStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::ReturnStatement;

    HIRExpression* value;

    HIRReturnStatement(Span span, HIRExpression* value, const Type* type = nullptr)
        : HIRStatement(static_kind, span, type), value(value) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRVarDeclaration : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::VarDeclaration;

    Visibility::Type visibility;
    StorageKind::Type storage_kind;
    std::string name;
    HIRExpression* initializer;

    HIRVarDeclaration(Span span, Visibility::Type visibility, StorageKind::Type storage_kind,
                      std::string name, const Type* type,
                      HIRExpression* initializer)
        : HIRStatement(static_kind, span, type), visibility(visibility),
          storage_kind(storage_kind), name(std::move(name)), initializer(initializer) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRParameter {
  public:
    std::string name;
    const Type* type;

    HIRParameter(std::string name, const Type* type) : name(std::move(name)), type(type) {}
};

export class HIRFunctionDeclaration : public HIRNode {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::FunctionDeclaration;

    Visibility::Type visibility;
    std::string name;
    std::vector<HIRParameter> parameters;
    const Type* return_type;
    HIRBlockStatement* body;
    bool variadic;
    const Type* type;

    HIRFunctionDeclaration(Span span, Visibility::Type visibility, std::string name,
                           std::vector<HIRParameter> parameters, const Type* return_type,
                           HIRBlockStatement* body, bool variadic, const Type* type)
        : HIRNode(static_kind, span), visibility(visibility), name(std::move(name)),
          parameters(std::move(parameters)), return_type(return_type), body(body),
          variadic(variadic), type(type) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export template <typename T>
class HIRVisitor {
  public:
    virtual ~HIRVisitor() = default;

    virtual T visit(HIRBlockStatement& node) = 0;
    virtual T visit(HIRNumberLiteral& node) = 0;
    virtual T visit(HIRFloatLiteral& node) = 0;
    virtual T visit(HIRStringLiteral& node) = 0;
    virtual T visit(HIRBooleanLiteral& node) = 0;
    virtual T visit(HIRIdentifierExpression& node) = 0;
    virtual T visit(HIRBinaryExpression& node) = 0;
    virtual T visit(HIRUnaryExpression& node) = 0;
    virtual T visit(HIRCallExpression& node) = 0;
    virtual T visit(HIRIndexExpression& node) = 0;
    virtual T visit(HIRMemberExpression& node) = 0;
    virtual T visit(HIRAssignExpression& node) = 0;
    virtual T visit(HIRStructLiteralExpression& node) = 0;
    virtual T visit(HIRIfExpression& node) = 0;
    virtual T visit(HIRTypeExpression& node) = 0;
    virtual T visit(HIRExpressionStatement& node) = 0;
    virtual T visit(HIRReturnStatement& node) = 0;
    virtual T visit(HIRVarDeclaration& node) = 0;
    virtual T visit(HIRFunctionDeclaration& node) = 0;
};

template <typename T>
auto HIRNode::accept(HIRVisitor<T>& visitor) -> T {
    switch (kind) {
    case HIRNodeKind::Type::BlockStatement:
        return static_cast<HIRBlockStatement*>(this)->accept(visitor);
    case HIRNodeKind::Type::NumberLiteral:
        return static_cast<HIRNumberLiteral*>(this)->accept(visitor);
    case HIRNodeKind::Type::FloatLiteral:
        return static_cast<HIRFloatLiteral*>(this)->accept(visitor);
    case HIRNodeKind::Type::StringLiteral:
        return static_cast<HIRStringLiteral*>(this)->accept(visitor);
    case HIRNodeKind::Type::BooleanLiteral:
        return static_cast<HIRBooleanLiteral*>(this)->accept(visitor);
    case HIRNodeKind::Type::IdentifierExpression:
        return static_cast<HIRIdentifierExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::BinaryExpression:
        return static_cast<HIRBinaryExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::UnaryExpression:
        return static_cast<HIRUnaryExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::CallExpression:
        return static_cast<HIRCallExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::IndexExpression:
        return static_cast<HIRIndexExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::MemberExpression:
        return static_cast<HIRMemberExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::AssignExpression:
        return static_cast<HIRAssignExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::StructLiteralExpression:
        return static_cast<HIRStructLiteralExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::IfExpression:
        return static_cast<HIRIfExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::TypeExpression:
        return static_cast<HIRTypeExpression*>(this)->accept(visitor);
    case HIRNodeKind::Type::ExpressionStatement:
        return static_cast<HIRExpressionStatement*>(this)->accept(visitor);
    case HIRReturnStatement::static_kind:
        return static_cast<HIRReturnStatement*>(this)->accept(visitor);
    case HIRVarDeclaration::static_kind:
        return static_cast<HIRVarDeclaration*>(this)->accept(visitor);
    case HIRFunctionDeclaration::static_kind:
        return static_cast<HIRFunctionDeclaration*>(this)->accept(visitor);
    }
    return T();
}

export using HIRArena = Arena<HIRNode>;
