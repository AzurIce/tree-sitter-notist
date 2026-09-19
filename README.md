# tree-sitter-notist

Tree-sitter grammar for Notist, covering both file kinds of the current
syntax: `.not` markup documents and `.notc` code modules.

The grammar tracks the reference parser in
[`crates/notist-next/src/syntax.rs`](https://github.com/AzurIce/Notist/blob/main/crates/notist-next/src/syntax.rs)
of the main Notist repository.

## What is covered

Markup (`.not` — the file parses as one implicit content literal):

- sections: a `= ` run at the start of a line opens a heading, with the level
  given by the number of `=`;
- styled spans `*strong*` and `_emphasis_`, nestable across each other and
  across lines;
- wikilinks `[[module::path]]` and `[[module::path#item]]`;
- escapes `\c`, which yield the escaped character as text;
- content blocks `[...]`, which nest the full markup grammar (including
  sections after a line break);
- `#` interpolation of a primary expression, whose postfix operators
  (`(...)`, `.name`, trailing `[...]`) must be adjacent to the expression,
  exactly as in the reference lexer;
- `#let`, `#use`, `#wasm` declarations inside markup.

Code (`.notc`, and the code contexts above):

- `use path::{a, b as c, self, *};`, `wasm "path";`,
  `let name = expr;`, and expression statements, all `;`-terminated;
- module-qualified names `a::b`, item targets `a::"item-id"`;
- calls with positional/named arguments and trailing content
  (`f(a, b: 1)[body]`), field access, `if`/`else`, lambdas with typed
  parameters and defaults, unary minus, and the comparison/additive/
  multiplicative operators;
- list `(a, b)`, dict `(k: v)`, and the empty forms `()` and `(:)`;
- `//` line comments and `"JSON-escaped"` strings.

```not
#use mermaid::diagram;
= Package graph
#diagram("graph LR; Source --> Code")
See [[vault::designs#grammar]] and *[[vault::guide]]*.
```

```notc
use vault::helpers::{self as helpers, format};
let heading = (title: String, level: Int = 2) => item("heading", (level: level, body: [#text(title)]));
heading(title: "Intro")[body];
```

## File-kind disambiguation

Both file kinds share one grammar and one expression language, and some
inputs are valid as both prose and code. The parser keeps both parses alive
(GLR) and prefers the structured code parse via dynamic precedence whenever
the whole file is valid as both; prose kills the code branch early.

Known limits of the single-grammar approach, all degrading to plain
un-highlighted text rather than errors:

- a code module whose *first* statement is a bare expression starting with
  an identifier, string, or `(` (e.g. `f(1);` or `a::"x";` as the first
  line) lexes as markup text unless a `let`/`use`/`wasm` statement or a
  blank line precedes it;
- inside a styled span, a line-start `= ` degrades to text (sections do not
  open inside `*`/`_` spans).

Editor-level divergences from the reference parser, kept deliberately:

- sections are flat siblings (the level lives on the marker) instead of
  nesting by heading level;
- horizontal whitespace between markup nodes is trivia, so text runs never
  keep leading/trailing spaces they do not need;
- markup text that starts with a word lexes as a sequence of word tokens, so
  word-led prose produces per-word `text` nodes.

## Development

```sh
tree-sitter generate --js-runtime native
tree-sitter test
cargo test
```

Build the web-tree-sitter wasm artifact (what the Obsidian plugin ships in
`assets/notist.wasm`; record the commit and date in the plugin's
`assets/UPSTREAM.txt`):

```sh
tree-sitter build --wasm  # produces tree-sitter-notist.wasm
```
