module;

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.lsp.analysis.symbols;

import zep.common.source.span;
import zep.common.source.position;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.token;
import zep.frontend.sema.symbols;
import zep.frontend.sema.types;
import zep.lsp.analysis.types;

export class Symbols {
  private:
    static std::string_view name(Node& node) {
        if (auto* value = node.as<TestDeclaration>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<FunctionDeclaration>(); value != nullptr) {
            return value->prototype->name;
        }

        if (auto* value = node.as<ExternFunctionDeclaration>(); value != nullptr) {
            return value->prototype->name;
        }

        if (auto* value = node.as<FunctionPrototype>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<StructDeclaration>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<EnumDeclaration>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<InterfaceDeclaration>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<TypeAliasDeclaration>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<VarDeclaration>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<ExternVarDeclaration>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<Field>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<Parameter>(); value != nullptr) {
            return value->name;
        }

        if (auto* value = node.as<EnumVariant>(); value != nullptr) {
            return value->name;
        }

        return {};
    }

    static std::optional<DocumentSymbol> document(Node& node, const std::filesystem::path& path,
            const std::vector<Token>& tokens, const std::unordered_map<Node*, Span>& ranges) {
        auto kind = CompletionKind::Type::Variable;
        std::vector<Node*> members;

        if (auto* structure = node.as<StructDeclaration>(); structure != nullptr) {
            kind = CompletionKind::Type::Struct;
            members.reserve(structure->fields.size() + structure->methods.size() +
                            structure->structs.size() + structure->enums.size());
            members.insert(members.end(), structure->fields.begin(), structure->fields.end());
            members.insert(members.end(), structure->methods.begin(), structure->methods.end());
            members.insert(members.end(), structure->structs.begin(), structure->structs.end());
            members.insert(members.end(), structure->enums.begin(), structure->enums.end());
        } else if (auto* enumeration = node.as<EnumDeclaration>(); enumeration != nullptr) {
            kind = CompletionKind::Type::Enum;
            members.reserve(enumeration->fields.size() + enumeration->variants.size() + enumeration->methods.size());
            members.insert(members.end(), enumeration->fields.begin(), enumeration->fields.end());
            members.insert(members.end(), enumeration->variants.begin(), enumeration->variants.end());
            members.insert(members.end(), enumeration->methods.begin(), enumeration->methods.end());
        } else if (auto* interface = node.as<InterfaceDeclaration>(); interface != nullptr) {
            kind = CompletionKind::Type::Interface;
            members.reserve(interface->methods.size());
            members.insert(members.end(), interface->methods.begin(), interface->methods.end());
        } else if (auto* variant = node.as<EnumVariant>(); variant != nullptr) {
            kind = CompletionKind::Type::EnumMember;
            members.reserve(variant->fields.size());
            members.insert(members.end(), variant->fields.begin(), variant->fields.end());
        } else if (node.is<FunctionDeclaration>() || node.is<ExternFunctionDeclaration>() ||
                   node.is<FunctionPrototype>() || node.is<TestDeclaration>()) {
            kind = CompletionKind::Type::Function;
        } else if (node.is<Field>()) {
            kind = CompletionKind::Type::Field;
        } else if (node.is<TypeAliasDeclaration>()) {
            kind = CompletionKind::Type::TypeParameter;
        } else if (!node.is<VarDeclaration>() && !node.is<ExternVarDeclaration>()) {
            return std::nullopt;
        }

        const auto label = name(node);

        if (label.empty()) {
            return std::nullopt;
        }

        std::vector<DocumentSymbol> children;
        children.reserve(members.size());

        for (auto* member : members) {
            if (auto child = document(*member, path, tokens, ranges); child.has_value()) {
                if (child->kind == CompletionKind::Type::Function) {
                    child->kind = CompletionKind::Type::Method;
                }

                children.push_back(std::move(*child));
            }
        }

        auto full = ranges.contains(&node) ? ranges.at(&node) : node.span;
        const auto selected = selection(node, tokens);
        full = cover(full, selected);

        if (node.is<StructDeclaration>() || node.is<EnumDeclaration>() || node.is<InterfaceDeclaration>()) {
            auto depth = std::size_t{0};
            auto parentheses = std::size_t{0};

            for (const auto& token : tokens) {
                if (std::pair(token.span.start.line, token.span.start.column) <
                    std::pair(selected.end.line, selected.end.column)) {
                    continue;
                }

                if (token.type == Token::Type::LeftParen) {
                    ++parentheses;
                } else if (token.type == Token::Type::RightParen && parentheses > 0) {
                    --parentheses;
                }

                if (parentheses != 0) {
                    continue;
                }

                if (token.type == Token::Type::LeftBrace) {
                    ++depth;
                } else if (token.type == Token::Type::RightBrace && depth > 0) {
                    --depth;

                    if (depth == 0) {
                        full = cover(full, token.span);

                        break;
                    }
                }
            }
        }

        DocumentSymbol result(std::string(label), kind, AnalysisLocation(path, full), selected,
                              std::move(children));
        result.location.span = range(result);

        const auto* function = node.as<FunctionDeclaration>();
        const auto* external = node.as<ExternFunctionDeclaration>();
        const FunctionType* signature = nullptr;

        if (function != nullptr && function->function_symbol != nullptr) {
            signature = function->function_symbol->signature();
        } else if (external != nullptr && external->function_symbol != nullptr) {
            signature = external->function_symbol->signature();
        }

        if (signature != nullptr) {
            result.detail = signature->to_string();
        }

        return result;
    }

