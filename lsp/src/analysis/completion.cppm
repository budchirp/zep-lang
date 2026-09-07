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

export module zep.lsp.analysis.completion;

import zep.common.source.position;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.builtin;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.token.keywords;
import zep.lsp.analysis.types;

class ParsedCursorContext {
  public:
    std::string receiver;
    std::string prefix;
    std::string callee;
    std::unordered_set<std::string> named_arguments;
    bool is_member_access = false;
    bool is_static_access = false;
    bool is_argument_list = false;

    ParsedCursorContext(std::string receiver, std::string prefix, bool is_member_access,
                        bool is_static_access = false)
        : receiver(std::move(receiver)), prefix(std::move(prefix)),
          is_member_access(is_member_access), is_static_access(is_static_access) {}
};

export class Completer {
  private:
    static ParsedCursorContext parse_cursor_context(std::string_view content, Position position) {
        std::size_t line_index = 1;
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

        auto cursor_column = position.column > 0
                                 ? std::min<std::size_t>(position.column - 1, current_line.size())
                                 : 0;
        auto line_prefix = current_line.substr(0, cursor_column);

        std::size_t end_ident = line_prefix.size();
        while (end_ident > 0 &&
               (std::isalnum(static_cast<unsigned char>(line_prefix[end_ident - 1])) != 0 ||
                line_prefix[end_ident - 1] == '_')) {
            --end_ident;
        }

        std::string prefix(line_prefix.substr(end_ident));

        if (end_ident >= 2 && line_prefix[end_ident - 2] == ':' &&
            line_prefix[end_ident - 1] == ':') {
            auto receiver_end = end_ident - 2;
            while (receiver_end > 0 &&
                   std::isspace(static_cast<unsigned char>(line_prefix[receiver_end - 1])) != 0) {
                --receiver_end;
            }

            auto receiver_start = receiver_end;
            while (receiver_start > 0 &&
                   (std::isalnum(static_cast<unsigned char>(line_prefix[receiver_start - 1])) != 0 ||
                    line_prefix[receiver_start - 1] == '_')) {
                --receiver_start;
            }

            std::string receiver(
                line_prefix.substr(receiver_start, receiver_end - receiver_start));
            return ParsedCursorContext(std::move(receiver), std::move(prefix), false, true);
        }

        if (end_ident > 0 && line_prefix[end_ident - 1] == '.') {
            if (end_ident >= 2 && line_prefix[end_ident - 2] == '.') {
                return ParsedCursorContext("", std::move(prefix), false);
            }

            std::size_t dot_position = end_ident - 1;
            while (dot_position > 0 &&
                   std::isspace(static_cast<unsigned char>(line_prefix[dot_position - 1])) != 0) {
                --dot_position;
            }

            std::size_t receiver_start = dot_position;
            while (receiver_start > 0 && (std::isalnum(static_cast<unsigned char>(
                                              line_prefix[receiver_start - 1])) != 0 ||
                                          line_prefix[receiver_start - 1] == '_' ||
                                          line_prefix[receiver_start - 1] == '.')) {
                --receiver_start;
            }

            std::string receiver(line_prefix.substr(receiver_start, dot_position - receiver_start));
            return ParsedCursorContext(std::move(receiver), std::move(prefix), true);
        }

        ParsedCursorContext result("", std::move(prefix), false);
        auto cursor_offset = line_start + cursor_column;
        auto nested_parentheses = std::size_t{0};
        auto open_parenthesis = std::string_view::npos;
        for (auto index = cursor_offset; index > 0; --index) {
            auto character = content[index - 1];
            if (character == ')') {
                ++nested_parentheses;
                continue;
            }

            if (character != '(') {
                continue;
            }

            if (nested_parentheses > 0) {
                --nested_parentheses;
                continue;
            }

            open_parenthesis = index - 1;
            break;
        }

        if (open_parenthesis == std::string_view::npos) {
            return result;
        }

        auto callee_end = open_parenthesis;
        while (callee_end > 0 &&
               std::isspace(static_cast<unsigned char>(content[callee_end - 1])) != 0) {
            --callee_end;
        }

        auto callee_start = callee_end;
        while (callee_start > 0) {
            auto character = content[callee_start - 1];
            if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_' &&
                character != '.' && character != ':') {
                break;
            }
            --callee_start;
        }

        if (callee_start == callee_end) {
            return result;
        }

        result.callee = std::string(content.substr(callee_start, callee_end - callee_start));
        result.is_argument_list = true;

