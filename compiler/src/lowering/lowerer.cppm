module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.compiler.lowering;

import zep.common.source.span;
import zep.common.context;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.context;
import zep.common.diagnostic.diagnostic;
import zep.common.diagnostic.collection;
import zep.frontend.sema.constants.evaluator;
import zep.frontend.sema.scopes.environment;
import zep.frontend.kind;
import zep.frontend.sema.declaration.attribute;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.checking.checker;
import zep.compiler.lowering.lifetime;
import zep.compiler.lowering.mangler;
import zep.compiler.lowering.specialization;
import zep.hir.node;
import zep.hir.program;
import zep.common.state;
import zep.frontend.sema.lifecycle;

export class Lowerer : public Visitor<HIRNode*> {
  private:
    class LoweringFailure {
      public:
        Span span;
        std::string message;

        LoweringFailure(Span span, std::string message) : span(span), message(std::move(message)) {}
    };

    SemaContext& sema;
    std::shared_ptr<HIRProgram> program;

    TypeResolver resolver;
    Facades facades;

    TemporaryNames names;
    Specializations specializations;
    Construction construction;
    Cleanup cleanup;
    Temporaries temporaries;

    std::unordered_map<const Type*, const Type*> lower_type_cache;
    std::unordered_map<const VariableSymbol*, std::pair<std::string, const PointerType*>>
        captured_variables;
    bool specializing = false;
    bool moving_receiver = false;
    bool consuming_block_tail = false;

    HIRCallExpression* make_call(CallExpression& node, std::unique_ptr<HIRCallTarget> target,
                                 std::vector<HIRExpression*> arguments);

    HIRCallExpression* bind_call(CallExpression& node, HIRExpression* callee,
                                 std::vector<HIRExpression*> arguments);

    HIRExpression* specialize_call(CallExpression& node, const std::string& name,
                                   const FunctionType* signature,
                                   const FunctionDeclaration* definition,
                                   const std::vector<GenericBinding>& bindings);

    HIRCallExpression* lower_captured_call(CallExpression& node, ClosureExpression& closure);

    HIRCallExpression* lower_call(CallExpression& node,
        std::optional<std::vector<HIRExpression*>> bound = std::nullopt);

    HIRExpression* lower_expression(Expression& node) {
        return static_cast<HIRExpression*>(visit_expression(node));
    }

    HIRStatement* lower_statement(Statement& node) {
        FullExpression expression(temporaries.current);
        StateGuard guard(temporaries.current, &expression);
        auto* statement = static_cast<HIRStatement*>(visit_statement(node));

        if (expression.declarations.empty()) {
            return statement;
        }

        auto* expression_statement = statement != nullptr ? statement->as<HIRExpressionStatement>() : nullptr;

        if (expression_statement != nullptr && expression_statement->expression != nullptr &&
            expression_statement->expression->type != nullptr &&
            !expression_statement->expression->type->is<VoidType>() &&
            !expression_statement->expression->type->is<NeverType>()) {
            auto* result = temporaries.complete(node.span, expression_statement->expression, expression);

            return program->nodes.create<HIRExpressionStatement>(node.span, result);
        }

        std::vector<HIRStatement*> statements;
        statements.reserve(expression.declarations.size() + expression.drops.size() + 1);
        statements.insert(statements.end(), expression.declarations.begin(), expression.declarations.end());

        if (statement != nullptr) {
            statements.push_back(statement);
        }

        if (node.as<ReturnStatement>() == nullptr && !terminates(statement)) {
            statements.insert(statements.end(), expression.drops.begin(), expression.drops.end());
        }

        return program->nodes.create<HIRStatementGroup>(node.span, std::move(statements));
    }

    HIRExpression* lower_condition(Expression& node) {
        FullExpression expression(temporaries.current);
        StateGuard guard(temporaries.current, &expression);
        auto* value = lower_expression(node);

        return temporaries.complete(node.span, value, expression);
    }

    HIRBlockStatement* lower_block(Statement& node) {
        return static_cast<HIRBlockStatement*>(lower_statement(node));
    }

    const Type* lower_type(const Type* type, Span span);

    const FunctionType* lower_function_type(const Type* type, Span span) {
        const auto* lowered = lower_type(type, span);

        return lowered != nullptr ? lowered->as<FunctionType>() : nullptr;
    }

    const Type* prepare_lifecycle(const Type* type, Span span, Operation::Type operation) {
        const auto* lowered = lower_type(type, span);
        const auto* structure = lowered != nullptr ? lowered->as<StructType>() : nullptr;
        if (structure == nullptr || structure->generic_arguments.empty()) {
            return lowered;
        }

        auto scope = resolver.create_substitution_scope();
        const auto arguments = lower_generic_arguments(structure->generic_arguments, span);
        const auto& parameters = structure->definition->generic_parameters;
        const auto count = std::min(parameters.size(), arguments.size());
        for (auto index = std::size_t{0}; index < count; ++index) {
            bind_parameter(parameters[index], arguments[index]);
        }

        resolver.bind_type_parameter(structure->name, structure);
        resolver.bind_type_parameter("Self", structure);
        const auto plan = sema.lifecycle.resolve(structure, operation, resolver);
        const auto* function = plan->function;
        const auto* definition = specializations.get_function(function);

        if (function == nullptr || sema.lifecycle.deleted(function) || definition == nullptr) {
            return lowered;
        }

        resolver.bind_type_parameter(function->parent, structure);
        const auto* signature = lower_function_type(function->signature(), span);

        if (signature == nullptr) {
            return lowered;
        }

        const auto instance = specializations.lifecycle(*function, *structure, signature);

        if (!instance.is_generated) {
            StateGuard function_guard(cleanup.scopes, std::vector<CleanupScope>());

            if (auto* specialization = specialize(*definition, instance.name, arguments);
                specialization != nullptr) {
                specializations.enqueue(specialization);
            }
        }

        return lowered;
    }

    static std::size_t parameter_offset(const FunctionType* function_type,
                                        std::size_t declared_count) {
        if (function_type == nullptr || function_type->parameters.size() <= declared_count) {
            return 0;
        }

        return function_type->parameters.size() - declared_count;
    }

    bool terminates(HIRStatement* statement) const {
        if (statement == nullptr) {
            return false;
        }

        if (statement->as<HIRReturnStatement>() != nullptr) {
            return true;
        }

        if (auto* group = statement->as<HIRStatementGroup>(); group != nullptr) {
            if (group->statements.empty()) {
                return false;
            }

            return terminates(group->statements.back());
        }

        auto* block = statement->as<HIRBlockStatement>();
        if (block == nullptr || block->statements.empty()) {
            return false;
        }

        return terminates(block->statements.back());
    }

    bool is_emittable_type(const Type* type, std::unordered_set<const Type*>& visited) {
        if (type == nullptr || !visited.insert(type).second) {
            return true;
        }

        if (const auto* named_type = type->as<NamedType>(); named_type != nullptr) {
            for (const auto& argument : named_type->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_emittable_type(argument.type, visited)) {
                    return false;
                }
            }

            if (named_type->name == "Self") {
                return true;
            }
        }

        const auto* resolved = resolver.resolve_type(type);
        if (resolved != nullptr && resolved != type) {
            return is_emittable_type(resolved, visited);
        }

        if (const auto* named_type = type->as<NamedType>(); named_type != nullptr) {
            return false;
        }

        if (const auto* pointer_type = type->as<PointerType>(); pointer_type != nullptr) {
            return is_emittable_type(pointer_type->element, visited);
        }

        if (const auto* array_type = type->as<ArrayType>(); array_type != nullptr) {
            return !std::holds_alternative<DependentArrayExtent>(array_type->extent) &&
                   is_emittable_type(array_type->element, visited);
        }

        if (const auto* function_type = type->as<FunctionType>(); function_type != nullptr) {
            return function_type->generic_parameters.empty();
        }

