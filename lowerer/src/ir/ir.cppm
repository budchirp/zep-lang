module;

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

export module zep.lowerer.ir;

import zep.common.position;
import zep.common.logger;
import zep.sema.kinds;
export import zep.lowerer.sema.types;
export import zep.lowerer.sema.scope.symbol;
export import zep.lowerer.sema.scope;

export enum class LoweredNodeKind : std::uint8_t {
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

export class LoweredNode {
  private:
  protected:
    explicit LoweredNode(LoweredNodeKind kind, Position position)
        : kind(kind), position(position) {}

    LoweredNode(const LoweredNode&) = delete;
    LoweredNode& operator=(const LoweredNode&) = delete;
    LoweredNode(LoweredNode&&) = default;
    LoweredNode& operator=(LoweredNode&&) = default;

  public:
    LoweredNodeKind kind;
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
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::BlockStatement;

    std::vector<std::unique_ptr<LoweredStatement>> statements;

    explicit LoweredBlockStatement(Position position,
                                   std::vector<std::unique_ptr<LoweredStatement>> statements)
        : LoweredStatement(static_kind, position), statements(std::move(statements)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredBlockStatement(\n";
        print_indent(depth + 1);
        std::cout << "statements: [";

        if (statements.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (const auto& stmt : statements) {
                stmt->dump(depth + 2);
                std::cout << ",\n";
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredNumberLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::NumberLiteral;

    std::string value;

    LoweredNumberLiteral(Position position, std::string value)
        : LoweredExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredNumberLiteral(value: \"" << value << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredFloatLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::FloatLiteral;

    std::string value;

    LoweredFloatLiteral(Position position, std::string value)
        : LoweredExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredFloatLiteral(value: \"" << value << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredStringLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::StringLiteral;

    std::string value;

    LoweredStringLiteral(Position position, std::string value)
        : LoweredExpression(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredStringLiteral(value: \"" << value << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredBooleanLiteral : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::BooleanLiteral;

    bool value;

    LoweredBooleanLiteral(Position position, bool value)
        : LoweredExpression(static_kind, position), value(value) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredBooleanLiteral(value: " << (value ? "true" : "false") << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredIdentifierExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::IdentifierExpression;

    std::string name;

    LoweredIdentifierExpression(Position position, std::string name)
        : LoweredExpression(static_kind, position), name(std::move(name)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredIdentifierExpression(name: \"" << name << "\")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredBinaryExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::BinaryExpression;

    enum class BinaryOperator : std::uint8_t {
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

    std::unique_ptr<LoweredExpression> left;
    BinaryOperator op;
    std::unique_ptr<LoweredExpression> right;

    LoweredBinaryExpression(Position position, std::unique_ptr<LoweredExpression> left,
                            BinaryOperator op, std::unique_ptr<LoweredExpression> right)
        : LoweredExpression(static_kind, position), left(std::move(left)), op(op),
          right(std::move(right)) {}

  private:
    static std::string operator_string(BinaryOperator binary_op) {
        switch (binary_op) {
        case BinaryOperator::Plus:
            return "+";
        case BinaryOperator::Minus:
            return "-";
        case BinaryOperator::Asterisk:
            return "*";
        case BinaryOperator::Divide:
            return "/";
        case BinaryOperator::Modulo:
            return "%";
        case BinaryOperator::Equals:
            return "==";
        case BinaryOperator::NotEquals:
            return "!=";
        case BinaryOperator::LessThan:
            return "<";
        case BinaryOperator::GreaterThan:
            return ">";
        case BinaryOperator::LessEqual:
            return "<=";
        case BinaryOperator::GreaterEqual:
            return ">=";
        case BinaryOperator::And:
            return "&&";
        case BinaryOperator::Or:
            return "||";
        case BinaryOperator::As:
            return "as";
        case BinaryOperator::Is:
            return "is";
        }
        return "?";
    }

  public:
    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredBinaryExpression(\n";
        print_indent(depth + 1);
        std::cout << "left: ";
        left->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "op: " << operator_string(op) << ",\n";

        print_indent(depth + 1);
        std::cout << "right: ";
        right->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredUnaryExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::UnaryExpression;

    enum class UnaryOperator : std::uint8_t {
        Plus,
        Minus,
        Not,
        Dereference,
        AddressOf,
    };

    UnaryOperator op;
    std::unique_ptr<LoweredExpression> operand;

    LoweredUnaryExpression(Position position, UnaryOperator op,
                           std::unique_ptr<LoweredExpression> operand)
        : LoweredExpression(static_kind, position), op(op), operand(std::move(operand)) {}

  private:
    static std::string operator_string(UnaryOperator unary_op) {
        switch (unary_op) {
        case UnaryOperator::Plus:
            return "+";
        case UnaryOperator::Minus:
            return "-";
        case UnaryOperator::Not:
            return "!";
        case UnaryOperator::Dereference:
            return "*";
        case UnaryOperator::AddressOf:
            return "&";
        }
        return "?";
    }

  public:
    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredUnaryExpression(\n";
        print_indent(depth + 1);
        std::cout << "op: " << operator_string(op) << ",\n";

        print_indent(depth + 1);
        std::cout << "operand: ";
        operand->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredCallExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::CallExpression;

    std::unique_ptr<LoweredExpression> callee;
    std::vector<std::unique_ptr<LoweredExpression>> arguments;

    LoweredCallExpression(Position position, std::unique_ptr<LoweredExpression> callee,
                          std::vector<std::unique_ptr<LoweredExpression>> arguments)
        : LoweredExpression(static_kind, position), callee(std::move(callee)),
          arguments(std::move(arguments)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredCallExpression(\n";
        print_indent(depth + 1);
        std::cout << "callee: ";
        callee->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "arguments: [";

        if (arguments.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (const auto& argument : arguments) {
                argument->dump(depth + 2);
                std::cout << ",\n";
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredIndexExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::IndexExpression;

    std::unique_ptr<LoweredExpression> object;
    std::unique_ptr<LoweredExpression> index;

    LoweredIndexExpression(Position position, std::unique_ptr<LoweredExpression> object,
                           std::unique_ptr<LoweredExpression> index)
        : LoweredExpression(static_kind, position), object(std::move(object)),
          index(std::move(index)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredIndexExpression(\n";
        print_indent(depth + 1);
        std::cout << "object: ";
        object->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "index: ";
        index->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredMemberExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::MemberExpression;

    std::unique_ptr<LoweredExpression> object;
    std::string member;

    LoweredMemberExpression(Position position, std::unique_ptr<LoweredExpression> object,
                            std::string member)
        : LoweredExpression(static_kind, position), object(std::move(object)),
          member(std::move(member)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredMemberExpression(\n";
        print_indent(depth + 1);
        std::cout << "object: ";
        object->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "member: \"" << member << "\"\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredAssignExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::AssignExpression;

    std::unique_ptr<LoweredExpression> target;
    std::unique_ptr<LoweredExpression> value;

    LoweredAssignExpression(Position position, std::unique_ptr<LoweredExpression> target,
                            std::unique_ptr<LoweredExpression> value)
        : LoweredExpression(static_kind, position), target(std::move(target)),
          value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredAssignExpression(\n";
        print_indent(depth + 1);
        std::cout << "target: ";
        target->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "value: ";
        value->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
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
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::StructLiteralExpression;

    std::string name;
    std::vector<LoweredStructLiteralField> fields;

    LoweredStructLiteralExpression(Position position, std::string name,
                                   std::vector<LoweredStructLiteralField> fields)
        : LoweredExpression(static_kind, position), name(std::move(name)),
          fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredStructLiteralExpression(\n";
        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "fields: [";

        if (fields.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (const auto& field : fields) {
                print_indent(depth + 2);
                std::cout << "LoweredStructLiteralField(\n";
                print_indent(depth + 3);
                std::cout << "name: \"" << field.name << "\",\n";
                print_indent(depth + 3);
                std::cout << "value: ";
                field.value->dump(depth + 3, false, false);
                std::cout << "\n";
                print_indent(depth + 2);
                std::cout << "),\n";
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredIfExpression : public LoweredExpression {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::IfExpression;

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
            print_indent(depth);
        }

        std::cout << "LoweredIfExpression(\n";
        print_indent(depth + 1);
        std::cout << "condition: ";
        condition->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "then_branch: ";
        then_branch->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "else_branch: ";
        if (else_branch != nullptr) {
            else_branch->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredExpressionStatement : public LoweredStatement {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::ExpressionStatement;

    std::unique_ptr<LoweredExpression> expression;

    explicit LoweredExpressionStatement(Position position,
                                        std::unique_ptr<LoweredExpression> expression)
        : LoweredStatement(static_kind, position), expression(std::move(expression)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredExpressionStatement(\n";
        print_indent(depth + 1);
        std::cout << "expression: ";
        expression->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredReturnStatement : public LoweredStatement {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::ReturnStatement;

    std::unique_ptr<LoweredExpression> value;

    LoweredReturnStatement(Position position, std::unique_ptr<LoweredExpression> value)
        : LoweredStatement(static_kind, position), value(std::move(value)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredReturnStatement(\n";
        print_indent(depth + 1);
        std::cout << "value: ";
        if (value != nullptr) {
            value->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredVarDeclaration : public LoweredStatement {
  public:
    static constexpr LoweredNodeKind static_kind = LoweredNodeKind::VarDeclaration;

    Visibility visibility;
    StorageKind storage_kind;
    std::string name;
    std::shared_ptr<LoweredType> type;
    std::unique_ptr<LoweredExpression> initializer;

    LoweredVarDeclaration(Position position, Visibility visibility, StorageKind storage_kind,
                          std::string name, std::shared_ptr<LoweredType> type,
                          std::unique_ptr<LoweredExpression> initializer)
        : LoweredStatement(static_kind, position), visibility(visibility),
          storage_kind(storage_kind), name(std::move(name)), type(std::move(type)),
          initializer(std::move(initializer)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "LoweredVarDeclaration(\n";
        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "storage_kind: " << storage_kind_string(storage_kind) << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: " << (type != nullptr ? type->to_string() : "null") << ",\n";

        print_indent(depth + 1);
        std::cout << "initializer: ";
        if (initializer != nullptr) {
            initializer->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};

export class LoweredFunctionDeclaration {
  private:
    Position position;

  public:
    Visibility visibility;
    std::string name;
    std::vector<LoweredParameter> parameters;
    std::shared_ptr<LoweredType> return_type;
    std::unique_ptr<LoweredBlockStatement> body;
    bool variadic;

    LoweredFunctionDeclaration(Position position, Visibility visibility, std::string name,
                               std::vector<LoweredParameter> parameters,
                               std::shared_ptr<LoweredType> return_type,
                               std::unique_ptr<LoweredBlockStatement> body, bool variadic)
        : position(position), visibility(visibility), name(std::move(name)),
          parameters(std::move(parameters)), return_type(std::move(return_type)),
          body(std::move(body)), variadic(variadic) {}

    void dump(int depth = 0) const {
        print_indent(depth);
        std::cout << "LoweredFunctionDeclaration(\n";
        print_indent(depth + 1);
        std::cout << "visibility: " << visibility_string(visibility) << ",\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "parameters: [";
        if (parameters.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (const auto& parameter : parameters) {
                parameter.dump(depth + 2);
                std::cout << ",\n";
            }
            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "return_type: " << (return_type != nullptr ? return_type->to_string() : "null")
                  << ",\n";

        print_indent(depth + 1);
        std::cout << "variadic: " << (variadic ? "true" : "false") << ",\n";

        print_indent(depth + 1);
        std::cout << "body: ";
        body->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";
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
        std::cout << "LoweredProgram(\n";

        print_indent(1);
        std::cout << "global_scope: ";
        if (global_scope != nullptr) {
            global_scope->dump(1);
        } else {
            std::cout << "null\n";
        }

        print_indent(1);
        std::cout << "functions: [";
        if (functions.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (const auto& function : functions) {
                function->dump(2);
                std::cout << ",\n";
            }
            print_indent(1);
            std::cout << "]\n";
        }

        std::cout << ")\n";
    }
};
