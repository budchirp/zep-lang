module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.lowerer.ast.node;

import zep.common.position;
import zep.common.logger;
import zep.sema.kinds;
export import zep.lowerer.sema.type;
export import zep.lowerer.sema.scope.symbol;
export import zep.lowerer.sema.scope;

export class LoweredNodeKind {
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

export class LoweredNode {
  private:
  protected:
    explicit LoweredNode(LoweredNodeKind::Type kind, Position position)
        : kind(kind), position(position) {}

    LoweredNode(const LoweredNode&) = delete;
    LoweredNode& operator=(const LoweredNode&) = delete;
    LoweredNode(LoweredNode&&) = default;
    LoweredNode& operator=(LoweredNode&&) = default;

  public:
    LoweredNodeKind::Type kind;
    Position position;

    virtual ~LoweredNode() = default;

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

export class LoweredExpression : public LoweredNode {
  private:
    std::shared_ptr<LoweredType> type;

  public:
    using LoweredNode::LoweredNode;

    void set_type(std::shared_ptr<LoweredType> type) { this->type = std::move(type); }
    std::shared_ptr<LoweredType> get_type() const { return type; }
};

export class LoweredStatement : public LoweredNode {
  private:
    std::shared_ptr<LoweredType> type;

  public:
    using LoweredNode::LoweredNode;

    void set_type(std::shared_ptr<LoweredType> type) { this->type = std::move(type); }
    std::shared_ptr<LoweredType> get_type() const { return type; }
};

export class LoweredBlockStatement : public LoweredStatement {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::BlockStatement;

    std::vector<std::unique_ptr<LoweredStatement>> statements;

