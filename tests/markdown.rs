use tree_sitter::{
    InputEdit, Node, Parser, Point, Query, QueryCursor, Range, StreamingIterator, Tree,
};
use tree_sitter_notist as notist;

fn parser(inline: bool) -> Parser {
    let mut p = Parser::new();
    p.set_language(
        &if inline {
            notist::MARKDOWN_INLINE_LANGUAGE
        } else {
            notist::MARKDOWN_LANGUAGE
        }
        .into(),
    )
    .unwrap();
    p
}
fn collect<'a>(node: Node<'a>, kind: &str, result: &mut Vec<Node<'a>>) {
    if node.kind() == kind {
        result.push(node);
    }
    for child in node.children(&mut node.walk()) {
        collect(child, kind, result);
    }
}
fn texts<'a>(trees: &[Tree], source: &'a str, kind: &str) -> Vec<&'a str> {
    let mut nodes = Vec::new();
    for tree in trees {
        collect(tree.root_node(), kind, &mut nodes);
    }
    nodes.iter().map(|n| &source[n.byte_range()]).collect()
}
// Exercise the exported injection queries as an editor does, preserving source
// positions and honoring the standard injection.include-children property.
fn injected(source: &str) -> Vec<Tree> {
    fn visit(source: &str, inline: bool, ranges: &[Range], trees: &mut Vec<Tree>, depth: usize) {
        assert!(depth < 20, "injection cycle");
        let mut p = parser(inline);
        p.set_included_ranges(ranges).unwrap();
        let tree = p.parse(source, None).unwrap();
        assert!(
            !tree.root_node().has_error(),
            "{source:?}: {}",
            tree.root_node().to_sexp()
        );
        let query = Query::new(
            &p.language().unwrap(),
            if inline {
                notist::MARKDOWN_INLINE_INJECTIONS_QUERY
            } else {
                notist::MARKDOWN_INJECTIONS_QUERY
            },
        )
        .unwrap();
        let mut cursor = QueryCursor::new();
        let mut matches = cursor.matches(&query, tree.root_node(), source.as_bytes());
        let mut next = Vec::new();
        while let Some(m) = matches.next() {
            let Some(language) = query
                .property_settings(m.pattern_index)
                .iter()
                .find(|p| p.key.as_ref() == "injection.language")
                .and_then(|p| p.value.as_deref())
            else {
                continue;
            };
            let child_inline = match language {
                "notist_markdown_inline" => true,
                "notist_markdown" => false,
                _ => continue,
            };
            let n = m.captures[0].node;
            let mut ranges = Vec::new();
            let mut start_byte = n.start_byte();
            let mut start_point = n.start_position();
            let include_children = query
                .property_settings(m.pattern_index)
                .iter()
                .any(|p| p.key.as_ref() == "injection.include-children");
            for child in n.children(&mut n.walk()).filter(|_| !include_children) {
                if start_byte < child.start_byte() {
                    ranges.push(Range {
                        start_byte,
                        end_byte: child.start_byte(),
                        start_point,
                        end_point: child.start_position(),
                    });
                }
                start_byte = child.end_byte();
                start_point = child.end_position();
            }
            if start_byte < n.end_byte() {
                ranges.push(Range {
                    start_byte,
                    end_byte: n.end_byte(),
                    start_point,
                    end_point: n.end_position(),
                });
            }
            if !ranges.is_empty() {
                next.push((child_inline, ranges));
            }
        }
        trees.push(tree);
        for (inline, ranges) in next {
            visit(source, inline, &ranges, trees, depth + 1);
        }
    }
    let mut trees = Vec::new();
    visit(source, false, &[], &mut trees, 0);
    trees
}

