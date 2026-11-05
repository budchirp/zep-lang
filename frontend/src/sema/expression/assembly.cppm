module;

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

export module zep.frontend.sema.expression.assembly;

import zep.common.context;
import zep.frontend.kind;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.expression.values;

export class AssemblyChecker {
  public:
    static void check(AssemblyExpression& node, Context& context, SemaContext& sema,
                      TypeResolver& resolver) {
        auto& assembly = node.assembly;
        std::unordered_map<std::string, std::size_t> indices;
        std::unordered_set<std::string> names;
        auto outputs = std::size_t{0};

        static const std::unordered_set<std::string> registers = {
            "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
            "eax", "ebx", "ecx", "edx", "esi", "edi", "ax", "bx", "cx", "dx", "al", "bl", "cl", "dl"};

        auto append = [&](const std::string& constraint) -> void {
            if (!assembly.constraints.empty()) {
                assembly.constraints += ',';
            }

            assembly.constraints += constraint;
        };

        for (const auto& operand : assembly.operands) {
            auto* argument = node.arguments[operand.argument];
            const auto* type = resolver.resolve_type(argument->type);

            if (!names.insert(operand.name).second) {
                context.diagnostics.add_error(operand.span, "duplicate assembly operand '" + operand.name + "'");
            }

            if (type != nullptr && !type->is<IntegerType>() && !type->is<PointerType>() &&
                !type->is<CharType>() && !type->is<BooleanType>()) {
                context.diagnostics.add_error(operand.span, "assembly register operands require integer or pointer values");
            }

            if (operand.constraint != "r" && operand.constraint != "i" && !registers.contains(operand.constraint)) {
                context.diagnostics.add_error(operand.span, "unsupported assembly register constraint '" + operand.constraint + "'");
            }

            if (operand.kind == AssemblyOperand::Kind::Type::Input) {
                if (operand.constraint == "i" && !argument->compile_time_value.has_value()) {
                    context.diagnostics.add_error(operand.span, "assembly immediate requires a compile-time value");
                }

                continue;
            }

            if (!Values::mutable_place(*argument, sema.env)) {
                context.diagnostics.add_error(operand.span, "assembly output requires a mutable place");
            }

            if (operand.constraint == "i") {
                context.diagnostics.add_error(operand.span, "assembly outputs cannot use immediate constraints");
            }

            indices.emplace(operand.name, outputs++);
            append("=&" + (operand.constraint == "r" ? operand.constraint : "{" + operand.constraint + "}"));
        }

        auto input = outputs;

        for (const auto& operand : assembly.operands) {
            if (operand.kind == AssemblyOperand::Kind::Type::Output) {
                continue;
            }

            if (operand.kind == AssemblyOperand::Kind::Type::InputOutput) {
                append(std::to_string(indices.at(operand.name)));
            } else {
                indices.emplace(operand.name, input);
                append(operand.constraint == "r" || operand.constraint == "i"
                           ? operand.constraint : "{" + operand.constraint + "}");
            }

            ++input;
        }

        std::unordered_set<std::string> options;

        for (const auto& option : assembly.options) {
            if (!options.insert(option).second ||
                (option != "att" && option != "intel" && option != "nomem" && option != "pure")) {
                context.diagnostics.add_error(node.span, "invalid or duplicate assembly option '" + option + "'");
            }
        }

        if (options.contains("att") && options.contains("intel")) {
            context.diagnostics.add_error(node.span, "assembly dialect options are mutually exclusive");
        }

        assembly.pure = options.contains("pure");
        assembly.intel = options.contains("intel");
        const auto nomem = options.contains("nomem");

        if (assembly.pure && (!nomem || outputs == 0)) {
            context.diagnostics.add_error(node.span, "pure assembly requires outputs and nomem");
        }

        if (!nomem) {
            append("~{memory}");
        }

        std::unordered_set<std::string> clobbers;

        for (const auto& clobber : assembly.clobbers) {
            if ((!registers.contains(clobber) && clobber != "flags" && clobber != "memory") ||
                !clobbers.insert(clobber).second) {
                context.diagnostics.add_error(node.span, "invalid, reserved, or duplicate assembly clobber '" + clobber + "'");
            }

            if (clobber == "memory") {
                if (nomem) {
                    context.diagnostics.add_error(node.span, "nomem assembly cannot clobber memory");
                }
            } else {
                append("~{" + clobber + "}");
            }
        }

        for (auto index = std::size_t{0}; index < node.text.size(); ++index) {
            const auto character = node.text[index];

            if ((character == '{' || character == '}') && index + 1 < node.text.size() &&
                node.text[index + 1] == character) {
                assembly.text += character;
                ++index;
            } else if (character == '{') {
                const auto end = node.text.find('}', index + 1);
                const auto name = node.text.substr(index + 1, end - index - 1);

                if (end == std::string::npos || !indices.contains(name)) {
                    context.diagnostics.add_error(node.span, "unknown or unterminated assembly operand placeholder");
                    break;
                }

                assembly.text += "${" + std::to_string(indices.at(name)) + "}";
                index = end;
            } else if (character == '}') {
                context.diagnostics.add_error(node.span, "unmatched closing brace in assembly template");
                break;
            } else {
                assembly.text += character == '$' ? "$$" : std::string(1, character);
            }
        }

        node.type = sema.primitives.entries.at("void");
    }
};
