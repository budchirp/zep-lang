#include <gtest/gtest.h>
#include <stdexcept>

import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.compiler.lowering.mangler;
import zep.compiler.lowering.specialization;
import zep.frontend.test.harness;
import zep.frontend.node.program;

class MonomorphizerTest : public testing::Test {
  protected:
    SemaContext sema;

    const Type* i32_type() const { return sema.primitives.entries.at("i32"); }

    const Type* u32_type() const { return sema.primitives.entries.at("u32"); }

    const Type* f32_type() const { return sema.primitives.entries.at("f32"); }

    const Type* boolean_type() const { return sema.primitives.entries.at("boolean"); }

    const Type* void_type() const { return sema.primitives.entries.at("void"); }

    static std::size_t hash(const GenericBinding& binding) { return GenericBindingHash{}(binding); }
};

TEST_F(MonomorphizerTest, EncodesIntegerTypes) {
    EXPECT_EQ(Mangler::encode(i32_type()), "i32");
    EXPECT_EQ(Mangler::encode(u32_type()), "u32");
}

TEST_F(MonomorphizerTest, EncodesFloatType) {
    EXPECT_EQ(Mangler::encode(f32_type()), "f32");
}

TEST_F(MonomorphizerTest, EncodesExactTypedFloatPayloads) {
    const auto* wide = sema.primitives.entries.at("f64");
    NamedType positive("Value", {GenericArgumentType("", CompileTimeValue(0.0, wide))});
    NamedType negative("Value", {GenericArgumentType("", CompileTimeValue(-0.0, wide))});
    NamedType single("Value", {GenericArgumentType("", CompileTimeValue(0.0, f32_type()))});

    EXPECT_NE(Mangler::encode(&positive), Mangler::encode(&negative));
    EXPECT_NE(Mangler::encode(&positive), Mangler::encode(&single));
    EXPECT_EQ(Mangler::encode(&negative), "N5ValueG1_25_cf64_f9223372036854775808E");
}

TEST_F(MonomorphizerTest, DistinguishesNestedGenericArgumentBoundaries) {
    NamedType first("A", {});
    NamedType second("B", {});
    NamedType third("C", {});
    NamedType nested_first("A", {GenericArgumentType("", &second)});
    NamedType nested_second("B", {GenericArgumentType("", &third)});
    NamedType left("Pair", {GenericArgumentType("", &nested_first), GenericArgumentType("", &third)});
    NamedType right("Pair", {GenericArgumentType("", &first), GenericArgumentType("", &nested_second)});

    EXPECT_NE(Mangler::encode(&left), Mangler::encode(&right));
}

TEST_F(MonomorphizerTest, EncodesBooleanType) {
    EXPECT_EQ(Mangler::encode(boolean_type()), "boolean");
}

TEST_F(MonomorphizerTest, EncodesVoidType) {
    EXPECT_EQ(Mangler::encode(void_type()), "void");
}

TEST_F(MonomorphizerTest, EncodesPointerTypes) {
    PointerType mutable_i32(i32_type(), true);
    PointerType immutable_i32(i32_type(), false);

    EXPECT_EQ(Mangler::encode(&mutable_i32), "pi32");
    EXPECT_EQ(Mangler::encode(&immutable_i32), "Pi32");
}

TEST_F(MonomorphizerTest, EncodesArrayTypes) {
    ArrayType four_i32(i32_type(), static_cast<std::size_t>(4));

    EXPECT_EQ(Mangler::encode(&four_i32), "A4i32");
}

TEST_F(MonomorphizerTest, EncodesNamedTypes) {
    NamedType option("Option", std::vector<GenericArgumentType>());

    EXPECT_EQ(Mangler::encode(&option), "N6Option");
}

TEST_F(MonomorphizerTest, ManglesFunctionWithEmptyParameters) {
    EXPECT_EQ(Mangler::mangle("main", {}), "main");
}

TEST_F(MonomorphizerTest, ManglesFunctionWithParameters) {
    std::vector<GenericBinding> types = {TypeBinding(i32_type()), TypeBinding(u32_type())};

    EXPECT_EQ(Mangler::mangle("add", types), "add$i32_u32");
}

