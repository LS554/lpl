# LPL Language — VS Code Extension

Syntax highlighting, autocomplete, and hover documentation for the LPL programming language.

## Features

- **Syntax highlighting** for `.lpl` and `.lph` files — keywords, types, strings, comments, operators, annotations
- **Autocomplete** — keywords, stdlib classes/methods (triggered by `.`), standard library headers (triggered by `<` after `include`), code snippets
- **Hover documentation** — hover over stdlib classes and methods for signatures, descriptions, and parameter docs. Hover over LPL-specific keywords (`owner`, `defer`, `move`, etc.) for quick explanations
- **Signature help** — parameter hints when calling stdlib methods

## Install (development)

```sh
cd editors/vscode/lpl-lang
npm install
npx tsc -p ./
```

Then in VS Code: **Extensions → ⋯ → Install from VSIX** or press `F5` to launch an Extension Development Host with this folder open.

Alternatively, symlink into your extensions directory:

```sh
ln -s "$(pwd)" ~/.vscode/extensions/lpl-lang
```

Restart VS Code and open any `.lpl` or `.lph` file.