#[test]
fn current_imports_and_header_expressions() {
    let source = "#import \"lib.notc\" as lib;\n\n#lib.math<InlineContent>(text: lib.label(1 + 2))[#children]\n\n#[removed]\n";
    let trees = injected(source);
    assert_eq!(texts(&trees, source, "import_declaration").len(), 1);
    assert_eq!(texts(&trees, source, "call").len(), 1);
    assert_eq!(texts(&trees, source, "comptime_arguments").len(), 1);
    assert_eq!(texts(&trees, source, "binary_expression"), ["1 + 2"]);
    assert!(texts(&trees, source, "reference").contains(&"#children"));
}
#[test]
fn recursive_markdown_example() {
    injected(include_str!("../examples/guide.notmd"));
    let source = include_str!("../examples/showcase.nmd");
    let trees = injected(source);
    assert_eq!(texts(&trees, source, "call").len(), 6);
    assert_eq!(texts(&trees, source, "annotation").len(), 2);
    assert_eq!(texts(&trees, source, "atx_heading").len(), 2);
    assert_eq!(texts(&trees, source, "pipe_table").len(), 1);
    assert!(!texts(&trees, source, "strong_emphasis").is_empty());
    assert_eq!(texts(&trees, source, "markdown_block_content").len(), 1);
    assert_eq!(texts(&trees, source, "markdown_inline_content").len(), 2);
    assert_eq!(texts(&trees, source, "math").len(), 2);
}
#[test]
fn markdown_contexts_and_shielding() {
    let source = "# Title with #badge()\n\n**#bold()[*inner*]** and [#link()](url)\n\n> #quoted()[text]\n> continued #next()\n\n- #item()[text]\n\n`#hidden()`\n\n```not\n#hidden()\n```\n\n#body()[a `]` and $]$ then #nested()]\n";
    let trees = injected(source);
    assert_eq!(texts(&trees, source, "call").len(), 8);
    assert!(
        !texts(&trees, source, "call")
            .iter()
            .any(|s| s.contains("hidden"))
    );
    assert_eq!(texts(&trees, source, "emphasis"), vec!["*inner*"]);
}
#[test]
fn shared_literals_and_markdown_flanks() {
    let unicode_body = "#调用()[　内容　]";
    let unicode_trees = injected(unicode_body);
    assert_eq!(
        texts(&unicode_trees, unicode_body, "markdown_block_content"),
        vec!["　内容　"]
    );
    let source = "@(id: r#\"x\"#, values: (true, -2, (), (:), (1,)))\n#名称.call(a: \"text\", b: (key: false))[ inline ]\n\n#x()[**inline**]\n";
    let trees = injected(source);
    assert_eq!(
        texts(&trees, source, "markdown_block_content"),
        vec![" inline "]
    );
    assert_eq!(
        texts(&trees, source, "markdown_inline_content"),
        vec!["**inline**"]
    );
    assert_eq!(texts(&trees, source, "call").len(), 2);
    assert_eq!(texts(&trees, source, "boolean"), vec!["true", "false"]);
}
#[test]
fn incomplete_markers_remain_markdown() {
    let spaced = injected("#x() [text]");
    assert_eq!(texts(&spaced, "#x() [text]", "call"), vec!["#x()"]);
    for source in [
        "#bare",
        "#x (1)",
        "#(text)",
        "@word",
        "# Heading",
        "## Heading",
        "[normal](link)",
    ] {
        let trees = injected(source);
        assert!(texts(&trees, source, "call").is_empty(), "{source}");
    }
}
#[test]
fn all_markdown_queries_compile() {
    for (inline, queries) in [
        (
            false,
            [
                notist::MARKDOWN_HIGHLIGHTS_QUERY,
                notist::MARKDOWN_INJECTIONS_QUERY,
                notist::MARKDOWN_FOLDS_QUERY,
                notist::MARKDOWN_INDENTS_QUERY,
                notist::MARKDOWN_OUTLINE_QUERY,
                notist::MARKDOWN_BRACKETS_QUERY,
            ],
        ),
        (
            true,
            [
                notist::MARKDOWN_INLINE_HIGHLIGHTS_QUERY,
                notist::MARKDOWN_INLINE_INJECTIONS_QUERY,
                notist::MARKDOWN_INLINE_FOLDS_QUERY,
                notist::MARKDOWN_INLINE_INDENTS_QUERY,
                notist::MARKDOWN_INLINE_OUTLINE_QUERY,
                notist::MARKDOWN_INLINE_BRACKETS_QUERY,
            ],
        ),
    ] {
        for query in queries {
            Query::new(&parser(inline).language().unwrap(), query).unwrap();
        }
    }
}
fn point(s: &str, byte: usize) -> Point {
    let prefix = &s[..byte];
    Point::new(
        prefix.bytes().filter(|b| *b == b'\n').count(),
        prefix.rfind('\n').map_or(byte, |i| byte - i - 1),
    )
}
fn snapshot(node: Node<'_>, out: &mut Vec<(String, Range, bool)>) {
    out.push((node.kind().to_owned(), node.range(), node.is_missing()));
    for child in node.children(&mut node.walk()) {
        snapshot(child, out);
    }
}
#[test]
fn incremental_markdown_scanners_match_fresh_trees() {
    for inline in [false, true] {
        for original in [
            include_str!("../examples/showcase.nmd"),
            include_str!("../examples/guide.notmd"),
        ] {
            for (offset, ch) in original.char_indices() {
                let mut p = parser(inline);
                let mut tree = p.parse(original, None).unwrap();
                let end = offset + ch.len_utf8();
                tree.edit(&InputEdit {
                    start_byte: offset,
                    old_end_byte: end,
                    new_end_byte: offset,
                    start_position: point(original, offset),
                    old_end_position: point(original, end),
                    new_end_position: point(original, offset),
                });
                let mut source = original.to_owned();
                source.replace_range(offset..end, "");
                let mut edited = p.parse(&source, Some(&tree)).unwrap();
                let fresh = p.parse(&source, None).unwrap();
                let mut left = Vec::new();
                let mut right = Vec::new();
                snapshot(edited.root_node(), &mut left);
                snapshot(fresh.root_node(), &mut right);
                assert_eq!(left, right, "delete {ch:?} at {offset}, inline={inline}");
                edited.edit(&InputEdit {
                    start_byte: offset,
                    old_end_byte: offset,
                    new_end_byte: end,
                    start_position: point(&source, offset),
                    old_end_position: point(&source, offset),
                    new_end_position: point(original, end),
                });
                let restored = p.parse(original, Some(&edited)).unwrap();
                let fresh = p.parse(original, None).unwrap();
                left.clear();
                right.clear();
                snapshot(restored.root_node(), &mut left);
                snapshot(fresh.root_node(), &mut right);
                assert_eq!(left, right, "restore {ch:?} at {offset}, inline={inline}");
            }
        }
    }
}
