module;

#include <cstdint>
#include <string>

export module zep.sema.kinds;

export enum class Visibility : std::uint8_t { Public, Private };

export enum class StorageKind : std::uint8_t { Const, Var, VarMut };

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
    return "unknown";
}
