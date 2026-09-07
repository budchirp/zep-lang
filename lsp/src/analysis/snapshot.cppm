module;

#include <filesystem>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.lsp.analysis.snapshot;

import zep.common.diagnostic.diagnostic;
import zep.common.source;
import zep.common.source.position;
import zep.common.source.span;
import zep.compiler;
import zep.lsp.analysis.completion;
import zep.lsp.analysis.catalog;
import zep.lsp.analysis.index;
import zep.lsp.analysis.types;
import zep.compiler.unit;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.workspace.project;

class IndexedModule {
  public:
    std::filesystem::path path;
    SyntaxIndex syntax;

    explicit IndexedModule(Module& module)
        : path(module.source->name), syntax(module.source->content, &module.syntax) {}
};

export class Analysis {
  private:
    std::optional<Project> project;
    std::unique_ptr<Compiler> compiler;
    Module* analyzed_module = nullptr;
    std::string content;
    std::vector<Diagnostic> diagnostic_entries;
    std::filesystem::path analyzed_path;
    std::vector<IndexedModule> indexes;

    static bool contains(Span span, Position position) {
        if (span.start.line == 0 || position.line < span.start.line || position.line > span.end.line) {
            return false;
        }

        if (position.line == span.start.line && position.column < span.start.column) {
            return false;
        }

        return position.line != span.end.line || position.column <= span.end.column;
    }

    static const void* identity(Node* node) {
        if (node == nullptr) {
            return nullptr;
        }

        if (auto* identifier = node->as<IdentifierExpression>(); identifier != nullptr) {
            if (identifier->var_symbol != nullptr) {
                return identifier->var_symbol;
            }
            if (identifier->function_symbol != nullptr) {
                return identifier->function_symbol;
            }
            if (identifier->type != nullptr && identifier->type->as_nominal() != nullptr) {
                return identifier->type->as_nominal()->definition;
            }
            return identifier->generic_declaration;
        }

        if (auto* variable = node->as<VarDeclaration>(); variable != nullptr) {
            return variable->variable_symbol;
        }
        if (auto* variable = node->as<ExternVarDeclaration>(); variable != nullptr) {
            return variable->variable_symbol;
        }
        if (auto* function = node->as<FunctionDeclaration>(); function != nullptr) {
            return function->function_symbol;
        }
        if (auto* function = node->as<ExternFunctionDeclaration>(); function != nullptr) {
            return function->function_symbol;
        }
        if (auto* structure = node->as<StructDeclaration>();
            structure != nullptr && structure->type != nullptr &&
            structure->type->as_nominal() != nullptr) {
            return structure->type->as_nominal()->definition;
        }
        if (auto* enumeration = node->as<EnumDeclaration>();
            enumeration != nullptr && enumeration->type != nullptr &&
            enumeration->type->as_nominal() != nullptr) {
            return enumeration->type->as_nominal()->definition;
        }
        if (auto* interface = node->as<InterfaceDeclaration>();
            interface != nullptr && interface->type != nullptr &&
            interface->type->as_nominal() != nullptr) {
            return interface->type->as_nominal()->definition;
        }
        if (auto* alias = node->as<TypeAliasDeclaration>(); alias != nullptr) {
            return alias->type;
        }
        if (auto* expression = node->as<QualifiedAccessExpression>(); expression != nullptr) {
            if (expression->function_symbol != nullptr) {
                return expression->function_symbol;
            }
            if (expression->variant_type != nullptr) {
                return expression->variant_type;
            }
            if (expression->enum_type != nullptr) {
                return expression->enum_type->definition;
            }
        }
        if (auto* member = node->as<MemberExpression>();
            member != nullptr && member->value != nullptr && member->value->type != nullptr) {
            const Type* receiver = member->value->type;
            if (const auto* pointer = receiver->as<PointerType>(); pointer != nullptr) {
                receiver = pointer->element;
            }
            const auto* structure = receiver != nullptr ? receiver->as<StructType>() : nullptr;
            for (const auto* current = structure; current != nullptr; current = current->base_type) {
                const auto* definition = current->definition->as<StructType>();
                if (const auto* field = definition != nullptr
                                            ? definition->find_field(member->member)
                                            : nullptr;
                    field != nullptr) {
                    return field;
                }
            }
        }
        if (auto* expression = node->as<EnumVariantExpression>();
            expression != nullptr && expression->variant_type != nullptr) {
            return expression->variant_type;
        }
        if (auto* call = node->as<CallExpression>();
            call != nullptr && call->resolved_target != nullptr) {
            if (const auto* direct = call->resolved_target->as<DirectCallTarget>();
                direct != nullptr) {
                return direct->function_symbol;
            }
            if (const auto* interface = call->resolved_target->as<InterfaceCallTarget>();
                interface != nullptr) {
                return interface->method_symbol;
            }
        }
        if (auto* type = node->as<TypeExpression>();
            type != nullptr && type->type != nullptr && type->type->as_nominal() != nullptr) {
            return type->type->as_nominal()->definition;
        }

        return nullptr;
    }

