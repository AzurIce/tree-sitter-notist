//! Notist language support for the tree-sitter parsing library.

use tree_sitter_language::LanguageFn;

unsafe extern "C" {
    fn tree_sitter_notist() -> *const ();
}

/// The tree-sitter language function for Notist.
pub const LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_notist) };

/// The generated node type definitions.
pub const NODE_TYPES: &str = include_str!("../../src/node-types.json");

/// The default syntax highlighting query.
pub const HIGHLIGHTS_QUERY: &str = include_str!("../../queries/highlights.scm");

/// The default code folding query.
pub const FOLDS_QUERY: &str = include_str!("../../queries/folds.scm");

/// The default indentation query.
pub const INDENTS_QUERY: &str = include_str!("../../queries/indents.scm");

/// The default outline query.
pub const OUTLINE_QUERY: &str = include_str!("../../queries/outline.scm");

/// The default bracket pairing query.
pub const BRACKETS_QUERY: &str = include_str!("../../queries/brackets.scm");

#[cfg(test)]
mod tests {
    use tree_sitter::Query;

    #[test]
    fn loads_the_grammar() {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&super::LANGUAGE.into())
            .expect("Notist grammar should load");
    }

    #[test]
    fn compiles_the_queries() {
        let language = super::LANGUAGE.into();
        Query::new(&language, super::HIGHLIGHTS_QUERY).expect("highlight query should compile");
        Query::new(&language, super::FOLDS_QUERY).expect("fold query should compile");
        Query::new(&language, super::INDENTS_QUERY).expect("indent query should compile");
        Query::new(&language, super::OUTLINE_QUERY).expect("outline query should compile");
        Query::new(&language, super::BRACKETS_QUERY).expect("brackets query should compile");
    }
}
