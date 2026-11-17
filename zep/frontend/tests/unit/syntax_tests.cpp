#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.kind;
import zep.frontend.debug.ast_dumper;
import zep.frontend.lexer;
import zep.frontend.token;
import zep.test.support;

TEST(FrontendParser, ParsesNamedModuleTests) {
    FrontendHarness harness("test \"x works with y\" { var value = 42 }\n");

    ASSERT_TRUE(harness.parse_succeeded());
    ASSERT_EQ(harness.program.statements.size(), 1U);

    const auto* test = harness.program.statements.front()->as<TestDeclaration>();

    ASSERT_NE(test, nullptr);
    EXPECT_EQ(test->name, "x works with y");
    EXPECT_EQ(test->name_span.start.column, 6U);
    EXPECT_FALSE(test->enabled);
    EXPECT_TRUE(test->function->prototype->parameters.empty());
}

TEST(FrontendParser, ParsesTypedErrorsAndProtectedExpressions) {
    FrontendHarness harness(R"zep(
struct Failure {}
@throws(Failure)
fn read() -> i32 { throw Failure {} }
fn work() -> i32 {
    var callback: @throws(Failure) () -> i32 = read
    return try { callback() } catch { error: Failure -> 0, }
}
)zep");
    ASSERT_TRUE(harness.parse_succeeded());

    const auto* function = harness.program.statements[1]->as<FunctionDeclaration>();
    ASSERT_NE(function, nullptr);
    ASSERT_EQ(function->prototype->errors.size(), 1U);
    EXPECT_TRUE(function->attributes.empty());
    const auto* body = function->body->statements.front()->as<ExpressionStatement>();
    ASSERT_NE(body, nullptr);
    EXPECT_NE(body->expression->as<ThrowExpression>(), nullptr);

    const auto* work = harness.program.statements.back()->as<FunctionDeclaration>();
    const auto* returned = work->body->statements.back()->as<ReturnStatement>();
    const auto* protected_expression = returned->value->as<TryExpression>();
    ASSERT_NE(protected_expression, nullptr);
    const auto* handled = protected_expression->value->as<CatchExpression>();
    ASSERT_NE(handled, nullptr);
    ASSERT_EQ(handled->arms.size(), 1U);
    EXPECT_EQ(handled->arms.front().name, "error");
}

TEST(FrontendParser, RejectsMalformedErrorDeclarationsAndHandlers) {
    for (const auto* source : {
        "@throws() fn read() -> void {}",
        "@throws(E) @throws(E) fn read() -> void {}",
        "fn read() -> void { work() catch {} }",
        "fn read() -> void { work() catch { error -> 0 } }",
        "fn read() -> void { work() catch { error: E 0 } }",
        "var callback: @throws(E) i32",
    }) {
        FrontendHarness harness(source);
        EXPECT_FALSE(harness.parse_succeeded()) << source;
    }
}

TEST(FrontendParser, RejectsInvalidTestDeclarations) {
    for (const auto* source : {
        "test \"\" {}", "test name {}", "test \"name\"() {}",
        "public test \"name\" {}", "private test \"name\" {}",
        "@os(\"linux\") test \"name\" {}",
        "fn check() -> void { test \"nested\" {} }",
        "struct Value { test \"nested\" {} }",
        "enum Value { Only test \"nested\" {} }",
    }) {
        FrontendHarness harness(source);

        EXPECT_FALSE(harness.parse_succeeded()) << source;
    }
}

TEST(FrontendLexer, TokenizesKeywordsOperatorsAndLiterals) {
    Lexer lexer("public fn main() -> i32 { return 1 + 2 != 4 && true }");
    const std::vector<Token::Type> expected{
        Token::Type::Public, Token::Type::Fn, Token::Type::Identifier,
        Token::Type::LeftParen, Token::Type::RightParen, Token::Type::Arrow,
        Token::Type::Identifier, Token::Type::LeftBrace, Token::Type::Return,
        Token::Type::Number, Token::Type::Plus, Token::Type::Number,
        Token::Type::NotEquals, Token::Type::Number, Token::Type::And,
        Token::Type::Boolean, Token::Type::RightBrace, Token::Type::Eof};

    for (auto type : expected) {
        EXPECT_EQ(lexer.next_token().type, type);
    }
}

TEST(FrontendLexer, HandlesStringAndCharEscapes) {
    Lexer lexer("\"a\\n\\\"\" '\\n'");
    const auto string = lexer.next_token();
    const auto character = lexer.next_token();

    EXPECT_EQ(string.type, Token::Type::String);
    EXPECT_EQ(string.value, "a\n\"");
    EXPECT_EQ(character.type, Token::Type::Char);
    EXPECT_EQ(character.value, "\n");
}