    static bool is_declaration(Node* node) {
        return node != nullptr &&
               (node->as<VarDeclaration>() != nullptr ||
                node->as<ExternVarDeclaration>() != nullptr ||
                node->as<FunctionDeclaration>() != nullptr ||
                node->as<ExternFunctionDeclaration>() != nullptr ||
                node->as<StructDeclaration>() != nullptr ||
                node->as<EnumDeclaration>() != nullptr ||
                node->as<InterfaceDeclaration>() != nullptr ||
                node->as<TypeAliasDeclaration>() != nullptr || node->as<Parameter>() != nullptr ||
                node->as<Field>() != nullptr || node->as<EnumVariant>() != nullptr);
    }

    static Span selection(Node* node) {
        if (auto* variable = node->as<VarDeclaration>(); variable != nullptr) {
            auto prefix_length = variable->storage_kind == StorageKind::Type::VarMut ? 8U : 4U;
            auto start = Position(variable->span.start.line,
                                  variable->span.start.column + prefix_length);
            return Span(start, Position(start.line, start.column + variable->name.length() - 1));
        }
        if (auto* function = node->as<FunctionDeclaration>();
            function != nullptr && function->prototype != nullptr) {
            auto start = function->prototype->span.start;
            return Span(start,
                        Position(start.line, start.column + function->prototype->name.length() - 1));
        }
        if (auto* structure = node->as<StructDeclaration>(); structure != nullptr) {
            auto start = Position(structure->span.start.line, structure->span.start.column + 7U);
            return Span(start, Position(start.line, start.column + structure->name.length() - 1));
        }
        if (auto* enumeration = node->as<EnumDeclaration>(); enumeration != nullptr) {
            auto start = Position(enumeration->span.start.line, enumeration->span.start.column + 5U);
            return Span(start, Position(start.line, start.column + enumeration->name.length() - 1));
        }
        if (auto* interface = node->as<InterfaceDeclaration>(); interface != nullptr) {
            auto start = Position(interface->span.start.line, interface->span.start.column + 10U);
            return Span(start, Position(start.line, start.column + interface->name.length() - 1));
        }
        if (auto* alias = node->as<TypeAliasDeclaration>(); alias != nullptr) {
            auto start = Position(alias->span.start.line, alias->span.start.column + 5U);
            return Span(start, Position(start.line, start.column + alias->name.length() - 1));
        }
        if (auto* member = node->as<MemberExpression>(); member != nullptr) {
            auto start = Position(member->span.end.line,
                                  member->span.end.column - member->member.length() + 1);
            return Span(start, member->span.end);
        }
        if (auto* call = node->as<CallExpression>(); call != nullptr) {
            return call->callee->span;
        }
        return node->span;
    }

    const IndexedModule* current_index() const {
        for (const auto& index : indexes) {
            std::error_code error_code;
            if (std::filesystem::equivalent(index.path, analyzed_path, error_code) && !error_code) {
                return &index;
            }
        }
        return indexes.empty() ? nullptr : &indexes.back();
    }

    const void* identity_at(Position position) const {
        const auto* index = current_index();
        if (index == nullptr) {
            return nullptr;
        }

        auto* node = index->syntax.node(position);
        auto result = identity(node);
        if (result != nullptr) {
            return result;
        }

        for (auto* candidate : index->syntax.nodes()) {
            auto* call = candidate->as<CallExpression>();
            if (call == nullptr || call->resolved_target == nullptr ||
                !contains(call->callee->span, position)) {
                continue;
            }

            if (const auto* direct = call->resolved_target->as<DirectCallTarget>();
                direct != nullptr) {
                return direct->function_symbol;
            }
            if (const auto* interface = call->resolved_target->as<InterfaceCallTarget>();
                interface != nullptr) {
                return interface->method_symbol;
            }
        }

        return nullptr;
    }

