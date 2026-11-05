module;

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.symbols:base;

import zep.common.source.span;
import zep.common.arena;
import zep.common.tagged;
import zep.frontend.kind;
import zep.frontend.sema.types;

export class AttributeInfo {
  public:
    std::string name;
    std::vector<std::string> arguments;

    AttributeInfo(std::string name, std::vector<std::string> arguments)
        : name(std::move(name)), arguments(std::move(arguments)) {}
};

export class Symbol : public Tagged {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Type, Variable, Function, EnumVariant };
    };

  protected:
    Symbol(Kind::Type kind, std::string name, Span span, Visibility::Type visibility,
           const Type* type, std::vector<AttributeInfo> attributes = {})
        : kind(kind), span(span), visibility(visibility), name(std::move(name)), type(type),
          attributes(std::move(attributes)) {}

  public:
    const Kind::Type kind;
    const Span span;
    const Visibility::Type visibility;
    const std::string name;
    const Type* type;
    std::vector<AttributeInfo> attributes;

    virtual ~Symbol() = default;

    const AttributeInfo* find_attribute(const std::string& attribute_name) const {
        for (const auto& attribute : attributes) {
            if (attribute.name == attribute_name) {
                return &attribute;
            }
        }

        return nullptr;
    }

    bool has_attribute(const std::string& attribute_name) const {
        return find_attribute(attribute_name) != nullptr;
    }
};

export using SymbolArena = Arena<Symbol>;
