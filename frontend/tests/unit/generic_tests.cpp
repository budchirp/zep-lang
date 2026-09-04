#include <gtest/gtest.h>
#include <string>

import zep.frontend.test.harness;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;
import zep.common.source.span;
import zep.frontend.sema.context;
import zep.frontend.sema.constant.environment;
import zep.frontend.sema.constant.evaluator;
import zep.frontend.sema.kind;
import zep.frontend.node;

TEST(FrontendTypeCheckerPositive, AcceptsGenericFunctionsStructsAndNamedGenericArguments) {
    const std::string source = R"zep(
struct Pair<A, B> {
    public:
        var first: A
        var second: B
}

fn first<A, B>(pair: Pair<A, B>) -> A {
    return pair.first
}

fn main() -> i32 {
    var pair = Pair<A: i32, B: cstr> { first: 1, second: "zep" }
    return first<A: i32, B: cstr>(pair)
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsConstGenericsAndGenericArraySizes) {
    const std::string source = R"zep(
struct Buffer<const N: i32> {
    public:
        var values: i32[N]
}

fn length<const N: i32>() -> i32 {
    var values: i32[N]
    return #length(values) as i32
}

fn main() -> i32 {
    return length<4>() + (#sizeof(Buffer<4>) as i32)
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendConstGenerics, EvaluatesDependentArrayExtentsAfterSubstitution) {
    FrontendHarness harness(R"zep(
struct Buffer<const N: i32> { public: var values: i32[N + 1] }
type Three = Buffer<2>
type Five = Buffer<4>
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto* three =
        harness.program.statements[1]->as<TypeAliasDeclaration>()->type->as<StructType>();
    const auto* five =
        harness.program.statements[2]->as<TypeAliasDeclaration>()->type->as<StructType>();
    ASSERT_NE(three, nullptr);
    ASSERT_NE(five, nullptr);
    EXPECT_EQ(std::get<ConcreteArrayExtent>(three->fields[0].type->as<ArrayType>()->extent).value,
              3U);
    EXPECT_EQ(std::get<ConcreteArrayExtent>(five->fields[0].type->as<ArrayType>()->extent).value,
              5U);
}

TEST(FrontendConstGenerics, StoresGenericDeclarationOnAstIdentifiers) {
    FrontendHarness harness(R"zep(
struct Buffer<const N: i32> { public: var values: i32[N] }
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto* declaration = harness.program.statements[0]->as<StructDeclaration>();
    ASSERT_NE(declaration, nullptr);
    ASSERT_EQ(declaration->fields.size(), 1U);
    ASSERT_EQ(declaration->fields[0]->type->array_sizes.size(), 1U);
    const auto* identifier =
        declaration->fields[0]->type->array_sizes[0]->as<IdentifierExpression>();
    ASSERT_NE(identifier, nullptr);

    EXPECT_EQ(identifier->generic_declaration, declaration->generic_parameters[0]);
}

TEST(FrontendConstGenerics, RejectsRuntimeAndNegativeArrayExtents) {
    EXPECT_TRUE(assert_type_check_error("fn main(size: i32) -> void { var values: i32[size] }\n"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
struct Buffer<const N: i32> { public: var values: i32[N - 1] }
type Invalid = Buffer<0>
)zep"));
}

TEST(FrontendConstGenerics, EvaluatesDependentSizeQueries) {
    FrontendHarness harness(R"zep(
struct Bytes<T> { public: var storage: u8[#sizeof(T)] }
type Word = Bytes<i32>
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto* word =
        harness.program.statements[1]->as<TypeAliasDeclaration>()->type->as<StructType>();
    ASSERT_NE(word, nullptr);
    EXPECT_EQ(std::get<ConcreteArrayExtent>(word->fields[0].type->as<ArrayType>()->extent).value,
              4U);
}

TEST(FrontendConstGenerics, ReportsSpecializedOverflowAtTheDependentExpression) {
    FrontendHarness harness(R"zep(
struct Buffer<const N: i32> { public: var values: i32[N + 1] }
type Invalid = Buffer<2147483647>
)zep");
    harness.type_check();
    const auto* declaration = harness.program.statements[0]->as<StructDeclaration>();
    ASSERT_NE(declaration, nullptr);
    ASSERT_EQ(declaration->fields.size(), 1);
    ASSERT_EQ(declaration->fields[0]->type->array_sizes.size(), 1);
    const auto* expression = declaration->fields[0]->type->array_sizes[0];
    ASSERT_NE(expression, nullptr);

    EXPECT_TRUE(
        assert_diagnostic(harness, "integer overflow in constant expression", expression->span));
}

TEST(FrontendConstGenerics, NominalCacheDistinguishesDefinitionsWithTheSameName) {
    FrontendHarness harness("struct Wrapper<T> { public: var value: T }\n");
    ASSERT_TRUE(harness.type_check_succeeded());
    auto& sema = harness.sema;
    TypeResolver resolver(sema.types, sema.env, harness.context.diagnostics);
    const auto* integer = sema.builtin_resolver.primitives.at("i32");
    StructType first("Item", {}, {FieldType("first", integer)});
    StructType second("Item", {}, {FieldType("second", integer)});
    NamedType first_wrapper("Wrapper", {GenericArgumentType("", &first)});
    NamedType second_wrapper("Wrapper", {GenericArgumentType("", &second)});

    const auto* first_result = resolver.resolve_type(&first_wrapper)->as<StructType>();
    const auto* second_result = resolver.resolve_type(&second_wrapper)->as<StructType>();
    ASSERT_NE(first_result, nullptr);
    ASSERT_NE(second_result, nullptr);
    ASSERT_EQ(first_result->fields.size(), 1);
    ASSERT_EQ(second_result->fields.size(), 1);
    EXPECT_NE(first_result, second_result);
    EXPECT_EQ(first_result->fields[0].type, &first);
    EXPECT_EQ(second_result->fields[0].type, &second);
}

TEST(FrontendConstGenerics, NominalCachePreservesNamedArgumentBindings) {
    FrontendHarness harness(R"zep(
struct Pair<A, B> { public: var first: A var second: B }
type First = Pair<A: i32, B: cstr>
type Second = Pair<B: i32, A: cstr>
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    auto& sema = harness.sema;
    TypeResolver resolver(sema.types, sema.env, harness.context.diagnostics);
    NamedType first_alias("First", {});
    NamedType second_alias("Second", {});
    const auto* first = resolver.resolve_type(&first_alias)->as<StructType>();
    const auto* second = resolver.resolve_type(&second_alias)->as<StructType>();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(first->fields.size(), 2);
    ASSERT_EQ(second->fields.size(), 2);

    EXPECT_EQ(first->fields[0].type, sema.builtin_resolver.primitives.at("i32"));
    EXPECT_EQ(first->fields[1].type, sema.builtin_resolver.primitives.at("cstr"));
    EXPECT_EQ(second->fields[0].type, sema.builtin_resolver.primitives.at("cstr"));
    EXPECT_EQ(second->fields[1].type, sema.builtin_resolver.primitives.at("i32"));
}

TEST(FrontendConstGenerics, SubstitutionUsesDeclarationIdentityAcrossShadowing) {
    FrontendHarness harness("");
    auto& sema = harness.sema;
    TypeResolver resolver(sema.types, sema.env, harness.context.diagnostics);
    const auto* integer = sema.builtin_resolver.primitives.at("i32");
    const auto outer_identity = static_cast<const void*>(&resolver);
    const auto inner_identity = static_cast<const void*>(&integer);
    GenericParameterType outer(GenericParameterType::Kind::Type::Const, "N", integer,
                               outer_identity);
    GenericParameterType inner(GenericParameterType::Kind::Type::Const, "N", integer,
                               inner_identity);
    resolver.bind_generic_parameter(outer);
    const auto original = resolver.const_parameter("N");
    ASSERT_TRUE(original.has_value());
    ASSERT_FALSE(original->is_concrete());

    {
        auto scope = resolver.create_substitution_scope();
        resolver.bind_generic_parameter(inner);
        resolver.bind_generic_binding(
            "N",
            ConstBinding(CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, integer,
                                          std::int64_t{5})),
            inner_identity);
        const auto resolved = resolver.resolve_binding(*original);
        EXPECT_FALSE(std::get<ConstBinding>(resolved).is_concrete());
        const auto current = resolver.const_parameter("N");
        ASSERT_TRUE(current.has_value());
        ASSERT_TRUE(current->is_concrete());
        EXPECT_EQ(*current->value, CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger,
                                                    integer, std::int64_t{5}));
    }

    EXPECT_EQ(resolver.const_parameter("N"), original);
}

TEST(FrontendConstGenerics, CanonicalizesNamedTypeArgumentOrder) {
    FrontendHarness harness(R"zep(
struct Pair<A, B> { public: var first: A var second: B }
type First = Pair<A: i32, B: cstr>
type Second = Pair<B: cstr, A: i32>
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    auto& sema = harness.sema;
    TypeResolver resolver(sema.types, sema.env, harness.context.diagnostics);
    NamedType first("First", {});
    NamedType second("Second", {});

    EXPECT_EQ(resolver.resolve_type(&first), resolver.resolve_type(&second));
}

TEST(FrontendConstGenerics, NormalizesConstArgumentPayloadsToTheDeclaredType) {
    FrontendHarness harness("struct Count<const N: i64> {}\ntype Three = Count<3>\n");
    ASSERT_TRUE(harness.type_check_succeeded());
    auto& sema = harness.sema;
    TypeResolver resolver(sema.types, sema.env, harness.context.diagnostics);
    NamedType alias("Three", {});
    const auto* type = resolver.resolve_type(&alias)->as<StructType>();
    ASSERT_NE(type, nullptr);
    ASSERT_EQ(type->generic_arguments.size(), 1);
    ASSERT_TRUE(type->generic_arguments[0].const_binding.has_value());
    const auto& binding = *type->generic_arguments[0].const_binding;
    ASSERT_TRUE(binding.is_concrete());

    EXPECT_EQ(*binding.value,
              CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger,
                               sema.builtin_resolver.primitives.at("i64"), std::int64_t{3}));
}

TEST(FrontendConstGenerics, RejectsTypeAndConstArgumentCategoryMismatches) {
    EXPECT_TRUE(assert_type_check_error("struct Box<T> {}\ntype Invalid = Box<3>\n"));
    EXPECT_TRUE(
        assert_type_check_error("struct Count<const N: i32> {}\ntype Invalid = Count<i32>\n"));
    EXPECT_TRUE(
        assert_type_check_error("struct Count<const N: u8> {}\ntype Invalid = Count<256>\n"));
}

TEST(FrontendConstGenerics, PreservesExpressionsInNestedTypeArguments) {
    FrontendHarness harness(R"zep(
struct Buffer<const N: i64> { public: var values: i32[N] }
struct Nested<const N: i64> { public: var buffer: Buffer<(N + 1)> }
type Three = Nested<2>
type Pointer = *Buffer<1 + 2>
type Callback = (Buffer<1 + 2>) -> Buffer<2 + 2>
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto* nested_declaration = harness.program.statements[1]->as<StructDeclaration>();
    ASSERT_NE(nested_declaration, nullptr);
    ASSERT_EQ(nested_declaration->fields.size(), 1U);
    const auto* nested_argument = nested_declaration->fields[0]->type->generic_arguments[0];
    ASSERT_TRUE(nested_argument->const_binding.has_value());
    ASSERT_FALSE(nested_argument->const_binding->value.has_value());
    const auto* expression = static_cast<const Expression*>(nested_argument->const_binding->source);
    ASSERT_NE(expression->as<BinaryExpression>(), nullptr);
    CompileTimeEnvironment environment;
    environment.bind("N",
                     GenericBinding(ConstBinding(CompileTimeValue(
                         CompileTimeValue::Kind::Type::SignedInteger,
                         harness.sema.builtin_resolver.primitives.at("i64"), std::int64_t{2}))),
                     nested_declaration->generic_parameters[0]);
    Evaluator evaluator(harness.context.diagnostics, &harness.sema.env, &environment, true);
    const auto direct_value = evaluator.evaluate_uncached(*const_cast<Expression*>(expression));
    ASSERT_TRUE(direct_value.has_value());
    EXPECT_EQ(*direct_value, CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger,
                                              harness.sema.builtin_resolver.primitives.at("i64"),
                                              std::int64_t{3}));
    const auto* nested_alias = harness.program.statements[2]->as<TypeAliasDeclaration>();
    ASSERT_NE(nested_alias, nullptr);
    const auto* nested = nested_alias->type->as<StructType>();
    ASSERT_NE(nested, nullptr);
    ASSERT_EQ(nested->fields.size(), 1U);
    const auto* nested_buffer = nested->fields[0].type->as<StructType>();
    ASSERT_NE(nested_buffer, nullptr);
    ASSERT_EQ(nested_buffer->fields.size(), 1U);
    const auto* nested_values = nested_buffer->fields[0].type->as<ArrayType>();
    ASSERT_NE(nested_values, nullptr);
    ASSERT_TRUE(std::holds_alternative<ConcreteArrayExtent>(nested_values->extent));
    EXPECT_EQ(std::get<ConcreteArrayExtent>(nested_values->extent).value, 3U);
    const auto* pointer_alias = harness.program.statements[3]->as<TypeAliasDeclaration>();
    ASSERT_NE(pointer_alias, nullptr);
    const auto* pointer = pointer_alias->type->as<PointerType>();
    ASSERT_NE(pointer, nullptr);
    const auto* buffer = pointer->element->as<StructType>();
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(std::get<ConcreteArrayExtent>(buffer->fields[0].type->as<ArrayType>()->extent).value,
              3);
    const auto* callback_alias = harness.program.statements[4]->as<TypeAliasDeclaration>();
    ASSERT_NE(callback_alias, nullptr);
    const auto* callback = callback_alias->type->as<FunctionType>();
    ASSERT_NE(callback, nullptr);
    const auto* result = callback->return_type->as<StructType>();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(std::get<ConcreteArrayExtent>(result->fields[0].type->as<ArrayType>()->extent).value,
              4);
}

TEST(FrontendConstGenerics, PreservesUnsignedTypeArgumentsAboveSignedRange) {
    FrontendHarness harness(
        "struct Count<const N: u64> {}\ntype Maximum = Count<18446744073709551615>\n");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto* declaration = harness.program.statements[1]->as<TypeAliasDeclaration>();
    ASSERT_NE(declaration, nullptr);
    const auto* type = declaration->type->as<StructType>();
    ASSERT_NE(type, nullptr);
    ASSERT_TRUE(type->generic_arguments[0].const_binding.has_value());
    const auto& binding = *type->generic_arguments[0].const_binding;
    ASSERT_TRUE(binding.is_concrete());
    EXPECT_EQ(*binding.value, CompileTimeValue(CompileTimeValue::Kind::Type::UnsignedInteger,
                                               harness.sema.builtin_resolver.primitives.at("u64"),
                                               static_cast<std::uint64_t>(UINT64_MAX)));
}

TEST(FrontendConstGenerics, ReportsTypeArgumentOverflowAtItsSource) {
    FrontendHarness harness("struct Count<const N: u8> {}\ntype Invalid = Count<256>\n");
    harness.type_check();
    const auto* declaration = harness.program.statements[1]->as<TypeAliasDeclaration>();
    ASSERT_NE(declaration, nullptr);
    ASSERT_EQ(declaration->target->generic_arguments.size(), 1);
    EXPECT_TRUE(assert_diagnostic(harness, "constant integer is out of range for 'u8'",
                                  declaration->target->generic_arguments[0]->span));
}

TEST(FrontendConstGenerics, DiagnosesOversizedArrayLiteralsWithoutParserConversion) {
    EXPECT_TRUE(
        assert_type_check_error("fn check() -> void { var values: i32[18446744073709551616] }\n"));
}
