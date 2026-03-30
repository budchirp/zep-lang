module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.hir.ir;

import zep.common.position;
import zep.common.logger;
import zep.frontend.sema.kinds;
export import zep.hir.sema.type;
export import zep.hir.sema.scope.symbol;
export import zep.hir.sema.scope;

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
    explicit HIRNode(HIRNodeKind::Type kind, Position position) : kind(kind), position(position) {}

    HIRNode(const HIRNode&) = delete;
    HIRNode& operator=(const HIRNode&) = delete;
    HIRNode(HIRNode&&) = default;
    HIRNode& operator=(HIRNode&&) = default;

  public:
    HIRNodeKind::Type kind;
    Position position;

    virtual ~HIRNode() = default;

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

export class HIRExpression : public HIRNode {
  private:
    std::shared_ptr<HIRType> type;

  public:
    using HIRNode::HIRNode;

    void set_type(std::shared_ptr<HIRType> type) { this->type = std::move(type); }
    std::shared_ptr<HIRType> get_type() const { return type; }
};

export class HIRStatement : public HIRNode {
  private:
    std::shared_ptr<HIRType> type;

  public:
    using HIRNode::HIRNode;

    void set_type(std::shared_ptr<HIRType> type) { this->type = std::move(type); }
    std::shared_ptr<HIRType> get_type() const { return type; }
};

