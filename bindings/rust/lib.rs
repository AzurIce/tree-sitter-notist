//! Tree-sitter parsers for Notist markup, signatures and Markdown frontends.
//!
//! ```
//! let mut parser = tree_sitter::Parser::new();
//! parser.set_language(&tree_sitter_notist::LANGUAGE.into()).unwrap();
//! let tree = parser.parse("= Hello\n#strong[Notist]", None).unwrap();
//! assert!(!tree.root_node().has_error());
//! ```
use tree_sitter_language::LanguageFn;

unsafe extern "C" {
    fn tree_sitter_notist() -> *const ();
    fn tree_sitter_notist_code() -> *const ();
    fn tree_sitter_notist_markdown() -> *const ();
    fn tree_sitter_notist_markdown_inline() -> *const ();
}

/// `.not` markup; the entry point never guesses a mode from the source.
pub const LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_notist) };
/// `.notc` content function declarations.
pub const CODE_LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_notist_code) };
pub const NODE_TYPES: &str = include_str!("../../src/node-types.json");
pub const CODE_NODE_TYPES: &str = include_str!("../../notist-code/src/node-types.json");
pub const HIGHLIGHTS_QUERY: &str = include_str!("../../queries/highlights.scm");
pub const FOLDS_QUERY: &str = include_str!("../../queries/folds.scm");
pub const INDENTS_QUERY: &str = include_str!("../../queries/indents.scm");
pub const OUTLINE_QUERY: &str = include_str!("../../queries/outline.scm");
pub const BRACKETS_QUERY: &str = include_str!("../../queries/brackets.scm");
pub const CODE_HIGHLIGHTS_QUERY: &str = include_str!("../../notist-code/queries/highlights.scm");
pub const CODE_FOLDS_QUERY: &str = include_str!("../../notist-code/queries/folds.scm");
pub const CODE_INDENTS_QUERY: &str = include_str!("../../notist-code/queries/indents.scm");
pub const CODE_OUTLINE_QUERY: &str = include_str!("../../notist-code/queries/outline.scm");
pub const CODE_BRACKETS_QUERY: &str = include_str!("../../notist-code/queries/brackets.scm");

/// `.nmd` / `.notmd` Markdown blocks. Use the injection queries with the inline language.
pub const MARKDOWN_LANGUAGE: LanguageFn =
    unsafe { LanguageFn::from_raw(tree_sitter_notist_markdown) };
/// Markdown inline syntax and Notist extensions; injected recursively into block trees.
pub const MARKDOWN_INLINE_LANGUAGE: LanguageFn =
    unsafe { LanguageFn::from_raw(tree_sitter_notist_markdown_inline) };
pub const MARKDOWN_NODE_TYPES: &str = include_str!("../../notist-markdown/src/node-types.json");
pub const MARKDOWN_HIGHLIGHTS_QUERY: &str =
    include_str!("../../notist-markdown/queries/highlights.scm");
pub const MARKDOWN_INJECTIONS_QUERY: &str =
    include_str!("../../notist-markdown/queries/injections.scm");
pub const MARKDOWN_FOLDS_QUERY: &str = include_str!("../../notist-markdown/queries/folds.scm");
pub const MARKDOWN_INDENTS_QUERY: &str = include_str!("../../notist-markdown/queries/indents.scm");
pub const MARKDOWN_OUTLINE_QUERY: &str = include_str!("../../notist-markdown/queries/outline.scm");
pub const MARKDOWN_BRACKETS_QUERY: &str =
    include_str!("../../notist-markdown/queries/brackets.scm");
pub const MARKDOWN_INLINE_NODE_TYPES: &str =
    include_str!("../../notist-markdown-inline/src/node-types.json");
pub const MARKDOWN_INLINE_HIGHLIGHTS_QUERY: &str =
    include_str!("../../notist-markdown-inline/queries/highlights.scm");
pub const MARKDOWN_INLINE_INJECTIONS_QUERY: &str =
    include_str!("../../notist-markdown-inline/queries/injections.scm");
pub const MARKDOWN_INLINE_FOLDS_QUERY: &str =
    include_str!("../../notist-markdown-inline/queries/folds.scm");
pub const MARKDOWN_INLINE_INDENTS_QUERY: &str =
    include_str!("../../notist-markdown-inline/queries/indents.scm");
pub const MARKDOWN_INLINE_OUTLINE_QUERY: &str =
    include_str!("../../notist-markdown-inline/queries/outline.scm");
pub const MARKDOWN_INLINE_BRACKETS_QUERY: &str =
    include_str!("../../notist-markdown-inline/queries/brackets.scm");
