#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.common.source.position;
import zep.common.source.span;
import zep.frontend.sema.kind;
import zep.frontend.sema.type;
import zep.hir.node;
import zep.hir.program;

TEST(HirNodes, PreserveKindTypeSpanAndChildIdentity) {
    HIRProgram program;
    IntegerType integer(false, 32);
    Span span(Position(2, 3), Position(2, 5));
    auto* value = program.context.nodes.create<HIRNumberLiteral>(span, "42", &integer);
    auto* returned = program.context.nodes.create<HIRReturnStatement>(span, value, &integer);

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
    auto* first = program.context.nodes.create<HIRNumberLiteral>(Span{}, "first");
    auto* returned = program.context.nodes.create<HIRReturnStatement>(Span{}, first);

    for (auto index = 0; index < 4096; ++index) {
        static_cast<void>(
            program.context.nodes.create<HIRNumberLiteral>(Span{}, std::to_string(index)));
    }

    EXPECT_EQ(returned->value, first);
    EXPECT_EQ(first->value, "first");
}

TEST(HirProgram, OrderedStatementsAndFunctionViewsShareNodes) {
    HIRProgram program;
    auto* value = program.context.nodes.create<HIRNumberLiteral>(Span{}, "42");
    auto* returned = program.context.nodes.create<HIRReturnStatement>(Span{}, value);
    auto* body = program.context.nodes.create<HIRBlockStatement>(
        Span{}, std::vector<HIRStatement*>{returned});
    auto* function = program.context.nodes.create<HIRFunctionDeclaration>(
        Span{}, Visibility::Type::Public, Linkage::Type::Internal, "answer", nullptr, body, false,
        nullptr);

    program.statements.push_back(function);
    program.functions.push_back(function);

    EXPECT_EQ(program.statements.front(), program.functions.front());
    EXPECT_EQ(program.functions.front()->body->statements.front(), returned);
    EXPECT_EQ(returned->value, value);
    EXPECT_TRUE(program.globals.empty());
    EXPECT_TRUE(program.referenced_types.empty());
}
