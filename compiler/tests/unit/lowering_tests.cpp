#include <gtest/gtest.h>
#include <string>
#include <utility>

import zep.frontend.test.harness;
import zep.compiler.lowering;
import zep.hir.node;
import zep.hir.program;
import zep.frontend.sema.kind;

class HirHarness {
  public:
    FrontendHarness frontend;

    explicit HirHarness(std::string content) : frontend(std::move(content)) {
        frontend.type_check();
    }

    std::shared_ptr<const HIRProgram> lower() {
        HIRLowerer lowerer(frontend.context, frontend.sema);
        return lowerer.lower(frontend.program);
    }

    bool succeeded() const { return !frontend.context.diagnostics.has_errors(); }
};

TEST(HirBuilder, LowersFunctionsAndReturns) {
    HirHarness harness("fn main() -> i32 { return 42 }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();

    ASSERT_EQ(program->statements.size(), 1U);
    auto* function = static_cast<HIRFunctionDeclaration*>(nullptr);
    for (auto* statement : program->statements) {
        auto* candidate = statement->as<HIRFunctionDeclaration>();
        if (candidate != nullptr && candidate->name.starts_with("main")) {
            function = candidate;
            break;
        }
    }
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(function->name, "main");
    ASSERT_NE(function->body, nullptr);
    ASSERT_FALSE(function->body->statements.empty());
    EXPECT_NE(function->body->statements.back()->as<HIRReturnStatement>(), nullptr);
}

TEST(HirBuilder, LowersDirectCapturedClosureInvocationWithHiddenReferences) {
    HirHarness harness(R"zep(
fn main() -> i32 {
    var value = 42
    return { -> value }()
}
)zep");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    ASSERT_NE(program, nullptr);

    auto* function = static_cast<HIRFunctionDeclaration*>(nullptr);
    for (auto* statement : program->statements) {
        auto* candidate = statement->as<HIRFunctionDeclaration>();
        if (candidate != nullptr && candidate->name.starts_with("main")) {
            function = candidate;
            break;
        }
    }
    ASSERT_NE(function, nullptr);
    auto* returned = function->body->statements.back()->as<HIRReturnStatement>();
    ASSERT_NE(returned, nullptr);
    auto* call = returned->value->as<HIRCallExpression>();
    ASSERT_NE(call, nullptr);
    EXPECT_NE(call->target->as<HIRDirectCallTarget>(), nullptr);
    ASSERT_EQ(call->arguments.size(), 1);
    EXPECT_NE(call->arguments.front()->as<HIRUnaryExpression>(), nullptr);
}

TEST(HirBuilder, LowersDirectCapturedClosureParametersWithHiddenReferences) {
    HirHarness harness(R"zep(
fn main() -> i32 {
    var value = 40
    return { amount: i32 -> value + amount }(2)
}
)zep");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    ASSERT_NE(program, nullptr);

    auto* function = static_cast<HIRFunctionDeclaration*>(nullptr);
    for (auto* statement : program->statements) {
        auto* candidate = statement->as<HIRFunctionDeclaration>();
        if (candidate != nullptr && candidate->name.starts_with("main")) {
            function = candidate;
            break;
        }
    }
    ASSERT_NE(function, nullptr);
    auto* returned = function->body->statements.back()->as<HIRReturnStatement>();
    ASSERT_NE(returned, nullptr);
    auto* call = returned->value->as<HIRCallExpression>();
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->arguments.size(), 2);
    EXPECT_NE(call->arguments.front()->as<HIRUnaryExpression>(), nullptr);
}