TEST(FrontendParser, ParsesDeclarationsAndExpressionPrecedence) {
    FrontendHarness harness("struct Point { public: var x: i32 var y: i32 }\n"
                            "fn main() -> i32 { return 1 + 2 * 3 }\n");

    ASSERT_TRUE(harness.parse_succeeded());
    ASSERT_EQ(harness.program.statements.size(), 2U);

    const auto* structure = harness.program.statements[0]->as<StructDeclaration>();
    const auto* function = harness.program.statements[1]->as<FunctionDeclaration>();

    ASSERT_NE(structure, nullptr);
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(structure->name, "Point");
    EXPECT_EQ(structure->fields.size(), 2U);
    EXPECT_EQ(function->prototype->name, "main");
    ASSERT_NE(function->body, nullptr);
    ASSERT_EQ(function->body->statements.size(), 1U);

    const auto* returned = function->body->statements.front()->as<ReturnStatement>();

    ASSERT_NE(returned, nullptr);
    ASSERT_NE(returned->value, nullptr);

    const auto* addition = returned->value->as<BinaryExpression>();

    ASSERT_NE(addition, nullptr);
    EXPECT_EQ(addition->op, BinaryOperator::Type::Plus);
    ASSERT_NE(addition->right, nullptr);

    const auto* multiplication = addition->right->as<BinaryExpression>();

    ASSERT_NE(multiplication, nullptr);
    EXPECT_EQ(multiplication->op, BinaryOperator::Type::Asterisk);
}

TEST(FrontendParser, ParsesClosureExpressions) {
    FrontendHarness harness("fn main() -> void { var function = { value: i32 -> value } }\n");

    ASSERT_TRUE(harness.parse_succeeded());
    ASSERT_EQ(harness.program.statements.size(), 1U);

    const auto* function = harness.program.statements[0]->as<FunctionDeclaration>();

    ASSERT_NE(function, nullptr);
    ASSERT_NE(function->body, nullptr);
    ASSERT_EQ(function->body->statements.size(), 1U);

    const auto* variable = function->body->statements[0]->as<VarDeclaration>();

    ASSERT_NE(variable, nullptr);

    const auto* closure = variable->initializer->as<ClosureExpression>();

    ASSERT_NE(closure, nullptr);
    ASSERT_EQ(closure->parameters.size(), 1U);
    EXPECT_EQ(closure->parameters[0]->name, "value");
}

TEST(FrontendParser, AstSnapshotRemainsStable) {
    FrontendHarness harness("fn main() -> i32 { return 1 + 2 }\n");

    ASSERT_TRUE(harness.parse_succeeded());

    const auto output = capture_stdout([&] {
        AstDumper dumper;
        dumper.dump_program(harness.program);
    });

    EXPECT_TRUE(Snapshot::matches("fixtures/snapshots/simple_ast.txt", output));
}

TEST(FrontendParser, DumpsCheckedErrorsAndNullThroughSharedDispatch) {
    FrontendHarness harness(R"zep(
@throws(Failure) fn read() -> i32 { throw Failure {} }
fn work() -> i32 {
    var empty = null
    return try { read() } catch { error: Failure -> 0, }
}
)zep");
    ASSERT_TRUE(harness.parse_succeeded());

    const auto output = capture_stdout([&] {
        AstDumper dumper;
        dumper.dump_program(harness.program);
    });

    for (const auto* node : {"TryExpression(", "CatchExpression(", "ThrowExpression(", "NullLiteral("}) {
        EXPECT_NE(output.find(node), std::string::npos) << node;
    }
}

TEST(FrontendParser, RecoversAfterMalformedExpressionsWithoutExceptions) {
    for (const auto* source : {
        "fn broken() -> void { var value = ; var next = 2 } fn recovered() -> void {}",
        "fn broken() -> void { for (; true; 1) {} } fn recovered() -> void {}",
        "fn broken() -> void { var value: & = 1 } fn recovered() -> void {}",
        "fn broken() -> void { var value = try work() catch { error: -> 0 } } fn recovered() -> void {}",
    }) {
        FrontendHarness harness(source);

        EXPECT_FALSE(harness.parse_succeeded()) << source;
        EXPECT_FALSE(harness.context.diagnostics.entries.empty());
    }
}

TEST(FrontendParser, RejectsAllocatingNewAndDeleteExpressions) {
    for (const auto* source : {
             "fn clear(value: i32) -> void { delete value }",
             "fn clear(value: &i32) -> void { delete value }",
             "fn clear(ptr: *mut void) -> void { delete ptr }",
             "fn allocate() -> void { new 42 }",
         }) {
        FrontendHarness harness(source);

        EXPECT_FALSE(harness.parse_succeeded()) << source;
    }
}

