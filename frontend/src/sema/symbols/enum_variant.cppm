module;

#include <string>
#include <utility>

export module zep.frontend.sema.symbols:enum_variant;

export import :base;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.types;

export class EnumVariantSymbol final : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::EnumVariant;

    const EnumType* const enum_type;
    const EnumVariantType* const variant_type;

    EnumVariantSymbol(std::string name, Span span, Visibility::Type visibility,
                      const EnumType* enum_type, const EnumVariantType* variant_type)
        : Symbol(static_kind, std::move(name), span, visibility, enum_type), enum_type(enum_type),
          variant_type(variant_type) {}
};