TEST(HirBuilder, RejectsUnsizedSpecializedSizeQueries) {
    FrontendHarness harness(R"zep(
fn size<T>() -> i64 { return #sizeof(T) }
fn main() -> i64 { return size<i32[]>() }
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    HIRLowerer lowerer(harness.context, harness.sema);

    EXPECT_EQ(lowerer.lower(harness.program), nullptr);
    ASSERT_EQ(lowerer.diagnostics.entries.size(), 1);
    EXPECT_EQ(lowerer.diagnostics.entries[0].message, "sizeof requires a concrete sized type");
}

TEST(HirBuilder, SpecializesDependentLengthQueriesFromArrayTypes) {
    FrontendHarness harness(R"zep(
fn total<const N: i32>(values: i32[N]) -> i64 {
    var extra: u8[#length(values) + 1]
    return #length(extra)
}
fn main() -> i64 {
    var values: i32[3] = [1, 2, 3]
    return total<3>(values)
}
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    HIRLowerer lowerer(harness.context, harness.sema);
    auto program = lowerer.lower(harness.program);
    ASSERT_NE(program, nullptr);

    HIRFunctionDeclaration* specialization = nullptr;
    for (auto* statement : program->statements) {
        auto* function = statement->as<HIRFunctionDeclaration>();
        if (function != nullptr && function->name.starts_with("total")) {
            specialization = function;
        }
    }

    ASSERT_NE(specialization, nullptr);
    ASSERT_FALSE(specialization->body->statements.empty());
    const auto* returned = specialization->body->statements.back()->as<HIRReturnStatement>();
    ASSERT_NE(returned, nullptr);
    ASSERT_NE(returned->value, nullptr);
    const auto* value = returned->value->as<HIRNumberLiteral>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->value, "4");
}

TEST(HirBuilder, LowersLoopsToLoopStatements) {
    HirHarness harness(
        "fn main() -> i32 { var mut total: i32 = 0 "
        "for (var mut i: i32 = 0; i < 3; i = i + 1) { total = total + i } return total }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    auto* function = program->statements[0]->as<HIRFunctionDeclaration>();
    ASSERT_NE(function, nullptr);

    auto found_loop = false;
    for (auto* statement : function->body->statements) {
        if (statement->as<HIRLoopStatement>() != nullptr) {
            found_loop = true;
        }
    }

    EXPECT_TRUE(found_loop);
}

TEST(HirBuilder, EmitsGenericSpecializations) {
    HirHarness harness("fn identity<T>(value: T) -> T { return value }\n"
                       "fn main() -> i32 { return identity<i32>(1) }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();

    auto function_count = std::size_t(0);
    for (auto* statement : program->statements) {
        if (statement->as<HIRFunctionDeclaration>() != nullptr) {
            function_count++;
        }
    }

    EXPECT_GE(function_count, 2U);
}

TEST(HirBuilder, InsertsCleanupForOwnedLocals) {
    HirHarness harness("struct Owner { public: var value: i32 fn ~Owner() -> void {} }\n"
                       "fn main() -> i32 { var owner = Owner { value: 1 } return owner.value }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();

    auto has_statement_group = false;
    for (auto* statement : program->statements) {
        auto* function = statement->as<HIRFunctionDeclaration>();
        if (function == nullptr || function->body == nullptr) {
            continue;
        }

        for (auto* body_statement : function->body->statements) {
            if (body_statement->as<HIRStatementGroup>() != nullptr) {
                has_statement_group = true;
            }
        }
    }

    EXPECT_TRUE(has_statement_group);
}

TEST(HirBuilder, LowersExternVarWithNameAttribute) {
    HirHarness harness("@name(\"custom_errno\")\n"
                       "extern var errno: i32\n"
                       "fn main() -> i32 { return errno }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();

    ASSERT_EQ(program->statements.size(), 2U);
    auto* var_decl = program->statements[0]->as<HIRVarDeclaration>();
    ASSERT_NE(var_decl, nullptr);
    EXPECT_EQ(var_decl->name, "custom_errno");
    EXPECT_EQ(var_decl->linkage, Linkage::Type::External);

    auto* function = program->statements[1]->as<HIRFunctionDeclaration>();
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(function->name, "main");
    ASSERT_NE(function->body, nullptr);
    ASSERT_FALSE(function->body->statements.empty());

    auto* return_stmt = function->body->statements.back()->as<HIRReturnStatement>();
    ASSERT_NE(return_stmt, nullptr);
    auto* value = return_stmt->value->as<HIRIdentifierExpression>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->name, "custom_errno");
}

TEST(HirBuilder, PreservesAbiForLocalExternCalls) {
    HirHarness harness("extern fn puts(value: cstr) -> i32\n"
                       "fn main() -> i32 { return puts(\"hello\") }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    ASSERT_EQ(program->statements.size(), 2U);

    auto* extern_function = program->statements[0]->as<HIRFunctionDeclaration>();
    ASSERT_NE(extern_function, nullptr);
    EXPECT_EQ(extern_function->abi, Abi::Type::C);

    auto* main_function = program->statements[1]->as<HIRFunctionDeclaration>();
    ASSERT_NE(main_function, nullptr);
    auto* return_statement = main_function->body->statements[0]->as<HIRReturnStatement>();
    ASSERT_NE(return_statement, nullptr);
    auto* call = return_statement->value->as<HIRCallExpression>();
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->target->as<HIRDirectCallTarget>()->function_symbol->abi, Abi::Type::C);
}

TEST(HirBuilder, UsesLanguageAbiForOrdinaryFunctions) {
    HirHarness harness("fn value() -> i32 { return 1 }\n"
                       "fn main() -> i32 { return value() }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    ASSERT_EQ(program->statements.size(), 2U);

    auto* value_function = program->statements[0]->as<HIRFunctionDeclaration>();
    ASSERT_NE(value_function, nullptr);
    EXPECT_EQ(value_function->abi, Abi::Type::Language);

    auto* main_function = program->statements[1]->as<HIRFunctionDeclaration>();
    ASSERT_NE(main_function, nullptr);
    auto* return_statement = main_function->body->statements[0]->as<HIRReturnStatement>();
    ASSERT_NE(return_statement, nullptr);
    auto* call = return_statement->value->as<HIRCallExpression>();
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->target->as<HIRDirectCallTarget>()->function_symbol->abi, Abi::Type::Language);
}
