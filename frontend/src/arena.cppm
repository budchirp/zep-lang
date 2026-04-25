module;

export module zep.frontend.arena;

import zep.common.arena;
import zep.frontend.ast;
import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;

export using NodeArena = Arena<Node>;
export using TypeArena = Arena<Type>;
export using ScopeArena = Arena<Scope>;
export using SymbolArena = Arena<Symbol>;
