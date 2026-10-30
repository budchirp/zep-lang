#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.common.source.position;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.type;
import zep.hir.node;
import zep.hir.program;

TEST(HirNodes, PreserveKindTypeSpanAndChildIdentity) {
    HIRProgram program;
    IntegerType integer(false, 32);
    Span span(Position(2, 3), Position(2, 5));
    auto* value = program.nodes.create<HIRNumberLiteral>(span, "42", &integer);
    auto* returned = program.nodes.create<HIRReturnStatement>(span, value, &integer);

    const HIRNode* node = value;
    EXPECT_EQ(node->as<HIRNumberLiteral>(), value);
    EXPECT_EQ(node->as<HIRStringLiteral>(), nullptr);
    EXPECT_EQ(value->type, &integer);
    EXPECT_EQ(value->span.start.line, 2U);
    EXPECT_EQ(value->span.end.column, 5U);
    EXPECT_EQ(returned->value, value);
}

TEST(HirNodes, ArenaGrowthPreservesReferences) {
    HIRProgram program;
    auto* first = program.nodes.create<HIRNumberLiteral>(Span{}, "first");
    auto* returned = program.nodes.create<HIRReturnStatement>(Span{}, first);

    for (auto index = 0; index < 4096; ++index) {
        static_cast<void>(
            program.nodes.create<HIRNumberLiteral>(Span{}, std::to_string(index)));
    }

    EXPECT_EQ(returned->value, first);
    EXPECT_EQ(first->value, "first");
}

TEST(HirProgram, OrderedStatementsPreserveFunctionAndChildIdentity) {
    HIRProgram program;
    auto* value = program.nodes.create<HIRNumberLiteral>(Span{}, "42");
    auto* returned = program.nodes.create<HIRReturnStatement>(Span{}, value);
    auto* body = program.nodes.create<HIRBlockStatement>(
        Span{}, std::vector<HIRStatement*>{returned});
    auto* function = program.nodes.create<HIRFunctionDeclaration>(
        Span{}, Visibility::Type::Public, Linkage::Type::Internal, "answer", nullptr, body, false,
        nullptr);

    program.statements.push_back(function);

    EXPECT_EQ(program.statements.front(), function);
    EXPECT_EQ(program.statements.front()->as<HIRFunctionDeclaration>()->body->statements.front(),
              returned);
    EXPECT_EQ(returned->value, value);
    EXPECT_TRUE(program.referenced_types.empty());
}
