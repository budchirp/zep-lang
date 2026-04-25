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
        BlockStatement,
        ExpressionStatement,
        ReturnStatement,
        VarDeclaration,
    };
};

export class HIRNode {
  private:
  protected:
    explicit HIRNode(HIRNodeKind::Type kind, Span span) : kind(kind), span(span) {}

    HIRNode(const HIRNode&) = delete;
    HIRNode& operator=(const HIRNode&) = delete;
    HIRNode(HIRNode&&) = default;
    HIRNode& operator=(HIRNode&&) = default;

  public:
    HIRNodeKind::Type kind;
    Span span;

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

    std::vector<std::unique_ptr<HIRStatement>> statements;

    explicit HIRBlockStatement(Span span, std::vector<std::unique_ptr<HIRStatement>> statements)
        : HIRStatement(static_kind, span), statements(std::move(statements)) {}
};

export class HIRNumberLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::NumberLiteral;

    std::string value;

    HIRNumberLiteral(Span span, std::string value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(std::move(value)) {}
};

export class HIRFloatLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::FloatLiteral;

    std::string value;

    HIRFloatLiteral(Span span, std::string value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(std::move(value)) {}
};

export class HIRStringLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::StringLiteral;

    std::string value;

    HIRStringLiteral(Span span, std::string value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(std::move(value)) {}
};

export class HIRBooleanLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BooleanLiteral;

    bool value;

    HIRBooleanLiteral(Span span, bool value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(value) {}
};

export class HIRIdentifierExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IdentifierExpression;

    std::string name;

    HIRIdentifierExpression(Span span, std::string name, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), name(std::move(name)) {}
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

    std::unique_ptr<HIRExpression> left;
    Operator::Type op;
    std::unique_ptr<HIRExpression> right;

    HIRBinaryExpression(Span span, std::unique_ptr<HIRExpression> left, Operator::Type op,
                        std::unique_ptr<HIRExpression> right, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), left(std::move(left)), op(op),
          right(std::move(right)) {}
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
    std::unique_ptr<HIRExpression> operand;

    HIRUnaryExpression(Span span, Operator::Type op, std::unique_ptr<HIRExpression> operand,
                       const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), op(op), operand(std::move(operand)) {}
};

export class HIRCallExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::CallExpression;

    std::unique_ptr<HIRExpression> callee;
    std::vector<std::unique_ptr<HIRExpression>> arguments;

    HIRCallExpression(Span span, std::unique_ptr<HIRExpression> callee,
                      std::vector<std::unique_ptr<HIRExpression>> arguments,
                      const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), callee(std::move(callee)),
          arguments(std::move(arguments)) {}
};

export class HIRIndexExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IndexExpression;

    std::unique_ptr<HIRExpression> object;
    std::unique_ptr<HIRExpression> index;

    HIRIndexExpression(Span span, std::unique_ptr<HIRExpression> object,
                       std::unique_ptr<HIRExpression> index, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), object(std::move(object)),
          index(std::move(index)) {}
};

export class HIRMemberExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::MemberExpression;

    std::unique_ptr<HIRExpression> object;
    std::string member;

    HIRMemberExpression(Span span, std::unique_ptr<HIRExpression> object, std::string member,
                        const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), object(std::move(object)),
          member(std::move(member)) {}
};

export class HIRAssignExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::AssignExpression;

    std::unique_ptr<HIRExpression> target;
    std::unique_ptr<HIRExpression> value;

    HIRAssignExpression(Span span, std::unique_ptr<HIRExpression> target,
                        std::unique_ptr<HIRExpression> value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), target(std::move(target)),
          value(std::move(value)) {}
};

export class HIRStructLiteralField {
  public:
    std::string name;
    std::unique_ptr<HIRExpression> value;

    HIRStructLiteralField(std::string name, std::unique_ptr<HIRExpression> value)
        : name(std::move(name)), value(std::move(value)) {}
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
};

export class HIRIfExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IfExpression;

    std::unique_ptr<HIRExpression> condition;
    std::unique_ptr<HIRStatement> then_branch;
    std::unique_ptr<HIRStatement> else_branch;

    HIRIfExpression(Span span, std::unique_ptr<HIRExpression> condition,
                    std::unique_ptr<HIRStatement> then_branch,
                    std::unique_ptr<HIRStatement> else_branch, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), condition(std::move(condition)),
          then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}
};

export class HIRExpressionStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::ExpressionStatement;

    std::unique_ptr<HIRExpression> expression;

    explicit HIRExpressionStatement(Span span, std::unique_ptr<HIRExpression> expression)
        : HIRStatement(static_kind, span, expression != nullptr ? expression->type : nullptr),
          expression(std::move(expression)) {}
};

export class HIRReturnStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::ReturnStatement;

    std::unique_ptr<HIRExpression> value;

    HIRReturnStatement(Span span, std::unique_ptr<HIRExpression> value, const Type* type = nullptr)
        : HIRStatement(static_kind, span, type), value(std::move(value)) {}
};

export class HIRVarDeclaration : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::VarDeclaration;

    Visibility::Type visibility;
    StorageKind::Type storage_kind;
    std::string name;
    std::unique_ptr<HIRExpression> initializer;

    HIRVarDeclaration(Span span, Visibility::Type visibility, StorageKind::Type storage_kind,
                      std::string name, const Type* type,
                      std::unique_ptr<HIRExpression> initializer)
        : HIRStatement(static_kind, span, type), visibility(visibility),
          storage_kind(storage_kind), name(std::move(name)), initializer(std::move(initializer)) {}
};

export class HIRParameter {
  public:
    std::string name;
    const Type* type;

    HIRParameter(std::string name, const Type* type) : name(std::move(name)), type(type) {}
};

export class HIRFunctionDeclaration {
  private:
    Span span;

  public:
    Visibility::Type visibility;
    std::string name;
    std::vector<HIRParameter> parameters;
    const Type* return_type;
    std::unique_ptr<HIRBlockStatement> body;
    bool variadic;

    HIRFunctionDeclaration(Span span, Visibility::Type visibility, std::string name,
                           std::vector<HIRParameter> parameters, const Type* return_type,
                           std::unique_ptr<HIRBlockStatement> body, bool variadic)
        : span(span), visibility(visibility), name(std::move(name)),
          parameters(std::move(parameters)), return_type(return_type), body(std::move(body)),
          variadic(variadic) {}
};

export class HIRProgram {
  public:
    std::vector<std::unique_ptr<HIRFunctionDeclaration>> functions;

    explicit HIRProgram(std::vector<std::unique_ptr<HIRFunctionDeclaration>> functions)
        : functions(std::move(functions)) {}
};
