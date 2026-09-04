module;

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.lsp.analysis.completer;

import zep.common.source.position;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.builtin;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.token.keywords;
import zep.lsp.protocol.types;

class ParsedCursorContext {
  public:
    std::string receiver;
    std::string prefix;
    bool is_member_access = false;

    explicit ParsedCursorContext(std::string receiver, std::string prefix, bool is_member_access)
        : receiver(std::move(receiver)), prefix(std::move(prefix)),
          is_member_access(is_member_access) {}
};

export class Completer {
  private:
    static ParsedCursorContext parse_cursor_context(std::string_view content,
                                                    lsp::Position position) {
        std::size_t line_index = 0;
        std::size_t line_start = 0;
        std::string_view current_line;

        for (std::size_t index = 0; index < content.size(); ++index) {
            if (line_index == position.line) {
                auto line_end = content.find('\n', line_start);
                if (line_end == std::string_view::npos) {
                    line_end = content.size();
                }
                current_line = content.substr(line_start, line_end - line_start);
                break;
            }

            if (content[index] == '\n') {
                ++line_index;
                line_start = index + 1;
            }
        }

        if (line_index != position.line) {
            return ParsedCursorContext("", "", false);
        }

        auto cursor_column = std::min<std::size_t>(position.character, current_line.size());
        auto line_prefix = current_line.substr(0, cursor_column);

        std::size_t end_ident = line_prefix.size();
        while (end_ident > 0 && (std::isalnum(static_cast<unsigned char>(line_prefix[end_ident - 1])) != 0 ||
                                 line_prefix[end_ident - 1] == '_')) {
            --end_ident;
        }

        std::string prefix(line_prefix.substr(end_ident));

        if (end_ident > 0 && line_prefix[end_ident - 1] == '.') {
            if (end_ident >= 2 && line_prefix[end_ident - 2] == '.') {
                return ParsedCursorContext("", std::move(prefix), false);
            }

            std::size_t dot_pos = end_ident - 1;
            while (dot_pos > 0 && std::isspace(static_cast<unsigned char>(line_prefix[dot_pos - 1])) != 0) {
                --dot_pos;
            }

            std::size_t receiver_start = dot_pos;
            while (receiver_start > 0 &&
                   (std::isalnum(static_cast<unsigned char>(line_prefix[receiver_start - 1])) != 0 ||
                    line_prefix[receiver_start - 1] == '_' ||
                    line_prefix[receiver_start - 1] == '.')) {
                --receiver_start;
            }

            std::string receiver(line_prefix.substr(receiver_start, dot_pos - receiver_start));
            return ParsedCursorContext(std::move(receiver), std::move(prefix), true);
        }

        return ParsedCursorContext("", std::move(prefix), false);
    }

    static const FunctionDeclaration* find_enclosing_function(const Program* program,
                                                              std::uint32_t line) {
        if (program == nullptr) {
            return nullptr;
        }

        for (auto* statement : program->statements) {
            if (auto* function = statement->as<FunctionDeclaration>(); function != nullptr) {
                auto end_line = function->body != nullptr ? function->body->span.end.line
                                                          : function->span.end.line;
                if (function->span.start.line <= line && end_line >= line) {
                    return function;
                }
            }

            if (auto* struct_decl = statement->as<StructDeclaration>(); struct_decl != nullptr) {
                for (auto* method : struct_decl->methods) {
                    if (method != nullptr) {
                        auto end_line = method->body != nullptr ? method->body->span.end.line
                                                                : method->span.end.line;
                        if (method->span.start.line <= line && end_line >= line) {
                            return method;
                        }
                    }
                }
            }
        }

        return nullptr;
    }

    static const Type* find_symbol_type_in_function(const FunctionDeclaration* function,
                                                    const std::string& name,
                                                    std::uint32_t line) {
        if (function == nullptr) {
            return nullptr;
        }

        if (function->prototype != nullptr) {
            for (auto* parameter : function->prototype->parameters) {
                if (parameter != nullptr && parameter->name == name) {
                    if (parameter->type != nullptr && parameter->type->type != nullptr) {
                        return parameter->type->type;
                    }
                }
            }
        }

        if (function->body != nullptr) {
            for (auto* statement : function->body->statements) {
                if (statement == nullptr || statement->span.start.line > line) {
                    continue;
                }

                if (auto* var_decl = statement->as<VarDeclaration>(); var_decl != nullptr) {
                    if (var_decl->name == name) {
                        if (var_decl->type != nullptr) {
                            return var_decl->type;
                        }

                        if (var_decl->initializer != nullptr &&
                            var_decl->initializer->type != nullptr) {
                            return var_decl->initializer->type;
                        }
                    }
                }
            }
        }

        return nullptr;
    }