TEST(FrontendParserMatrix, ParsesDirectImportsAndAliases) {
    const std::string source = R"zep(
import math.ops.add
import math.ops.subtract as minus
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesAttributesOnFunctionsAndTypes) {
    const std::string source = R"zep(
@name("renamed_answer")
fn answer() -> i32 {
    return 42
}

@target("linux")
struct LinuxOnly {
    public:
        var value: i32 = 1
}

@mangle(false)
extern fn puts(value: cstr) -> i32
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesGenericTypeAliases) {
    const std::string source = R"zep(
struct Pair<A, B> {
    public:
        var first: A
        var second: B
}

type IntPair = Pair<i32, i32>
type Alias<T> = Pair<T, T>
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesExternFunctionsVarsAndVariadicPrototypes) {
    const std::string source = R"zep(
extern fn printf(format: cstr, ...) -> i32
extern fn memcpy(destination: *mut void, source: *void, count: i64) -> *mut void
extern var errno: i32
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesStructVisibilityDefaultsMethodsNestedTypesAndInheritance) {
    const std::string source = R"zep(
struct Node {
    public:
        var id: i32
}

struct Entity : Node {
    public:
        var value: i32 = 0

        static fn make(value: i32) -> Entity {
            return Entity { id: 0, value: value }
        }

        fn set(value: i32) mut -> void {
            self.value = value
        }

    private:
        var hidden: i32 = 1

        struct Meta {
            public:
                var label: cstr
        }

        enum Kind {
            Basic
            Advanced
        }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesEnumsWithPayloadsBackingGenericsAndMethods) {
    const std::string source = R"zep(
enum Status : i32 {
    Ok = 0
    Failed = 1

    public:
        fn is_ok() -> boolean {
            return value == 0
        }
}

enum Option<T> {
    None
    Some { value: T }

    public:
        fn is_some() -> boolean {
            return true
        }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesInterfaceInheritanceAndMutableMethods) {
    const std::string source = R"zep(
interface Readable {
    public:
        fn read() -> i32
}

interface Writer : Readable {
    public:
        fn write(value: i32) mut -> void
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesConstGenericFunctionsAndNamedGenericCalls) {
    const std::string source = R"zep(
fn identity<T>(value: T) -> T {
    return value
}

fn pick<const N: i32, T>(value: T) -> T {
    return value
}

fn main() -> i32 {
    return pick<N: 3, T: i32>(identity<i32>(1))
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesComplexTypeExpressions) {
    const std::string source = R"zep(
struct Box<T> {
    public:
        var value: T
}

fn accept(
    pointer: *mut Box<i32>,
    matrix: i32[2][3],
    callback: (i32, *void) -> boolean,
    qualified: math.types.Pair<i32, cstr>
) -> void {
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesMemberIndexPointerAndStaticAccess) {
    const std::string source = R"zep(
fn main() -> i32 {
    var mut values: i32[3]
    var pointer: *mut i32 = &mut values[0]
    pointer->value = Math::identity(values[1])
    return pointer->value
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesIfWhenSubjectAndSubjectlessExpressions) {
    const std::string source = R"zep(
enum Flag {
    On
    Off
}

fn main(flag: Flag, condition: boolean) -> i32 {
    var value = if (condition) {
        1
    } else {
        2
    }

    return when (flag) {
        Flag::On -> value,
        Flag::Off if (condition) -> 3,
        else -> when {
            condition -> 4,
            else -> 0,
        },
    }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesWhileForAndDefer) {
    const std::string source = R"zep(
fn main() -> i32 {
    var mut total: i32 = 0
    while (total < 3) {
        total = total + 1
    }

    for (var mut i: i32 = 0; i < 4; i = i + 1) {
        total = total + i
    }

    defer {
        total = total + 1
    }

    return total
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesAssignmentUnaryCastIsAndPrecedence) {
    const std::string source = R"zep(
fn main(value: any) -> i32 {
    var mut result: i32 = 0
    result = -1 + +2 * 3 as i32

    if (!(value is i32) || result <= 0) {
        return 0
    }

    return result
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesStructAndEnumLiteralsWithNamedPayloads) {
    const std::string source = R"zep(
struct Point {
    public:
        var x: i32
        var y: i32
}

enum MaybePoint {
    None
    Some { value: Point }
}

fn main() -> MaybePoint {
    return MaybePoint::Some { value: Point { x: 1, y: 2 } }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesTypedAndInferredClosures) {
    const std::string source = R"zep(
fn main() -> void {
    var typed = { left: i32, right: i32 -> left + right }
    var inferred = { value -> value }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ReportsParserErrorsWithoutExiting) {
    FrontendHarness legacy_import("import math { add }\n");
    EXPECT_FALSE(legacy_import.parse_succeeded());
    ASSERT_FALSE(legacy_import.context.diagnostics.entries.empty());

    FrontendHarness public_import("public import math.add\n");
    EXPECT_FALSE(public_import.parse_succeeded());
    ASSERT_FALSE(public_import.context.diagnostics.entries.empty());

    FrontendHarness unknown_builtin("fn main() -> i32 { return #missing(i32) }\n");
    EXPECT_FALSE(unknown_builtin.parse_succeeded());
    ASSERT_FALSE(unknown_builtin.context.diagnostics.entries.empty());
    EXPECT_EQ(unknown_builtin.context.diagnostics.entries.front().message,
              "unknown builtin function '#missing'");
}
