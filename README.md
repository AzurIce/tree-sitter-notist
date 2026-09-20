# tree-sitter-notist

Two Tree-sitter parsers for Notist: `notist` parses `.not` markup documents,
and `notist_code` parses `.notc` code modules. Both share expression rules
and the external scanner in this repository.

The grammar tracks the reference parser in
[`crates/notist-syntax/src/lib.rs`](https://github.com/AzurIce/Notist/blob/main/crates/notist-syntax/src/lib.rs)
of the main Notist repository.

## What is covered

Markup (`.not` — the file parses as one implicit content literal):

- sections: a `= ` run at the start of a line opens a heading, with the level
  given by the number of `=`;
- annotations `@expr` and `@!expr`, opaque backtick raw regions, `$math$`,
  automatic HTTP(S) links, comments, and line-start list markers;
- styled spans `*strong*` and `_emphasis_`, nestable across each other and
  across lines;
- wikilinks `[[module::path]]` and `[[module::path::"Parent"::"Child"]]`;
- escapes `\c`, which yield the escaped character as text;
- ordinary paired brackets `[text]` in Markup; Code Content literals and
  trailing content arguments remain distinct nodes;
- `#` interpolation of a primary expression, whose postfix operators
  (`(...)`, `.name`, trailing `[...]`) must be adjacent to the expression,
  exactly as in the reference lexer;
- `#let`, `#use`, `#wasm` declarations inside markup.

Code (`.notc`, and the code contexts above):

- `use path::{a, b as c, self, *};`, `wasm "path";`,
  `let name = expr;`, and expression statements, all `;`-terminated;
- module-qualified names `a::b`, item targets `a::"Parent"::"Child"`;
- calls with positional/named arguments and trailing content
  (`f(a, b: 1)[body]`), field access, `if`/`else`, lambdas with typed
  parameters and defaults, unary minus, and the comparison/additive/
  multiplicative operators;
- list `(a, b)`, dict `(k: v)`, and the empty forms `()` and `(:)`;
- `//` line comments, nested `/* */` comments, and `"JSON-escaped"` strings.

```not
#use mermaid::diagram;
= Package graph
#diagram("graph LR; Source --> Code")
See [[vault::designs::"Grammar"]] and *[[vault::guide]]*.
```

```notc
use vault::helpers::{self as helpers, format};
let heading = (title: String, level: Int = 2) => item("heading", (level: level, body: [#text(title)]));
heading(title: "Intro")[body];
```

An Item's `label` defaults to its section title and can be overridden by
`@(label: "...")`. Label paths end at the target itself; preceding labels
match ancestors in order and may skip intervening ancestors. Repeated labels
are legal. Notist's analysis resolves the path and reports missing or
ambiguous targets; this grammar only describes its source syntax.

## Parser selection

Select the parser by file kind, not by file contents:

| File | Grammar | Grammar directory | Rust binding |
| --- | --- | --- | --- |
| `.not` | `notist` | `.` | `LANGUAGE` |
| `.notc` | `notist_code` | `notist-code` | `CODE_LANGUAGE` |

`let x = 1;` is prose in `.not` and a declaration in `.notc`.
Markup uses `#let x = 1;` for declarations; Code content literals enter
Markup mode. `notist-code/grammar.js` inherits the root grammar and changes
only its name and entry point. Its scanner wrapper compiles the same scanner
source under distinct exported symbols.

Known editor approximations:

- inside a styled span, a line-start `= ` degrades to text (sections do not
  open inside `*`/`_` spans).
- identifiers containing underscores compete with emphasis delimiters;
  style word boundaries remain an editor approximation.

Editor-level divergences from the reference parser, kept deliberately:

- sections are flat siblings (the level lives on the marker) instead of
  nesting by heading level;
- standalone Item annotations before a heading retain their lexical source
  position. A preceding section's Tree-sitter fold range can include them;
  attachment to the following heading, and the lexical scope of `@!`
  annotations, are resolved by the Notist frontend;
- lists expose their source markers; indentation-based Item grouping is
  performed by the Notist frontend, not this highlighting grammar;
- horizontal whitespace between markup nodes is trivia, so text runs never
  keep leading/trailing spaces they do not need;
- markup words use `text_word` nodes, separate from code identifiers,
  keywords, and numbers so ordinary prose does not receive code highlighting.

## Development

```sh
tree-sitter generate --js-runtime native
tree-sitter test
cd notist-code
tree-sitter generate --js-runtime native
tree-sitter test
cd ..
node scripts/sync-queries.mjs
cargo test -j8 -- --test-threads=4
```

Build the grammar WASM artifact for editor consumers:

```sh
tree-sitter build --wasm .
tree-sitter build --wasm notist-code
```

`queries/` is the authoritative shared query set. The sync script generates
`notist-code/queries/` with additional top-level Code declaration captures.
Both sets are checked against their own parser by the Rust tests.
Editors register two languages and associate both with the same Notist LSP;
the grammar repository does not start language servers.