TEST_F(MonomorphizerTest, MangleParametersReturnsNameForEmptyList) {
    EXPECT_EQ(Mangler::mangle_parameters("main", {}), "main");
}

TEST_F(MonomorphizerTest, CacheDetectsFirstSpecializationAsNew) {
    Specializations cache;
    std::vector<GenericBinding> types = {TypeBinding(i32_type())};

    auto result = cache.get_or_create(i32_type(), "identity", types);

    EXPECT_FALSE(result.is_generated);
    EXPECT_EQ(result.name, "identity$i32");
}

TEST_F(MonomorphizerTest, CacheDetectsDuplicateSpecialization) {
    Specializations cache;
    std::vector<GenericBinding> types = {TypeBinding(i32_type())};

    cache.get_or_create(i32_type(), "identity", types);
    auto result = cache.get_or_create(i32_type(), "identity", types);

    EXPECT_TRUE(result.is_generated);
}

TEST_F(MonomorphizerTest, CacheRegistersAndRetrievesGenericFunctions) {
    Specializations cache;
    FrontendHarness source("fn generic<T>(value: T) -> T { return value }");
    ASSERT_TRUE(source.type_check_succeeded());
    auto* declaration = source.program.statements.front()->as<FunctionDeclaration>();
    ASSERT_NE(declaration, nullptr);

    cache.register_function("generic", declaration);

    EXPECT_EQ(cache.get_function("generic"), declaration);
    EXPECT_EQ(cache.get_struct("generic"), nullptr);
}

TEST_F(MonomorphizerTest, CacheReusesTheFirstNameForOneIdentity) {
    Specializations cache;
    NumberLiteral declaration(Span(), "1");

    auto first = cache.get_or_create(&declaration, "spec1", {});
    auto second = cache.get_or_create(&declaration, "spec2", {});

    EXPECT_FALSE(first.is_generated);
    EXPECT_TRUE(second.is_generated);
    EXPECT_EQ(first.name, second.name);
}

TEST_F(MonomorphizerTest, ConstBindingsDistinguishPayloadsAndTypes) {
    Specializations cache;
    const auto first =
        CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, i32_type(), std::int64_t{2});
    const auto second =
        CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, i32_type(), std::int64_t{5});
    const auto wide = CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger,
                                       sema.primitives.entries.at("i64"), std::int64_t{2});
    const auto first_result = cache.get_or_create(i32_type(), "count", {ConstBinding(first)});
    const auto second_result = cache.get_or_create(i32_type(), "count", {ConstBinding(second)});
    const auto wide_result = cache.get_or_create(i32_type(), "count", {ConstBinding(wide)});

    EXPECT_FALSE(first_result.is_generated);
    EXPECT_FALSE(second_result.is_generated);
    EXPECT_FALSE(wide_result.is_generated);
    EXPECT_NE(first_result.name, second_result.name);
    EXPECT_NE(first_result.name, wide_result.name);
    EXPECT_TRUE(cache.get_or_create(i32_type(), "count", {ConstBinding(first)}).is_generated);
}

TEST_F(MonomorphizerTest, ConcreteConstBindingsShareStableKeys) {
    const auto value =
        CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, i32_type(), std::int64_t{7});
    const GenericBinding first = ConstBinding(value);
    const GenericBinding second = ConstBinding(value);

    EXPECT_EQ(first, second);
    EXPECT_EQ(hash(first), hash(second));
    EXPECT_EQ(Mangler::encode(first), Mangler::encode(second));
}

TEST_F(MonomorphizerTest, UnevaluatedConstBindingsCannotBeMangled) {
    NumberLiteral first(Span(), "7");
    NumberLiteral second(Span(), "7");
    const GenericBinding left = ConstBinding::dependent_expression(&first, i32_type());
    const GenericBinding right = ConstBinding::dependent_expression(&second, i32_type());

    EXPECT_NE(left, right);
    EXPECT_NE(hash(left), hash(right));
    EXPECT_THROW(Mangler::encode(left), std::invalid_argument);
}

