module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.hir.node;

import zep.common.position;
import zep.common.span;
import zep.frontend.sema.kind;
import zep.frontend.sema.type;
import zep.frontend.node;
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
    explicit HIRNode(HIRNodeKind::Type kind, Span span) : kind(kind), span(span) {}

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
};

export using HIRNodeArena = Arena<HIRNode>;

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

    HIRExpression* left;
    BinaryExpression::Operator::Type op;
    HIRExpression* right;

    HIRBinaryExpression(Span span, HIRExpression* left, BinaryExpression::Operator::Type op,
                        HIRExpression* right, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), left(left), op(op), right(right) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRUnaryExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::UnaryExpression;

    UnaryExpression::Operator::Type op;
    HIRExpression* operand;

    HIRUnaryExpression(Span span, UnaryExpression::Operator::Type op, HIRExpression* operand,
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

    HIRCallExpression(Span span, HIRExpression* callee, std::vector<HIRExpression*> arguments,
                      const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), callee(callee), arguments(std::move(arguments)) {}

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

    HIRIndexExpression(Span span, HIRExpression* object, HIRExpression* index,
                       const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), object(object), index(index) {}

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
        : HIRExpression(static_kind, span, type), object(object), member(std::move(member)) {}

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

    HIRAssignExpression(Span span, HIRExpression* target, HIRExpression* value,
                        const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), target(target), value(value) {}

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
        : HIRExpression(static_kind, span, type), name(std::move(name)), fields(std::move(fields)) {
    }

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

    HIRIfExpression(Span span, HIRExpression* condition, HIRStatement* then_branch,
                    HIRStatement* else_branch, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), condition(condition), then_branch(then_branch),
          else_branch(else_branch) {}

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
                      std::string name, const Type* type, HIRExpression* initializer)
        : HIRStatement(static_kind, span, type), visibility(visibility), storage_kind(storage_kind),
          name(std::move(name)), initializer(initializer) {}

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

export class HIRFunctionDeclaration : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::FunctionDeclaration;

    Visibility::Type visibility;
    std::string name;
    std::vector<HIRParameter> parameters;
    const Type* return_type;
    HIRBlockStatement* body;
    bool variadic;
    Linkage::Type linkage;

    HIRFunctionDeclaration(Span span, Visibility::Type visibility, Linkage::Type linkage,
                           std::string name, std::vector<HIRParameter> parameters,
                           const Type* return_type, HIRBlockStatement* body, bool variadic,
                           const Type* type)
        : HIRStatement(static_kind, span, type), visibility(visibility), name(std::move(name)),
          parameters(std::move(parameters)), return_type(return_type), body(body),
          variadic(variadic), linkage(linkage) {}

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

    T visit_expression(HIRExpression& expression) {
        if (auto* number_literal = expression.as<HIRNumberLiteral>(); number_literal != nullptr) {
            return visit(*number_literal);
        }
        if (auto* float_literal = expression.as<HIRFloatLiteral>(); float_literal != nullptr) {
            return visit(*float_literal);
        }
        if (auto* string_literal = expression.as<HIRStringLiteral>(); string_literal != nullptr) {
            return visit(*string_literal);
        }
        if (auto* boolean_literal = expression.as<HIRBooleanLiteral>();
            boolean_literal != nullptr) {
            return visit(*boolean_literal);
        }
        if (auto* identifier = expression.as<HIRIdentifierExpression>(); identifier != nullptr) {
            return visit(*identifier);
        }
        if (auto* binary = expression.as<HIRBinaryExpression>(); binary != nullptr) {
            return visit(*binary);
        }
        if (auto* unary = expression.as<HIRUnaryExpression>(); unary != nullptr) {
            return visit(*unary);
        }
        if (auto* call = expression.as<HIRCallExpression>(); call != nullptr) {
            return visit(*call);
        }
        if (auto* index = expression.as<HIRIndexExpression>(); index != nullptr) {
            return visit(*index);
        }
        if (auto* member = expression.as<HIRMemberExpression>(); member != nullptr) {
            return visit(*member);
        }
        if (auto* assign = expression.as<HIRAssignExpression>(); assign != nullptr) {
            return visit(*assign);
        }
        if (auto* struct_literal = expression.as<HIRStructLiteralExpression>();
            struct_literal != nullptr) {
            return visit(*struct_literal);
        }
        if (auto* if_expression = expression.as<HIRIfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }
        if (auto* type_expression = expression.as<HIRTypeExpression>();
            type_expression != nullptr) {
            return visit(*type_expression);
        }

        return T();
    }

    T visit_statement(HIRStatement& statement) {
        if (auto* block = statement.as<HIRBlockStatement>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* expression_statement = statement.as<HIRExpressionStatement>();
            expression_statement != nullptr) {
            return visit(*expression_statement);
        }
        if (auto* return_statement = statement.as<HIRReturnStatement>();
            return_statement != nullptr) {
            return visit(*return_statement);
        }
        if (auto* var_declaration = statement.as<HIRVarDeclaration>(); var_declaration != nullptr) {
            return visit(*var_declaration);
        }
        if (auto* function_declaration = statement.as<HIRFunctionDeclaration>();
            function_declaration != nullptr) {
            return visit(*function_declaration);
        }

        return T();
    }

    T visit_node(HIRNode& node) {
        if (auto* type_expression = node.as<HIRTypeExpression>(); type_expression != nullptr) {
            return visit(*type_expression);
        }

        if (auto* number_literal = node.as<HIRNumberLiteral>(); number_literal != nullptr) {
            return visit(*number_literal);
        }
        if (auto* float_literal = node.as<HIRFloatLiteral>(); float_literal != nullptr) {
            return visit(*float_literal);
        }
        if (auto* string_literal = node.as<HIRStringLiteral>(); string_literal != nullptr) {
            return visit(*string_literal);
        }
        if (auto* boolean_literal = node.as<HIRBooleanLiteral>(); boolean_literal != nullptr) {
            return visit(*boolean_literal);
        }
        if (auto* identifier = node.as<HIRIdentifierExpression>(); identifier != nullptr) {
            return visit(*identifier);
        }
        if (auto* binary = node.as<HIRBinaryExpression>(); binary != nullptr) {
            return visit(*binary);
        }
        if (auto* unary = node.as<HIRUnaryExpression>(); unary != nullptr) {
            return visit(*unary);
        }
        if (auto* call = node.as<HIRCallExpression>(); call != nullptr) {
            return visit(*call);
        }
        if (auto* index = node.as<HIRIndexExpression>(); index != nullptr) {
            return visit(*index);
        }
        if (auto* member = node.as<HIRMemberExpression>(); member != nullptr) {
            return visit(*member);
        }
        if (auto* assign = node.as<HIRAssignExpression>(); assign != nullptr) {
            return visit(*assign);
        }
        if (auto* struct_literal = node.as<HIRStructLiteralExpression>();
            struct_literal != nullptr) {
            return visit(*struct_literal);
        }
        if (auto* if_expression = node.as<HIRIfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }

        if (auto* block = node.as<HIRBlockStatement>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* expression_statement = node.as<HIRExpressionStatement>();
            expression_statement != nullptr) {
            return visit(*expression_statement);
        }
        if (auto* return_statement = node.as<HIRReturnStatement>(); return_statement != nullptr) {
            return visit(*return_statement);
        }
        if (auto* var_declaration = node.as<HIRVarDeclaration>(); var_declaration != nullptr) {
            return visit(*var_declaration);
        }
        if (auto* function_declaration = node.as<HIRFunctionDeclaration>();
            function_declaration != nullptr) {
            return visit(*function_declaration);
        }

        return T();
    }
};
