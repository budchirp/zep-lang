#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <vector>

import zep.frontend.debug.ast_dumper;
import zep.frontend.lexer;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.type;
import zep.frontend.test.harness;
import zep.frontend.token;
import zep.test.support;

TEST(FrontendLexer, TokenizesKeywordsOperatorsAndLiterals) {
    Lexer lexer("public fn main() -> i32 { return 1 + 2 != 4 && true }");

    std::vector<Token::Type> expected{
        Token::Type::Public,     Token::Type::Fn,         Token::Type::Identifier,
        Token::Type::LeftParen,  Token::Type::RightParen, Token::Type::Arrow,
        Token::Type::Identifier, Token::Type::LeftBrace,  Token::Type::Return,
        Token::Type::Number,     Token::Type::Plus,       Token::Type::Number,
        Token::Type::NotEquals,  Token::Type::Number,     Token::Type::And,
        Token::Type::Boolean,    Token::Type::RightBrace, Token::Type::Eof};

    for (auto type : expected) {
        Token token = lexer.next_token();
        EXPECT_EQ(token.type, type);
    }
}

TEST(FrontendLexer, HandlesStringAndCharEscapes) {
    Lexer lexer("\"a\\n\\\"\" '\\n'");

    Token string_token = lexer.next_token();
    Token char_token = lexer.next_token();

    EXPECT_EQ(string_token.type, Token::Type::String);
    EXPECT_EQ(string_token.value, "a\n\"");
    EXPECT_EQ(char_token.type, Token::Type::Char);
    EXPECT_EQ(char_token.value, "\n");
}

TEST(FrontendParser, ParsesDeclarationsAndExpressionPrecedence) {
    FrontendHarness harness("struct Point { public: var x: i32 var y: i32 }\n"
                            "fn main() -> i32 { return 1 + 2 * 3 }\n");

    ASSERT_FALSE(harness.context.diagnostics.has_errors());
    ASSERT_EQ(harness.program.statements.size(), 2U);

    auto* structure = harness.program.statements[0]->as<StructDeclaration>();
    auto* function = harness.program.statements[1]->as<FunctionDeclaration>();

    ASSERT_NE(structure, nullptr);
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(structure->name, "Point");
    EXPECT_EQ(structure->fields.size(), 2U);
    EXPECT_EQ(function->prototype->name, "main");
}

TEST(FrontendParser, ParsesClosureExpressions) {
    FrontendHarness harness("fn main() -> void { var f = { x: i32 -> x } }\n");

    ASSERT_FALSE(harness.context.diagnostics.has_errors());
    ASSERT_EQ(harness.program.statements.size(), 1U);

    auto* function = harness.program.statements[0]->as<FunctionDeclaration>();
    ASSERT_NE(function, nullptr);
    ASSERT_NE(function->body, nullptr);
    ASSERT_EQ(function->body->statements.size(), 1U);

    auto* variable = function->body->statements[0]->as<VarDeclaration>();
    ASSERT_NE(variable, nullptr);
    auto* closure = variable->initializer->as<ClosureExpression>();

    ASSERT_NE(closure, nullptr);
    ASSERT_EQ(closure->parameters.size(), 1U);
    EXPECT_EQ(closure->parameters[0]->name, "x");
}

TEST(FrontendParser, AstSnapshotRemainsStable) {
    FrontendHarness harness("fn main() -> i32 { return 1 + 2 }\n");
    ASSERT_FALSE(harness.context.diagnostics.has_errors());

    auto output = capture_stdout([&] {
        AstDumper dumper;
        dumper.dump_program(harness.program);
    });

    EXPECT_TRUE(Snapshot::matches("fixtures/snapshots/simple_ast.txt", output));
}

TEST(FrontendTypes, ChecksCompatibilityRules) {
    SemaContext sema;
    const Type* i32 = sema.builtin_resolver.primitives.at("i32");
    const Type* u32 = sema.builtin_resolver.primitives.at("u32");
    const Type* void_type = sema.builtin_resolver.primitives.at("void");

    PointerType mutable_i32(i32, true);
    PointerType immutable_i32(i32, false);
    PointerType immutable_void(void_type, false);
    ArrayType four_i32(i32, static_cast<std::size_t>(4));
    ArrayType four_u32(u32, static_cast<std::size_t>(4));

    EXPECT_TRUE(i32->accepts(i32));
    EXPECT_FALSE(i32->accepts(u32));
    EXPECT_TRUE(immutable_i32.accepts(&mutable_i32));
    EXPECT_FALSE(mutable_i32.accepts(&immutable_i32));
    EXPECT_TRUE(immutable_void.accepts(&mutable_i32));
    EXPECT_FALSE(four_i32.accepts(&four_u32));
}

TEST(FrontendSema, AcceptsRepresentativeLanguageFeatures) {
    std::vector<std::string> snippets{
        "fn identity<T>(value: T) -> T { return value }\n"
        "fn main() -> i32 { return identity<i32>(1) }\n",

        "enum Option { None Some { value: i32 } }\n"
        "fn main() -> i32 { var item = Option::Some { value: 7 } "
        "return when (item) { Option::Some { value } -> value, else -> 0, } }\n",

        "fn main() -> i32 { var mut total: i32 = 0 "
        "for (var mut i: i32 = 0; i < 4; i = i + 1) { total = total + i } return total }\n",

        "interface Shape { public: fn area() -> i32 }\n"
        "struct Square : Shape { public: var size: i32 "
        "override fn area() -> i32 { return size * size } }\n"
        "fn main() -> i32 { var square = Square { size: 4 } return square.area() }\n",

        "struct Owner { public: var value: i32 fn ~Owner() -> void {} }\n"
        "fn take(value: Owner) -> i32 { return value.value }\n"
        "fn main() -> i32 { var owner = Owner { value: 1 } return take(owner) }\n"};

    for (const auto& snippet : snippets) {
        EXPECT_TRUE(FrontendChecks::type_check_ok(snippet)) << snippet;
    }
}