    static const Type* resolve_path_type(const Program* program, const Scope* scope,
                                         const std::string& path_expression,
                                         std::uint32_t line) {
        if (path_expression.empty()) {
            return nullptr;
        }

        std::vector<std::string> segments;
        std::size_t start = 0;
        while (start < path_expression.size()) {
            auto dot = path_expression.find('.', start);
            if (dot == std::string::npos) {
                segments.push_back(path_expression.substr(start));
                break;
            }

            segments.push_back(path_expression.substr(start, dot - start));
            start = dot + 1;
        }

        if (segments.empty()) {
            return nullptr;
        }

        const Type* current_type = nullptr;

        auto* function = find_enclosing_function(program, line);
        if (function != nullptr) {
            current_type = find_symbol_type_in_function(function, segments.front(), line);
        }

        if (current_type == nullptr && scope != nullptr) {
            for (const auto* current_scope = scope; current_scope != nullptr;
                 current_scope = current_scope->parent) {
                if (const auto* var = current_scope->lookup_var(segments.front());
                    var != nullptr && var->type != nullptr) {
                    current_type = var->type;
                    break;
                }

                if (const auto* ty = current_scope->lookup_type(segments.front());
                    ty != nullptr && ty->type != nullptr) {
                    current_type = ty->type;
                    break;
                }
            }
        }

        if (current_type == nullptr && program != nullptr) {
            for (auto* statement : program->statements) {
                if (auto* struct_decl = statement->as<StructDeclaration>();
                    struct_decl != nullptr && struct_decl->name == segments.front()) {
                    current_type = struct_decl->type;
                    break;
                }

                if (auto* enum_decl = statement->as<EnumDeclaration>();
                    enum_decl != nullptr && enum_decl->name == segments.front()) {
                    current_type = enum_decl->type;
                    break;
                }

                if (auto* var_decl = statement->as<VarDeclaration>();
                    var_decl != nullptr && var_decl->name == segments.front()) {
                    current_type = var_decl->type;
                    break;
                }
            }
        }

        if (current_type == nullptr) {
            return nullptr;
        }

        for (std::size_t index = 1; index < segments.size(); ++index) {
            if (auto* pointer = current_type->as<PointerType>(); pointer != nullptr) {
                current_type = pointer->element;
            }

            if (current_type == nullptr) {
                return nullptr;
            }

            const auto& member_name = segments[index];
            const Type* next_type = nullptr;

            if (auto* struct_type = current_type->as<StructType>(); struct_type != nullptr) {
                for (const auto* check = struct_type; check != nullptr; check = check->base_type) {
                    if (const auto* field = check->find_field(member_name); field != nullptr) {
                        next_type = field->type;
                        break;
                    }

                    for (const auto& method : check->methods) {
                        if (method.name == member_name) {
                            next_type = method.type;
                            break;
                        }
                    }

                    if (next_type != nullptr) {
                        break;
                    }
                }
            }

            if (next_type == nullptr) {
                return nullptr;
            }

            current_type = next_type;
        }

        return current_type;
    }

    static void collect_members_for_type(const Type* type, const std::string& prefix,
                                         std::vector<lsp::CompletionItem>& items,
                                         std::unordered_set<std::string>& seen) {
        if (type == nullptr) {
            return;
        }

        if (auto* pointer = type->as<PointerType>(); pointer != nullptr) {
            type = pointer->element;
        }

        if (type == nullptr) {
            return;
        }

        if (auto* struct_type = type->as<StructType>(); struct_type != nullptr) {
            for (const auto* check = struct_type; check != nullptr; check = check->base_type) {
                for (const auto& field : check->fields) {
                    if (!prefix.empty() && !field.name.starts_with(prefix)) {
                        continue;
                    }

                    if (seen.insert(field.name).second) {
                        auto detail = field.type != nullptr ? field.type->to_string() : "";
                        items.emplace_back(field.name, lsp::CompletionItemKind::Type::Field,
                                           std::move(detail));
                    }
                }

                for (const auto& method : check->methods) {
                    if (method.name.starts_with("~") || method.name == check->name) {
                        continue;
                    }

                    if (!prefix.empty() && !method.name.starts_with(prefix)) {
                        continue;
                    }

                    if (seen.insert(method.name).second) {
                        auto detail = method.type != nullptr ? method.type->to_string() : "";
                        items.emplace_back(method.name, lsp::CompletionItemKind::Type::Method,
                                           std::move(detail));
                    }
                }
            }
            return;
        }

        if (auto* interface_type = type->as<InterfaceType>(); interface_type != nullptr) {
            for (const auto& method : interface_type->methods) {
                if (!prefix.empty() && !method.name.starts_with(prefix)) {
                    continue;
                }

                if (seen.insert(method.name).second) {
                    auto detail = method.type != nullptr ? method.type->to_string() : "";
                    items.emplace_back(method.name, lsp::CompletionItemKind::Type::Method,
                                       std::move(detail));
                }
            }
            return;
        }

        if (auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            for (const auto& variant : enum_type->variants) {
                if (!prefix.empty() && !variant.name.starts_with(prefix)) {
                    continue;
                }

                if (seen.insert(variant.name).second) {
                    items.emplace_back(variant.name, lsp::CompletionItemKind::Type::EnumMember,
                                       "variant");
                }
            }
            return;
        }
    }

