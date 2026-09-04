module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.hir.node;

import zep.common.source.position;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
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
        StringArrayExpression,
        CharLiteral,
        BooleanLiteral,
        NullLiteral,
        IdentifierExpression,
        BinaryExpression,
        UnaryExpression,
        CoerceExpression,
        DropFlagClearExpression,
        CallExpression,
        IndexExpression,
        MemberExpression,
        AssignExpression,
        StructLiteralExpression,
        EnumVariantExpression,
        IfExpression,
        WhenExpression,
        ClosureExpression,
        BlockExpression,
        TypeExpression,
        BlockStatement,
        StatementGroup,
        ExpressionStatement,
        ReturnStatement,
        LoopStatement,
        VarDeclaration,
        FunctionDeclaration,
        ArrayLiteralExpression,
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

export class HIRBlockExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BlockExpression;

    HIRBlockStatement* body;

    HIRBlockExpression(Span span, HIRBlockStatement* body, const Type* type)
        : HIRExpression(static_kind, span, type), body(body) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRStatementGroup : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::StatementGroup;

    std::vector<HIRStatement*> statements;

    explicit HIRStatementGroup(Span span, std::vector<HIRStatement*> statements)
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

export class HIRStringArrayExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::StringArrayExpression;

    std::vector<std::string> values;

    HIRStringArrayExpression(Span span, std::vector<std::string> values, const Type* type)
        : HIRExpression(static_kind, span, type), values(std::move(values)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRCharLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::CharLiteral;

    std::uint8_t value;

    HIRCharLiteral(Span span, std::uint8_t value, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(value) {}

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

export class HIRNullLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::NullLiteral;

    HIRNullLiteral(Span span, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRIdentifierExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IdentifierExpression;

    std::string name;
    const VariableSymbol* variable_symbol;
    const FunctionSymbol* function_symbol;

    HIRIdentifierExpression(Span span, std::string name, const Type* type = nullptr,
                            const VariableSymbol* variable_symbol = nullptr,
                            const FunctionSymbol* function_symbol = nullptr)
        : HIRExpression(static_kind, span, type), name(std::move(name)),
          variable_symbol(variable_symbol), function_symbol(function_symbol) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRBinaryExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BinaryExpression;

    HIRExpression* left;
    BinaryOperator::Type op;
    HIRExpression* right;

    HIRBinaryExpression(Span span, HIRExpression* left, BinaryOperator::Type op,
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

    UnaryOperator::Type op;
    HIRExpression* operand;

    HIRUnaryExpression(Span span, UnaryOperator::Type op, HIRExpression* operand,
                       const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), op(op), operand(operand) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRCoerceExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::CoerceExpression;

    HIRExpression* value;
    Coercion::Type coercion;
    const Type* source_type;

    HIRCoerceExpression(Span span, HIRExpression* value, const Type* type, Coercion::Type coercion,
                        const Type* source_type)
        : HIRExpression(static_kind, span, type), value(value), coercion(coercion),
          source_type(source_type) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRCallTarget {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Direct, Indirect, Interface, Intrinsic };
    };

    const Kind::Type kind;

    explicit HIRCallTarget(Kind::Type kind) : kind(kind) {}
    virtual ~HIRCallTarget() = default;

    template <typename T>
    const T* as() const {
        return kind == T::static_kind ? static_cast<const T*>(this) : nullptr;
    }
};

export class HIRDirectCallTarget final : public HIRCallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Direct;
    const FunctionSymbol* function_symbol;
    std::string emitted_name;
    const FunctionType* function_type;

    HIRDirectCallTarget(const FunctionSymbol& function_symbol, std::string emitted_name,
                        const FunctionType* function_type = nullptr)
        : HIRCallTarget(static_kind), function_symbol(&function_symbol),
          emitted_name(std::move(emitted_name)),
          function_type(function_type != nullptr ? function_type : function_symbol.function_type) {}
};

export class HIRIndirectCallTarget final : public HIRCallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Indirect;
    HIRExpression* callee;
    const FunctionType* function_type;

    HIRIndirectCallTarget(HIRExpression* callee, const FunctionType& function_type)
        : HIRCallTarget(static_kind), callee(callee), function_type(&function_type) {}
};

