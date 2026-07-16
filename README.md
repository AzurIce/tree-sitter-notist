# tree-sitter-notist

Tree-sitter grammar for the Notist markup language.

The grammar follows [`docs/grammar.not`](https://github.com/AzurIce/Notist/blob/main/docs/grammar.not) in the main Notist repository. It supports wiki references, transparent scopes, calls with trailing content, typed literal arguments, postfix attributes, escaped and hash-delimited raw strings, inline raw spans, and fenced raw blocks.

````not
#heading(level=2)[Content]
#raw(text=r#"fn main() {}"#, lang="rust")
```rust
fn main() {}
```
````

Generate and test the parser with:

```sh
tree-sitter generate --js-runtime native
tree-sitter test
cargo test
```
