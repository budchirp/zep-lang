module;

#include <utility>
#include <vector>

export module zep.frontend.arena.node;

import zep.frontend.ast;

export class NodeArena {
  private:
    std::vector<Node*> nodes;

  public:
    NodeArena() = default;

    NodeArena(const NodeArena&) = delete;
    NodeArena& operator=(const NodeArena&) = delete;
    NodeArena(NodeArena&&) = delete;
    NodeArena& operator=(NodeArena&&) = delete;

    ~NodeArena() {
        for (auto* node : nodes) {
            delete node;
        }
    }

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        T* pointer = new T(std::forward<Args>(args)...);
        nodes.push_back(pointer);
        return pointer;
    }
};
