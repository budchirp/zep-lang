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
import zep.frontend.kind;
import zep.frontend.sema.expression.builtin;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;
import zep.frontend.token.keywords;
import zep.frontend.lexer;
import zep.frontend.token;
import zep.frontend.sema.resolution.callable;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.context;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.expression.values;
import zep.common.diagnostic.collection;
import zep.lsp.analysis.index;
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
    bool type_only = false;
    bool suppressed = false;
    bool is_field_list = false;
    bool is_cast = false;
    std::size_t type_start = 0;
    std::string generic;
    std::size_t generic_index = 0;
    std::size_t positional_arguments = 0;
    Position access;
    Position call;

    ParsedCursorContext(std::string receiver, std::string prefix, bool is_member_access,
                        bool is_static_access = false)
        : receiver(std::move(receiver)), prefix(std::move(prefix)),
          is_member_access(is_member_access), is_static_access(is_static_access) {}
};

export class Completer {
  private:
    static bool scan_arguments(ParsedCursorContext& context, std::string_view content) {
        Lexer lexer(content);
        auto nesting = std::size_t{0};
        auto named = false;
        auto value = false;
        std::string identifier;

        while (true) {
            const auto token = lexer.next_token();

            if (token.type == Token::Type::Eof) {
                return !named && nesting == 0;
            }

            if (nesting == 0 && token.type == Token::Type::Comma) {
                if (value && !named) {
                    ++context.positional_arguments;
                }

                named = false;
                value = false;
                identifier.clear();

                continue;
            }

            if (nesting == 0) {
                value = true;

                if (token.type == Token::Type::Colon && !identifier.empty()) {
                    context.named_arguments.insert(identifier);
                    named = true;
                }

                identifier = token.type == Token::Type::Identifier ? std::string(token.value) : "";
            }

            if (token.type == Token::Type::LeftParen || token.type == Token::Type::LeftBrace ||
                token.type == Token::Type::LeftBracket) {
                ++nesting;
            } else if (nesting > 0 &&
                       (token.type == Token::Type::RightParen || token.type == Token::Type::RightBrace ||
                        token.type == Token::Type::RightBracket)) {
                --nesting;
            }
        }
    }

    static std::size_t offset(std::string_view content, Position position) {
        auto start = std::size_t{0};

        for (auto line = std::size_t{1}; line < position.line; ++line) {
            const auto end = content.find('\n', start);

            if (end == std::string_view::npos) {
                return content.size();
            }

            start = end + 1;
        }

        auto end = content.find('\n', start);

        if (end == std::string_view::npos) {
            end = content.size();
        }

        return start + std::min(position.column > 0 ? position.column - 1 : 0, end - start);
    }

    static bool declared_type(std::string_view content, std::string name) {
        name = name.substr(0, std::min(name.find('<'), name.find("::")));
        Lexer lexer(content);
        auto declaration = false;
        auto import_line = std::size_t{0};

        while (true) {
            const auto token = lexer.next_token();

            if (token.type == Token::Type::Eof) {
                return false;
            }

            if (token.span.start.line != import_line) {
                import_line = 0;
            }

            if (token.type == Token::Type::Identifier && token.value == name &&
                (declaration || import_line != 0)) {
                return true;
            }

            declaration = token.type == Token::Type::Struct || token.type == Token::Type::Enum ||
                          token.type == Token::Type::Interface || token.type == Token::Type::Type ||
                          token.type == Token::Type::Fn;

            if (token.type == Token::Type::Import) {
                import_line = token.span.start.line;
            }
        }
    }

