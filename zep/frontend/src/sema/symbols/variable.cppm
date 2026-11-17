module;

#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.symbols:variable;

export import :base;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.types;

export class VariableSymbol final : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Variable;

    const StorageKind::Type storage_kind;

    VariableSymbol(std::string name, Span span, Visibility::Type visibility,
                   StorageKind::Type storage_kind, const Type* type,
                   std::vector<AttributeInfo> attributes = {})
        : Symbol(static_kind, std::move(name), span, visibility, type, std::move(attributes)),
          storage_kind(storage_kind) {}
};
