# tree-sitter-notist

Tree-sitter grammars for current Notist: `.not` markup, `.notc` pure-function
modules, and `.nmd` / `.notmd` Markdown with Notist extensions. The repository follows the framework of the archived
version, with the grammar rebuilt against `notist-syntax` and Notist's current
[syntax documentation](https://github.com/AzurIce/Notist/blob/main/docs/code/syntax.not).

| File | Language | Grammar directory | Rust binding |
| --- | --- | --- | --- |
| `.not` | `notist` | `.` | `LANGUAGE` |
| `.notc` | `notist_code` | `notist-code` | `CODE_LANGUAGE` |
| `.nmd`, `.notmd` | `notist_markdown` | `notist-markdown` | `MARKDOWN_LANGUAGE` |
| injected Markdown inline content | `notist_markdown_inline` | `notist-markdown-inline` | `MARKDOWN_INLINE_LANGUAGE` |

The file type selects a fixed entry point. `fn demo()->Content;` is prose in
`.not`, and a signature in `.notc`. Markup `#fn()` is a content call.

## Supported syntax

- Headings, unordered/ordered list markers (`-`/`+`), dividers and pipe tables.
- `*strong*`, `_emphasis_`, `~strike~`, escapes and explicit paragraph breaks.
- Inline raw spans, fenced raw blocks, and opaque inline/block math payloads.
- `[label](target "title")`, `![alt](target)`, and `[[path#id|label]]` links.
- Nested block comments and line comments, preserving Markup's `://` exemption.
- `#module.name<ct>(args)[body]`, `#name[body]`, `#name` references, and
  `@(dict)` / `@!(dict)` annotations. Call parts and path segments are adjacent;
  `#(...)` and the removed anonymous `#[body]` form remain prose. Use explicit
  `#node[body]` for transparent content. Reference legality is checked by Notist.
- File-level `#import "relative.notc" as name;` and Code `import` declarations.
- Shared literals: Unit `()`, Bool, integer/decimal numbers, adjacent negative
  numbers, grouping `(x)`, arrays `(,)` / `(x,)`, dicts `(:)` / `(key: value)`,
  and all four ordinary/raw inline/multiline string forms.
- `.notc` `fn` declarations with optional compile-time and ordinary parameter
  lists, required/optional/defaulted parameters, a `[children: Type]` mount,
  recursive qualified types, literal defaults, and trailing commas.
  Code keywords are reserved throughout Code. Hyphenated and Unicode
  identifiers work; adjacent `Content->Content` keeps `->` separate.

```not
@(id: "intro")
= Introduction
#widgets.panel(title: "Demo")[
*Hello* from [[guide.not#syntax|Notist]].
]
```

```notc
fn panel(title?: String)[children: BlockContent] -> BlockContent;
fn math<C: Type = InlineContent>(text: String) -> C;
fn double(n: Int) -> Int { n * 2 }
```

Function bodies and call arguments support name/member references, calls,
immutable `let`, arithmetic/comparison/Bool operators, `if` / `else`, `match`,
`comptime`, collections and content literals. An attached `<` starts compile-time
arguments; comparison `<` must be separated from its left operand by trivia.
Parenthesize comparisons inside compile-time arguments. `use`, mutable state,
lambdas and closures are not part of current Code.

## Markdown frontend

The Markdown frontend uses CommonMark block/inline syntax with GFM tables and
strikethrough, extended with Notist calls, references, imports, annotations and
shared header expressions. `**strong**`, `*emphasis*`, `~~strike~~`, Markdown
headings, links, quotes, nested lists and code blocks retain Markdown syntax.
Code spans, fenced code, strings and math shield Notist markers.

```nmd
# Introduction

#panel(title: "Demo")[
## Nested Markdown

- **Hello** from #badge(text: "Notist")[*inline children*]
]
```

Register **both** Markdown languages and their `injections.scm` queries. The
block grammar injects inline regions, table cells and standalone Notist
constructs into `notist_markdown_inline`. A call body with whitespace on both
sides (`[ body ]`) injects `notist_markdown`; other bodies (`[body]`) inject
`notist_markdown_inline`, recursively. Parsing only the block grammar gives
opaque `inline` and `notist_block` nodes; Rust applications must follow the
injection queries using included ranges to obtain the full tree. The integration
tests show this traversal and honor the queries' `injection.include-children`
settings.

File detection selects this frontend for `.nmd` and `.notmd`. Applications may
explicitly select it for `.md` or `.markdown`, as Notist itself does. Ordinary
Markdown file associations are left to the editor. HTML and fenced code languages
use their normal injections when those parsers are installed.

The vendored Markdown grammar is pinned to upstream v0.5.0; provenance and local
scanner changes are recorded in [vendor/tree-sitter-markdown/README.md](vendor/tree-sitter-markdown/README.md).
Unclosed code spans or math inside a call body can keep its payload opaque until
EOF while editing. Markdown table delimiters and container indentation follow
the upstream grammar and may differ from Notist's lowering diagnostics. Task
checkboxes, wikilinks and metadata frontmatter are not enabled.

## Native editor tree and boundaries

This parser produces an editor syntax tree. Notist's own parser/lowering owns
semantic diagnostics, registry validation, paragraph grouping, indentation-based
list grouping, section nesting and native content-flank classification. Headings are
flat nodes; lists expose individual markers; all call children use a
`content_block` node. Bare URLs remain prose.
Import placement, literal-default validity and other semantic restrictions remain
Notist's responsibility; the editor grammar does not load imported modules.

Tables validate the header/delimiter width and alignment cells, and expose
opaque logical row nodes. Cell contents do not receive nested syntax captures.
Table recognition starts at column zero with a prose character or `|`; headers
starting with a styled span or a content call are currently treated as prose.
Subsequent table rows must contain an unescaped outer pipe. Nested list/body
indentation is not used to recognize tables or raw fences.

Styles skip paired raw regions while searching for their closing delimiter and
stop at blank lines or block boundaries. Complex delimiter nesting and
link labels containing opaque bracket payloads can differ from the reference
parser. Styles are recognized in heading titles using the same inline rules.
Incomplete markup markers usually fall back to text; malformed signatures,
annotations, call literals and unclosed raw blocks may expose Tree-sitter error
or missing nodes. Unclosed strings/comments stay opaque for editing; Notist
reports their lexical diagnostics.

Each grammar has its own `highlights.scm`, `folds.scm`, `indents.scm`,
`outline.scm`, and `brackets.scm`. The Rust crate exports all four languages,
node type JSON, query sets, and Markdown injection queries. Editors register the
languages they need and use the corresponding directory's queries. No LSP or editor plugin is bundled.

## Development

Use a C compiler, Node.js, Rust and Tree-sitter CLI 0.26.11. Generated C parsers
and Unicode tables are checked in; Rust consumers do not need Node or the CLI.
The [Tree-sitter documentation](https://tree-sitter.github.io/tree-sitter/creating-parsers/1-getting-started.html)
describes CLI installation.

```sh
nix develop
node scripts/generate.mjs
node scripts/test.mjs
cargo fmt --check
```

Alternatively install the development CLI with `npm ci`, then run
`npm run generate` and `npm test`. Corpus expectations live in `test/corpus/`
and each frontend's `test/corpus/` directory. Rust integration tests check node contents,
queries, recovery, and incremental edits against fresh trees including ranges.

```sh
tree-sitter parse -p . examples/showcase.not
tree-sitter parse -p notist-code examples/signatures.notc
tree-sitter parse -p notist-markdown examples/showcase.nmd
tree-sitter highlight examples/guide.notmd
cargo package --allow-dirty
```

`-p` explicitly selects a grammar without requiring global parser-directory
configuration. Highlighting uses `tree-sitter init-config` with this repository's
parent in `parser-directories` so the CLI can discover all injection languages.
`scripts/generate-unicode.mjs` regenerates identifier tables
using Node's Unicode `Alphabetic`/`Number` properties; the checked-in table
records its Unicode version. Regenerate it only when intentionally updating
Unicode data, then regenerate all parsers and run the tests.

With Emscripten available, build the four editor WASM artifacts:

```sh
tree-sitter build --wasm .
tree-sitter build --wasm notist-code
tree-sitter build --wasm notist-markdown
tree-sitter build --wasm notist-markdown-inline
```

The `.notc` scanner wrapper compiles the shared scanner under separate exported
symbols. Cargo and Tree-sitter both track its shared C source and Unicode header
when deciding whether to rebuild.

## License

Notist-specific code: MIT OR Apache-2.0. Vendored Markdown code and derived
Markdown parsers retain the upstream MIT license, included under `vendor/`.