  public:
    static Span cover(Span first, Span second) {
        if (first.start.line == 0) {
            return second;
        }

        if (second.start.line == 0) {
            return first;
        }

        const auto start = std::min(std::pair(first.start.line, first.start.column),
                                    std::pair(second.start.line, second.start.column));
        const auto end = std::max(std::pair(first.end.line, first.end.column),
                                  std::pair(second.end.line, second.end.column));

        return Span(Position(start.first, start.second), Position(end.first, end.second));
    }

    static Span range(const DocumentSymbol& symbol) {
        auto result = cover(symbol.location.span, symbol.selection);

        for (const auto& child : symbol.children) {
            result = cover(result, range(child));
        }

        return result;
    }

    static Span selection(Node& node, const std::vector<Token>& tokens) {
        if (auto* test = node.as<TestDeclaration>(); test != nullptr) {
            return test->name_span;
        }

        if (auto* call = node.as<CallExpression>(); call != nullptr) {
            return selection(*call->callee, tokens);
        }

        const auto* member = node.as<MemberExpression>();
        const auto label = member != nullptr ? std::string_view(member->member) : name(node);
        auto start = node.span.start;
        auto result = node.span;

        if (auto* function = node.as<FunctionDeclaration>(); function != nullptr) {
            start = function->prototype->span.start;
        } else if (auto* function = node.as<ExternFunctionDeclaration>(); function != nullptr) {
            start = function->prototype->span.start;
        }

        for (auto index = std::size_t{0}; !label.empty() && index < tokens.size(); ++index) {
            const auto& token = tokens[index];

            if (std::pair(token.span.start.line, token.span.start.column) < std::pair(start.line, start.column)) {
                continue;
            }

            if (member != nullptr && std::pair(token.span.start.line, token.span.start.column) >=
                std::pair(node.span.end.line, node.span.end.column)) {
                break;
            }

            if (token.value == label) {
                result = Span(token.span.start,
                    Position(token.span.start.line, token.span.start.column + token.value.size()));

                if (member == nullptr) {
                    return result;
                }
            }

            if (label.starts_with('~') && token.type == Token::Type::Tilde && index + 1 < tokens.size() &&
                tokens[index + 1].value == label.substr(1)) {
                return cover(token.span, tokens[index + 1].span);
            }

            if (label == "operator=" && token.value == "operator" && index + 1 < tokens.size()) {
                return cover(token.span, tokens[index + 1].span);
            }
        }

        return result;
    }

    static std::vector<DocumentSymbol> documents(const Program& program, const std::filesystem::path& path,
            const std::vector<Token>& tokens, const std::unordered_map<Node*, Span>& ranges) {
        std::vector<DocumentSymbol> result;
        result.reserve(program.statements.size());

        for (auto* statement : program.statements) {
            if (auto symbol = document(*statement, path, tokens, ranges); symbol.has_value()) {
                result.push_back(std::move(*symbol));
            }
        }

        return result;
    }
};