export class HIRBlockStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BlockStatement;

    std::vector<std::unique_ptr<HIRStatement>> statements;

    explicit HIRBlockStatement(Position position,
                               std::vector<std::unique_ptr<HIRStatement>> statements)
        : HIRStatement(static_kind, position), statements(std::move(statements)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRBlockStatement(\n");
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

export class HIRNumberLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::NumberLiteral;

    std::string value;

    HIRNumberLiteral(Position position, std::string value)
        : HIRExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRNumberLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class HIRFloatLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::FloatLiteral;

    std::string value;

    HIRFloatLiteral(Position position, std::string value)
        : HIRExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRFloatLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class HIRStringLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::StringLiteral;

    std::string value;

    HIRStringLiteral(Position position, std::string value)
        : HIRExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRStringLiteral(value: \"", value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class HIRBooleanLiteral : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BooleanLiteral;

    bool value;

    HIRBooleanLiteral(Position position, bool value)
        : HIRExpression(static_kind, position), value(value) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRBooleanLiteral(value: ", (value ? "true" : "false"), ")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class HIRIdentifierExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IdentifierExpression;

    std::string name;

    HIRIdentifierExpression(Position position, std::string name)
        : HIRExpression(static_kind, position), name(std::move(name)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRIdentifierExpression(name: \"", name, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};

export class HIRBinaryOperator {
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

export class HIRBinaryExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::BinaryExpression;

    std::unique_ptr<HIRExpression> left;
    HIRBinaryOperator::Type op;
    std::unique_ptr<HIRExpression> right;

    HIRBinaryExpression(Position position, std::unique_ptr<HIRExpression> left,
                        HIRBinaryOperator::Type op, std::unique_ptr<HIRExpression> right)
        : HIRExpression(static_kind, position), left(std::move(left)), op(op),
          right(std::move(right)) {}

  private:
    static std::string operator_string(HIRBinaryOperator::Type binary_op) {
        switch (binary_op) {
        case HIRBinaryOperator::Type::Plus:
            return "+";
        case HIRBinaryOperator::Type::Minus:
            return "-";
        case HIRBinaryOperator::Type::Asterisk:
            return "*";
        case HIRBinaryOperator::Type::Divide:
            return "/";
        case HIRBinaryOperator::Type::Modulo:
            return "%";
        case HIRBinaryOperator::Type::Equals:
            return "==";
        case HIRBinaryOperator::Type::NotEquals:
            return "!=";
        case HIRBinaryOperator::Type::LessThan:
            return "<";
        case HIRBinaryOperator::Type::GreaterThan:
            return ">";
        case HIRBinaryOperator::Type::LessEqual:
            return "<=";
        case HIRBinaryOperator::Type::GreaterEqual:
            return ">=";
        case HIRBinaryOperator::Type::And:
            return "&&";
        case HIRBinaryOperator::Type::Or:
            return "||";
        case HIRBinaryOperator::Type::As:
            return "as";
        case HIRBinaryOperator::Type::Is:
            return "is";
        }
        return "?";
    }

  public:
    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRBinaryExpression(\n");
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

export class HIRUnaryOperator {
  public:
    enum class Type : std::uint8_t {
        Plus,
        Minus,
        Not,
        Dereference,
        AddressOf,
    };
};

export class HIRUnaryExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::UnaryExpression;

    HIRUnaryOperator::Type op;
    std::unique_ptr<HIRExpression> operand;

    HIRUnaryExpression(Position position, HIRUnaryOperator::Type op,
                       std::unique_ptr<HIRExpression> operand)
        : HIRExpression(static_kind, position), op(op), operand(std::move(operand)) {}

  private:
    static std::string operator_string(HIRUnaryOperator::Type unary_op) {
        switch (unary_op) {
        case HIRUnaryOperator::Type::Plus:
            return "+";
        case HIRUnaryOperator::Type::Minus:
            return "-";
        case HIRUnaryOperator::Type::Not:
            return "!";
        case HIRUnaryOperator::Type::Dereference:
            return "*";
        case HIRUnaryOperator::Type::AddressOf:
            return "&";
        }
        return "?";
    }

  public:
    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRUnaryExpression(\n");
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

export class HIRCallExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::CallExpression;

    std::unique_ptr<HIRExpression> callee;
    std::vector<std::unique_ptr<HIRExpression>> arguments;

    HIRCallExpression(Position position, std::unique_ptr<HIRExpression> callee,
                      std::vector<std::unique_ptr<HIRExpression>> arguments)
        : HIRExpression(static_kind, position), callee(std::move(callee)),
          arguments(std::move(arguments)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRCallExpression(\n");
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

export class HIRIndexExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IndexExpression;

    std::unique_ptr<HIRExpression> object;
    std::unique_ptr<HIRExpression> index;

    HIRIndexExpression(Position position, std::unique_ptr<HIRExpression> object,
                       std::unique_ptr<HIRExpression> index)
        : HIRExpression(static_kind, position), object(std::move(object)), index(std::move(index)) {
    }

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRIndexExpression(\n");
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

export class HIRMemberExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::MemberExpression;

    std::unique_ptr<HIRExpression> object;
    std::string member;

    HIRMemberExpression(Position position, std::unique_ptr<HIRExpression> object,
                        std::string member)
        : HIRExpression(static_kind, position), object(std::move(object)),
          member(std::move(member)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRMemberExpression(\n");
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

export class HIRAssignExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::AssignExpression;

    std::unique_ptr<HIRExpression> target;
    std::unique_ptr<HIRExpression> value;

    HIRAssignExpression(Position position, std::unique_ptr<HIRExpression> target,
                        std::unique_ptr<HIRExpression> value)
        : HIRExpression(static_kind, position), target(std::move(target)), value(std::move(value)) {
    }

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRAssignExpression(\n");
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

    HIRStructLiteralExpression(Position position, std::string name,
                               std::vector<HIRStructLiteralField> fields)
        : HIRExpression(static_kind, position), name(std::move(name)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRStructLiteralExpression(\n");
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
                Logger::print("HIRStructLiteralField(\n");
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

export class HIRIfExpression : public HIRExpression {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::IfExpression;

    std::unique_ptr<HIRExpression> condition;
    std::unique_ptr<HIRStatement> then_branch;
    std::unique_ptr<HIRStatement> else_branch;

    HIRIfExpression(Position position, std::unique_ptr<HIRExpression> condition,
                    std::unique_ptr<HIRStatement> then_branch,
                    std::unique_ptr<HIRStatement> else_branch)
        : HIRExpression(static_kind, position), condition(std::move(condition)),
          then_branch(std::move(then_branch)), else_branch(std::move(else_branch)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRIfExpression(\n");
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

export class HIRExpressionStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::ExpressionStatement;

    std::unique_ptr<HIRExpression> expression;

    explicit HIRExpressionStatement(Position position, std::unique_ptr<HIRExpression> expression)
        : HIRStatement(static_kind, position), expression(std::move(expression)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRExpressionStatement(\n");
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

export class HIRReturnStatement : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::ReturnStatement;

    std::unique_ptr<HIRExpression> value;

    HIRReturnStatement(Position position, std::unique_ptr<HIRExpression> value)
        : HIRStatement(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRReturnStatement(\n");
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

export class HIRVarDeclaration : public HIRStatement {
  public:
    static constexpr HIRNodeKind::Type static_kind = HIRNodeKind::Type::VarDeclaration;

    Visibility::Type visibility;
    StorageKind::Type storage_kind;
    std::string name;
    std::shared_ptr<HIRType> type;
    std::unique_ptr<HIRExpression> initializer;

    HIRVarDeclaration(Position position, Visibility::Type visibility,
                      StorageKind::Type storage_kind, std::string name,
                      std::shared_ptr<HIRType> type, std::unique_ptr<HIRExpression> initializer)
        : HIRStatement(static_kind, position), visibility(visibility), storage_kind(storage_kind),
          name(std::move(name)), type(std::move(type)), initializer(std::move(initializer)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("HIRVarDeclaration(\n");
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

export class HIRFunctionDeclaration {
  private:
    Position position;

  public:
    Visibility::Type visibility;
    std::string name;
    std::vector<HIRParameter> parameters;
    std::shared_ptr<HIRType> return_type;
    std::unique_ptr<HIRBlockStatement> body;
    bool variadic;

    HIRFunctionDeclaration(Position position, Visibility::Type visibility, std::string name,
                           std::vector<HIRParameter> parameters,
                           std::shared_ptr<HIRType> return_type,
                           std::unique_ptr<HIRBlockStatement> body, bool variadic)
        : position(position), visibility(visibility), name(std::move(name)),
          parameters(std::move(parameters)), return_type(std::move(return_type)),
          body(std::move(body)), variadic(variadic) {}

    void dump(int depth = 0) const {
        Logger::print_indent(depth);
        Logger::print("HIRFunctionDeclaration(\n");
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

export class HIRProgram {
  public:
    std::unique_ptr<HIRScope> global_scope;
    std::vector<std::unique_ptr<HIRFunctionDeclaration>> functions;

    HIRProgram(std::unique_ptr<HIRScope> global_scope,
               std::vector<std::unique_ptr<HIRFunctionDeclaration>> functions)
        : global_scope(std::move(global_scope)), functions(std::move(functions)) {}

    void dump() const {
        Logger::print("HIRProgram(\n");

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
