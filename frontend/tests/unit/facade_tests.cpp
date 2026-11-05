#include <gtest/gtest.h>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.types.compatibility;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;

TEST(FrontendFacades, PrefersConcreteWrappersAndResolvesGenericBackings) {
    FrontendHarness harness(R"zep(
interface Primitive<T> {}

public struct Generic<T: i32> : Primitive<T> {
    public:
        var value: T

        fn read() -> T { return value }
}

public struct Specific : Primitive<i32> {
    public:
        var value: i32

        fn read_specific() -> i32 { return value }
}

fn main() -> i32 {
    var generic = Generic<i64> { value: 7 as i64 }
    var value = generic.read() + (7 as i64).read()

    return 42.read_specific() + (value as i32)
}
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());

    TypeResolver resolver(harness.sema.types, harness.sema.env, harness.context.diagnostics);
    Facades facades(harness.sema.facades, resolver);

    const auto* narrow = harness.sema.primitives.entries.at("i32");
    const auto* wide = harness.sema.primitives.entries.at("i64");
    const auto* concrete = facades.wrapper(narrow);
    const auto* generic = facades.wrapper(wide);

    ASSERT_NE(concrete, nullptr);
    ASSERT_NE(generic, nullptr);
    EXPECT_EQ(concrete->name, "Specific");
    EXPECT_EQ(facades.wrapper(wide), generic);
    EXPECT_EQ(facades.backing(generic), wide);
    ASSERT_EQ(generic->generic_arguments.size(), 1U);
    EXPECT_EQ(generic->generic_arguments.front().type, wide);
}

TEST(FrontendFacades, PreservesReferenceMutabilityDuringBackingAdaptation) {
    FrontendHarness harness(R"zep(
interface Primitive<T> {}

public struct Wrapper : Primitive<i32> {
    public:
        var value: i32
}
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());

    TypeResolver resolver(harness.sema.types, harness.sema.env, harness.context.diagnostics);
    Facades facades(harness.sema.facades, resolver);
    Compatibility compatibility(harness.sema.types, resolver, facades);

    const auto* wrapper = harness.sema.env.root_scope->lookup_type("Wrapper");
    ASSERT_NE(wrapper, nullptr);

    const auto* primitive = harness.sema.primitives.entries.at("i32");
    PointerType shared_wrapper(wrapper->type, false, PointerType::ReferenceKind::Type::Borrow);
    PointerType mutable_wrapper(wrapper->type, true, PointerType::ReferenceKind::Type::Borrow);
    PointerType shared_value(primitive, false, PointerType::ReferenceKind::Type::Borrow);
    PointerType mutable_value(primitive, true, PointerType::ReferenceKind::Type::Borrow);

    EXPECT_TRUE(compatibility.accepts(&shared_wrapper, &mutable_value));
    EXPECT_FALSE(compatibility.accepts(&mutable_wrapper, &shared_value));
}

TEST(FrontendScopes, FindsMembersThroughInstantiatedAliases) {
    FrontendHarness harness(R"zep(
struct Record<T> {
    public:
        var value: T

        static fn answer() -> i32 { return 42 }
}

type Alias = Record<i32>
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());

    const auto* record = harness.sema.env.root_scope->lookup_type("Record");
    const auto* alias = harness.sema.env.root_scope->lookup_type("Alias");
    ASSERT_NE(record, nullptr);
    ASSERT_NE(alias, nullptr);

    const auto* members = Scope::members(alias->type);
    ASSERT_NE(members, nullptr);

    EXPECT_EQ(members, Scope::members(record->type));
    EXPECT_NE(members->find_local_function("answer"), nullptr);
}
