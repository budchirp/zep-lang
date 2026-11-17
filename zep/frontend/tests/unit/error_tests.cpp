#include <gtest/gtest.h>
#include <string>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.sema.types;
import zep.frontend.sema.checking.checker;

class ErrorChecks {
  public:
    static bool accepts(const std::string& source) {
        FrontendHarness harness("interface Throwable {}\n" + source);
        SemanticChecker checker(harness.context, harness.sema);
        auto* declaration = harness.program.statements.front()->as<InterfaceDeclaration>();
        checker.visit(*declaration);
        harness.sema.errors.throwable = declaration->type->as<InterfaceType>();
        checker.check(harness.program);

        return !harness.context.diagnostics.has_errors();
    }
};

TEST(FrontendErrors, RequiresTheCanonicalThrowableInterface) {
    for (const auto* source : {
        "enum E { Bad } @throws(E) fn work() -> void { throw E::Bad }",
        "enum E : Throwable { Bad } @throws(E) fn work() -> void { throw E::Bad }",
        "struct E {} @throws(E) fn work() -> void { throw E {} }",
        "interface Other {} struct E : Other {} @throws(E) fn work() -> void { throw E {} }",
        "@throws(Throwable) fn work() -> void {}",
    }) {
        EXPECT_FALSE(ErrorChecks::accepts(source)) << source;
    }

    EXPECT_TRUE(assert_type_check_error(R"zep(
interface Throwable {}
struct Fake : Throwable {}
@throws(Fake) fn work() -> void { throw Fake {} }
)zep"));

    EXPECT_TRUE(ErrorChecks::accepts(R"zep(
struct Error : Throwable {}
struct Exception : Throwable {}
struct FileError : Error {}
struct ParseError : Exception {}
interface Specific : Throwable {}
struct Direct : Specific {}
@throws(FileError, ParseError, Direct) fn work() -> void { throw FileError {} }
)zep"));
}

TEST(FrontendErrors, RequiresThrowableConstraintsForGenericErrors) {
    EXPECT_TRUE(ErrorChecks::accepts(R"zep(
fn moving<T>(value: &T) -> move T { return &value as move T }
@throws(T) fn raise<T: Throwable>(value: T) -> never { throw moving(value) }
struct Failure : Throwable {}
fn work() -> void { raise(Failure {}) catch { error: Failure -> {}, } }
)zep"));

    EXPECT_FALSE(ErrorChecks::accepts(R"zep(
@throws(T) fn raise<T>(value: T) -> void { throw value }
)zep"));
}

TEST(FrontendErrors, HandlesAndExplicitlyPropagatesDeclaredErrors) {
    EXPECT_TRUE(ErrorChecks::accepts(R"zep(
struct Failure : Throwable {}
@throws(Failure)
fn read() -> i32 { throw Failure {} }
@throws(Failure)
fn forward() -> i32 { return try read() }
fn main() -> i32 {
    return try { forward() } catch { error: Failure -> 42, }
}
)zep"));
}

TEST(FrontendErrors, RequiresHandlingEvenForDiscardedCalls) {
    for (const auto* body : {"read()", "return read()", "var value = read()"}) {
        EXPECT_FALSE(ErrorChecks::accepts(
            std::string("struct Failure : Throwable {}\n@throws(Failure) fn read() -> i32 { throw Failure {} }\n") +
            "@throws(Failure) fn work() -> i32 { " + body + " }\n"));
    }

    EXPECT_FALSE(ErrorChecks::accepts(R"zep(
struct Failure : Throwable {}
@throws(Failure) fn read() -> i32 { throw Failure {} }
fn work() -> i32 { return try read() }
)zep"));
}

TEST(FrontendErrors, RequiresExplicitPropagationAfterPartialRecovery) {
    const auto source = std::string(R"zep(
struct First : Throwable {}
struct Second : Throwable {}
@throws(First, Second)
fn read() -> i32 { throw First {} }
@throws(Second)
fn work() -> i32 { return )zep");

    EXPECT_TRUE(ErrorChecks::accepts(source +
        "try read() catch { error: First -> 0, } }\n"));
    EXPECT_FALSE(ErrorChecks::accepts(source +
        "read() catch { error: First -> 0, } }\n"));
}

TEST(FrontendErrors, MatchesCanonicalErrorSetsInCallbacks) {
    EXPECT_TRUE(ErrorChecks::accepts(R"zep(
struct First : Throwable {}
struct Second : Throwable {}
@throws(First, Second) fn read() -> i32 { throw First {} }
fn work() -> i32 {
    var callback: @throws(Second, First) () -> i32 = read
    return callback() catch { error: First -> 1, error: Second -> 2, }
}
)zep"));

    EXPECT_FALSE(ErrorChecks::accepts(R"zep(
struct Failure : Throwable {}
@throws(Failure) fn read() -> i32 { throw Failure {} }
fn work() -> void { var callback: () -> i32 = read }
)zep"));

    EXPECT_FALSE(ErrorChecks::accepts(R"zep(
struct First : Throwable {}
struct Second : Throwable {}
@throws(First) fn read() -> i32 { throw First {} }
@throws(Second) fn read() -> i32 { throw Second {} }
)zep"));
}

TEST(FrontendErrors, InfersLambdaErrorsAndAcceptsStructPayloads) {
    EXPECT_TRUE(ErrorChecks::accepts(R"zep(
struct Failure : Throwable { public: var code: i32 }
fn main() -> i32 {
    return { -> throw Failure { code: 42 } }() catch { error: Failure -> error.code, }
}
)zep"));
}

TEST(FrontendErrors, RejectsInvalidErrorsAndHandlers) {
    for (const auto* source : {
        "@throws(i32) fn work() -> i32 { throw 1 }",
        "struct E : Throwable {} @throws(E, E) fn work() -> void {}",
        "struct E : Throwable {} fn work() -> void { throw E {} }",
        "struct E : Throwable {} @throws(E) fn read() -> i32 { throw E {} } fn work() -> i32 { return read() catch { error: E -> 0, other: E -> 1, } }",
        "struct E : Throwable {} fn work() -> i32 { return 1 catch { error: E -> 0, } }",
        "struct E : Throwable {} @throws(E) fn read() -> i32 { throw E {} } fn work() -> i32 { return read() catch { error: E -> false, } }",
    }) {
        EXPECT_FALSE(ErrorChecks::accepts(source)) << source;
    }
}

TEST(FrontendErrors, KeepsEntryPointsForeignCallsAndCleanupNonthrowing) {
    for (const auto* source : {
        "struct E : Throwable {} @throws(E) fn main() -> i32 { throw E {} }",
        "struct E : Throwable {} @name(\"main\") @throws(E) fn entry() -> i32 { throw E {} }",
        "struct E : Throwable {} @throws(E) extern fn read() -> i32",
        "struct E : Throwable {} extern fn apply(callback: @throws(E) () -> void) -> void",
        "struct E : Throwable {} extern fn callback() -> @throws(E) () -> void",
        "struct E : Throwable {} struct Callbacks { public: var call: @throws(E) () -> void } extern fn apply(callbacks: *Callbacks) -> void",
        "struct E : Throwable {} struct Owner { public: @throws(E) fn ~Owner() -> void { throw E {} } }",
        "struct E : Throwable {} @throws(E) fn read() -> void { throw E {} } @throws(E) fn work() -> void { defer { try read() } }",
    }) {
        EXPECT_FALSE(ErrorChecks::accepts(source)) << source;
    }
}