    explicit LoweredBlockStatement(Position position,
                                   std::vector<std::unique_ptr<LoweredStatement>> statements)
        : LoweredStatement(static_kind, position), statements(std::move(statements)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredBlockStatement(\n");
        Logger::print_indent(depth + 1);
        Logger::print("statements: [");

        if (statements.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (const auto& stmt : statements) {
                stmt->dump(depth + 2);
                Logger::print(",\n");
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
};

export class LoweredNumberLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::NumberLiteral;

    std::string value;

    LoweredNumberLiteral(Position position, std::string value)
        : LoweredExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredNumberLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredFloatLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::FloatLiteral;

    std::string value;

    LoweredFloatLiteral(Position position, std::string value)
        : LoweredExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredFloatLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredStringLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::StringLiteral;

    std::string value;

    LoweredStringLiteral(Position position, std::string value)
        : LoweredExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredStringLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredBooleanLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::BooleanLiteral;

    bool value;

    LoweredBooleanLiteral(Position position, bool value)
        : LoweredExpression(static_kind, position), value(value) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredBooleanLiteral(value: ", (value ? "true" : "false"), ")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredIdentifierExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind =
        LoweredNodeKind::Type::IdentifierExpression;

    std::string name;

    LoweredIdentifierExpression(Position position, std::string name)
        : LoweredExpression(static_kind, position), name(std::move(name)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredIdentifierExpression(name: \"", name, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredBinaryOperator {
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
};

export class LoweredBinaryExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::BinaryExpression;

    std::unique_ptr<LoweredExpression> left;
    LoweredBinaryOperator::Type op;
    std::unique_ptr<LoweredExpression> right;

    LoweredBinaryExpression(Position position, std::unique_ptr<LoweredExpression> left,
                            LoweredBinaryOperator::Type op,
                            std::unique_ptr<LoweredExpression> right)
        : LoweredExpression(static_kind, position), left(std::move(left)), op(op),
          right(std::move(right)) {}

  private:
    static std::string operator_string(LoweredBinaryOperator::Type binary_op) {
        switch (binary_op) {
        case LoweredBinaryOperator::Type::Plus:
            return "+";
        case LoweredBinaryOperator::Type::Minus:
            return "-";
        case LoweredBinaryOperator::Type::Asterisk:
            return "*";
        case LoweredBinaryOperator::Type::Divide:
            return "/";
        case LoweredBinaryOperator::Type::Modulo:
            return "%";
        case LoweredBinaryOperator::Type::Equals:
            return "==";
        case LoweredBinaryOperator::Type::NotEquals:
            return "!=";
        case LoweredBinaryOperator::Type::LessThan:
            return "<";
        case LoweredBinaryOperator::Type::GreaterThan:
            return ">";
        case LoweredBinaryOperator::Type::LessEqual:
            return "<=";
        case LoweredBinaryOperator::Type::GreaterEqual:
            return ">=";
        case LoweredBinaryOperator::Type::And:
            return "&&";
        case LoweredBinaryOperator::Type::Or:
            return "||";
        case LoweredBinaryOperator::Type::As:
            return "as";
        case LoweredBinaryOperator::Type::Is:
            return "is";
        }
        return "?";
    }

  public:
    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredBinaryExpression(\n");
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
};

export class LoweredUnaryOperator {
  public:
    enum class Type : std::uint8_t {
        Plus,
        Minus,
        Not,
        Dereference,
        AddressOf,
    };
};

export class LoweredUnaryExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::UnaryExpression;

    LoweredUnaryOperator::Type op;
    std::unique_ptr<LoweredExpression> operand;

    LoweredUnaryExpression(Position position, LoweredUnaryOperator::Type op,
                           std::unique_ptr<LoweredExpression> operand)
        : LoweredExpression(static_kind, position), op(op), operand(std::move(operand)) {}

  private:
    static std::string operator_string(LoweredUnaryOperator::Type unary_op) {
        switch (unary_op) {
        case LoweredUnaryOperator::Type::Plus:
            return "+";
        case LoweredUnaryOperator::Type::Minus:
            return "-";
        case LoweredUnaryOperator::Type::Not:
            return "!";
        case LoweredUnaryOperator::Type::Dereference:
            return "*";
        case LoweredUnaryOperator::Type::AddressOf:
            return "&";
        }
        return "?";
    }

  public:
    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredUnaryExpression(\n");
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
};

export class LoweredCallExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::CallExpression;

    std::unique_ptr<LoweredExpression> callee;
    std::vector<std::unique_ptr<LoweredExpression>> arguments;

    LoweredCallExpression(Position position, std::unique_ptr<LoweredExpression> callee,
                          std::vector<std::unique_ptr<LoweredExpression>> arguments)
        : LoweredExpression(static_kind, position), callee(std::move(callee)),
          arguments(std::move(arguments)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredCallExpression(\n");
        Logger::print_indent(depth + 1);
        Logger::print("callee: ");
        callee->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("arguments: [");

        if (arguments.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (const auto& argument : arguments) {
                argument->dump(depth + 2);
                Logger::print(",\n");
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
};

export class LoweredIndexExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::IndexExpression;

    std::unique_ptr<LoweredExpression> object;
    std::unique_ptr<LoweredExpression> index;

    LoweredIndexExpression(Position position, std::unique_ptr<LoweredExpression> object,
                           std::unique_ptr<LoweredExpression> index)
        : LoweredExpression(static_kind, position), object(std::move(object)),
          index(std::move(index)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredIndexExpression(\n");
        Logger::print_indent(depth + 1);
        Logger::print("object: ");
        object->dump(depth + 1, false, false);
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
};

export class LoweredMemberExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::MemberExpression;

    std::unique_ptr<LoweredExpression> object;
    std::string member;

    LoweredMemberExpression(Position position, std::unique_ptr<LoweredExpression> object,
                            std::string member)
        : LoweredExpression(static_kind, position), object(std::move(object)),
          member(std::move(member)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredMemberExpression(\n");
        Logger::print_indent(depth + 1);
        Logger::print("object: ");
        object->dump(depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("member: \"", member, "\"\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredAssignExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::AssignExpression;

    std::unique_ptr<LoweredExpression> target;
    std::unique_ptr<LoweredExpression> value;

    LoweredAssignExpression(Position position, std::unique_ptr<LoweredExpression> target,
                            std::unique_ptr<LoweredExpression> value)
        : LoweredExpression(static_kind, position), target(std::move(target)),
          value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredAssignExpression(\n");
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
};

export class LoweredStructLiteralField {
  public:
    std::string name;
    std::unique_ptr<LoweredExpression> value;

    LoweredStructLiteralField(std::string name, std::unique_ptr<LoweredExpression> value)
        : name(std::move(name)), value(std::move(value)) {}
};

export class LoweredStructLiteralExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind =
        LoweredNodeKind::Type::StructLiteralExpression;

    std::string name;
    std::vector<LoweredStructLiteralField> fields;

    LoweredStructLiteralExpression(Position position, std::string name,
                                   std::vector<LoweredStructLiteralField> fields)
        : LoweredExpression(static_kind, position), name(std::move(name)),
          fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredStructLiteralExpression(\n");
        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("fields: [");

        if (fields.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (const auto& field : fields) {
                Logger::print_indent(depth + 2);
                Logger::print("LoweredStructLiteralField(\n");
                Logger::print_indent(depth + 3);
                Logger::print("name: \"", field.name, "\",\n");
                Logger::print_indent(depth + 3);
                Logger::print("value: ");
                field.value->dump(depth + 3, false, false);
                Logger::print("\n");
                Logger::print_indent(depth + 2);
                Logger::print("),\n");
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
};

export class LoweredIfExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::IfExpression;

    std::unique_ptr<LoweredExpression> condition;
    std::unique_ptr<LoweredStatement> then_branch;
    std::unique_ptr<LoweredStatement> else_branch;

    LoweredIfExpression(Position position, std::unique_ptr<LoweredExpression> condition,
                        std::unique_ptr<LoweredStatement> then_branch,
                        std::unique_ptr<LoweredStatement> else_branch)
        : LoweredExpression(static_kind, position), condition(std::move(condition)),
          then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredIfExpression(\n");
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
        if (else_branch != nullptr) {
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
};

export class LoweredExpressionStatement : public LoweredStatement {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::ExpressionStatement;

    std::unique_ptr<LoweredExpression> expression;

    explicit LoweredExpressionStatement(Position position,
                                        std::unique_ptr<LoweredExpression> expression)
        : LoweredStatement(static_kind, position), expression(std::move(expression)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredExpressionStatement(\n");
        Logger::print_indent(depth + 1);
        Logger::print("expression: ");
        expression->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class LoweredReturnStatement : public LoweredStatement {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::ReturnStatement;

    std::unique_ptr<LoweredExpression> value;

    LoweredReturnStatement(Position position, std::unique_ptr<LoweredExpression> value)
        : LoweredStatement(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredReturnStatement(\n");
        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        if (value != nullptr) {
            value->dump(depth + 1, false, false);
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
};

export class LoweredVarDeclaration : public LoweredStatement {
  public:
    static constexpr LoweredNodeKind::Type static_kind = LoweredNodeKind::Type::VarDeclaration;

    Visibility::Type visibility;
    StorageKind::Type storage_kind;
    std::string name;
    std::shared_ptr<LoweredType> type;
    std::unique_ptr<LoweredExpression> initializer;

    LoweredVarDeclaration(Position position, Visibility::Type visibility,
                          StorageKind::Type storage_kind, std::string name,
                          std::shared_ptr<LoweredType> type,
                          std::unique_ptr<LoweredExpression> initializer)
        : LoweredStatement(static_kind, position), visibility(visibility),
          storage_kind(storage_kind), name(std::move(name)), type(std::move(type)),
          initializer(std::move(initializer)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("LoweredVarDeclaration(\n");
        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("storage_kind: ", StorageKind::to_string(storage_kind), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ", (type != nullptr ? type->to_string() : "null"), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("initializer: ");
        if (initializer != nullptr) {
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
};

export class LoweredFunctionDeclaration {
  private:
    Position position;

  public:
    Visibility::Type visibility;
    std::string name;
    std::vector<LoweredParameter> parameters;
    std::shared_ptr<LoweredType> return_type;
    std::unique_ptr<LoweredBlockStatement> body;
    bool variadic;

    LoweredFunctionDeclaration(Position position, Visibility::Type visibility, std::string name,
                               std::vector<LoweredParameter> parameters,
                               std::shared_ptr<LoweredType> return_type,
                               std::unique_ptr<LoweredBlockStatement> body, bool variadic)
        : position(position), visibility(visibility), name(std::move(name)),
          parameters(std::move(parameters)), return_type(std::move(return_type)),
          body(std::move(body)), variadic(variadic) {}

    void dump(int depth = 0) const {
        Logger::print_indent(depth);
        Logger::print("LoweredFunctionDeclaration(\n");
        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("parameters: [");
        if (parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (const auto& parameter : parameters) {
                parameter.dump(depth + 2);
                Logger::print(",\n");
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("return_type: ", (return_type != nullptr ? return_type->to_string() : "null"),
                      ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("variadic: ", (variadic ? "true" : "false"), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        body->dump(depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }
};

export class LoweredProgram {
  public:
    std::unique_ptr<LoweredScope> global_scope;
    std::vector<std::unique_ptr<LoweredFunctionDeclaration>> functions;

    LoweredProgram(std::unique_ptr<LoweredScope> global_scope,
                   std::vector<std::unique_ptr<LoweredFunctionDeclaration>> functions)
        : global_scope(std::move(global_scope)), functions(std::move(functions)) {}

    void dump() const {
        Logger::print("LoweredProgram(\n");

        Logger::print_indent(1);
        Logger::print("global_scope: ");
        if (global_scope != nullptr) {
            global_scope->dump(1, false, true);
        } else {
            Logger::print("null\n");
        }

        Logger::print_indent(1);
        Logger::print("functions: [");
        if (functions.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (const auto& function : functions) {
                function->dump(2);
                Logger::print(",\n");
            }
            Logger::print_indent(1);
            Logger::print("]\n");
        }

        Logger::print(")\n");
    }
};