    std::optional<AnalysisLocation> field_definition(const void* symbol_identity) const {
        for (const auto& index : indexes) {
            for (auto* node : index.syntax.nodes()) {
                auto* structure = node->as<StructDeclaration>();
                if (structure == nullptr || structure->type == nullptr) {
                    continue;
                }
                const auto* structure_type = structure->type->as<StructType>();
                if (structure_type == nullptr) {
                    continue;
                }
                const auto* definition = structure_type->definition->as<StructType>();
                if (definition == nullptr) {
                    continue;
                }
                for (const auto& field_type : definition->fields) {
                    if (&field_type != symbol_identity) {
                        continue;
                    }
                    for (auto* field : structure->fields) {
                        if (field->name == field_type.name) {
                            return AnalysisLocation(index.path, field->span);
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    static std::optional<Hover> describe(Node* node, const Scope* scope) {
        if (node == nullptr) {
            return std::nullopt;
        }

        if (auto* identifier = node->as<IdentifierExpression>(); identifier != nullptr) {
            if (identifier->var_symbol != nullptr && identifier->var_symbol->type != nullptr) {
                return Hover("var " + identifier->name + ": " +
                                 identifier->var_symbol->type->to_string(),
                             identifier->span);
            }

            if (identifier->function_symbol != nullptr &&
                identifier->function_symbol->function_type != nullptr) {
                return Hover("fn " + identifier->name + ": " +
                                 identifier->function_symbol->function_type->to_string(),
                             identifier->span);
            }

            if (identifier->type != nullptr) {
                return Hover(identifier->name + ": " + identifier->type->to_string(),
                             identifier->span);
            }

            if (scope != nullptr) {
                if (const auto* variable = scope->lookup_var(identifier->name);
                    variable != nullptr && variable->type != nullptr) {
                    return Hover("var " + identifier->name + ": " + variable->type->to_string(),
                                 identifier->span);
                }

                if (const auto* function = scope->lookup_function(identifier->name);
                    function != nullptr && function->function_type != nullptr) {
                    return Hover("fn " + identifier->name + ": " +
                                     function->function_type->to_string(),
                                 identifier->span);
                }

                if (const auto* type = scope->lookup_type(identifier->name);
                    type != nullptr && type->type != nullptr) {
                    return Hover("type " + identifier->name + ": " + type->type->to_string(),
                                 identifier->span);
                }
            }

            return Hover(identifier->name, identifier->span);
        }

        if (auto* variable = node->as<VarDeclaration>(); variable != nullptr) {
            auto type = variable->type != nullptr ? variable->type->to_string() : "unknown";
            return Hover("var " + variable->name + ": " + type, variable->span);
        }

        if (auto* function = node->as<FunctionDeclaration>(); function != nullptr) {
            auto type = function->function_symbol != nullptr &&
                                function->function_symbol->function_type != nullptr
                            ? function->function_symbol->function_type->to_string()
                            : "fn " + function->prototype->name;
            return Hover("fn " + function->prototype->name + ": " + type, function->span);
        }

        if (auto* parameter = node->as<Parameter>(); parameter != nullptr) {
            auto type = parameter->type != nullptr && parameter->type->type != nullptr
                            ? parameter->type->type->to_string()
                            : "";
            return Hover("parameter " + parameter->name + (type.empty() ? "" : ": " + type),
                         parameter->span);
        }

        if (auto* field = node->as<Field>(); field != nullptr) {
            auto type = field->type != nullptr && field->type->type != nullptr
                            ? field->type->type->to_string()
                            : "";
            return Hover("field " + field->name + (type.empty() ? "" : ": " + type), field->span);
        }

        if (auto* prototype = node->as<FunctionPrototype>(); prototype != nullptr) {
            auto type = prototype->return_type != nullptr && prototype->return_type->type != nullptr
                            ? prototype->return_type->type->to_string()
                            : "void";
            return Hover("fn " + prototype->name + ": " + type, prototype->span);
        }

        if (auto* structure = node->as<StructDeclaration>(); structure != nullptr) {
            return Hover("struct " + structure->name, structure->span);
        }

        if (auto* enumeration = node->as<EnumDeclaration>(); enumeration != nullptr) {
            return Hover("enum " + enumeration->name, enumeration->span);
        }

        if (auto* expression = dynamic_cast<Expression*>(node);
            expression != nullptr && expression->type != nullptr) {
            return Hover(expression->type->to_string(), expression->span);
        }

        return std::nullopt;
    }

  public:
    Analysis(Project project, std::unique_ptr<Compiler> compiler, Module* module,
             std::string content, std::vector<Diagnostic> diagnostics)
        : project(std::move(project)), compiler(std::move(compiler)), analyzed_module(module),
          content(std::move(content)), diagnostic_entries(std::move(diagnostics)),
          analyzed_path(module != nullptr ? std::filesystem::path(module->source->name)
                                          : std::filesystem::path()) {
        indexes.reserve(this->compiler->modules().size());
        for (auto* indexed_module : this->compiler->modules()) {
            indexes.emplace_back(*indexed_module);
        }
    }

    explicit Analysis(std::vector<Diagnostic> diagnostics)
        : diagnostic_entries(std::move(diagnostics)) {}

    const Program* program() const {
        return analyzed_module != nullptr ? &analyzed_module->syntax : nullptr;
    }

    const Scope* scope() const {
        return analyzed_module != nullptr ? analyzed_module->scope : nullptr;
    }

    const std::vector<Diagnostic>& diagnostics() const { return diagnostic_entries; }

    std::optional<Hover> hover(Position position) const {
        const auto* index = current_index();
        if (index == nullptr) {
            return std::nullopt;
        }
        for (auto* node : index->syntax.nodes()) {
            if (is_declaration(node) && contains(selection(node), position)) {
                if (auto result = describe(node, scope()); result.has_value()) {
                    return result;
                }
            }
        }
        return describe(index->syntax.node(position), scope());
    }

    std::vector<Completion> complete(Position position) const {
        if (project.has_value()) {
            ModuleCatalog catalog(*project);
            if (auto imports = catalog.complete(content, position, analyzed_path);
                imports.has_value()) {
                return std::move(*imports);
            }
        }
        const Scope* completion_scope = scope();
        const auto* index = current_index();
        if (index != nullptr) {
            if (const auto* indexed_scope = index->syntax.scope(position);
                indexed_scope != nullptr) {
                completion_scope = indexed_scope;
            }
        }
        return Completer::complete(content, position, completion_scope);
    }

    std::vector<SemanticToken> tokens(std::optional<Span> range = std::nullopt) const {
        const auto* index = current_index();
        if (index == nullptr) {
            return {};
        }
        return range.has_value() ? index->syntax.tokens(*range) : index->syntax.tokens();
    }

    std::optional<AnalysisLocation> definition(Position position) const {
        auto symbol_identity = identity_at(position);
        if (symbol_identity == nullptr) {
            return std::nullopt;
        }

        for (const auto& index : indexes) {
            for (auto* node : index.syntax.nodes()) {
                if (is_declaration(node) && identity(node) == symbol_identity) {
                    return AnalysisLocation(index.path, selection(node));
                }
            }
        }
        return field_definition(symbol_identity);
    }

    std::vector<AnalysisLocation> references(Position position,
                                             bool include_declaration) const {
        std::vector<AnalysisLocation> result;
        auto symbol_identity = identity_at(position);
        if (symbol_identity == nullptr) {
            return result;
        }

        for (const auto& index : indexes) {
            for (auto* node : index.syntax.nodes()) {
                if (identity(node) != symbol_identity || (!include_declaration && is_declaration(node))) {
                    continue;
                }
                result.emplace_back(index.path, selection(node));
            }
        }
        if (include_declaration) {
            if (auto declaration = field_definition(symbol_identity); declaration.has_value()) {
                result.push_back(std::move(*declaration));
            }
        }
        return result;
    }

    std::vector<DocumentHighlight> highlights(Position position) const {
        std::vector<DocumentHighlight> result;
        auto symbol_identity = identity_at(position);
        const auto* index = current_index();
        if (symbol_identity == nullptr || index == nullptr) {
            return result;
        }

        for (auto* node : index->syntax.nodes()) {
            if (identity(node) == symbol_identity) {
                result.emplace_back(selection(node), is_declaration(node)
                                                          ? DocumentHighlight::Kind::Type::Write
                                                          : DocumentHighlight::Kind::Type::Read);
            }
        }
        if (auto declaration = field_definition(symbol_identity);
            declaration.has_value() && declaration->path == index->path) {
            result.emplace_back(declaration->span, DocumentHighlight::Kind::Type::Write);
        }
        return result;
    }

    std::vector<DocumentSymbol> document_symbols() const {
        std::vector<DocumentSymbol> result;
        if (program() == nullptr) {
            return result;
        }

        result.reserve(program()->statements.size());
        for (auto* statement : program()->statements) {
            if (auto* function = statement->as<FunctionDeclaration>();
                function != nullptr && function->prototype != nullptr) {
                result.emplace_back(function->prototype->name, CompletionKind::Type::Function,
                                    AnalysisLocation(analyzed_path, function->span),
                                    selection(function));
            } else if (auto* structure = statement->as<StructDeclaration>(); structure != nullptr) {
                std::vector<DocumentSymbol> children;
                children.reserve(structure->fields.size() + structure->methods.size());
                for (auto* field : structure->fields) {
                    children.emplace_back(field->name, CompletionKind::Type::Field,
                                          AnalysisLocation(analyzed_path, field->span), field->span);
                }
                for (auto* method : structure->methods) {
                    children.emplace_back(method->prototype->name, CompletionKind::Type::Method,
                                          AnalysisLocation(analyzed_path, method->span),
                                          selection(method));
                }
                result.emplace_back(structure->name, CompletionKind::Type::Struct,
                                    AnalysisLocation(analyzed_path, structure->span),
                                    selection(structure), std::move(children));
            } else if (auto* enumeration = statement->as<EnumDeclaration>(); enumeration != nullptr) {
                result.emplace_back(enumeration->name, CompletionKind::Type::Enum,
                                    AnalysisLocation(analyzed_path, enumeration->span),
                                    selection(enumeration));
            } else if (auto* interface = statement->as<InterfaceDeclaration>();
                       interface != nullptr) {
                result.emplace_back(interface->name, CompletionKind::Type::Interface,
                                    AnalysisLocation(analyzed_path, interface->span),
                                    selection(interface));
            } else if (auto* alias = statement->as<TypeAliasDeclaration>(); alias != nullptr) {
                result.emplace_back(alias->name, CompletionKind::Type::TypeParameter,
                                    AnalysisLocation(analyzed_path, alias->span), selection(alias));
            } else if (auto* variable = statement->as<VarDeclaration>(); variable != nullptr) {
                result.emplace_back(variable->name, CompletionKind::Type::Variable,
                                    AnalysisLocation(analyzed_path, variable->span),
                                    selection(variable));
            }
        }
        return result;
    }

    std::optional<SignatureHelp> signature(Position position) const {
        const auto* index = current_index();
        if (index == nullptr) {
            return std::nullopt;
        }

        CallExpression* selected = nullptr;
        for (auto* node : index->syntax.nodes()) {
            auto* call = node->as<CallExpression>();
            if (call != nullptr && contains(call->span, position) &&
                (selected == nullptr || call->span.start.line >= selected->span.start.line)) {
                selected = call;
            }
        }
        if (selected == nullptr || selected->resolved_target == nullptr) {
            return std::nullopt;
        }

        const FunctionType* function_type = nullptr;
        if (const auto* direct = selected->resolved_target->as<DirectCallTarget>();
            direct != nullptr) {
            function_type = direct->function_symbol->function_type;
        } else if (const auto* indirect = selected->resolved_target->as<IndirectCallTarget>();
                   indirect != nullptr) {
            function_type = indirect->function_type;
        } else if (const auto* interface = selected->resolved_target->as<InterfaceCallTarget>();
                   interface != nullptr) {
            function_type = interface->method_symbol->function_type;
        }
        if (function_type == nullptr) {
            return std::nullopt;
        }

        std::vector<SignatureParameter> parameters;
        parameters.reserve(function_type->parameters.size());
        for (const auto& parameter : function_type->parameters) {
            parameters.emplace_back(parameter.name + ": " +
                                    (parameter.type != nullptr ? parameter.type->to_string()
                                                               : "unknown"));
        }

        std::size_t active_parameter = 0;
        for (std::size_t index_value = 0; index_value < selected->arguments.size(); ++index_value) {
            if (selected->arguments[index_value]->span.start.line < position.line ||
                (selected->arguments[index_value]->span.start.line == position.line &&
                 selected->arguments[index_value]->span.start.column <= position.column)) {
                active_parameter = index_value;
            }
        }

        std::vector<Signature> signatures;
        signatures.emplace_back(function_type->to_string(), std::move(parameters));
        return SignatureHelp(std::move(signatures), 0,
                             std::min(active_parameter, function_type->parameters.size() - 1));
    }
};
