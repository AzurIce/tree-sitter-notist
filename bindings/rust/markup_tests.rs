use tree_sitter::StreamingIterator;
use tree_sitter::{InputEdit, Node, Parser, Point, Tree};

fn parser() -> Parser {
    let mut parser = Parser::new();
    parser.set_language(&crate::LANGUAGE.into()).unwrap();
    parser
}

#[test]
fn file_kind_is_fixed_even_when_prose_is_valid_code() {
    let source = "let x = true; use helpers::{a, b}; wasm \"plugin.wasm\"; f(1 + 2);";
    let markup = parser().parse(source, None).unwrap();
    assert!(
        !markup.root_node().has_error(),
        "{}",
        markup.root_node().to_sexp()
    );
    assert_eq!(text_of(&markup, source, "markup_file").len(), 1);
    assert!(text_of(&markup, source, "let_statement").is_empty());

    let query = tree_sitter::Query::new(&crate::LANGUAGE.into(), crate::HIGHLIGHTS_QUERY).unwrap();
    let mut cursor = tree_sitter::QueryCursor::new();
    let mut captures = cursor.captures(&query, markup.root_node(), source.as_bytes());
    assert!(
        captures.next().is_none(),
        "plain prose must not receive code highlighting"
    );

    let mut code = Parser::new();
    code.set_language(&crate::CODE_LANGUAGE.into()).unwrap();
    let tree = code.parse(source, None).unwrap();
    assert!(
        !tree.root_node().has_error(),
        "{}",
        tree.root_node().to_sexp()
    );
    assert_eq!(text_of(&tree, source, "let_statement").len(), 1);

    for source in [
        "f(1);",
        "a::\"item\";",
        "\"string\";",
        "(1 + 2);",
        "[let x = 1; #x];",
    ] {
        let tree = code.parse(source, None).unwrap();
        assert!(
            !tree.root_node().has_error(),
            "{source}: {}",
            tree.root_node().to_sexp()
        );
        assert_eq!(text_of(&tree, source, "code_file").len(), 1);
    }
    assert!(
        code.parse("Ordinary prose.", None)
            .unwrap()
            .root_node()
            .has_error()
    );
}

#[test]
fn markup_interpolation_and_code_content_keep_their_modes() {
    let source = "let plain = true; #let real: Int = 1; #real";
    let tree = parser().parse(source, None).unwrap();
    assert!(!tree.root_node().has_error());
    assert_eq!(
        text_of(&tree, source, "declaration_let"),
        ["let real: Int = 1;"]
    );
    let mut code = Parser::new();
    code.set_language(&crate::CODE_LANGUAGE.into()).unwrap();
    let source = "let body = [let plain = true; #let real = 1; #real];";
    let tree = code.parse(source, None).unwrap();
    assert!(!tree.root_node().has_error());
    assert_eq!(text_of(&tree, source, "let_statement").len(), 1);
    assert_eq!(text_of(&tree, source, "declaration_let"), ["let real = 1;"]);
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
    let mut code_parser = Parser::new();
    code_parser
        .set_language(&crate::CODE_LANGUAGE.into())
        .unwrap();
    for source in [
        "let x = \"` $ @ /*\"; x;",
        "// leading\nlet x = 6 / 2; // trailing\n",
        "let x = [@!(lang: \"en\") $x$ `code`]; x;",
        "let f = (x: Int) => x + 1; f(2);",
    ] {
        let tree = code_parser.parse(source, None).unwrap();
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
