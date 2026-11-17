#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

import zep.frontend.sema.context;
import zep.frontend.sema.types;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.test.harness;

TEST(FrontendTypes, ChecksCompatibilityRules) {
    SemaContext sema;
    const auto* integer = sema.primitives.entries.at("i32");
    const auto* unsigned_integer = sema.primitives.entries.at("u32");

    PointerType mutable_integer(integer, true);
    PointerType immutable_integer(integer, false);
    PointerType immutable_void(sema.primitives.entries.at("void"), false);
    ArrayType integers(integer, static_cast<std::size_t>(4));
    ArrayType unsigned_integers(unsigned_integer, static_cast<std::size_t>(4));

    EXPECT_TRUE(integer->accepts(integer));
    EXPECT_FALSE(integer->accepts(unsigned_integer));
    EXPECT_TRUE(immutable_integer.accepts(&mutable_integer));
    EXPECT_FALSE(mutable_integer.accepts(&immutable_integer));
    EXPECT_TRUE(immutable_void.accepts(&mutable_integer));
    EXPECT_FALSE(integers.accepts(&unsigned_integers));

    NamedType parameter("T", {}, &mutable_integer);
    NamedType renamed("Renamed", {}, &mutable_integer);
    NamedType shadowed("T", {}, &immutable_integer);

    EXPECT_TRUE(parameter.accepts(&renamed));
    EXPECT_FALSE(parameter.accepts(&shadowed));
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

TEST(FrontendScopes, PreservesOverloadOrderVisibilityAndReferencesAcrossInsertions) {
    FrontendHarness harness("public fn choose(value: i32) -> i32 { return value }\n"
                            "fn choose(value: i64) -> i64 { return value }\n");

    ASSERT_TRUE(harness.type_check_succeeded());

    auto& root = *harness.sema.env.root_scope;
    const auto& overloads = root.lookup_function_overloads("choose");

    ASSERT_EQ(overloads.size(), 2U);
    EXPECT_EQ(overloads[0]->signature()->parameters[0].type,
              harness.sema.primitives.entries.at("i32"));
    EXPECT_EQ(overloads[1]->signature()->parameters[0].type,
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

TEST(FrontendPointers, PreservesArrayShapeBeforeSourceDecay) {
    SemaContext sema;
    const auto* integer = sema.primitives.entries.at("i32");
    ArrayType row(integer, static_cast<std::size_t>(3));
    ArrayType other_row(integer, static_cast<std::size_t>(4));
    ArrayType grid(&row, static_cast<std::size_t>(2));
    ArrayType other_grid(&other_row, static_cast<std::size_t>(2));
    ArrayType longer_grid(&row, static_cast<std::size_t>(3));
    PointerType elements(integer, true);
    PointerType rows(&row, true);
    PointerType read_only_rows(&row, false);
    PointerType other_rows(&other_row, true);
    PointerType grids(&grid, true);
    PointerType other_grids(&other_grid, true);
    PointerType longer_grids(&longer_grid, true);

    EXPECT_TRUE(rows.accepts(&rows));
    EXPECT_TRUE(grids.accepts(&grids));
    EXPECT_TRUE(read_only_rows.accepts(&rows));
    EXPECT_FALSE(rows.accepts(&read_only_rows));
    EXPECT_FALSE(rows.accepts(&other_rows));
    EXPECT_FALSE(grids.accepts(&other_grids));
    EXPECT_FALSE(grids.accepts(&longer_grids));
    EXPECT_TRUE(elements.accepts(&rows));
    EXPECT_TRUE(rows.accepts(&grids));
    EXPECT_FALSE(rows.accepts(&elements));
    EXPECT_FALSE(grids.accepts(&rows));
}

TEST(FrontendPointers, RequiresCastsFromVoidToTypedPointers) {
    SemaContext sema;
    const auto* integer = sema.primitives.entries.at("i32");
    const auto* void_type = sema.primitives.entries.at("void");
    PointerType elements(integer, true);
    PointerType read_only_elements(integer, false);
    PointerType storage(void_type, true);
    PointerType read_only_storage(void_type, false);

    EXPECT_TRUE(storage.accepts(&elements));
    EXPECT_TRUE(read_only_storage.accepts(&elements));
    EXPECT_FALSE(storage.accepts(&read_only_elements));
    EXPECT_FALSE(elements.accepts(&storage));
    EXPECT_FALSE(read_only_elements.accepts(&storage));
    EXPECT_FALSE(read_only_elements.accepts(&read_only_storage));
}

TEST(FrontendPointers, BindsArrayAliasesAcrossCallForms) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
type Row = i32[3]
type Alias = Row
type Grid = Row[2]

fn accept<T>(ptr: *mut T) -> void {}
fn read(ptr: *Alias) -> i32 { return ptr[0][0] }
fn grid(ptr: *Grid) -> i32 { return ptr[0][1][2] }
fn borrow<T>(value: &T) -> i64 { return #sizeof(T) }
fn check(ptr: *mut Row, rows: *mut Grid) -> i32 {
    accept(ptr)
    accept<Alias>(ptr)
    accept(ptr: rows)
    var callback = read
    var size = borrow(ptr[0])
    return callback(ptr) + grid(rows) + (size as i32)
}
)zep"));
}

TEST(FrontendPointers, RejectsInvalidArrayAndVoidArguments) {
    for (const auto* source : {
        "type Row = i32[3]\nfn take(ptr: *mut Row) -> void {}\n"
        "fn check(ptr: *mut i32) -> void { take(ptr) }",
        "type Row = i32[3]\nfn take(ptr: *mut Row) -> void {}\n"
        "fn check(ptr: *mut i32[4]) -> void { take(ptr) }",
        "type Grid = i32[3][2]\nfn take(ptr: *mut Grid) -> void {}\n"
        "fn check(ptr: *mut i32[4][2]) -> void { take(ptr) }",
        "type Row = i32[3]\nfn take<T>(ptr: *mut T) -> void {}\n"
        "fn check(ptr: *Row) -> void { take<Row>(ptr) }",
        "fn take(ptr: *i32) -> void {}\nfn check(ptr: *mut void) -> void { take(ptr) }",
        "fn take(value: cstr) -> void {}\nfn check(ptr: *mut i32[3]) -> void { take(ptr) }",
        "fn take(ptr: *mut char[3]) -> void {}\nfn check(value: cstr) -> void { take(value) }",
    }) {
        EXPECT_TRUE(assert_type_check_error(source)) << source;
    }
}

TEST(FrontendPointers, PreservesExplicitCastsAndByteArrayStrings) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
fn text(value: cstr) -> void {}
fn check(storage: *mut void, chars: *mut char[3], bytes: *mut u8[3]) -> void {
    var ptr = storage as *mut i32
    text(chars)
    text(bytes)
}
)zep"));
}

TEST(FrontendPointers, PrefersArrayPointeesOverDecayAndErasure) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
type Row = i32[3]
fn choose(ptr: *Row) -> i32 { return 1 }
fn choose(ptr: *mut Row) -> i32 { return 4 }
fn choose(ptr: *i32) -> i32 { return 2 }
fn choose(ptr: *void) -> i32 { return 3 }
fn check(ptr: *mut Row, read_only: *Row) -> i32 { return choose(ptr) + choose(read_only) }
)zep"));
}
