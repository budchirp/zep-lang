module;

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.compiler.tests;

import zep.compiler.unit;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.declaration.attribute;
import zep.workspace.package;
import zep.frontend.kind;
import zep.common.source.span;

export class TestCase {
  public:
    Module& module;
    TestDeclaration& declaration;
    std::string name;
    std::string symbol;

    TestCase(Module& module, TestDeclaration& declaration, std::string symbol)
        : module(module), declaration(declaration),
          name(module.owner->manifest.name + "." + module.path.string() + "::" + declaration.name),
          symbol(std::move(symbol)) {}
};

export class TestSuite {
  private:
    static std::string identity(const Module& module, std::size_t ordinal) {
        const auto source = module.owner->manifest.name + ":" + module.scope->name;
        constexpr auto digits = "0123456789abcdef";
        auto result = std::string("__zep_test$");
        result.reserve(result.size() + source.size() * 2 + 24);

        for (const auto character : source) {
            const auto byte = static_cast<unsigned char>(character);
            result += digits[byte >> 4];
            result += digits[byte & 15];
        }

        return result + "$" + std::to_string(ordinal);
    }

    static void name(SemaContext& sema, std::vector<Attribute*>& attributes, Span span,
                     std::string symbol) {
        auto* value = sema.nodes.create<StringLiteral>(span, std::move(symbol));

        if (auto* attribute = find_attribute(attributes, ".test_name"); attribute != nullptr) {
            attribute->arguments = {value};
        } else {
            attributes.push_back(sema.nodes.create<Attribute>(
                span, ".test_name", std::vector<Expression*>{value}));
        }
    }

  public:
    std::vector<TestCase> cases;

    TestSuite(SemaContext& sema, std::vector<Module*> modules, const Package& root,
              const std::string& filter) : cases() {
        std::ranges::sort(modules, [](const Module* left, const Module* right) {
            if (left->owner->manifest.name != right->owner->manifest.name) {
                return left->owner->manifest.name < right->owner->manifest.name;
            }

            return left->path.string() < right->path.string();
        });

        auto capacity = std::size_t{0};

        for (const auto* module : modules) {
            capacity += module->syntax.statements.size();
        }

        cases.reserve(capacity);
        std::unordered_set<const Scope*> borrowed_scopes;
        borrowed_scopes.reserve(modules.size());

        for (auto* module : modules) {
            if (module->scope->parent != nullptr) {
                borrowed_scopes.insert(module->scope->parent);
            }
        }

        for (auto* module : modules) {
            const auto borrowed = borrowed_scopes.contains(module->scope);

            for (auto ordinal = std::size_t{0}; ordinal < module->syntax.statements.size(); ++ordinal) {
                auto* statement = module->syntax.statements[ordinal];

                if (auto* function = statement->as<FunctionDeclaration>(); function != nullptr) {
                    const auto linker_name = AttributeResolver::get_linker_name(
                        function->prototype->name, AttributeResolver::convert(function->attributes));

                    if (linker_name == "main" ||
                        (borrowed && function->visibility == Visibility::Type::Private &&
                         linker_name == function->prototype->name)) {
                        name(sema, function->attributes, function->span,
                             identity(*module, ordinal) + "$function");
                    }
                }

                if (auto* variable = statement->as<VarDeclaration>(); variable != nullptr && borrowed) {
                    const auto linker_name = AttributeResolver::get_linker_name(
                        variable->name, AttributeResolver::convert(variable->attributes));

                    name(sema, variable->attributes, variable->span,
                         linker_name == variable->name ? identity(*module, ordinal) + "$global"
                                                       : linker_name);
                }

                auto* test = statement->as<TestDeclaration>();

                if (test == nullptr || module->owner != &root) {
                    continue;
                }

                TestCase candidate(*module, *test, identity(*module, ordinal));
                name(sema, test->function->attributes, test->function->span, candidate.symbol);

                if (filter.empty() || candidate.name.contains(filter)) {
                    test->enabled = true;
                    cases.push_back(std::move(candidate));
                }
            }
        }
    }

    std::string dispatcher() const {
        auto source = std::string("extern fn strcmp(left: cstr, right: cstr) -> i32\n");
        source.reserve(256 + cases.size() * 256);

        for (auto index = std::size_t{0}; index < cases.size(); ++index) {
            source += "@name(\"" + cases[index].symbol + "\") extern fn invoke_" +
                std::to_string(index) + "() -> void\n";
        }

        source += "public fn main(count: i32, arguments: *cstr) -> i32 {\n"
                  "if (count != 2) { return 2 }\n";

        for (auto index = std::size_t{0}; index < cases.size(); ++index) {
            const auto number = std::to_string(index);
            source += "if (strcmp(arguments[1], \"" + number + "\") == 0) { invoke_" +
                number + "() return 0 }\n";
        }

        return source + "return 2\n}\n";
    }
};
