module;

#include <unordered_map>

export module zep.frontend.sema.scope.definition;

import zep.frontend.node;
import zep.frontend.sema.scope;

export class FunctionDefinition {
  public:
    const FunctionSymbol* symbol;
    const FunctionDeclaration* declaration;

    FunctionDefinition(const FunctionSymbol& symbol, const FunctionDeclaration& declaration)
        : symbol(&symbol), declaration(&declaration) {}
};

export class DefinitionRegistry {
  private:
    std::unordered_map<const FunctionSymbol*, FunctionDefinition> definitions;

  public:
    void add(const FunctionSymbol& symbol, const FunctionDeclaration& declaration) {
        definitions.emplace(&symbol, FunctionDefinition(symbol, declaration));
    }

    const FunctionDefinition* find(const FunctionSymbol& symbol) const {
        const auto iterator = definitions.find(&symbol);
        return iterator == definitions.end() ? nullptr : &iterator->second;
    }
};
