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

/// The default language injection query.
pub const INJECTIONS_QUERY: &str = include_str!("../../queries/injections.scm");

#[cfg(test)]
mod tests {
    #[test]
    fn loads_the_grammar() {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&super::LANGUAGE.into())
            .expect("Notist grammar should load");
    }
}
