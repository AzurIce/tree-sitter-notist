# tree-sitter-notist

Tree-sitter grammar for the Notist markup language.

The grammar follows `docs/grammar.not` in the main Notist repository. It supports wiki references, transparent scopes, content calls, typed literal arguments, postfix attributes, legacy backtick shielding, and extensible raw call delimiters.

```not
#heading(level=2)[Content]
#raw(lang="rust")![fn main() {}]!
#raw!![body containing ]!]!!
```

Generate and test the parser with:

```sh
tree-sitter generate --js-runtime native
tree-sitter test
cargo test
```