        if (const auto* nominal_type = type->as_nominal(); nominal_type != nullptr) {
            if (!nominal_type->generic_parameters.empty()) {
                return false;
            }

            for (const auto& argument : nominal_type->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_emittable_type(argument.type, visited)) {
                    return false;
                }
            }
        }

        if (const auto* struct_type = type->as<StructType>(); struct_type != nullptr) {
            for (const auto& field : struct_type->fields) {
                if (!is_emittable_type(field.type, visited)) {
                    return false;
                }
            }

            return true;
        }

        if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            if (!is_emittable_type(enum_type->backing_type, visited)) {
                return false;
            }

            for (const auto& variant : enum_type->variants) {
                for (const auto& field : variant.fields) {
                    if (!is_emittable_type(field.type, visited)) {
                        return false;
                    }
                }
            }

            return true;
        }

        if (const auto* interface_type = type->as<InterfaceType>(); interface_type != nullptr) {
            for (const auto* parent_interface : interface_type->interfaces) {
                if (!is_emittable_type(parent_interface, visited)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool is_emittable_function_type(const FunctionType* function_type) {
        if (function_type == nullptr || !function_type->generic_parameters.empty()) {
            return false;
        }

        std::unordered_set<const Type*> visited;
        if (!is_emittable_type(function_type->return_type, visited)) {
            return false;
        }

        for (const auto& parameter : function_type->parameters) {
            if (!is_emittable_type(parameter.type, visited)) {
                return false;
            }
        }

        return true;
    }

    void bind_parameter(const GenericParameterType& parameter, const GenericBinding& binding) {
        resolver.bind_generic_binding(parameter.name, binding, parameter.declaration);
    }

    bool is_concrete_binding(const GenericBinding& binding) const {
        const auto* type = std::get_if<TypeBinding>(&binding);
        if (type != nullptr) {
            return resolver.is_concrete(type->type);
        }

        return std::get<ConstBinding>(binding).value.has_value();
    }

    void apply_substitutions(const std::vector<GenericParameter*>& parameters,
                             const std::vector<GenericBinding>& arguments) {
        const auto count = std::min(parameters.size(), arguments.size());

        for (std::size_t index = 0; index < count; ++index) {
            resolver.bind_generic_binding(parameters[index]->name, arguments[index],
                                          parameters[index]);
        }
    }

    void apply_parent_substitutions(const std::string& parent,
                                    const std::vector<GenericBinding>& arguments) {
        if (parent.empty() || arguments.empty()) {
            return;
        }

        const auto* parent_symbol = sema.env.current_scope->lookup_type(parent);
        const auto* nominal =
            parent_symbol != nullptr ? parent_symbol->type->as_nominal() : nullptr;
        if (nominal == nullptr) {
            return;
        }

        const auto count = std::min(nominal->generic_parameters.size(), arguments.size());
        for (std::size_t index = 0; index < count; ++index) {
            bind_parameter(nominal->generic_parameters[index], arguments[index]);
        }
    }

    std::vector<GenericBinding>
    lower_generic_arguments(const std::vector<GenericArgumentType>& arguments, Span span) {
        std::vector<GenericBinding> bindings;
        bindings.reserve(arguments.size());

        for (const auto& argument : arguments) {
            if (argument.is_const()) {
                bindings.push_back(resolver.resolve_binding(argument.binding()));
            } else {
                bindings.emplace_back(TypeBinding(lower_type(argument.type, span)));
            }
        }

        return bindings;
    }

    std::vector<GenericBinding>
    lower_generic_arguments(const std::vector<GenericArgument*>& arguments, Span span) {
        std::vector<GenericBinding> bindings;
        bindings.reserve(arguments.size());

        for (const auto* argument : arguments) {
            if (argument->const_binding.has_value()) {
                bindings.push_back(resolver.resolve_binding(*argument->const_binding));
            } else {
                bindings.emplace_back(TypeBinding(lower_type(argument->type->type, span)));
            }
        }

        return bindings;
    }

    const Type* lower_enum_type(const EnumType* type, Span span);

    const Type* lower_interface_type(const InterfaceType* type, Span span);

    const Type* lower_struct_type(const Type* source, const StructType* type, Span span);

    std::string type_name(const NominalType& type) const;

    HIRFunctionDeclaration* specialize(const FunctionDeclaration& declaration,
                                       const std::string& mangled_name,
                                       const std::vector<GenericBinding>& argument_types) {
        auto scope = resolver.create_substitution_scope();

        const auto* declared_function_type =
            declaration.type != nullptr ? declaration.type->as<FunctionType>() : nullptr;
        if (declared_function_type != nullptr) {
            const auto count =
                std::min(declared_function_type->generic_parameters.size(), argument_types.size());

            for (std::size_t index = 0; index < count; ++index) {
                bind_parameter(declared_function_type->generic_parameters[index],
                               argument_types[index]);
            }
        }

        const auto variadic = declaration.prototype->is_variadic;
        const auto* return_type =
            lower_type(declaration.prototype->return_type->type, declaration.span);

        const auto* type_base = lower_type(declaration.type, declaration.span);
        const auto* function_type = type_base != nullptr ? type_base->as<FunctionType>() : nullptr;

        if (function_type == nullptr) {
            return nullptr;
        }

        std::vector<ParameterType> parameters;
        parameters.reserve(function_type->parameters.size());

        for (const auto& parameter : function_type->parameters) {
            parameters.emplace_back(parameter.name, lower_type(parameter.type, declaration.span));
        }

        const auto* mangled_type =
            sema.types.create<FunctionType>(mangled_name, return_type, std::move(parameters),
                                            std::vector<GenericParameterType>(), variadic);
        if (!is_emittable_function_type(mangled_type)) {
            return nullptr;
        }

        auto mangled = Mangler::is_mangled(declaration.function_symbol);
        ScopeGuard function_scope(sema.env.current_scope, sema.env.scopes,
                                  Scope::Kind::Type::Function, declaration.prototype->name);

        auto offset = parameter_offset(mangled_type, declaration.prototype->parameters.size());

        if (offset > 0) {
            const auto& self = mangled_type->parameters.front();
            sema.env.current_scope->define_var(
                self.name, sema.env.symbols.create<VariableSymbol>(
                               self.name, declaration.prototype->span, Visibility::Type::Private,
                               StorageKind::Type::Var, self.type));
        }

        for (std::size_t index = 0; index < declaration.prototype->parameters.size(); ++index) {
            auto* parameter = declaration.prototype->parameters[index];
            const auto parameter_index = index + offset;
            const auto* parameter_type = parameter_index < mangled_type->parameters.size()
                                             ? mangled_type->parameters[parameter_index].type
                                             : nullptr;

            sema.env.current_scope->define_var(
                parameter->name, sema.env.symbols.create<VariableSymbol>(
                                     parameter->name, parameter->span, Visibility::Type::Private,
                                     StorageKind::Type::Var, parameter_type));
        }

        StateGuard specialization_guard(specializing, true);

        auto* body = build_defaulted_constructor(declaration, mangled_type);
        if (body == nullptr) {
            body = build_owned_body(declaration.prototype->parameters, mangled_type, *declaration.body, 0,
                declaration.prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::Move);
        }

        return program->nodes.create<HIRFunctionDeclaration>(
            declaration.span, declaration.visibility, Linkage::Type::LinkOnceODR, mangled_name,
            return_type, body, variadic, mangled_type, mangled, Abi::Type::Language);
    }

    HIRExpression* lower_compile_time_value(Span span, CompileTimeValue value) {
        const auto* type = lower_type(value.type, span);

        switch (value.kind) {
        case CompileTimeValue::Kind::Type::SignedInteger:
            return program->nodes.create<HIRNumberLiteral>(
                span, std::to_string(std::get<std::int64_t>(value.payload)), type);

        case CompileTimeValue::Kind::Type::UnsignedInteger:
            return program->nodes.create<HIRNumberLiteral>(
                span, std::to_string(std::get<std::uint64_t>(value.payload)), type);

        case CompileTimeValue::Kind::Type::Float:
            return program->nodes.create<HIRFloatLiteral>(span, value.to_string(), type);

        case CompileTimeValue::Kind::Type::Boolean:
            return program->nodes.create<HIRBooleanLiteral>(
                span, std::get<bool>(value.payload), type);

        case CompileTimeValue::Kind::Type::Char:
            return program->nodes.create<HIRCharLiteral>(
                span, std::get<std::uint8_t>(value.payload), type);

        case CompileTimeValue::Kind::Type::String:
            return program->nodes.create<HIRStringLiteral>(
                span, std::get<std::string>(value.payload), type);

        case CompileTimeValue::Kind::Type::Null:
            return program->nodes.create<HIRNullLiteral>(span, type);

        case CompileTimeValue::Kind::Type::Struct: {
            const auto* structure = type != nullptr ? type->as<StructType>() : nullptr;
            const auto& values = std::get<std::vector<CompileTimeValue>>(value.payload);
            if (structure == nullptr || structure->fields.size() != values.size()) {
                throw LoweringFailure(span, "constant struct requires concrete field types");
            }

            std::vector<HIRStructLiteralField> fields;
            fields.reserve(values.size());
            for (auto index = std::size_t{0}; index < values.size(); ++index) {
                auto* expression = lower_compile_time_value(span, values[index]);
                if (expression == nullptr) {
                    throw LoweringFailure(
                        span, "constant struct field cannot be represented at runtime");
                }
                fields.emplace_back(structure->fields[index].name, expression);
            }
            return program->nodes.create<HIRStructLiteralExpression>(
                span, structure->name, std::move(fields), type);
        }
        case CompileTimeValue::Kind::Type::Array: {
            const auto& elements = std::get<std::vector<CompileTimeValue>>(value.payload);
            const auto* array = type != nullptr ? type->as<ArrayType>() : nullptr;
            if (array == nullptr || array->element == nullptr) {
                throw LoweringFailure(span, "constant array requires a concrete element type");
            }
            if (!array->element->is<StringType>()) {
                std::vector<HIRExpression*> expressions;
                expressions.reserve(elements.size());
                for (const auto& element : elements) {
                    auto* expression = lower_compile_time_value(span, element);
                    if (expression == nullptr) {
                        throw LoweringFailure(
                            span, "constant array element cannot be represented at runtime");
                    }
                    expressions.push_back(expression);
                }
                return program->nodes.create<HIRArrayLiteralExpression>(
                    span, std::move(expressions), type);
            }
            std::vector<std::string> strings;
            strings.reserve(elements.size());
            for (const auto& element : elements) {
                if (element.kind != CompileTimeValue::Kind::Type::String) {
                    throw LoweringFailure(
                        span, "constant array element cannot be represented at runtime");
                }
                strings.push_back(std::get<std::string>(element.payload));
            }
            return program->nodes.create<HIRStringArrayExpression>(span, std::move(strings),
                                                                           type);
        }
        }

        return nullptr;
    }

    std::vector<HIRStructLiteralField>
    lower_fields(std::vector<StructLiteralField*>& fields) {
        std::vector<HIRStructLiteralField> result;
        result.reserve(fields.size());

        for (auto* field : fields) {
            result.emplace_back(field->name, lower_expression(*field->value));
        }

        return result;
    }

    void define_parameters(FunctionPrototype& prototype, const FunctionType* function_type) {
        auto offset = parameter_offset(function_type, prototype.parameters.size());
        if (offset > 0) {
            const auto& self = function_type->parameters.front();
            sema.env.current_scope->define_var(self.name, sema.env.symbols.create<VariableSymbol>(
                                                              self.name, prototype.span,
                                                              Visibility::Type::Private,
                                                              StorageKind::Type::Var, self.type));
        }

        for (auto* parameter : prototype.parameters) {
            const auto* parameter_type = resolver.resolve_type(parameter->type->type);
            sema.env.current_scope->define_var(
                parameter->name, sema.env.symbols.create<VariableSymbol>(
                                     parameter->name, prototype.span, Visibility::Type::Private,
                                     StorageKind::Type::Var, parameter_type));
        }
    }

    HIRBlockStatement* build_defaulted_constructor(const FunctionDeclaration& declaration, const FunctionType* type) {
        const auto* symbol = declaration.function_symbol;
        if (symbol == nullptr || !symbol->has_attribute(".defaulted_constructor")) {
            return nullptr;
        }
        const auto& parameter = type->parameters.front();
        const auto* pointer_type = parameter.type->as<PointerType>();
        auto* pointer = program->nodes.create<HIRIdentifierExpression>(declaration.span, parameter.name, parameter.type);
        auto* source = program->nodes.create<HIRUnaryExpression>(declaration.span, UnaryOperator::Type::Dereference,
            pointer, pointer_type->element);
        const auto operation = symbol->has_role(ConstructorKind::Type::Move)
            ? Operation::Type::Move : Operation::Type::Copy;
        auto* result = construction.construct(declaration.span, source,
            sema.lifecycle.resolve(type->return_type, operation, resolver, LifecycleMode::Type::Defaulted));
        std::vector<HIRStatement*> statements;
        statements.reserve(1);
        statements.push_back(program->nodes.create<HIRReturnStatement>(declaration.span, result));
        return program->nodes.create<HIRBlockStatement>(declaration.span, std::move(statements));
    }

    HIRBlockStatement* build_owned_body(const std::vector<Parameter*>& source_parameters,
                                        const FunctionType* function_type, BlockStatement& body,
                                        std::size_t hidden_parameter_count = 0, bool moves_receiver = false) {
        StateGuard function_guard(cleanup.scopes, std::vector<CleanupScope>());
        StateGuard expression_guard(temporaries.current, static_cast<FullExpression*>(nullptr));
        StateGuard receiver_guard(moving_receiver, moves_receiver);
        StateGuard tail_guard(consuming_block_tail, false);
        CleanupScopeGuard scope_guard(cleanup);

        std::vector<HIRStatement*> parameter_flags;
        parameter_flags.reserve(source_parameters.size());

        auto offset = parameter_offset(function_type, source_parameters.size());
        if (offset < hidden_parameter_count) {
            return nullptr;
        }

        for (std::size_t i = 0; i < source_parameters.size(); ++i) {
            auto* parameter = source_parameters[i];
            const auto parameter_index = i + offset;
            const auto* lowered_type =
                function_type != nullptr && parameter_index < function_type->parameters.size()
                    ? function_type->parameters[parameter_index].type
                    : nullptr;

            if (auto* flag = cleanup.register_value(parameter->name, parameter->name,
                                                     parameter->span, lowered_type);
                flag != nullptr) {
                parameter_flags.push_back(flag);
            }
        }

        auto* lowered_body = static_cast<HIRBlockStatement*>(lower_statement(body));
        lowered_body->statements.insert(lowered_body->statements.begin(), parameter_flags.begin(),
                                        parameter_flags.end());

        if (!terminates(lowered_body)) {
            auto parameter_destruction = cleanup.emit();
            lowered_body->statements.insert(lowered_body->statements.end(),
                                            parameter_destruction.begin(),
                                            parameter_destruction.end());
        }

        return lowered_body;
    }

    HIRNode* lower_identifier(IdentifierExpression& node) {
        const auto* var_symbol = node.var_symbol != nullptr
                                     ? node.var_symbol
                                     : sema.env.current_scope->lookup_var(node.name);

        const auto* reference = var_symbol != nullptr ? var_symbol->type->as<PointerType>() : nullptr;
        if (reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            const auto* lowered_reference = lower_type(reference, node.span)->as<PointerType>();
            auto* pointer = program->nodes.create<HIRIdentifierExpression>(node.span, node.name, lowered_reference, var_symbol);
            return program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, pointer, lowered_reference->element);
        }

        if (auto iterator = captured_variables.find(var_symbol);
            iterator != captured_variables.end()) {
            const auto& [name, pointer_type] = iterator->second;
            auto* pointer = program->nodes.create<HIRIdentifierExpression>(node.span, name,
                                                                                   pointer_type);

            return program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, pointer, pointer_type->element);
        }

        const auto* source_type =
            var_symbol != nullptr && var_symbol->type != nullptr ? var_symbol->type : node.type;
        const auto* type = lower_type(source_type, node.span);
        auto name = node.name;

        if (var_symbol != nullptr) {
            name = Mangler::linker_name(node.name, var_symbol);
        } else if (type != nullptr) {
            const auto* function_type = type->as<FunctionType>();

            if (function_type != nullptr) {
                const auto* symbol = node.function_symbol;

                if (symbol == nullptr) {
                    throw LoweringFailure(node.span, "function value has no resolved semantic symbol");
                }

                name = Mangler::function_name(symbol->base_name(), symbol, function_type);

                register_function_symbol(
                    name, symbol->visibility,
                    Linkage::Type::External, function_type, node.span, symbol,
                    symbol->abi);
            }
        }

        return program->nodes.create<HIRIdentifierExpression>(
            node.span, name, type, var_symbol, node.function_symbol);
    }

    void register_type_symbol(const std::string& name, Visibility::Type visibility,
                              const Type* type, Span span) {
        if (sema.env.root_scope->lookup_type(name) != nullptr) {
            return;
        }

        auto* symbol = sema.env.symbols.create<TypeSymbol>(name, span, visibility, type);
        sema.env.root_scope->define_type(name, symbol);
    }

    void register_function_symbol(const std::string& name, Visibility::Type visibility,
                                  Linkage::Type linkage, const Type* type, Span span,
                                  const FunctionSymbol* source_symbol = nullptr,
                                  Abi::Type abi = Abi::Type::Language) {
        if (sema.env.root_scope->lookup_function(name) != nullptr) {
            return;
        }

        auto role = source_symbol != nullptr ? source_symbol->role
                                                      : FunctionSymbol::Role(CallableKind::Type::Function);
        auto parent = source_symbol != nullptr ? source_symbol->parent : std::string();

        auto attributes =
            source_symbol != nullptr ? source_symbol->attributes : std::vector<AttributeInfo>();

        auto* symbol = sema.env.symbols.create<FunctionSymbol>(
            name, span, visibility, linkage, type, role, parent, std::move(attributes), abi,
            source_symbol != nullptr && source_symbol->is_deleted);
        sema.env.root_scope->define_function(name, symbol);
    }

    void register_var_symbol(const std::string& name, Visibility::Type visibility,
                             StorageKind::Type storage_kind, const Type* type, Span span,
                             std::vector<AttributeInfo> attributes = {}) {
        if (sema.env.root_scope->lookup_var(name) != nullptr) {
            return;
        }

        auto* symbol = sema.env.symbols.create<VariableSymbol>(name, span, visibility, storage_kind,
                                                               type, std::move(attributes));
        sema.env.root_scope->define_var(name, symbol);
    }

    template <typename Declaration>
    void register_nominal(Declaration& node) {
        if (!node.generic_parameters.empty()) {
            if constexpr (std::is_same_v<Declaration, StructDeclaration>) {
                specializations.register_struct(node.name, &node);
            }

            return;
        }

        const auto* lowered_type = lower_type(node.type, node.span);
        if (!resolver.is_concrete(lowered_type)) {
            return;
        }
        const auto* symbol = sema.env.current_scope->lookup_type(node.name);

        register_type_symbol(Mangler::linker_name(node.name, symbol), node.visibility, lowered_type,
                             node.span);
    }

    void register_function(FunctionDeclaration& node) {
        if (!node.prototype->generic_parameters.empty()) {
            auto key = node.function_symbol != nullptr ? node.function_symbol->base_name()
                                                       : node.prototype->name;
            specializations.register_function(key, &node, node.function_symbol);
            return;
        }

        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* function_type =
            lowered_type != nullptr ? lowered_type->as<FunctionType>() : nullptr;
        if (function_type == nullptr) {
            return;
        }

        if (!is_emittable_function_type(function_type)) {
            auto key = node.function_symbol != nullptr ? node.function_symbol->base_name()
                                                       : node.prototype->name;
            specializations.register_function(key, &node, node.function_symbol);
            return;
        }

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, function_type);
        auto linkage = Mangler::function_linkage(base_name, symbol);

        register_function_symbol(name, node.visibility, linkage, function_type, node.span, symbol);
    }

    void register_extern_function(ExternFunctionDeclaration& node) {
        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* function_type =
            lowered_type != nullptr ? lowered_type->as<FunctionType>() : nullptr;
        if (function_type == nullptr) {
            return;
        }
        if (!is_emittable_function_type(function_type)) {
            return;
        }

        const auto* symbol = node.function_symbol;
        auto name = Mangler::function_name(node.prototype->name, symbol, function_type);

        register_function_symbol(name, node.visibility, Linkage::Type::External, function_type,
                                 node.span, symbol, Abi::Type::C);
    }

    void register_variable(VarDeclaration& node) {
        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* symbol = node.variable_symbol;
        auto attributes = symbol != nullptr ? symbol->attributes : std::vector<AttributeInfo>();

        register_var_symbol(Mangler::linker_name(node.name, symbol), node.visibility,
                            node.storage_kind, lowered_type, node.span, std::move(attributes));
    }

    void register_extern_variable(ExternVarDeclaration& node) {
        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* symbol = node.variable_symbol;
        auto attributes = symbol != nullptr ? symbol->attributes : std::vector<AttributeInfo>();

        register_var_symbol(Mangler::linker_name(node.name, symbol), node.visibility,
                            StorageKind::Type::Var, lowered_type, node.span, std::move(attributes));
    }

    void register_program_symbols(Program& ast_program) {
        for (auto& statement : ast_program.statements) {
            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                register_nominal(*struct_declaration);

                for (auto* method : struct_declaration->methods) {
                    register_function(*method);
                }
            }

            if (auto* interface_declaration = statement->as<InterfaceDeclaration>();
                interface_declaration != nullptr) {
                register_nominal(*interface_declaration);
            }

            if (auto* enum_declaration = statement->as<EnumDeclaration>();
                enum_declaration != nullptr) {
                register_nominal(*enum_declaration);

                for (auto* method : enum_declaration->methods) {
                    register_function(*method);
                }
            }

            if (auto* function_declaration = statement->as<FunctionDeclaration>();
                function_declaration != nullptr) {
                register_function(*function_declaration);
            }

            if (auto* extern_function_declaration = statement->as<ExternFunctionDeclaration>();
                extern_function_declaration != nullptr) {
                register_extern_function(*extern_function_declaration);
            }

            if (auto* variable_declaration = statement->as<VarDeclaration>();
                variable_declaration != nullptr) {
                register_variable(*variable_declaration);
            }

            if (auto* extern_variable_declaration = statement->as<ExternVarDeclaration>();
                extern_variable_declaration != nullptr) {
                register_extern_variable(*extern_variable_declaration);
            }
        }
    }

    void drain_specializations() {
        while (true) {
            std::vector<HIRFunctionDeclaration*> pending;
            specializations.drain(pending);

            if (pending.empty()) {
                break;
            }

            for (auto* function : pending) {
                register_function_symbol(function->name, function->visibility, function->linkage,
                                         function->type, function->span);
                program->statements.push_back(function);
            }
        }
    }

  public:
    using Visitor<HIRNode*>::visit;
    Diagnostics& diagnostics;

    explicit Lowerer(Context& context, SemaContext& sema)
        : sema(sema), program(std::make_shared<HIRProgram>()),
          resolver(sema.types, sema.env, context.diagnostics),
          facades(sema.facades, resolver),
          construction(*program, sema, resolver, names, specializations,
              [this](const Type* type, Span span, Operation::Type operation) -> const Type* {
                  return prepare_lifecycle(type, span, operation);
              }),
          cleanup(*program, sema, resolver, construction, names),
          temporaries(*program, sema, cleanup, names),
          diagnostics(context.diagnostics) {}

    HIRNode* visit_expression(Expression& node) {
        return Visitor<HIRNode*>::visit_expression(node);
    }

    HIRNode* visit_statement(Statement& node) { return Visitor<HIRNode*>::visit_statement(node); }

    HIRNode* visit(TypeExpression& node) override {
        const auto* lowered = lower_type(node.type, node.span);

        return program->nodes.create<HIRTypeExpression>(node.span, lowered);
    }

    HIRNode* visit(NumberLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRNumberLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(FloatLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRFloatLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(StringLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRStringLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(CharLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRCharLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(BooleanLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRBooleanLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(NullLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->nodes.create<HIRNullLiteral>(node.span, type);
    }

    HIRNode* visit(IdentifierExpression& node) override {
        if (node.compile_time_value.has_value()) {
            if (auto* lowered = lower_compile_time_value(node.span, *node.compile_time_value);
                lowered != nullptr) {
                return lowered;
            }
        }

        if (node.var_symbol == nullptr) {
            const auto binding = resolver.const_parameter(node.name);
            if (binding.has_value()) {
                if (!binding->value.has_value()) {
                    throw LoweringFailure(
                        node.span, "runtime expression requires a concrete const generic argument");
                }
                return lower_compile_time_value(node.span, *binding->value);
            }
        }

        if (node.implicit_self_field) {
            const auto* self_symbol = sema.env.current_scope->lookup_var("self");
            const auto* self_type =
                self_symbol != nullptr ? lower_type(self_symbol->type, node.span) : nullptr;
            const auto* pointer_type =
                self_type != nullptr ? self_type->as<PointerType>() : nullptr;
            const auto* object_type = pointer_type != nullptr ? pointer_type->element : nullptr;

            auto* self = program->nodes.create<HIRIdentifierExpression>(node.span, "self",
                                                                                self_type);
            auto* object = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, self, object_type);

            if (node.name == "value" && object_type != nullptr && !object_type->is<StructType>()) {
                return object;
            }

            return program->nodes.create<HIRMemberExpression>(
                node.span, object, node.name, lower_type(node.type, node.span));
        }

        return lower_identifier(node);
    }

    HIRNode* visit(BinaryExpression& node) override {
        auto* left = lower_expression(*node.left);
        auto* right = lower_expression(*node.right);
        const auto* type = node.op == BinaryExpression::Operator::Type::As && right != nullptr &&
                                   right->as<HIRTypeExpression>() != nullptr
                               ? right->as<HIRTypeExpression>()->type_value
                               : lower_type(node.type, node.span);

        return program->nodes.create<HIRBinaryExpression>(
            node.span, left, node.op, right, type);
    }

    HIRNode* visit(UnaryExpression& node) override {
        auto* operand = lower_expression(*node.operand);

        if (node.op == UnaryOperator::Type::AddressOf || node.op == UnaryOperator::Type::AddressOfMut) {
            operand = temporaries.capture(operand, node.span);
        }
        if (operand == nullptr) {
            return nullptr;
        }

        const auto* pointer_type =
            operand->type != nullptr ? operand->type->as<PointerType>() : nullptr;
        const auto* type =
            node.op == UnaryExpression::Operator::Type::Dereference && pointer_type != nullptr
                ? pointer_type->element
                : lower_type(node.type, node.span);

        return program->nodes.create<HIRUnaryExpression>(
            node.span, node.op, operand, type);
    }

    HIRNode* visit(CoerceExpression& node) override {
        auto* value = lower_expression(*node.value);
        const auto* type = lower_type(node.type, node.span);
        if (node.coercion == Coercion::Type::BaseSlice) {
            value = temporaries.capture(value, node.span);
            const auto* source_pointer_type = sema.types.create<PointerType>(value->type, false);
            const auto* pointer_type = sema.types.create<PointerType>(type, false);
            auto* address = program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::AddressOf,
                value, source_pointer_type);
            auto* pointer = program->nodes.create<HIRCoerceExpression>(node.span, address, pointer_type,
                Coercion::Type::None, source_pointer_type);
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, pointer, type);
        }
        if (node.coercion == Coercion::Type::CopyValue || node.coercion == Coercion::Type::MoveValue) {
            if (node.operation == nullptr) {
                throw LoweringFailure(node.span, "construction has no resolved lifecycle plan");
            }

            return construction.construct(node.span, value, node.operation);
        }
        const auto* source_type = lower_type(node.source_type, node.span);

        return program->nodes.create<HIRCoerceExpression>(node.span, value, type,
                                                                  node.coercion, source_type);
    }

    HIRNode* visit(BuiltinCall& node) override {
        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }

        if (node.name == "sizeof") {
            if (node.type_argument == nullptr || node.type_argument->type == nullptr) {
                return nullptr;
            }

            const auto* sized_type = lower_type(node.type_argument->source_type, node.span);
            auto layout = TypeLayout().resolve(sized_type);
            if (layout.state != TypeLayout::Result::State::Concrete &&
                node.type_argument->type != node.type_argument->source_type) {
                sized_type = lower_type(node.type_argument->type, node.span);
                layout = TypeLayout().resolve(sized_type);
            }

            if (layout.state != TypeLayout::Result::State::Concrete) {
                throw LoweringFailure(node.span, "sizeof requires a concrete sized type");
            }
            const auto* type = lower_type(node.type, node.span);

            return program->nodes.create<HIRNumberLiteral>(node.span, std::to_string(layout.size),
                                                                   type);
        }

        if (node.name == "length") {
            Evaluator evaluator(diagnostics, &sema.env, &resolver.substitutions);
            const auto result = evaluator.evaluate(node);

            return result.value.has_value() ? lower_compile_time_value(node.span, *result.value)
                                            : nullptr;
        }

        return nullptr;
    }

    HIRNode* visit(AssemblyExpression& node) override {
        std::vector<HIRExpression*> arguments;
        arguments.reserve(node.arguments.size());

        for (auto* argument : node.arguments) {
            arguments.push_back(lower_expression(*argument));
        }

        return program->nodes.create<HIRAssemblyExpression>(
            node.span, node.assembly, std::move(arguments), lower_type(node.type, node.span));
    }

    HIRNode* visit(CallExpression& node) override {
        if (node.destruction != nullptr) {
            const auto* member = node.callee->as<MemberExpression>();
            auto* value = lower_expression(*member->value);
            const auto* identifier = member->value->as<IdentifierExpression>();
            auto* action = cleanup.destroy(node.span,
                identifier != nullptr ? identifier->name : std::string(), value,
                lower_type(member->value->type, node.span), node.destruction);

            return program->nodes.create<HIRBlockExpression>(node.span,
                program->nodes.create<HIRBlockStatement>(node.span, std::vector<HIRStatement*>{action}),
                sema.primitives.entries.at("void"));
        }

        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }
        auto* call = lower_call(node);
        if (node.reference_result != nullptr && call != nullptr) {
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, call,
                lower_type(node.type, node.span));
        }
        return call;
    }

    HIRNode* visit(IndexExpression& node) override {
        auto* value = lower_expression(*node.value);
        value = temporaries.capture(value, node.span);
        auto* index = lower_expression(*node.index);
        const Type* type = nullptr;

        if (const auto* array_type =
                value != nullptr && value->type != nullptr ? value->type->as<ArrayType>() : nullptr;
            array_type != nullptr) {
            type = array_type->element;
        } else if (const auto* pointer_type = value != nullptr && value->type != nullptr
                                                  ? value->type->as<PointerType>()
                                                  : nullptr;
                   pointer_type != nullptr) {
            type = pointer_type->element;
        } else if (value != nullptr && value->type != nullptr && value->type->is<StringType>()) {
            type = sema.primitives.entries.at("char");
        } else {
            type = lower_type(node.type, node.span);
        }

        auto* result = program->nodes.create<HIRIndexExpression>(node.span, value, index, type);
        if (const auto* reference = type->as<PointerType>(); reference != nullptr &&
            reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, result, reference->element);
        }
        return result;
    }

    HIRNode* visit(MemberExpression& node) override {
        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }
        auto* value = lower_expression(*node.value);
        value = temporaries.capture(value, node.span);
        const Type* type = nullptr;

        const auto* lowered_value_type = value != nullptr ? value->type : nullptr;
        if (const auto* pointer_type =
                lowered_value_type != nullptr ? lowered_value_type->as<PointerType>() : nullptr;
            pointer_type != nullptr) {
            lowered_value_type = pointer_type->element;
        }

        if (const auto* struct_type =
                lowered_value_type != nullptr ? lowered_value_type->as<StructType>() : nullptr;
            struct_type != nullptr) {
            if (const auto* field = struct_type->find_field(node.member); field != nullptr) {
                type = field->type;
            }
        }

        if (type == nullptr) {
            type = lower_type(node.type, node.span);
        }

        const auto* enum_type =
            node.value->type != nullptr ? node.value->type->as<EnumType>() : nullptr;
        if (enum_type != nullptr && enum_type->backing_type != nullptr && node.member == "value") {
            return program->nodes.create<HIRCoerceExpression>(
                node.span, value, type, Coercion::Type::None,
                lower_type(node.value->type, node.span));
        }

        if (node.member == "value" && facades.value(node.value->type) != nullptr) {
            return value;
        }

        auto* result = program->nodes.create<HIRMemberExpression>(node.span, value, node.member, type);
        if (const auto* reference = type->as<PointerType>(); reference != nullptr &&
            reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            return program->nodes.create<HIRUnaryExpression>(node.span, UnaryOperator::Type::Dereference, result, reference->element);
        }
        return result;
    }

    HIRNode* visit(QualifiedAccessExpression& node) override {
        if (node.variant_type != nullptr) {
            const auto* type = lower_type(node.type, node.span);
            const auto* enum_type = type != nullptr ? type->as<EnumType>() : nullptr;

            if (enum_type == nullptr) {
                return nullptr;
            }

            if (enum_type->backing_type != nullptr) {
                if (!node.enum_value.has_value()) {
                    return nullptr;
                }

                auto* value = lower_compile_time_value(node.span, *node.enum_value);
                return program->nodes.create<HIRCoerceExpression>(
                    node.span, value, type, Coercion::Type::None,
                    lower_type(node.enum_value->type, node.span));
            }

            return program->nodes.create<HIREnumVariantExpression>(
                node.span, enum_type->name, node.member, std::vector<HIRStructLiteralField>{},
                node.variant_type->index, type);
        }

        if (node.function_symbol == nullptr) {
            return nullptr;
        }

        const auto* type = lower_type(node.type, node.span);
        auto name = Mangler::function_name(node.function_symbol->base_name(), node.function_symbol,
                                           type != nullptr ? type->as<FunctionType>() : nullptr);

        register_function_symbol(name, node.function_symbol->visibility, Linkage::Type::External,
                                 type, node.span, node.function_symbol, node.function_symbol->abi);

        return program->nodes.create<HIRIdentifierExpression>(node.span, name, type, nullptr,
                                                              node.function_symbol);
    }

    HIRNode* visit(NewExpression& node) override {
        if (node.storage == nullptr && node.allocation == nullptr) {
            throw LoweringFailure(node.span, "new is missing its resolved allocation");
        }

        auto* pointer = lower_expression(node.storage != nullptr ? *node.storage : *node.allocation);
        const auto* pointer_type = lower_type(node.type, node.span)->as<PointerType>();
        pointer = program->nodes.create<HIRCoerceExpression>(
            node.span, pointer, pointer_type, Coercion::Type::None, pointer->type);

        auto name = names.next("placement");
        auto* captured = program->nodes.create<HIRIdentifierExpression>(node.span, name, pointer_type);
        auto* target = program->nodes.create<HIRUnaryExpression>(
            node.span, UnaryOperator::Type::Dereference, captured, pointer_type->element);

        std::vector<HIRStatement*> statements;
        statements.reserve(3);
        statements.push_back(program->nodes.create<HIRVarDeclaration>(node.span,
            Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
            name, pointer_type, pointer, false));

        auto* value = lower_expression(*node.value);
        auto* assignment = program->nodes.create<HIRAssignExpression>(
            node.span, target, value, pointer_type->element);
        statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, assignment));
        statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, captured));

        return program->nodes.create<HIRBlockExpression>(node.span,
            program->nodes.create<HIRBlockStatement>(node.span, std::move(statements)), pointer_type);
    }

    HIRNode* visit(DeleteExpression& node) override {
        const auto* void_type = sema.primitives.entries.at("void");
        std::vector<HIRStatement*> statements;
        statements.reserve(2);

        if (node.value->as<NullLiteral>() == nullptr) {
            if (node.release == nullptr || node.operation == nullptr) {
                throw LoweringFailure(node.span, "delete is missing its resolved memory operation");
            }

            auto* value = lower_expression(*node.value);
            const auto* pointer_type = value->type->as<PointerType>();
            auto name = names.next("delete");
            auto* pointer = program->nodes.create<HIRIdentifierExpression>(node.span, name, pointer_type);
            statements.push_back(program->nodes.create<HIRVarDeclaration>(node.span,
                Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                name, pointer_type, value, false));

            auto* target = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, pointer, pointer_type->element);
            std::vector<HIRStatement*> actions;
            construction.destroy(node.span, target, node.operation, actions);

            auto* release = lower_call(*node.release, std::vector<HIRExpression*>{pointer});
            actions.push_back(program->nodes.create<HIRExpressionStatement>(node.span, release));

            auto* null = program->nodes.create<HIRNullLiteral>(node.span, pointer_type);
            auto* condition = program->nodes.create<HIRBinaryExpression>(node.span,
                pointer, BinaryOperator::Type::NotEquals, null, sema.primitives.entries.at("boolean"));
            auto* body = program->nodes.create<HIRBlockStatement>(node.span, std::move(actions));
            auto* branch = program->nodes.create<HIRIfExpression>(node.span, condition, body, nullptr, void_type);
            statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, branch));
        }

        return program->nodes.create<HIRBlockExpression>(node.span,
            program->nodes.create<HIRBlockStatement>(node.span, std::move(statements)), void_type);
    }

    HIRNode* visit(AssignExpression& node) override {

        if (node.resolved_assignment != nullptr) {
            return visit(*node.resolved_assignment);
        }
        auto* target = lower_expression(*node.target);
        auto* value = lower_expression(*node.value);
        const auto* type = lower_type(node.type, node.span);

        if (target != nullptr && type != nullptr &&
            sema.lifecycle.needs_drop(type, resolver)) {
            const auto* pointer_type = sema.types.create<PointerType>(type, true);
            const auto pointer_name = names.next("return_tmp");
            const auto value_name = names.next("return_tmp");
            auto* address = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::AddressOfMut, target, pointer_type);
            auto* pointer = program->nodes.create<HIRIdentifierExpression>(node.span, pointer_name, pointer_type);
            auto* destination = program->nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, pointer, type);
            auto* replacement = program->nodes.create<HIRIdentifierExpression>(node.span, value_name, type);
            std::vector<HIRStatement*> statements;
            statements.reserve(6);
            statements.push_back(program->nodes.create<HIRVarDeclaration>(
                node.span, Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                pointer_name, pointer_type, address, false));
            statements.push_back(program->nodes.create<HIRVarDeclaration>(
                node.span, Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                value_name, type, value, false));

            std::vector<HIRStatement*> drops;
            construction.destroy(node.span, destination,
                sema.lifecycle.resolve(type, Operation::Type::Destroy, resolver), drops);
            auto* drop_body = program->nodes.create<HIRBlockStatement>(node.span, std::move(drops));
            auto flag_name = std::string();
            if (const auto* identifier = node.target->as<IdentifierExpression>(); identifier != nullptr) {
                flag_name = cleanup.flag(identifier->name);
            }

            const auto* boolean_type = sema.primitives.entries.at("boolean");
            if (flag_name.empty()) {
                statements.push_back(drop_body);
            } else {
                auto* condition = program->nodes.create<HIRIdentifierExpression>(node.span, flag_name, boolean_type);
                auto* drop = program->nodes.create<HIRIfExpression>(
                    node.span, condition, drop_body, nullptr, sema.primitives.entries.at("void"));
                statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, drop));
            }

            auto* assignment = program->nodes.create<HIRAssignExpression>(node.span, destination, replacement, type);
            statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, assignment));
            if (!flag_name.empty()) {
                auto* flag = program->nodes.create<HIRIdentifierExpression>(node.span, flag_name, boolean_type);
                auto* initialized = program->nodes.create<HIRBooleanLiteral>(node.span, true, boolean_type);
                auto* rearm = program->nodes.create<HIRAssignExpression>(node.span, flag, initialized, boolean_type);
                statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, rearm));
            }

            statements.push_back(program->nodes.create<HIRExpressionStatement>(node.span, destination));
            return program->nodes.create<HIRBlockExpression>(
                node.span, program->nodes.create<HIRBlockStatement>(node.span, std::move(statements)), type);
        }

        return program->nodes.create<HIRAssignExpression>(node.span, target, value, type);
    }

    HIRNode* visit(StructLiteralExpression& node) override {
        if (facades.backing(node.type) != nullptr) {
            for (auto* field : node.fields) {
                if (field->name == "value") {
                    return lower_expression(*field->value);
                }
            }
        }

        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            auto* qualified = node.name->as<QualifiedAccessExpression>();
            if (qualified == nullptr || qualified->variant_type == nullptr) {
                return nullptr;
            }

            auto fields = lower_fields(node.fields);

            return program->nodes.create<HIREnumVariantExpression>(
                node.span, enum_type->name, qualified->member, std::move(fields),
                qualified->variant_type->index, type);
        }

        const auto* struct_type = type->as<StructType>();
        if (struct_type == nullptr) {
            return nullptr;
        }

        auto fields = lower_fields(node.fields);

        return program->nodes.create<HIRStructLiteralExpression>(
            node.span, struct_type->name, std::move(fields), type);
    }

    HIRNode* visit(EnumVariantExpression& node) override {
        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        const auto* enum_type = type->as<EnumType>();
        if (enum_type == nullptr || node.variant_type == nullptr) {
            return nullptr;
        }

        auto fields = lower_fields(node.fields);

        return program->nodes.create<HIREnumVariantExpression>(
            node.span, enum_type->name, node.variant_name, std::move(fields),
            node.variant_type->index, type);
    }

    HIRNode* visit(IfExpression& node) override {
        auto* condition = lower_condition(*node.condition);
        StateGuard tail_guard(consuming_block_tail, true);
        auto* then_branch = lower_statement(*node.then_branch);
        auto* else_branch =
            node.else_branch != nullptr ? lower_statement(*node.else_branch) : nullptr;
        const auto* type = lower_type(node.type, node.span);

        return program->nodes.create<HIRIfExpression>(node.span, condition, then_branch,
                                                              else_branch, type);
    }

    HIRNode* visit(WhenArm& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(WhenExpression& node) override {
        StateGuard tail_guard(consuming_block_tail, true);
        auto* subject = node.subject != nullptr ? lower_expression(*node.subject) : nullptr;
        subject = temporaries.capture(subject, node.span);
        const auto* type = lower_type(node.type, node.span);

        std::vector<HIRWhenArm> arms;
        arms.reserve(node.arms.size());

        for (auto* arm : node.arms) {
            ScopeGuard arm_scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block,
                                 "when_arm");

            std::vector<HIRWhenPattern> patterns;
            patterns.reserve(arm->patterns.size());

            for (auto* pattern : arm->patterns) {
                if (pattern->pattern_kind == WhenPattern::PatternKind::Type::Expression) {
                    patterns.emplace_back(lower_expression(*pattern->expression));
                    continue;
                }

                std::vector<HIRWhenPatternField> fields;
                fields.reserve(pattern->fields.size());

                if (pattern->enum_type != nullptr && pattern->variant_type != nullptr) {
                    for (auto* field : pattern->fields) {
                        const auto* declared_field =
                            pattern->variant_type->find_field(field->field_name);
                        const auto* field_type = declared_field != nullptr
                                                     ? lower_type(declared_field->type, field->span)
                                                     : nullptr;

                        sema.env.current_scope->define_var(field->binding_name,
                                                           sema.env.symbols.create<VariableSymbol>(
                                                               field->binding_name, field->span,
                                                               Visibility::Type::Private,
                                                               StorageKind::Type::Var, field_type));

                        fields.emplace_back(field->field_name, field->binding_name, field_type);
                    }
                }

                const auto variant_index =
                    pattern->variant_type != nullptr ? pattern->variant_type->index : 0;
                const auto enum_name =
                    pattern->enum_type != nullptr ? pattern->enum_type->name : std::string();
                HIRExpression* variant_value = nullptr;
                if (pattern->enum_type != nullptr && pattern->enum_type->backing_type != nullptr &&
                    pattern->variant_type != nullptr) {
                    if (pattern->variant_type->discriminant.has_value()) {
                        variant_value = lower_compile_time_value(
                            pattern->span, *pattern->variant_type->discriminant);
                    }
                }

                patterns.emplace_back(enum_name, pattern->variant_name, variant_index,
                                      variant_value, std::move(fields));
            }

            auto* guard = arm->guard != nullptr ? lower_expression(*arm->guard) : nullptr;
            auto* body = lower_statement(*arm->body);

            arms.emplace_back(arm->is_else, std::move(patterns), guard, body);
        }

        auto* result = program->nodes.create<HIRWhenExpression>(node.span, subject, std::move(arms),
                                                               node.is_exhaustive, type);
        result->borrow_payloads = moving_receiver;
        return result;
    }

    HIRNode* visit(WhileStatement& node) override {
        auto* condition = lower_condition(*node.condition);
        auto* body = lower_statement(*node.body);
        const auto* type = lower_type(node.type, node.span);

        return program->nodes.create<HIRLoopStatement>(
            node.span, std::vector<HIRStatement*>{}, condition, nullptr, body, type);
    }

    HIRNode* visit(ForStatement& node) override {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block, "for");

        std::vector<HIRStatement*> initializers;
        initializers.reserve(1);

        if (auto* initializer = lower_statement(*node.initializer); initializer != nullptr) {
            initializers.push_back(initializer);
        }

        auto* condition = lower_condition(*node.condition);
        auto* step = node.step != nullptr ? lower_condition(*node.step) : nullptr;
        auto* body = lower_statement(*node.body);
        const auto* type = lower_type(node.type, node.span);

        return program->nodes.create<HIRLoopStatement>(node.span, std::move(initializers),
                                                               condition, step, body, type);
    }

    HIRNode* visit(ClosureExpression& node) override {
        auto name = names.next("closure");
        const auto* type_base = lower_type(node.type, node.span);
        const auto* type = type_base != nullptr ? type_base->as<FunctionType>() : nullptr;

        if (type == nullptr) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         "closure");

        for (std::size_t i = 0; i < node.parameters.size(); ++i) {
            sema.env.current_scope->define_var(
                node.parameters[i]->name,
                sema.env.symbols.create<VariableSymbol>(
                    node.parameters[i]->name, node.span, Visibility::Type::Private,
                    StorageKind::Type::Var, type->parameters[i].type));
        }

        auto* body = build_owned_body(node.parameters, type, *node.body);

        auto* function = program->nodes.create<HIRFunctionDeclaration>(
            node.span, Visibility::Type::Private, Linkage::Type::Internal, name, type->return_type,
            body, false, type, false);

        program->statements.push_back(function);

        return program->nodes.create<HIRIdentifierExpression>(node.span, name, type);
    }

    HIRNode* visit(BlockExpression& node) override {
        StateGuard tail_guard(consuming_block_tail, true);
        auto* body = lower_block(*node.body);
        return program->nodes.create<HIRBlockExpression>(node.span, body,
                                                                 lower_type(node.type, node.span));
    }

    HIRNode* visit(ArrayLiteralExpression& node) override {
        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        std::vector<HIRExpression*> elements;
        elements.reserve(node.elements.size());
        for (auto* element : node.elements) {
            auto* lowered = lower_expression(*element);
            if (lowered == nullptr) {
                return nullptr;
            }

            elements.push_back(lowered);
        }

        return program->nodes.create<HIRArrayLiteralExpression>(node.span,
                                                                        std::move(elements), type);
    }

    HIRNode* visit(BlockStatement& node) override {
        const auto consumes_tail = consuming_block_tail;
        StateGuard tail_guard(consuming_block_tail, false);
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block,
                         "block");
        CleanupScopeGuard cleanup_guard(cleanup);

        std::vector<HIRStatement*> body;
        body.reserve(node.statements.size());
        auto terminated = false;

        for (auto index = std::size_t{0}; index < node.statements.size(); ++index) {
            auto* statement = node.statements[index];
            if (auto* lowered = lower_statement(*statement); lowered != nullptr) {
                const auto* source_expression = statement->as<ExpressionStatement>();
                const auto* source_call = source_expression != nullptr ? source_expression->expression->as<CallExpression>() : nullptr;
                const auto owns_result = source_expression != nullptr &&
                    ((source_call != nullptr && source_call->reference_result == nullptr) ||
                     source_expression->expression->as<BlockExpression>() != nullptr ||
                     source_expression->expression->as<IfExpression>() != nullptr ||
                     source_expression->expression->as<WhenExpression>() != nullptr ||
                     source_expression->expression->as<StructLiteralExpression>() != nullptr ||
                     source_expression->expression->as<EnumVariantExpression>() != nullptr ||
                     source_expression->expression->as<CoerceExpression>() != nullptr ||
                     source_expression->expression->as<ArrayLiteralExpression>() != nullptr);
                if (auto* expression_statement = lowered->as<HIRExpressionStatement>();
                    (index + 1 < node.statements.size() || !consumes_tail) && owns_result && expression_statement != nullptr &&
                    expression_statement->expression != nullptr &&
                    sema.lifecycle.needs_drop(expression_statement->expression->type,
                                                    resolver)) {
                    auto* expression = expression_statement->expression;
                    auto name = names.next("return_tmp");
                    auto* temporary = program->nodes.create<HIRVarDeclaration>(
                        statement->span, Visibility::Type::Private, Linkage::Type::Internal,
                        StorageKind::Type::Var, name, expression->type, expression, false);
                    auto* value = program->nodes.create<HIRIdentifierExpression>(
                        statement->span, name, expression->type);
                    std::vector<HIRStatement*> statements;
                    statements.reserve(2);
                    statements.push_back(temporary);
                    construction.destroy(statement->span, value,
                        sema.lifecycle.resolve(expression->type, Operation::Type::Destroy, resolver), statements);
                    lowered = program->nodes.create<HIRStatementGroup>(
                        statement->span, std::move(statements));
                }

                body.push_back(lowered);

                if (terminates(lowered)) {
                    terminated = true;
                    break;
                }
            }
        }

        if (!terminated) {
            auto scope_destruction = cleanup.emit();
            HIRExpression* tail_value = nullptr;
            if (consumes_tail && !body.empty() && !scope_destruction.empty()) {
                auto* tail = body.back()->as<HIRExpressionStatement>();
                if (tail != nullptr && tail->expression != nullptr && tail->expression->type != nullptr &&
                    !tail->expression->type->is<VoidType>() && !tail->expression->type->is<NeverType>()) {
                    auto name = names.next("return_tmp");
                    auto* expression = tail->expression;
                    body.back() = program->nodes.create<HIRVarDeclaration>(node.span,
                        Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
                        name, expression->type, expression, false);
                    tail_value = program->nodes.create<HIRIdentifierExpression>(node.span, name, expression->type);
                }
            }
            body.insert(body.end(), scope_destruction.begin(), scope_destruction.end());
            if (tail_value != nullptr) {
                body.push_back(program->nodes.create<HIRExpressionStatement>(node.span, tail_value));
            }
        }

        auto* result = program->nodes.create<HIRBlockStatement>(node.span, std::move(body));
        result->type = lower_type(node.type, node.span);
        return result;
    }

    HIRNode* visit(ExpressionStatement& node) override {
        auto* expression = lower_expression(*node.expression);

        return program->nodes.create<HIRExpressionStatement>(node.span, expression);
    }

    HIRNode* visit(ReturnStatement& node) override {
        auto* value = node.value != nullptr ? lower_expression(*node.value) : nullptr;

        std::vector<HIRStatement*> cleanup_statements;
        cleanup_statements.reserve(8);
        for (auto* expression = temporaries.current; expression != nullptr; expression = expression->parent) {
            cleanup_statements.insert(cleanup_statements.end(), expression->drops.begin(), expression->drops.end());
        }
        auto scope_cleanup = cleanup.emit(Cleanup::Extent::Type::Function);
        cleanup_statements.insert(cleanup_statements.end(), scope_cleanup.begin(), scope_cleanup.end());
        if (cleanup_statements.empty()) {
            return program->nodes.create<HIRReturnStatement>(node.span, value);
        }

        std::vector<HIRStatement*> statements;
        statements.reserve(cleanup_statements.size() + 2);
        value = temporaries.finish(node.span, value, statements, cleanup_statements);
        statements.push_back(program->nodes.create<HIRReturnStatement>(node.span, value));

        return program->nodes.create<HIRBlockStatement>(node.span, std::move(statements));
    }

    HIRNode* visit(DeferStatement& node) override {
        auto* body = lower_statement(*node.body);
        cleanup.defer(body);

        return nullptr;
    }

    HIRNode* visit(VarDeclaration& node) override {
        const auto original_drop_count = temporaries.current != nullptr ? temporaries.current->drops.size() : 0;
        HIRExpression* initializer = nullptr;
        if (node.initializer != nullptr) {
            if (sema.env.current_scope->is_global() &&
                node.initializer->compile_time_value.has_value()) {
                initializer = lower_compile_time_value(
                    node.span, *node.initializer->compile_time_value);
            } else {
                initializer = lower_expression(*node.initializer);
            }
        }
        const auto* source_type = node.type != nullptr     ? node.type
                                  : initializer != nullptr ? initializer->type
                                                           : nullptr;
        const auto* type = lower_type(source_type, node.span);
        const auto* reference = type != nullptr ? type->as<PointerType>() : nullptr;
        if (temporaries.current != nullptr && reference != nullptr &&
            reference->reference_kind != PointerType::ReferenceKind::Type::None) {
            const auto added_count = temporaries.current->drops.size() - original_drop_count;
            for (auto index = added_count; index > 0; --index) {
                cleanup.defer(temporaries.current->drops[index - 1]);
            }
            temporaries.current->drops.erase(temporaries.current->drops.begin(), temporaries.current->drops.begin() + static_cast<std::ptrdiff_t>(added_count));
        }

        const auto* symbol = node.variable_symbol;
        auto name = Mangler::linker_name(node.name, symbol);

        auto* declaration = program->nodes.create<HIRVarDeclaration>(
            node.span, node.visibility, Linkage::Type::Internal, node.storage_kind, std::move(name),
            type, initializer, sema.env.current_scope->is_global(),
            AttributeResolver::convert(node.attributes));

        if ((!sema.env.current_scope->is_global() || specializing) &&
            !sema.env.current_scope->has_local_var(node.name)) {
            sema.env.current_scope->define_var(
                node.name, sema.env.symbols.create<VariableSymbol>(
                               node.name, node.span, node.visibility, node.storage_kind, type));
        }

        if (auto* flag = cleanup.register_value(node.name, declaration->name, node.span, type); flag != nullptr) {
            std::vector<HIRStatement*> statements;
            statements.reserve(2);
            statements.push_back(declaration);
            statements.push_back(flag);

            return program->nodes.create<HIRStatementGroup>(node.span,
                                                                    std::move(statements));
        }

        return declaration;
    }

    HIRNode* visit(ExternVarDeclaration& node) override {
        const auto* type = lower_type(node.type, node.span);
        const auto* symbol = node.variable_symbol;
        auto name = Mangler::linker_name(node.name, symbol);

        return program->nodes.create<HIRVarDeclaration>(
            node.span, node.visibility, Linkage::Type::External, StorageKind::Type::Var,
            std::move(name), type, nullptr, true, AttributeResolver::convert(node.attributes));
    }

    HIRNode* visit(FunctionDeclaration& node) override {
        if (node.is_deleted) {
            return nullptr;
        }
        if (!node.prototype->generic_parameters.empty()) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         node.prototype->name);

        auto variadic = node.prototype->is_variadic;
        const auto* return_type = lower_type(node.prototype->return_type->type, node.span);
        const auto* type = lower_function_type(node.type, node.span);
        if (type == nullptr || !is_emittable_function_type(type)) {
            return nullptr;
        }

        define_parameters(*node.prototype, type);

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, type);
        auto linkage = Mangler::function_linkage(base_name, symbol);
        auto mangled = Mangler::is_mangled(symbol);


        auto* body = build_defaulted_constructor(node, type);
        if (body == nullptr) {
            body = build_owned_body(node.prototype->parameters, type, *node.body, 0,
                node.prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::Move);
        }

        return program->nodes.create<HIRFunctionDeclaration>(
            node.span, node.visibility, linkage, std::move(name), return_type, body, variadic, type,
            mangled, Abi::Type::Language, symbol);
    }

    HIRNode* visit(ExternFunctionDeclaration& node) override {
        auto variadic = node.prototype->is_variadic;
        const auto* return_type = lower_type(node.prototype->return_type->type, node.span);
        const auto* type = lower_function_type(node.type, node.span);

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, type);
        auto mangled = Mangler::is_mangled(symbol);

        return program->nodes.create<HIRFunctionDeclaration>(
            node.span, node.visibility, Linkage::Type::External, std::move(name), return_type,
            nullptr, variadic, type, mangled, Abi::Type::C, symbol);
    }

    HIRNode* visit(StructDeclaration& node) override {
        for (auto* method : node.methods) {
            if (auto* lowered = lower_statement(*method); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        for (auto* nested_struct : node.structs) {
            visit(*nested_struct);
        }

        for (auto* nested_enum : node.enums) {
            visit(*nested_enum);
        }

        return nullptr;
    }

    HIRNode* visit(EnumDeclaration& node) override {
        for (auto* method : node.methods) {
            if (auto* lowered = lower_statement(*method); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        return nullptr;
    }

    HIRNode* visit(Parameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Argument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(FunctionPrototype& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Field& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(EnumVariant& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(StructLiteralField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(WhenPatternField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(WhenPattern& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericParameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericArgument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Attribute& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(ImportStatement& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(InterfaceDeclaration& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(TypeAliasDeclaration& node [[maybe_unused]]) override { return nullptr; }

    void register_generic_declarations(const std::vector<const Program*>& programs) {
        for (const auto* program_node : programs) {
            if (program_node == nullptr) {
                continue;
            }

            for (auto* statement : program_node->statements) {
                if (auto* struct_declaration = statement->as<StructDeclaration>();
                    struct_declaration != nullptr) {
                    if (!struct_declaration->generic_parameters.empty()) {
                        specializations.register_struct(struct_declaration->name,
                                                               struct_declaration);
                    }

                    for (auto* method : struct_declaration->methods) {
                        if (!method->prototype->generic_parameters.empty() ||
                            !struct_declaration->generic_parameters.empty()) {
                            auto base_name =
                                method->function_symbol != nullptr
                                    ? method->function_symbol->base_name()
                                    : struct_declaration->name + "::" + method->prototype->name;
                            specializations.register_function(base_name, method,
                                                                     method->function_symbol);
                        }
                    }
                }

                if (auto* enum_declaration = statement->as<EnumDeclaration>();
                    enum_declaration != nullptr) {
                    for (auto* method : enum_declaration->methods) {
                        if (!method->prototype->generic_parameters.empty() ||
                            !enum_declaration->generic_parameters.empty()) {
                            auto base_name =
                                method->function_symbol != nullptr
                                    ? method->function_symbol->base_name()
                                    : enum_declaration->name + "::" + method->prototype->name;
                            specializations.register_function(base_name, method,
                                                                     method->function_symbol);
                        }
                    }
                }

                if (auto* function_declaration = statement->as<FunctionDeclaration>();
                    function_declaration != nullptr) {
                    if (!function_declaration->prototype->generic_parameters.empty()) {
                        auto base_name = function_declaration->function_symbol != nullptr
                                             ? function_declaration->function_symbol->base_name()
                                             : function_declaration->prototype->name;
                        specializations.register_function(
                            base_name, function_declaration, function_declaration->function_symbol);
                    }
                }
            }
        }
    }

    std::shared_ptr<HIRProgram> lower_module(Program& program_node, Scope* module_scope,
                                             const std::vector<const Program*>& all_programs) try {
        sema.env.current_scope = module_scope != nullptr ? module_scope : sema.env.root_scope;

        register_generic_declarations(all_programs);
        register_program_symbols(program_node);

        for (auto& statement : program_node.statements) {
            if (auto* lowered = lower_statement(*statement); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        std::vector<HIRStatement*> global_destruction;
        for (auto iterator = program->statements.rbegin(); iterator != program->statements.rend();
             ++iterator) {
            auto* statement = *iterator;
            auto* variable = statement->as<HIRVarDeclaration>();
            if (variable == nullptr || !variable->is_global || variable->initializer == nullptr ||
                !sema.lifecycle.needs_drop(variable->type, resolver)) {
                continue;
            }

            auto* value = program->nodes.create<HIRIdentifierExpression>(
                variable->span, variable->name, variable->type);
            construction.destroy(variable->span, value,
                sema.lifecycle.resolve(variable->type, Operation::Type::Destroy, resolver), global_destruction);
        }

        if (!global_destruction.empty()) {
            const auto* void_type = sema.primitives.entries.at("void");
            const auto* function_type = sema.types.create<FunctionType>(
                std::string(), void_type, std::vector<ParameterType>(),
                std::vector<GenericParameterType>(), false);
            auto name = ".zep.global_dtor." +
                        (module_scope != nullptr ? module_scope->name : std::string("module"));
            auto* body = program->nodes.create<HIRBlockStatement>(
                Span(), std::move(global_destruction));
            auto* destructor = program->nodes.create<HIRFunctionDeclaration>(
                Span(), Visibility::Type::Private, Linkage::Type::Internal, std::move(name),
                void_type, body, false, function_type, false, Abi::Type::Language, nullptr);
            destructor->is_global_destructor = true;
            program->statements.push_back(destructor);
        }

        drain_specializations();

        for (const auto& entry : sema.env.root_scope->local_types()) {
            if (entry.second != nullptr) {
                program->referenced_types.push_back(entry.second);
            }
        }

        return diagnostics.has_errors() ? nullptr : program;
    } catch (const LoweringFailure& failure) {
        diagnostics.add_error(failure.span, failure.message);
        return nullptr;
    } catch (const std::runtime_error& failure) {
        diagnostics.add_error(Span(), failure.what());
        return nullptr;
    }

    std::shared_ptr<HIRProgram> lower(Program& program_node) {
        std::vector<const Program*> programs{&program_node};
        return lower_module(program_node, sema.env.root_scope, programs);
    }
};