export class HIRInterfaceCallTarget final : public HIRCallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Interface;
    const FunctionSymbol* method_symbol;
    std::size_t slot;

    HIRInterfaceCallTarget(const FunctionSymbol& method_symbol, std::size_t slot)
        : HIRCallTarget(static_kind), method_symbol(&method_symbol), slot(slot) {}
};

export class HIRIntrinsicCallTarget final : public HIRCallTarget {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Intrinsic;
    Intrinsic::Type intrinsic;

    explicit HIRIntrinsicCallTarget(Intrinsic::Type intrinsic)
        : HIRCallTarget(static_kind), intrinsic(intrinsic) {}
};

export class HIRCallExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::CallExpression;

    std::vector<HIRExpression*> arguments;
    std::unique_ptr<HIRCallTarget> target;

    HIRCallExpression(Span span, std::unique_ptr<HIRCallTarget> target,
                      std::vector<HIRExpression*> arguments, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), arguments(std::move(arguments)),
          target(std::move(target)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRDropFlagClearExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::DropFlagClearExpression;

    HIRExpression* value;
    std::string drop_flag_name;

    HIRDropFlagClearExpression(Span span, HIRExpression* value, std::string drop_flag_name,
                               const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), value(value),
          drop_flag_name(std::move(drop_flag_name)) {}

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

export class HIREnumVariantExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::EnumVariantExpression;

    std::string enum_name;
    std::string variant_name;
    std::vector<HIRStructLiteralField> fields;
    std::size_t variant_index;

    HIREnumVariantExpression(Span span, std::string enum_name, std::string variant_name,
                             std::vector<HIRStructLiteralField> fields, std::size_t variant_index,
                             const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), enum_name(std::move(enum_name)),
          variant_name(std::move(variant_name)), fields(std::move(fields)),
          variant_index(variant_index) {}

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

export class HIRWhenPatternKind {
  public:
    enum class Type : std::uint8_t {
        Expression,
        EnumVariant,
    };
};

export class HIRWhenPatternField {
  public:
    std::string field_name;
    std::string binding_name;
    const Type* type;

    HIRWhenPatternField(std::string field_name, std::string binding_name, const Type* type)
        : field_name(std::move(field_name)), binding_name(std::move(binding_name)), type(type) {}
};

export class HIRWhenPattern {
  public:
    HIRWhenPatternKind::Type pattern_kind;
    HIRExpression* expression;
    std::string enum_name;
    std::string variant_name;
    std::size_t variant_index;
    HIRExpression* variant_value;
    std::vector<HIRWhenPatternField> fields;

    explicit HIRWhenPattern(HIRExpression* expression)
        : pattern_kind(HIRWhenPatternKind::Type::Expression), expression(expression), enum_name(),
          variant_name(), variant_index(0), variant_value(nullptr), fields() {}

    HIRWhenPattern(std::string enum_name, std::string variant_name, std::size_t variant_index,
                   HIRExpression* variant_value, std::vector<HIRWhenPatternField> fields)
        : pattern_kind(HIRWhenPatternKind::Type::EnumVariant), expression(nullptr),
          enum_name(std::move(enum_name)), variant_name(std::move(variant_name)),
          variant_index(variant_index), variant_value(variant_value), fields(std::move(fields)) {}
};

export class HIRWhenArm {
  public:
    bool is_else;
    std::vector<HIRWhenPattern> patterns;
    HIRExpression* guard;
    HIRStatement* body;

    HIRWhenArm(bool is_else, std::vector<HIRWhenPattern> patterns, HIRExpression* guard,
               HIRStatement* body)
        : is_else(is_else), patterns(std::move(patterns)), guard(guard), body(body) {}
};

export class HIRWhenExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::WhenExpression;

    HIRExpression* subject;
    std::vector<HIRWhenArm> arms;
    bool is_exhaustive;

    HIRWhenExpression(Span span, HIRExpression* subject, std::vector<HIRWhenArm> arms,
                      bool is_exhaustive, const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), subject(subject), arms(std::move(arms)),
          is_exhaustive(is_exhaustive) {}

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

export class HIRLoopStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::LoopStatement;

    std::vector<HIRStatement*> initializers;
    HIRExpression* condition;
    HIRExpression* step;
    HIRStatement* body;

    HIRLoopStatement(Span span, std::vector<HIRStatement*> initializers, HIRExpression* condition,
                     HIRExpression* step, HIRStatement* body, const Type* type = nullptr)
        : HIRStatement(static_kind, span, type), initializers(std::move(initializers)),
          condition(condition), step(step), body(body) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRArrayLiteralExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::ArrayLiteralExpression;

    std::vector<HIRExpression*> elements;

    HIRArrayLiteralExpression(Span span, std::vector<HIRExpression*> elements,
                              const Type* type = nullptr)
        : HIRExpression(static_kind, span, type), elements(std::move(elements)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRVarDeclaration : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::VarDeclaration;

    Visibility::Type visibility;
    Linkage::Type linkage;
    StorageKind::Type storage_kind;
    std::string name;
    HIRExpression* initializer;
    bool is_global;
    std::vector<AttributeInfo> attributes;

    HIRVarDeclaration(Span span, Visibility::Type visibility, Linkage::Type linkage,
                      StorageKind::Type storage_kind, std::string name, const Type* type,
                      HIRExpression* initializer, bool is_global = false,
                      std::vector<AttributeInfo> attributes = {})
        : HIRStatement(static_kind, span, type), visibility(visibility), linkage(linkage),
          storage_kind(storage_kind), name(std::move(name)), initializer(initializer),
          is_global(is_global), attributes(std::move(attributes)) {}

    template <typename T>
    T accept(HIRVisitor<T>& visitor) {
        return visitor.visit(*this);
    }
};

export class HIRFunctionDeclaration : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::FunctionDeclaration;

    Visibility::Type visibility;
    std::string name;
    const FunctionSymbol* function_symbol;
    const Type* return_type;
    HIRBlockStatement* body;
    bool variadic;
    Linkage::Type linkage;
    bool is_mangled;
    Abi::Type abi;

    HIRFunctionDeclaration(Span span, Visibility::Type visibility, Linkage::Type linkage,
                           std::string name, const Type* return_type, HIRBlockStatement* body,
                           bool variadic, const Type* type, bool is_mangled = true,
                           Abi::Type abi = Abi::Type::Language,
                           const FunctionSymbol* function_symbol = nullptr)
        : HIRStatement(static_kind, span, type), visibility(visibility), name(std::move(name)),
          function_symbol(function_symbol), return_type(return_type), body(body),
          variadic(variadic), linkage(linkage), is_mangled(is_mangled), abi(abi) {}

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
    virtual T visit(HIRBlockExpression& node) = 0;
    virtual T visit(HIRNumberLiteral& node) = 0;
    virtual T visit(HIRFloatLiteral& node) = 0;
    virtual T visit(HIRStringLiteral& node) = 0;
    virtual T visit(HIRStringArrayExpression& node) = 0;
    virtual T visit(HIRCharLiteral& node) = 0;
    virtual T visit(HIRBooleanLiteral& node) = 0;
    virtual T visit(HIRNullLiteral& node) = 0;
    virtual T visit(HIRIdentifierExpression& node) = 0;
    virtual T visit(HIRBinaryExpression& node) = 0;
    virtual T visit(HIRUnaryExpression& node) = 0;
    virtual T visit(HIRCoerceExpression& node) = 0;
    virtual T visit(HIRDropFlagClearExpression& node) = 0;
    virtual T visit(HIRCallExpression& node) = 0;
    virtual T visit(HIRIndexExpression& node) = 0;
    virtual T visit(HIRMemberExpression& node) = 0;
    virtual T visit(HIRAssignExpression& node) = 0;
    virtual T visit(HIRStructLiteralExpression& node) = 0;
    virtual T visit(HIREnumVariantExpression& node) = 0;
    virtual T visit(HIRIfExpression& node) = 0;
    virtual T visit(HIRWhenExpression& node) = 0;
    virtual T visit(HIRArrayLiteralExpression& node) = 0;
    virtual T visit(HIRTypeExpression& node) = 0;
    virtual T visit(HIRStatementGroup& node) = 0;
    virtual T visit(HIRExpressionStatement& node) = 0;
    virtual T visit(HIRReturnStatement& node) = 0;
    virtual T visit(HIRLoopStatement& node) = 0;
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
        if (auto* string_array = expression.as<HIRStringArrayExpression>();
            string_array != nullptr) {
            return visit(*string_array);
        }
        if (auto* char_literal = expression.as<HIRCharLiteral>(); char_literal != nullptr) {
            return visit(*char_literal);
        }
        if (auto* boolean_literal = expression.as<HIRBooleanLiteral>();
            boolean_literal != nullptr) {
            return visit(*boolean_literal);
        }
        if (auto* null_literal = expression.as<HIRNullLiteral>(); null_literal != nullptr) {
            return visit(*null_literal);
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
        if (auto* coerce = expression.as<HIRCoerceExpression>(); coerce != nullptr) {
            return visit(*coerce);
        }
        if (auto* clear = expression.as<HIRDropFlagClearExpression>(); clear != nullptr) {
            return visit(*clear);
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
        if (auto* enum_variant = expression.as<HIREnumVariantExpression>();
            enum_variant != nullptr) {
            return visit(*enum_variant);
        }
        if (auto* if_expression = expression.as<HIRIfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }
        if (auto* when_expression = expression.as<HIRWhenExpression>();
            when_expression != nullptr) {
            return visit(*when_expression);
        }
        if (auto* block = expression.as<HIRBlockExpression>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* type_expression = expression.as<HIRTypeExpression>();
            type_expression != nullptr) {
            return visit(*type_expression);
        }
        if (auto* array_literal = expression.as<HIRArrayLiteralExpression>();
            array_literal != nullptr) {
            return visit(*array_literal);
        }

        return T();
    }

    T visit_statement(HIRStatement& statement) {
        if (auto* block = statement.as<HIRBlockStatement>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* group = statement.as<HIRStatementGroup>(); group != nullptr) {
            return visit(*group);
        }
        if (auto* expression_statement = statement.as<HIRExpressionStatement>();
            expression_statement != nullptr) {
            return visit(*expression_statement);
        }
        if (auto* return_statement = statement.as<HIRReturnStatement>();
            return_statement != nullptr) {
            return visit(*return_statement);
        }
        if (auto* loop_statement = statement.as<HIRLoopStatement>(); loop_statement != nullptr) {
            return visit(*loop_statement);
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
        if (auto* string_array = node.as<HIRStringArrayExpression>(); string_array != nullptr) {
            return visit(*string_array);
        }
        if (auto* char_literal = node.as<HIRCharLiteral>(); char_literal != nullptr) {
            return visit(*char_literal);
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
        if (auto* coerce = node.as<HIRCoerceExpression>(); coerce != nullptr) {
            return visit(*coerce);
        }
        if (auto* clear = node.as<HIRDropFlagClearExpression>(); clear != nullptr) {
            return visit(*clear);
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
        if (auto* enum_variant = node.as<HIREnumVariantExpression>(); enum_variant != nullptr) {
            return visit(*enum_variant);
        }
        if (auto* if_expression = node.as<HIRIfExpression>(); if_expression != nullptr) {
            return visit(*if_expression);
        }
        if (auto* when_expression = node.as<HIRWhenExpression>(); when_expression != nullptr) {
            return visit(*when_expression);
        }
        if (auto* block = node.as<HIRBlockExpression>(); block != nullptr) {
            return visit(*block);
        }

        if (auto* block = node.as<HIRBlockStatement>(); block != nullptr) {
            return visit(*block);
        }
        if (auto* group = node.as<HIRStatementGroup>(); group != nullptr) {
            return visit(*group);
        }
        if (auto* expression_statement = node.as<HIRExpressionStatement>();
            expression_statement != nullptr) {
            return visit(*expression_statement);
        }
        if (auto* return_statement = node.as<HIRReturnStatement>(); return_statement != nullptr) {
            return visit(*return_statement);
        }
        if (auto* loop_statement = node.as<HIRLoopStatement>(); loop_statement != nullptr) {
            return visit(*loop_statement);
        }
        if (auto* var_declaration = node.as<HIRVarDeclaration>(); var_declaration != nullptr) {
            return visit(*var_declaration);
        }
        if (auto* function_declaration = node.as<HIRFunctionDeclaration>();
            function_declaration != nullptr) {
            return visit(*function_declaration);
        }
        if (auto* array_literal = node.as<HIRArrayLiteralExpression>(); array_literal != nullptr) {
            return visit(*array_literal);
        }

        return T();
    }
};
