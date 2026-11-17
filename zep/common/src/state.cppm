module;

#include <utility>

export module zep.common.state;

export template <typename Value>
class StateGuard {
  private:
    Value& state;
    Value previous;

  public:
    StateGuard(Value& state, Value value)
        : state(state), previous(std::exchange(state, std::move(value))) {}

    StateGuard(const StateGuard&) = delete;
    StateGuard& operator=(const StateGuard&) = delete;

    ~StateGuard() { state = std::move(previous); }
};
