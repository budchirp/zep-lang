module;

#include <cstdint>
#include <string>

export module zep.sema.kinds;

export enum class Linkage : std::uint8_t { External, Internal };

export enum class Visibility : std::uint8_t { Public, Private };

export enum class StorageKind : std::uint8_t { Const, Var, VarMut };

export std::string linkage_string(Linkage linkage) {
    return linkage == Linkage::External ? "external" : "internal";
}

export std::string visibility_string(Visibility visibility) {
    return visibility == Visibility::Public ? "public" : "private";
}

export std::string storage_kind_string(StorageKind kind) {
    switch (kind) {
    case StorageKind::Const:
        return "const";
    case StorageKind::Var:
        return "var";
    case StorageKind::VarMut:
        return "var mut";
    }
}