    static bool code_at(std::string_view content, Position position, std::size_t offset,
                         ParsedCursorContext& context) {
        Lexer lexer(content);
        auto boundary = std::size_t{0};
        auto angles = std::size_t{0};
        std::string identifier;

        while (true) {
            const auto token = lexer.next_token();
            const auto after = token.span.start.line > position.line ||
                (token.span.start.line == position.line && token.span.start.column >= position.column);

            if (after || token.type == Token::Type::Eof) {
                const auto gap = content.substr(std::min(boundary, offset), offset - std::min(boundary, offset));

                return std::ranges::all_of(gap, [](unsigned char character) {
                    return std::isspace(character) != 0;
                });
            }

            if ((token.type == Token::Type::String || token.type == Token::Type::Char) &&
                token.span.contains(position)) {
                const auto quote = token.type == Token::Type::String ? '"' : '\'';
                const auto ended = token.span.end.line == position.line &&
                    token.span.end.column == position.column && offset > 0 && content[offset - 1] == quote;

                return ended;
            }

            switch (token.type) {
            case Token::Type::Colon:
            case Token::Type::Arrow:
            case Token::Type::As:
            case Token::Type::Is:
                if ((token.type == Token::Type::As || token.type == Token::Type::Is) &&
                    token.span.contains(position)) {
                    break;
                }

                context.type_only = true;
                context.is_cast = token.type == Token::Type::As || token.type == Token::Type::Is;
                context.type_start = lexer.take_checkpoint().byte_position;
                break;
            case Token::Type::LessThan:
                if (angles == 0 && !identifier.empty() && declared_type(content, identifier)) {
                    if (!context.type_only) {
                        context.type_start = boundary;
                    }

                    context.type_only = true;
                    context.generic = identifier;
                    context.generic_index = 0;
                }

                if (context.type_only) {
                    ++angles;
                }
                break;
            case Token::Type::GreaterThan:
                if (angles > 0) {
                    --angles;
                }

                if (angles == 0) {
                    context.generic.clear();
                }
                break;
            case Token::Type::Comma:
                if (angles > 0) {
                    if (angles == 1) {
                        ++context.generic_index;
                    }

                    break;
                }
                [[fallthrough]];
            case Token::Type::Assign:
            case Token::Type::Semicolon:
            case Token::Type::LeftBrace:
            case Token::Type::RightBrace:
            case Token::Type::RightParen:
            case Token::Type::Return:
            case Token::Type::Var:
            case Token::Type::Const:
            case Token::Type::Fn:
            case Token::Type::Plus:
            case Token::Type::Minus:
                context.type_only = false;
                context.is_cast = false;
                angles = 0;
                context.generic.clear();
                break;
            default:
                break;
            }

            identifier = token.type == Token::Type::Identifier ? std::string(token.value) : "";

            if (token.span.contains(position)) {
                return true;
            }

            boundary = lexer.take_checkpoint().byte_position;
        }
    }

