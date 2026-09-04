module;

#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.test.harness;

import zep.common.context;
import zep.common.source;
import zep.common.target;
import zep.common.source.span;
import zep.frontend.lexer;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.parser;
import zep.frontend.sema.context;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.checker;
import zep.frontend.token;

export class FrontendHarness {
  public:
    Source source;
    Context context;
    SemaContext sema;
    Program program;

    explicit FrontendHarness(std::string content)
        : source("test.zep", std::move(content)), context(source), sema(TargetInfo()),
          program(parse_program()) {}

    Program parse_program() {
        Parser parser(context, sema, Lexer(context.source.content));
        return parser.parse();
    }

    bool parse_succeeded() const { return !context.diagnostics.has_errors(); }

    void type_check() {
        TypeChecker checker(context, sema);
        checker.check(program);
    }

    bool type_check_succeeded() {
        type_check();
        return !context.diagnostics.has_errors();
    }

    VarDeclaration* variable(const std::string& name, const std::string& function = "") const {
        const auto* statements = &program.statements;

        if (!function.empty()) {
            for (auto* statement : program.statements) {
                auto* declaration = statement->as<FunctionDeclaration>();
                if (declaration != nullptr && declaration->prototype->name == function) {
                    statements = &declaration->body->statements;
                    break;
                }
            }
        }

        for (auto* statement : *statements) {
            auto* declaration = statement->as<VarDeclaration>();
            if (declaration != nullptr && declaration->name == name) {
                return declaration;
            }
        }

        return nullptr;
    }

    Expression* initializer(const std::string& name, const std::string& function = "") const {
        auto* declaration = variable(name, function);
        return declaration != nullptr ? declaration->initializer : nullptr;
    }
};

export testing::AssertionResult assert_diagnostic(const FrontendHarness& harness,
                                                  const std::string& message, Span span) {
    for (const auto& diagnostic : harness.context.diagnostics.entries) {
        if (diagnostic.message == message &&
            diagnostic.location.span.start.line == span.start.line &&
            diagnostic.location.span.start.column == span.start.column &&
            diagnostic.location.span.end.line == span.end.line &&
            diagnostic.location.span.end.column == span.end.column) {
            return testing::AssertionSuccess();
        }
    }

    return testing::AssertionFailure()
           << "missing diagnostic at " << harness.source.name << ":" << span.start.line << ":"
           << span.start.column << ": " << message;
}

export std::vector<Token> lex_all(const std::string& content) {
    std::vector<Token> tokens;
    tokens.reserve(content.size() + 1);
    Lexer lexer(content);

    while (true) {
        Token token = lexer.next_token();
        tokens.push_back(token);
        if (token.type == Token::Type::Eof) {
            break;
        }
    }

    return tokens;
}

export testing::AssertionResult assert_parse_ok(const std::string& content) {
    FrontendHarness harness(content);

    if (harness.parse_succeeded()) {
        return testing::AssertionSuccess();
    }

    return testing::AssertionFailure() << "parse failed for:\n" << content;
}

export testing::AssertionResult assert_type_check_ok(const std::string& content) {
    FrontendHarness harness(content);
    if (!harness.parse_succeeded()) {
        return testing::AssertionFailure() << "parse failed for:\n" << content;
    }

    if (harness.type_check_succeeded()) {
        return testing::AssertionSuccess();
    }

    harness.context.diagnostics.print();
    return testing::AssertionFailure() << "type check failed for:\n" << content;
}

export testing::AssertionResult assert_type_check_error(const std::string& content) {
    FrontendHarness harness(content);
    if (!harness.parse_succeeded()) {
        return testing::AssertionFailure() << "parse failed for:\n" << content;
    }

    harness.type_check();
    if (harness.context.diagnostics.has_errors()) {
        return testing::AssertionSuccess();
    }

    return testing::AssertionFailure() << "expected type check error but succeeded for:\n"
                                       << content;
}

export class FrontendChecks {
  public:
    static bool parse_ok(const std::string& content) {
        FrontendHarness harness(content);

        return harness.parse_succeeded();
    }

    static bool type_check_ok(const std::string& content) {
        FrontendHarness harness(content);
        if (!harness.parse_succeeded()) {
            return false;
        }

        return harness.type_check_succeeded();
    }

    static bool type_check_error(const std::string& content) {
        FrontendHarness harness(content);
        if (!harness.parse_succeeded()) {
            return false;
        }

        harness.type_check();
        return harness.context.diagnostics.has_errors();
    }
};
