module;

#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.symbols:type;

export import :base;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.types;

export class TypeSymbol final : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Type;

    TypeSymbol(std::string name, Span span, Visibility::Type visibility, const Type* type,
               std::vector<AttributeInfo> attributes = {})
        : Symbol(static_kind, std::move(name), span, visibility, type, std::move(attributes)) {}
};
