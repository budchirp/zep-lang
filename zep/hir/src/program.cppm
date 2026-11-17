module;

#include <vector>
#include <string>
#include <utility>

export module zep.hir.program;

import zep.hir.node;
import zep.frontend.sema.types;

export class HIRType {
  public:
    std::string name;
    const Type* type;

    HIRType(std::string name, const Type* type) : name(std::move(name)), type(type) {}
};

export class HIRProgram {
  public:
    HIRNodeArena nodes;

    std::vector<HIRType> referenced_types;

    std::vector<HIRStatement*> statements;

    HIRProgram() = default;
    HIRProgram(const HIRProgram&) = delete;
    HIRProgram& operator=(const HIRProgram&) = delete;
    HIRProgram(HIRProgram&&) = delete;
    HIRProgram& operator=(HIRProgram&&) = delete;
};
