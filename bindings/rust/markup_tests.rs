use tree_sitter::{InputEdit, Node, Parser, Point, Tree};

fn parser() -> Parser {
    let mut parser = Parser::new();
    parser.set_language(&crate::LANGUAGE.into()).unwrap();
    parser
}

fn nodes<'a>(node: Node<'a>, kind: &str, out: &mut Vec<Node<'a>>) {
    if node.kind() == kind {
        out.push(node);
    }
    for child in node.children(&mut node.walk()) {
        nodes(child, kind, out);
    }
}

fn text_of(tree: &Tree, source: &str, kind: &str) -> Vec<String> {
    let mut found = Vec::new();
    nodes(tree.root_node(), kind, &mut found);
    found
        .iter()
        .map(|node| source[node.byte_range()].to_owned())
        .collect()
}

#[test]
fn markup_boundaries_preserve_opaque_regions() {
    let source = "@!(lang: \"en\") @(id: \"intro\")\n= Title\n`@! #bad $` $\"$\" + x$\n/* outer /* nested */ end */\nLine\\\nNext \\u{1f600}\n- One\n  + Two\nText [ordinary] https://example.com/a_(b).\n";
    let tree = parser().parse(source, None).unwrap();
    assert!(
        !tree.root_node().has_error(),
        "{}",
        tree.root_node().to_sexp()
    );
    assert_eq!(text_of(&tree, source, "annotation").len(), 2);
    assert_eq!(text_of(&tree, source, "raw"), ["`@! #bad $`"]);
    assert_eq!(text_of(&tree, source, "math"), ["$\"$\" + x$"]);
    assert_eq!(
        text_of(&tree, source, "comment"),
        ["/* outer /* nested */ end */"]
    );
    assert_eq!(text_of(&tree, source, "escape"), ["\\\n", "\\u{1f600}"]);
    assert_eq!(text_of(&tree, source, "list_marker"), ["-", "+"]);
    assert_eq!(
        text_of(&tree, source, "autolink"),
        ["https://example.com/a_(b)"]
    );
    assert_eq!(text_of(&tree, source, "bracketed_text"), ["[ordinary]"]);
}

#[test]
fn markup_does_not_capture_code_strings_or_operators() {
    for source in [
        "let x = \"` $ @ /*\"; x;",
        "// leading\nlet x = 6 / 2; // trailing\n",
        "let x = [@!(lang: \"en\") $x$ `code`]; x;",
        "let f = (x: Int) => x + 1; f(2);",
    ] {
        let tree = parser().parse(source, None).unwrap();
        assert!(
            !tree.root_node().has_error(),
            "{source}: {}",
            tree.root_node().to_sexp()
        );
        assert_eq!(text_of(&tree, source, "code_file").len(), 1, "{source}");
    }
}

#[test]
fn incremental_raw_fence_edits_match_fresh_parses() {
    let original = "Text `opaque @bad` after $x$";
    let offset = original.find("` after").unwrap();
    let mut parser = parser();
    let mut tree = parser.parse(original, None).unwrap();
    let mut source = original.to_owned();
    for insert in [false, true] {
        let old_len = usize::from(!insert);
        let new_len = usize::from(insert);
        tree.edit(&InputEdit {
            start_byte: offset,
            old_end_byte: offset + old_len,
            new_end_byte: offset + new_len,
            start_position: Point::new(0, offset),
            old_end_position: Point::new(0, offset + old_len),
            new_end_position: Point::new(0, offset + new_len),
        });
        source.replace_range(offset..offset + old_len, if insert { "`" } else { "" });
        tree = parser.parse(&source, Some(&tree)).unwrap();
        let fresh = parser.parse(&source, None).unwrap();
        assert_eq!(tree.root_node().to_sexp(), fresh.root_node().to_sexp());
        assert_eq!(
            text_of(&tree, &source, "raw"),
            text_of(&fresh, &source, "raw")
        );
        assert_eq!(text_of(&tree, &source, "math").len(), usize::from(insert));
    }
}
