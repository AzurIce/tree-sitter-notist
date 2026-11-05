# Vendored tree-sitter-markdown

Source: https://github.com/tree-sitter-grammars/tree-sitter-markdown
Tag: v0.5.0
Commit: afaa4138517363362f54c89330c9d79391e81168
License: MIT (see LICENSE)

Only grammar inputs, C scanners, and baseline highlight/injection queries are
vendored. Generated parsers belong to the extended Notist grammars. No network
or npm dependency is needed to build the checked-in C parsers.

Local changes to upstream scanner files:

- The block scanner conditionally appends NOTIST_BLOCK and its paragraph
  interruption bitmap entry. Heading/annotation probes recognize complete
  standalone Notist constructs without interpreting multiline payloads as
  Markdown blocks. Native Markdown heading probes remain unchanged when the
  extension is disabled.
- The inline scanner accepts MARKDOWN_INLINE_SCANNER_STORAGE so its exports can
  become private functions when composed with the shared Notist scanner.

Everything else is extended in notist-markdown/, notist-markdown-inline/ and
common/. These extensions fix ATX heading EOF handling, set deterministic
CommonMark plus table/strikethrough options, expose Notist literal/header nodes,
and inject call bodies recursively into the appropriate Markdown grammar.

To update, replace the unmodified upstream inputs at the pinned revision,
reapply these scanner changes, regenerate all four parsers, and run the corpus
and Rust integration tests. Do not enable upstream extensions via environment
variables; common/markdown-options.js fixes the frontend's options.