        auto arguments = content.substr(open_parenthesis + 1,
                                        cursor_offset - open_parenthesis - 1);
        auto nesting = std::size_t{0};
        for (std::size_t index = 0; index < arguments.size();) {
            auto character = arguments[index];
            if (character == '(' || character == '{' || character == '[') {
                ++nesting;
                ++index;
                continue;
            }
            if (character == ')' || character == '}' || character == ']') {
                if (nesting > 0) {
                    --nesting;
                }
                ++index;
                continue;
            }
            if (nesting != 0 ||
                (std::isalpha(static_cast<unsigned char>(character)) == 0 && character != '_')) {
                ++index;
                continue;
            }

            auto name_start = index;
            while (index < arguments.size() &&
                   (std::isalnum(static_cast<unsigned char>(arguments[index])) != 0 ||
                    arguments[index] == '_')) {
                ++index;
            }

            auto separator = index;
            while (separator < arguments.size() &&
                   std::isspace(static_cast<unsigned char>(arguments[separator])) != 0) {
                ++separator;
            }
            if (separator < arguments.size() && arguments[separator] == ':') {
                result.named_arguments.insert(
                    std::string(arguments.substr(name_start, index - name_start)));
            }
        }

        return result;
    }

    static const Type* resolve_path_type(const Scope* scope,
                                         const std::string& path_expression) {
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

        if (scope != nullptr) {
            for (const auto* current_scope = scope; current_scope != nullptr;
                 current_scope = current_scope->parent) {
                if (const auto* variable = current_scope->lookup_var(segments.front());
                    variable != nullptr && variable->type != nullptr) {
                    current_type = variable->type;
                    break;
                }

                if (const auto* type = current_scope->lookup_type(segments.front());
                    type != nullptr && type->type != nullptr) {
                    current_type = type->type;
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
                                         std::vector<Completion>& items,
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
                        items.emplace_back(field.name, CompletionKind::Type::Field,
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
                        items.emplace_back(method.name, CompletionKind::Type::Method,
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
                    items.emplace_back(method.name, CompletionKind::Type::Method,
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
                    items.emplace_back(variant.name, CompletionKind::Type::EnumMember, "variant");
                }
            }
            return;
        }
    }

    static void collect_static_members(const Scope* scope, const std::string& type_name,
                                       const std::string& prefix,
                                       std::vector<Completion>& items,
                                       std::unordered_set<std::string>& seen) {
        if (scope == nullptr) {
            return;
        }

        const TypeSymbol* type_symbol = nullptr;
        for (const auto* current_scope = scope; current_scope != nullptr;
             current_scope = current_scope->parent) {
            type_symbol = current_scope->lookup_type(type_name);
            if (type_symbol != nullptr) {
                break;
            }
        }

        if (type_symbol == nullptr || type_symbol->member_scope == nullptr) {
            return;
        }

        for (const auto& [name, overloads] : type_symbol->member_scope->local_functions()) {
            if (!prefix.empty() && !name.starts_with(prefix)) {
                continue;
            }

            const FunctionSymbol* selected = nullptr;
            for (const auto* function : overloads->functions) {
                if (function != nullptr &&
                    function->callable_kind == FunctionSymbol::Kind::Type::StaticMethod) {
                    selected = function;
                    break;
                }
            }

            if (selected != nullptr && seen.insert(name).second) {
                auto detail = selected->function_type != nullptr
                                  ? selected->function_type->to_string()
                                  : std::string();
                items.emplace_back(name, CompletionKind::Type::Method, std::move(detail));
            }
        }

        for (const auto& [name, symbol] : type_symbol->member_scope->local_variants()) {
            if ((prefix.empty() || name.starts_with(prefix)) && seen.insert(name).second) {
                items.emplace_back(name, CompletionKind::Type::EnumMember, "variant");
            }
        }
    }

    static void add_parameters(const FunctionType* function_type,
                               const ParsedCursorContext& context, Position position,
                               std::vector<Completion>& items,
                               std::unordered_set<std::string>& seen) {
        if (function_type == nullptr) {
            return;
        }

        auto replacement = Span(
            Position(position.line, position.column - context.prefix.length()), position);
        for (const auto& parameter : function_type->parameters) {
            if (context.named_arguments.contains(parameter.name) ||
                (!context.prefix.empty() && !parameter.name.starts_with(context.prefix)) ||
                !seen.insert(parameter.name).second) {
                continue;
            }

            auto detail = parameter.type != nullptr ? parameter.type->to_string() : std::string();
            items.emplace_back(parameter.name, CompletionKind::Type::Field, std::move(detail),
                               replacement, parameter.name + ": ", "0_" + parameter.name);
        }
    }

    static void collect_named_arguments(const Scope* scope, const ParsedCursorContext& context,
                                        Position position, std::vector<Completion>& items,
                                        std::unordered_set<std::string>& seen) {
        if (scope == nullptr || !context.is_argument_list || context.callee.empty()) {
            return;
        }

        if (auto separator = context.callee.rfind("::"); separator != std::string::npos) {
            auto type_name = context.callee.substr(0, separator);
            auto function_name = context.callee.substr(separator + 2);
            const auto* type_symbol = scope->lookup_type(type_name);
            const auto* overloads = type_symbol != nullptr && type_symbol->member_scope != nullptr
                                        ? type_symbol->member_scope->find_local_function_overloads(
                                              function_name)
                                        : nullptr;
            if (overloads != nullptr) {
                for (const auto* function : *overloads) {
                    add_parameters(function->function_type, context, position, items, seen);
                }
            }
            return;
        }

        if (auto separator = context.callee.rfind('.'); separator != std::string::npos) {
            auto receiver = context.callee.substr(0, separator);
            auto method_name = context.callee.substr(separator + 1);
            const auto* receiver_type = resolve_path_type(scope, receiver);
            if (receiver_type != nullptr && receiver_type->as<PointerType>() != nullptr) {
                receiver_type = receiver_type->as<PointerType>()->element;
            }
            if (const auto* structure = receiver_type != nullptr
                                            ? receiver_type->as<StructType>()
                                            : nullptr;
                structure != nullptr) {
                for (const auto* current = structure; current != nullptr;
                     current = current->base_type) {
                    for (const auto& method : current->methods) {
                        if (method.name == method_name) {
                            add_parameters(method.type, context, position, items, seen);
                        }
                    }
                }
            }
            return;
        }

        const auto& overloads = scope->lookup_function_overloads(context.callee);
        for (const auto* function : overloads) {
            add_parameters(function->function_type, context, position, items, seen);
        }

        const auto* type_symbol = scope->lookup_type(context.callee);
        const auto* constructors = type_symbol != nullptr && type_symbol->member_scope != nullptr
                                       ? type_symbol->member_scope->find_local_function_overloads(
                                             context.callee)
                                       : nullptr;
        if (constructors != nullptr) {
            for (const auto* constructor : *constructors) {
                add_parameters(constructor->function_type, context, position, items, seen);
            }
        }
    }

  public:
    static std::vector<Completion> complete(std::string_view content, Position position,
                                            const Scope* scope) {
        auto context = parse_cursor_context(content, position);

        std::vector<Completion> items;
        items.reserve(128);
        std::unordered_set<std::string> seen;

        auto target_line = position.line;

        if (context.is_static_access) {
            collect_static_members(scope, context.receiver, context.prefix, items, seen);
            return items;
        }

        if (context.is_member_access) {
            const auto* receiver_type = resolve_path_type(scope, context.receiver);

            if (receiver_type != nullptr) {
                collect_members_for_type(receiver_type, context.prefix, items, seen);
            }

            return items;
        }

        collect_named_arguments(scope, context, position, items, seen);

        if (scope != nullptr) {
            for (const auto* current_scope = scope; current_scope != nullptr;
                 current_scope = current_scope->parent) {
                for (const auto& [name, symbol] : current_scope->local_types()) {
                    if ((current_scope->kind == Scope::Kind::Type::Function ||
                         current_scope->kind == Scope::Kind::Type::Block) &&
                        symbol->span.start.line > target_line) {
                        continue;
                    }
                    if (seen.insert(name).second) {
                        items.emplace_back(name, CompletionKind::Type::Struct, "type");
                    }
                }

                for (const auto& [name, symbol] : current_scope->local_variables()) {
                    if ((current_scope->kind == Scope::Kind::Type::Function ||
                         current_scope->kind == Scope::Kind::Type::Block) &&
                        symbol->span.start.line > target_line) {
                        continue;
                    }
                    if (seen.insert(name).second) {
                        items.emplace_back(name, CompletionKind::Type::Variable, "variable");
                    }
                }

                for (const auto& [name, overloads] : current_scope->local_functions()) {
                    if ((current_scope->kind == Scope::Kind::Type::Function ||
                         current_scope->kind == Scope::Kind::Type::Block) &&
                        !overloads->functions.empty() &&
                        overloads->functions.front()->span.start.line > target_line) {
                        continue;
                    }
                    if (seen.insert(name).second) {
                        items.emplace_back(name, CompletionKind::Type::Function, "function");
                    }
                }

                for (const auto& [name, symbol] : current_scope->local_variants()) {
                    if ((current_scope->kind == Scope::Kind::Type::Function ||
                         current_scope->kind == Scope::Kind::Type::Block) &&
                        symbol->span.start.line > target_line) {
                        continue;
                    }
                    if (seen.insert(name).second) {
                        items.emplace_back(name, CompletionKind::Type::EnumMember, "variant");
                    }
                }
            }
        }

        for (const auto& builtin_name : BuiltinResolver::builtin_names()) {
            if (seen.insert(builtin_name).second) {
                items.emplace_back(builtin_name, CompletionKind::Type::Function, "builtin");
            }
        }

        for (const auto& [keyword, _] : keywords) {
            std::string name(keyword);
            if (seen.insert(name).second) {
                items.emplace_back(std::move(name), CompletionKind::Type::Keyword, "keyword");
            }
        }

        if (!context.prefix.empty()) {
            std::erase_if(items, [&context](const Completion& item) {
                return !item.label.starts_with(context.prefix);
            });
        }

        std::ranges::sort(items, [](const Completion& left, const Completion& right) {
            return left.label < right.label;
        });

        return items;
    }
};
