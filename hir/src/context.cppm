module;

#include <memory>

export module zep.hir.context;

import zep.common.context;
import zep.hir.node;

export class HIRContext {
  private:
    std::unique_ptr<HIRNodeArena> node_arena;

  public:
    HIRNodeArena& nodes;

    HIRContext() : node_arena(std::make_unique<HIRNodeArena>()), nodes(*node_arena) {}
    HIRContext(const HIRContext&) = delete;
    HIRContext& operator=(const HIRContext&) = delete;
    HIRContext(HIRContext&&) = delete;
    HIRContext& operator=(HIRContext&& other) noexcept = delete;
};