TEST_F(MonomorphizerTest, ConstBindingsEncodeCompleteAggregatePayloads) {
    ArrayType type(i32_type(), static_cast<std::size_t>(2));
    const auto one =
        CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, i32_type(), std::int64_t{1});
    const auto two =
        CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, i32_type(), std::int64_t{2});
    const auto first = CompileTimeValue(CompileTimeValue::Kind::Type::Array, &type,
                                        std::vector<CompileTimeValue>{one, two});
    const auto second = CompileTimeValue(CompileTimeValue::Kind::Type::Array, &type,
                                         std::vector<CompileTimeValue>{two, one});
    const GenericBinding first_binding = ConstBinding(first);
    const GenericBinding second_binding = ConstBinding(second);

    EXPECT_NE(first_binding, second_binding);
    EXPECT_NE(Mangler::encode(first_binding), Mangler::encode(second_binding));
    EXPECT_EQ(hash(first_binding), hash(GenericBinding(ConstBinding(first))));
}

TEST_F(MonomorphizerTest, CacheUsesDefiningIdentityInsteadOfLinkerName) {
    Specializations cache;
    const std::vector<GenericBinding> arguments = {TypeBinding(i32_type())};
    EXPECT_FALSE(cache.get_or_create(i32_type(), "same", arguments).is_generated);
    EXPECT_FALSE(cache.get_or_create(u32_type(), "same", arguments).is_generated);
    EXPECT_TRUE(cache.get_or_create(i32_type(), "same", arguments).is_generated);
}

TEST_F(MonomorphizerTest, EqualBindingsShareKeysAcrossTypeAllocations) {
    Specializations cache;
    IntegerType first(false, 32);
    IntegerType second(false, 32);
    const GenericBinding left = TypeBinding(&first);
    const GenericBinding right = TypeBinding(&second);

    EXPECT_EQ(left, right);
    EXPECT_EQ(hash(left), hash(right));
    EXPECT_FALSE(cache.get_or_create(i32_type(), "identity", {left}).is_generated);
    EXPECT_TRUE(cache.get_or_create(i32_type(), "identity", {right}).is_generated);
}

TEST_F(MonomorphizerTest, RenamedNominalsPreserveBindingIdentityAndHash) {
    StructType original("Owner", {}, {});
    StructType lowered("Owner$lowered", {}, {}, {}, nullptr, {}, {}, &original);
    PointerType source(&original, true);
    PointerType target(&lowered, true);
    Specializations cache;

    EXPECT_TRUE(source.same(&target));
    EXPECT_EQ(source.hash(), target.hash());
    EXPECT_FALSE(cache.get_or_create(i32_type(), "identity", {TypeBinding(&source)}).is_generated);
    EXPECT_TRUE(cache.get_or_create(i32_type(), "identity", {TypeBinding(&target)}).is_generated);
}

TEST_F(MonomorphizerTest, FunctionBindingsUseSignaturesInsteadOfDisplayNames) {
    FunctionType first("first", i32_type(), {ParameterType("source", i32_type())}, {}, false);
    FunctionType second("second", i32_type(), {ParameterType("value", i32_type())}, {}, false);
    FunctionType different("first", boolean_type(), {ParameterType("source", boolean_type())}, {}, false);
    Specializations cache;

    EXPECT_TRUE(first.same(&second));
    EXPECT_EQ(first.hash(), second.hash());
    EXPECT_EQ(Mangler::encode(&first), Mangler::encode(&second));
    EXPECT_NE(Mangler::encode(&first), Mangler::encode(&different));
    EXPECT_FALSE(cache.get_or_create(i32_type(), "identity", {TypeBinding(&first)}).is_generated);
    EXPECT_TRUE(cache.get_or_create(i32_type(), "identity", {TypeBinding(&second)}).is_generated);
    EXPECT_FALSE(cache.get_or_create(i32_type(), "identity", {TypeBinding(&different)}).is_generated);
}