  public:
    Completer() = default;

    static std::vector<lsp::CompletionItem> complete(std::string_view content,
                                                     lsp::Position position,
                                                     const Program* program,
                                                     const Scope* scope) {
        auto context = parse_cursor_context(content, position);

        std::vector<lsp::CompletionItem> items;
        items.reserve(128);
        std::unordered_set<std::string> seen;

        auto target_line = position.line + 1;

        if (context.is_member_access) {
            const auto* receiver_type =
                resolve_path_type(program, scope, context.receiver, target_line);

            if (receiver_type != nullptr) {
                collect_members_for_type(receiver_type, context.prefix, items, seen);
            }

            if (program != nullptr) {
                for (auto* statement : program->statements) {
                    if (auto* enum_decl = statement->as<EnumDeclaration>();
                        enum_decl != nullptr && enum_decl->name == context.receiver) {
                        for (auto* variant : enum_decl->variants) {
                            if (variant != nullptr) {
                                if (context.prefix.empty() || variant->name.starts_with(context.prefix)) {
                                    if (seen.insert(variant->name).second) {
                                        items.emplace_back(variant->name,
                                                           lsp::CompletionItemKind::Type::EnumMember,
                                                           "variant");
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return items;
        }

        auto* function = find_enclosing_function(program, target_line);
        if (function != nullptr) {
            if (function->prototype != nullptr) {
                for (auto* parameter : function->prototype->parameters) {
                    if (parameter != nullptr && seen.insert(parameter->name).second) {
                        auto detail = (parameter->type != nullptr && parameter->type->type != nullptr)
                                          ? parameter->type->type->to_string()
                                          : "parameter";
                        items.emplace_back(parameter->name,
                                           lsp::CompletionItemKind::Type::Variable,
                                           std::move(detail));
                    }
                }
            }

            if (function->body != nullptr) {
                for (auto* statement : function->body->statements) {
                    if (statement == nullptr || statement->span.start.line > target_line) {
                        continue;
                    }

                    if (auto* var_decl = statement->as<VarDeclaration>(); var_decl != nullptr) {
                        if (seen.insert(var_decl->name).second) {
                            auto detail = var_decl->type != nullptr
                                              ? var_decl->type->to_string()
                                              : "variable";
                            items.emplace_back(var_decl->name,
                                               lsp::CompletionItemKind::Type::Variable,
                                               std::move(detail));
                        }
                    }
                }
            }
        }

        if (scope != nullptr) {
            for (const auto* current_scope = scope; current_scope != nullptr;
                 current_scope = current_scope->parent) {
                for (const auto& [name, _] : current_scope->local_types()) {
                    if (seen.insert(name).second) {
                        items.emplace_back(name, lsp::CompletionItemKind::Type::Struct, "type");
                    }
                }

                for (const auto& [name, _] : current_scope->local_variables()) {
                    if (seen.insert(name).second) {
                        items.emplace_back(name, lsp::CompletionItemKind::Type::Variable,
                                           "variable");
                    }
                }

                for (const auto& [name, _] : current_scope->local_functions()) {
                    if (seen.insert(name).second) {
                        items.emplace_back(name, lsp::CompletionItemKind::Type::Function,
                                           "function");
                    }
                }

                for (const auto& [name, _] : current_scope->local_variants()) {
                    if (seen.insert(name).second) {
                        items.emplace_back(name, lsp::CompletionItemKind::Type::EnumMember,
                                           "variant");
                    }
                }
            }
        }

        if (program != nullptr) {
            for (auto* statement : program->statements) {
                if (auto* struct_decl = statement->as<StructDeclaration>(); struct_decl != nullptr) {
                    if (seen.insert(struct_decl->name).second) {
                        items.emplace_back(struct_decl->name, lsp::CompletionItemKind::Type::Struct,
                                           "struct");
                    }
                }

                if (auto* enum_decl = statement->as<EnumDeclaration>(); enum_decl != nullptr) {
                    if (seen.insert(enum_decl->name).second) {
                        items.emplace_back(enum_decl->name, lsp::CompletionItemKind::Type::Enum,
                                           "enum");
                    }
                }

                if (auto* fn_decl = statement->as<FunctionDeclaration>(); fn_decl != nullptr) {
                    if (fn_decl->prototype != nullptr &&
                        seen.insert(fn_decl->prototype->name).second) {
                        items.emplace_back(fn_decl->prototype->name,
                                           lsp::CompletionItemKind::Type::Function, "function");
                    }
                }
            }
        }

        for (const auto& builtin_name : BuiltinResolver::builtin_names()) {
            if (seen.insert(builtin_name).second) {
                items.emplace_back(builtin_name, lsp::CompletionItemKind::Type::Function,
                                   "builtin");
            }
        }

        for (const auto& [keyword, _] : keywords) {
            std::string name(keyword);
            if (seen.insert(name).second) {
                items.emplace_back(std::move(name), lsp::CompletionItemKind::Type::Keyword,
                                   "keyword");
            }
        }

        return items;
    }
};
