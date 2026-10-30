#include <filesystem>
#include <limits>
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <vector>

import zep.frontend.debug.ast_dumper;
import zep.frontend.lexer;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.scope;
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

TEST(FrontendScopes, PreservesOverloadOrderVisibilityAndReferencesAcrossInsertions) {
    FrontendHarness harness("public fn choose(value: i32) -> i32 { return value }\n"
                            "fn choose(value: i64) -> i64 { return value }\n");
    ASSERT_TRUE(harness.type_check_succeeded());

    auto& root = *harness.sema.env.root_scope;
    const auto& overloads = root.lookup_function_overloads("choose");
    ASSERT_EQ(overloads.size(), 2U);
    EXPECT_EQ(overloads[0]->function_type->parameters[0].type,
              harness.sema.primitives.entries.at("i32"));
    EXPECT_EQ(overloads[1]->function_type->parameters[0].type,
              harness.sema.primitives.entries.at("i64"));

    const auto* stored = root.find_local_function_overloads("choose");
    for (auto index = 0; index < 100; ++index) {
        root.define_function("alias_" + std::to_string(index), overloads.front());
    }

    EXPECT_EQ(root.find_local_function_overloads("choose"), stored);
    EXPECT_EQ(root.lookup_function("choose"), overloads.front());
    const auto exported = root.find_exported_function_overloads("choose");
    ASSERT_EQ(exported.size(), 1U);
    EXPECT_EQ(exported.front(), overloads.front());

    Scope child(Scope::Kind::Type::Block, "child", &root);
    EXPECT_EQ(&child.lookup_function_overloads("choose"), stored);
    child.define_function("choose", overloads.back());
    ASSERT_EQ(child.lookup_function_overloads("choose").size(), 1U);
    EXPECT_EQ(child.lookup_function("choose"), overloads.back());
    EXPECT_EQ(child.find_local_function_overloads("missing"), nullptr);
    EXPECT_TRUE(child.lookup_function_overloads("missing").empty());
}

TEST(FrontendTypes, ChecksCompatibilityRules) {
    SemaContext sema;
    const Type* i32 = sema.primitives.entries.at("i32");
    const Type* u32 = sema.primitives.entries.at("u32");
    const Type* void_type = sema.primitives.entries.at("void");

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

TEST(FrontendTypes, LayoutDistinguishesDependentInvalidAndConcreteTypes) {
    SemaContext sema;
    TypeLayout layout;
    NamedType parameter("T", {});
    ArrayType unsized(sema.primitives.entries.at("i32"), UnsizedArrayExtent());
    ArrayType overflow(sema.primitives.entries.at("i64"),
                       static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()));

    EXPECT_EQ(layout.resolve(&parameter).state, TypeLayout::Result::State::Dependent);
    EXPECT_EQ(layout.resolve(&unsized).state, TypeLayout::Result::State::Invalid);
    EXPECT_EQ(layout.resolve(&overflow).state, TypeLayout::Result::State::Invalid);
    EXPECT_EQ(layout.resolve(sema.primitives.entries.at("void")).state,
              TypeLayout::Result::State::Invalid);

    const auto integer = layout.resolve(sema.primitives.entries.at("i32"));
    ASSERT_EQ(integer.state, TypeLayout::Result::State::Concrete);
    EXPECT_EQ(integer.size, 4U);
    EXPECT_EQ(integer.alignment, 4U);
}

TEST(FrontendTypes, LayoutPadsAggregatesAndRejectsCycles) {
    SemaContext sema;
    StructType aggregate("Padded", {},
                         {FieldType("first", sema.primitives.entries.at("char")),
                          FieldType("second", sema.primitives.entries.at("i64")),
                          FieldType("third", sema.primitives.entries.at("char"))});
    TypeLayout layout;
    const auto result = layout.resolve(&aggregate);
    ASSERT_EQ(result.state, TypeLayout::Result::State::Concrete);
    EXPECT_EQ(result.size, 24U);
    EXPECT_EQ(result.alignment, 8U);

    StructType recursive("Recursive", {}, {});
    recursive.fields.emplace_back("self", &recursive);
    EXPECT_EQ(layout.resolve(&recursive).state, TypeLayout::Result::State::Invalid);
    EXPECT_EQ(layout.resolve(&aggregate).state, TypeLayout::Result::State::Concrete);
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
        EXPECT_TRUE(assert_type_check_ok(snippet));
    }
}
