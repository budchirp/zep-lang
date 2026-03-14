# Code Standards

## Naming Conventions

- **Files**: `snake_case`

- **Classes**: `PascalCase`
- **Enums/Enum Values**: `PascalCase`
- **Functions/Methods**: `snake_case`
- **Variables/Parameters/Fields**: `snake_case`

## Code Style

- DO NOT add comments to code
- ALWAYS write explicit `private:` and `public:` sections in classes, with `private:` first
- Use `auto` when the type can be deduced; DO NOT use `auto` when creating instances (e.g., `Parser parser`)
- DO NOT use shortcut names (e.g., `ty` for type, `ctx` for context)
- ALWAYS use classes (never structs); use explicit constructors to ensure all members are initialized
- ALWAYS ASK QUESTIONS. Do not change code for unrelated issues; ask and I will guide you.
- Use modern C++ features. (e.g., std::print)

## Project Structure

- C++26 with modules (`.cppm` extension)