    static ParsedCursorContext parse_cursor_context(std::string_view content, Position position) {
        const auto cursor = offset(content, position);
        const auto newline = cursor > 0 ? content.rfind('\n', cursor - 1) : std::string_view::npos;
        const auto line_start = newline != std::string_view::npos ? newline + 1 : 0;
        const auto line_end = content.find('\n', line_start);
        const auto current_line = content.substr(line_start,
            line_end != std::string_view::npos ? line_end - line_start : content.size() - line_start);
        const auto cursor_column = cursor - line_start;
        auto line_prefix = current_line.substr(0, cursor_column);
        ParsedCursorContext lexical("", "", false);

        if (!code_at(content, position, line_start + cursor_column, lexical)) {
            lexical.suppressed = true;

            return lexical;
        }

        std::size_t end_ident = line_prefix.size();
        while (end_ident > 0 &&
               (std::isalnum(static_cast<unsigned char>(line_prefix[end_ident - 1])) != 0 ||
                line_prefix[end_ident - 1] == '_')) {
            --end_ident;
        }

        if (end_ident > 0 && line_prefix[end_ident - 1] == '#') {
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
            auto nesting = std::size_t{0};

            while (receiver_start > 0) {
                const auto character = line_prefix[receiver_start - 1];

                if (character == '>') {
                    ++nesting;
                } else if (character == '<' && nesting > 0) {
                    --nesting;
                } else if (nesting == 0 && std::isalnum(static_cast<unsigned char>(character)) == 0 &&
                           character != '_') {
                    break;
                }

                --receiver_start;
            }

            std::string receiver(
                line_prefix.substr(receiver_start, receiver_end - receiver_start));
            return ParsedCursorContext(std::move(receiver), std::move(prefix), false, true);
        }

        const auto arrow = end_ident >= 2 && line_prefix.substr(end_ident - 2, 2) == "->";

        if (end_ident > 0 && (line_prefix[end_ident - 1] == '.' || arrow)) {
            if (end_ident >= 2 && line_prefix[end_ident - 2] == '.') {
                return ParsedCursorContext("", std::move(prefix), false);
            }

            std::size_t dot_position = end_ident - (arrow ? 2 : 1);
            const auto access = Position(position.line, dot_position + 1);
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
            ParsedCursorContext result(std::move(receiver), std::move(prefix), true);
            result.access = access;

            return result;
        }

        ParsedCursorContext result("", std::move(prefix), false);
        result.type_only = lexical.type_only;
        result.is_cast = lexical.is_cast;
        result.type_start = lexical.type_start;
        result.generic = std::move(lexical.generic);
        result.generic_index = lexical.generic_index;
        const auto before_cursor = content.substr(0, cursor);
        const auto brace = before_cursor.rfind('{');

        if (brace != std::string_view::npos) {
            auto end = brace;

            while (end > 0 && std::isspace(static_cast<unsigned char>(content[end - 1])) != 0) {
                --end;
            }

            auto start = end;
            auto nesting = std::size_t{0};

            while (start > 0) {
                const auto character = content[start - 1];

                if (character == '>') {
                    ++nesting;
                } else if (character == '<' && nesting > 0) {
                    --nesting;
                } else if (nesting == 0 && std::isalnum(static_cast<unsigned char>(character)) == 0 &&
                           character != '_' && character != ':') {
                    break;
                }

                --start;
            }

            if (start < end) {
                auto before = content.substr(0, start);

                while (!before.empty() && std::isspace(static_cast<unsigned char>(before.back())) != 0) {
                    before.remove_suffix(1);
                }

                if (!before.ends_with("->") && !before.ends_with("struct") &&
                    !before.ends_with("enum") && !before.ends_with("interface")) {
                    result.receiver = std::string(content.substr(start, end - start));
                    result.is_field_list = scan_arguments(result, before_cursor.substr(brace + 1));
                }
            }
        }
        auto cursor_offset = line_start + cursor_column;
        auto nested_parentheses = std::size_t{0};
        auto open_parenthesis = std::string_view::npos;
        for (auto index = cursor_offset; index > 0; --index) {
            auto character = content[index - 1];

            if (nested_parentheses == 0 && (character == '{' || character == '}' || character == ';')) {
                break;
            }
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

        if (callee_end > 0 && content[callee_end - 1] == '>') {
            auto nesting = std::size_t{1};
            --callee_end;

            while (callee_end > 0 && nesting > 0) {
                --callee_end;

                if (content[callee_end] == '>') {
                    ++nesting;
                } else if (content[callee_end] == '<') {
                    --nesting;
                }
            }
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

        auto declaration = callee_start;

        while (declaration > 0 && std::isspace(static_cast<unsigned char>(content[declaration - 1])) != 0) {
            --declaration;
        }

        if (declaration >= 2 && content.substr(declaration - 2, 2) == "fn") {
            return result;
        }

        result.callee = std::string(content.substr(callee_start, callee_end - callee_start));
        result.named_arguments.clear();
        result.positional_arguments = 0;
        result.is_field_list = false;
        result.is_argument_list = true;
        const auto size_builtin = callee_start > 0 && content[callee_start - 1] == '#' && result.callee == "sizeof";
        result.type_only = size_builtin || result.is_cast;

        if (size_builtin) {
            result.type_start = open_parenthesis + 1;
        }
        result.call = Position(1, 1);

        for (auto index = std::size_t{0}; index < open_parenthesis; ++index) {
            if (content[index] == '\n') {
                result.call.next_line();
                result.call.column = 1;
            } else {
                result.call.increment_column();
            }
        }

        result.is_argument_list = !size_builtin && scan_arguments(result, content.substr(open_parenthesis + 1,
                                                                  cursor_offset - open_parenthesis - 1));

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

    static const NominalType* enclosing(const Scope* scope) {
        for (const auto* current = scope; current != nullptr; current = current->parent) {
            if (current->kind != Scope::Kind::Type::Struct && current->kind != Scope::Kind::Type::Enum &&
                current->kind != Scope::Kind::Type::Interface) {
                continue;
            }

            const auto* symbol = current->lookup_type(current->name);

            if (symbol != nullptr && symbol->type != nullptr) {
                return symbol->type->as_nominal();
            }
        }

        return nullptr;
    }

    static void collect_members_for_type(const Type* type, const ParsedCursorContext& context,
                                         const Scope* scope, std::vector<Completion>& items,
                                         std::unordered_set<std::string>& seen,
                                         const Expression* receiver = nullptr, SemaContext* sema = nullptr) {
        auto append = [&](const std::string& name, const Type* member_type,
                          CompletionKind::Type kind) -> void {
            if (!name.starts_with(context.prefix) || !seen.insert(name).second) {
                return;
            }

            items.emplace_back(name, kind, member_type != nullptr ? member_type->to_string() : "");
        };

        if (type == nullptr) {
            return;
        }

        if (auto* pointer = type->as<PointerType>(); pointer != nullptr) {
            type = pointer->element;
        }

        if (type != nullptr && type->as_nominal() == nullptr && sema != nullptr) {
            Diagnostics diagnostics;
            TypeResolver resolver(sema->types, sema->env, diagnostics);
            Facades facades(sema->facades, resolver);
            type = facades.wrapper(type);
        }

        const auto* owner = enclosing(scope);

        while (type != nullptr) {
            const auto* structure = type->as<StructType>();
            const auto* members = Scope::members(type);

            if (structure != nullptr && !context.is_static_access) {
                const auto inherited = structure->base_type != nullptr ? structure->base_type->fields.size() : 0;

                for (auto index = inherited; index < structure->fields.size(); ++index) {
                    const auto& field = structure->fields[index];
                    if (MemberResolver::is_visible(field.visibility, owner, structure)) {
                        append(field.name, field.type, CompletionKind::Type::Field);
                    }
                }
            }

            if (members != nullptr) {
                for (const auto& [name, overloads] : members->local_functions()) {
                    for (const auto* function : overloads) {
                        if (function == nullptr || function->is_deleted || name == "operator=" ||
                            (context.is_static_access ? !function->static_method() : !function->instance()) ||
                            (!function->extension() &&
                             !MemberResolver::is_visible(function->visibility, owner, type->as_nominal()))) {
                            continue;
                        }

                        const auto* signature = function->signature();

                        if (!context.is_static_access && receiver != nullptr && sema != nullptr) {
                            const auto* self = signature != nullptr && !signature->parameters.empty()
                                ? signature->parameters.front().type->as<PointerType>() : nullptr;
                            const auto* pointer = receiver->type != nullptr ? receiver->type->as<PointerType>() : nullptr;
                            const auto moving = Values::moving(*receiver);
                            const auto temporary = pointer == nullptr && !Values::place(*receiver);
                            const auto is_mutable = pointer != nullptr ? pointer->is_mutable
                                : temporary || moving || Values::mutable_place(*receiver, sema->env);

                            if ((function->has_attribute(".move_receiver") && !moving && !temporary) ||
                                (self != nullptr && self->is_mutable && !is_mutable)) {
                                continue;
                            }
                        }

                        append(name, signature, CompletionKind::Type::Method);
                    }
                }
            } else if (const auto* interface = type->as<InterfaceType>();
                       interface != nullptr && !context.is_static_access) {
                for (const auto& method : interface->methods) {
                    append(method.name, method.type, CompletionKind::Type::Method);
                }
            }

            if (const auto* enumeration = type->as<EnumType>();
                enumeration != nullptr && context.is_static_access) {
                for (const auto& variant : enumeration->variants) {
                    append(variant.name, type, CompletionKind::Type::EnumMember);
                }
            }

            type = structure != nullptr ? structure->base_type : nullptr;
        }
    }

    static void add_parameters(const FunctionType* function_type,
                               const ParsedCursorContext& context, Position position,
                               std::vector<Completion>& items,
                               std::unordered_set<std::string>& seen) {
        if (function_type == nullptr) {
            return;
        }

        for (const auto& name : context.named_arguments) {
            if (std::ranges::none_of(function_type->parameters, [&name](const auto& parameter) {
                    return parameter.name == name;
                })) {
                return;
            }
        }

        auto replacement = Span(
            Position(position.line, position.column - context.prefix.length()), position);
        auto positional = context.positional_arguments;

        for (const auto& parameter : function_type->parameters) {
            if (parameter.name == "self") {
                continue;
            }

            if (positional > 0) {
                --positional;

                continue;
            }

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
                                        std::unordered_set<std::string>& seen,
                                        const SyntaxIndex* syntax) {
        if (scope == nullptr || !context.is_argument_list || context.callee.empty()) {
            return;
        }

        if (syntax != nullptr) {
            for (const auto* node : syntax->nodes()) {
                const auto* call = node->as<CallExpression>();

                if (call == nullptr || call->span.start.line != context.call.line ||
                    call->span.start.column != context.call.column) {
                    continue;
                }

                const auto* type = call->callee->type;
                const auto* function = type != nullptr ? type->as<FunctionType>() : nullptr;

                if (function != nullptr) {
                    add_parameters(function, context, position, items, seen);

                    return;
                }

                if (const auto* member = call->callee->as<MemberExpression>(); member != nullptr) {
                    type = member->value->type;

                    if (const auto* pointer = type != nullptr ? type->as<PointerType>() : nullptr;
                        pointer != nullptr) {
                        type = pointer->element;
                    }

                    const auto* members = Scope::members(type);
                    const auto* overloads = members != nullptr
                        ? members->find_local_function_overloads(member->member) : nullptr;

                    if (overloads != nullptr) {
                        for (const auto* candidate : *overloads) {
                            if (!candidate->is_deleted) {
                                add_parameters(candidate->signature(), context, position, items, seen);
                            }
                        }

                        return;
                    }
                }
            }
        }

        if (auto separator = context.callee.rfind("::"); separator != std::string::npos) {
            auto type_name = context.callee.substr(0, separator);
            auto function_name = context.callee.substr(separator + 2);
            const auto* type_symbol = scope->lookup_type(type_name);
            const auto* members = Scope::members(type_symbol != nullptr ? type_symbol->type : nullptr);
            const auto* overloads = members != nullptr
                ? members->find_local_function_overloads(function_name) : nullptr;
            if (overloads != nullptr) {
                for (const auto* function : *overloads) {
                    add_parameters(function->signature(), context, position, items, seen);
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
            add_parameters(function->signature(), context, position, items, seen);
        }

        if (const auto* variable = scope->lookup_var(context.callee);
            variable != nullptr && variable->type != nullptr) {
            add_parameters(variable->type->as<FunctionType>(), context, position, items, seen);
        }

        const auto* type_symbol = scope->lookup_type(context.callee);
        const auto* members = Scope::members(type_symbol != nullptr ? type_symbol->type : nullptr);
        const auto* constructors = members != nullptr
            ? members->find_local_function_overloads(context.callee) : nullptr;
        if (constructors != nullptr) {
            for (const auto* constructor : *constructors) {
                add_parameters(constructor->signature(), context, position, items, seen);
            }
        }
    }

    static void collect_scope(const Scope& scope, const ParsedCursorContext& context, Position position,
                              std::vector<Completion>& items, std::unordered_set<std::string>& seen,
                              bool exports = false) {
        auto append = [&](const std::string& name, const Symbol* symbol, CompletionKind::Type kind) {
            if (symbol == nullptr || !name.starts_with(context.prefix) ||
                (exports && symbol->visibility != Visibility::Type::Public)) {
                return;
            }

            if ((scope.kind == Scope::Kind::Type::Function || scope.kind == Scope::Kind::Type::Block) &&
                (symbol->span.start.line > position.line ||
                 (symbol->span.start.line == position.line && symbol->span.start.column > position.column))) {
                return;
            }

            if (seen.insert(name).second) {
                items.emplace_back(name, kind, symbol->type != nullptr ? symbol->type->to_string() : "");
            }
        };

        for (const auto& [name, symbol] : scope.local_types()) {
            const auto* type = symbol->type;
            auto kind = CompletionKind::Type::Struct;

            if (type != nullptr && type->is<EnumType>()) {
                kind = CompletionKind::Type::Enum;
            } else if (type != nullptr && type->is<InterfaceType>()) {
                kind = CompletionKind::Type::Interface;
            } else if (type != nullptr && type->is<NamedType>()) {
                kind = CompletionKind::Type::TypeParameter;
            }

            append(name, symbol, kind);
        }

        if (context.type_only) {
            return;
        }

        for (const auto& [name, symbol] : scope.local_variables()) {
            append(name, symbol, CompletionKind::Type::Variable);
        }

        for (const auto& [name, overloads] : scope.local_functions()) {
            for (const auto* function : overloads) {
                if (function != nullptr && !function->is_deleted) {
                    append(name, function, CompletionKind::Type::Function);
                }
            }
        }

        for (const auto& [name, symbol] : scope.local_variants()) {
            append(name, symbol, CompletionKind::Type::EnumMember);
        }
    }

    static std::vector<Completion> finish(std::vector<Completion> items,
                                          const ParsedCursorContext& context, Position position,
                                          std::string_view content) {
        const auto start = Position(position.line, position.column - context.prefix.size());
        const auto cursor = offset(content, position);
        auto end = cursor;

        while (end < content.size() &&
               (std::isalnum(static_cast<unsigned char>(content[end])) != 0 || content[end] == '_')) {
            ++end;
        }

        for (auto& item : items) {
            auto replacement_end = end;

            if (item.insertion.ends_with(": ") && replacement_end < content.size() && content[replacement_end] == ':') {
                ++replacement_end;

                while (replacement_end < content.size() &&
                       (content[replacement_end] == ' ' || content[replacement_end] == '\t')) {
                    ++replacement_end;
                }
            }

            item.replacement = Span(start, Position(position.line, position.column + replacement_end - cursor));
        }

        std::ranges::sort(items, [](const auto& left, const auto& right) {
            return left.sort_key != right.sort_key ? left.sort_key < right.sort_key
                                                   : left.label < right.label;
        });

        return items;
    }

  public:
    static std::string recover(std::string_view content, Position position) {
        const auto context = parse_cursor_context(content, position);
        std::string result(content);

        if (context.suppressed) {
            return result;
        }

        const auto cursor = offset(content, position);
        auto prefix = content.substr(0, cursor);

        while (!prefix.empty() && std::isspace(static_cast<unsigned char>(prefix.back())) != 0) {
            prefix.remove_suffix(1);
        }

        if (prefix.ends_with('.') || prefix.ends_with("::") || prefix.ends_with("->")) {
            result.insert(cursor, "__zep_completion");
        } else if (context.type_only && context.prefix.empty()) {
            result.insert(cursor, "i32");
        } else if (context.is_field_list && !context.prefix.empty() &&
                   declared_type(content, context.receiver)) {
            auto end = cursor;

            while (end < content.size() &&
                   (std::isalnum(static_cast<unsigned char>(content[end])) != 0 || content[end] == '_')) {
                ++end;
            }

            auto separator = end;

            while (separator < content.size() &&
                   std::isspace(static_cast<unsigned char>(content[separator])) != 0) {
                ++separator;
            }

            if (separator == content.size() || content[separator] != ':') {
                result.insert(end, ": 0");
            }
        } else if (prefix.ends_with('=') || prefix.ends_with(':')) {
            result.insert(cursor, "0");
        }

        if (context.type_only) {
            Lexer type_tokens(std::string_view(result).substr(context.type_start));
            auto angles = std::size_t{0};
            auto parentheses = std::size_t{0};
            auto end = context.type_start;

            while (true) {
                const auto before = type_tokens.take_checkpoint().byte_position;
                const auto token = type_tokens.next_token();
                end = context.type_start + before;

                if (token.type == Token::Type::Eof || token.type == Token::Type::Assign ||
                    token.type == Token::Type::Semicolon || token.type == Token::Type::LeftBrace ||
                    token.type == Token::Type::RightBrace ||
                    (token.type == Token::Type::RightParen && parentheses == 0)) {
                    break;
                }

                if (token.type == Token::Type::LessThan) {
                    ++angles;
                } else if (token.type == Token::Type::GreaterThan && angles > 0) {
                    --angles;
                } else if (token.type == Token::Type::LeftParen) {
                    ++parentheses;
                } else if (token.type == Token::Type::RightParen && parentheses > 0) {
                    --parentheses;
                }
            }

            result.insert(end, angles, '>');
        }

        Lexer lexer(result);
        std::vector<Token::Type> delimiters;
        delimiters.reserve(16);

        while (true) {
            const auto token = lexer.next_token();

            if (token.type == Token::Type::Eof) {
                break;
            }

            if (token.type == Token::Type::LeftParen || token.type == Token::Type::LeftBrace ||
                token.type == Token::Type::LeftBracket) {
                delimiters.push_back(token.type);
            } else if (!delimiters.empty() &&
                       ((token.type == Token::Type::RightParen && delimiters.back() == Token::Type::LeftParen) ||
                        (token.type == Token::Type::RightBrace && delimiters.back() == Token::Type::LeftBrace) ||
                        (token.type == Token::Type::RightBracket && delimiters.back() == Token::Type::LeftBracket))) {
                delimiters.pop_back();
            }
        }

        if (!delimiters.empty()) {
            result += '\n';
        }

        while (!delimiters.empty()) {
            switch (delimiters.back()) {
            case Token::Type::LeftParen:
                result += ')';
                break;
            case Token::Type::LeftBracket:
                result += ']';
                break;
            default:
                result += '}';
                break;
            }

            delimiters.pop_back();
        }

        return result;
    }

    static std::vector<Completion> complete(std::string_view content, Position position,
                                            const Scope* scope, const SyntaxIndex* syntax = nullptr,
                                            SemaContext* sema = nullptr) {
        auto context = parse_cursor_context(content, position);

        if (context.suppressed) {
            return {};
        }

        std::vector<Completion> items;
        items.reserve(128);
        std::unordered_set<std::string> seen;

        if (!context.generic.empty() && scope != nullptr) {
            const std::vector<GenericParameterType>* parameters = nullptr;

            if (const auto* type = scope->lookup_type(context.generic); type != nullptr && type->type != nullptr) {
                if (const auto* nominal = type->type->as_nominal(); nominal != nullptr) {
                    parameters = &nominal->generic_parameters;
                }
            } else if (const auto* function = scope->lookup_function(context.generic);
                       function != nullptr && function->signature() != nullptr) {
                parameters = &function->signature()->generic_parameters;
            }

            if (parameters != nullptr && context.generic_index < parameters->size()) {
                context.type_only = !(*parameters)[context.generic_index].is_const();
            }
        }

        if (context.is_field_list && scope != nullptr) {
            const auto separator = context.receiver.rfind("::");
            const auto name = context.receiver.substr(0,
                std::min(context.receiver.find('<'), separator));
            const auto* symbol = scope->lookup_type(name);
            const auto* type = symbol != nullptr ? symbol->type : nullptr;

            if (type != nullptr && type->is<StructType>()) {
                collect_members_for_type(type, context, scope, items, seen, nullptr, sema);
                std::erase_if(items, [&context](const auto& item) {
                    return item.kind != CompletionKind::Type::Field || context.named_arguments.contains(item.label);
                });

                for (auto& item : items) {
                    item.insertion = item.label + ": ";
                }

                return finish(std::move(items), context, position, content);
            }

            if (const auto* enumeration = type != nullptr ? type->as<EnumType>() : nullptr;
                enumeration != nullptr && separator != std::string::npos) {
                const auto* variant = enumeration->find_variant(context.receiver.substr(separator + 2));

                if (variant != nullptr) {
                    for (const auto& field : variant->fields) {
                        if (field.name.starts_with(context.prefix) && !context.named_arguments.contains(field.name)) {
                            items.emplace_back(field.name, CompletionKind::Type::Field,
                                field.type->to_string(), Span(), field.name + ": ");
                        }
                    }

                    return finish(std::move(items), context, position, content);
                }
            }
        }

        if (context.is_static_access) {
            const auto name = context.receiver.substr(0, context.receiver.find('<'));
            const auto* symbol = scope != nullptr ? scope->lookup_type(name) : nullptr;

            if (symbol != nullptr) {
                collect_members_for_type(symbol->type, context, scope, items, seen, nullptr, sema);
            }

            return finish(std::move(items), context, position, content);
        }

        if (context.is_member_access) {
            const auto* receiver_type = resolve_path_type(scope, context.receiver);
            const Expression* receiver = nullptr;

            if (syntax != nullptr) {
                for (const auto* node : syntax->nodes()) {
                    if (const auto* member = node->as<MemberExpression>(); member != nullptr &&
                        member->span.start.line == context.access.line &&
                        member->span.start.column == context.access.column) {
                        receiver = member->value;
                        receiver_type = receiver->type;

                        break;
                    }
                }
            }

            if (receiver_type != nullptr) {
                collect_members_for_type(receiver_type, context, scope, items, seen, receiver, sema);
            } else if (receiver != nullptr && receiver->scope != nullptr &&
                       receiver->scope->kind == Scope::Kind::Type::Module) {
                collect_scope(*receiver->scope, context, position, items, seen, true);
            }

            return finish(std::move(items), context, position, content);
        }

        collect_named_arguments(scope, context, position, items, seen, syntax);

        if (scope != nullptr) {
            for (const auto* current_scope = scope; current_scope != nullptr;
                 current_scope = current_scope->parent) {
                collect_scope(*current_scope, context, position, items, seen);
            }
        }

        if (context.type_only) {
            if (std::string_view("move").starts_with(context.prefix) && seen.insert("move").second) {
                items.emplace_back("move", CompletionKind::Type::Keyword, "move reference", Span(), "move ");
            }

            auto before = content.substr(0, offset(content, position) - context.prefix.size());

            while (!before.empty() && std::isspace(static_cast<unsigned char>(before.back())) != 0) {
                before.remove_suffix(1);
            }

            if ((before.ends_with('*') || before.ends_with('&')) &&
                std::string_view("mut").starts_with(context.prefix) && seen.insert("mut").second) {
                items.emplace_back("mut", CompletionKind::Type::Keyword, "mutable reference or pointer", Span(), "mut ");
            }

            return finish(std::move(items), context, position, content);
        }

        for (const auto name : BuiltinChecker::names) {
            auto builtin_name = "#" + std::string(name);
            if (builtin_name.starts_with(context.prefix) && seen.insert(builtin_name).second) {
                items.emplace_back(builtin_name, CompletionKind::Type::Function, "builtin");
            }
        }

        for (const auto& [keyword, _] : keywords) {
            std::string name(keyword);
            if (name.starts_with(context.prefix) && seen.insert(name).second) {
                items.emplace_back(std::move(name), CompletionKind::Type::Keyword, "keyword");
            }
        }

        return finish(std::move(items), context, position, content);
    }
};
