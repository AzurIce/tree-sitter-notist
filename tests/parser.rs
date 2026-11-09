use tree_sitter::{InputEdit, Node, Parser, Point, Query, QueryCursor, StreamingIterator, Tree};
use tree_sitter_notist as notist;

fn parser(code: bool) -> Parser {
    let mut parser = Parser::new();
    parser
        .set_language(
            &if code {
                notist::CODE_LANGUAGE
            } else {
                notist::LANGUAGE
            }
            .into(),
        )
        .unwrap();
    parser
}
fn parse(source: &str, code: bool) -> Tree {
    let tree = parser(code).parse(source, None).unwrap();
    assert!(
        !tree.root_node().has_error(),
        "{source:?}: {}",
        tree.root_node().to_sexp()
    );
    tree
}
fn nodes<'a>(node: Node<'a>, kind: &str, result: &mut Vec<Node<'a>>) {
    if node.kind() == kind {
        result.push(node);
    }
    let mut cursor = node.walk();
    for child in node.children(&mut cursor) {
        nodes(child, kind, result);
    }
}
fn text_of<'a>(tree: &Tree, source: &'a str, kind: &str) -> Vec<&'a str> {
    let mut result = Vec::new();
    nodes(tree.root_node(), kind, &mut result);
    result.iter().map(|n| &source[n.byte_range()]).collect()
}
fn snapshot(node: Node<'_>, result: &mut Vec<(String, usize, usize, Point, Point, bool)>) {
    result.push((
        node.kind().to_owned(),
        node.start_byte(),
        node.end_byte(),
        node.start_position(),
        node.end_position(),
        node.is_missing(),
    ));
    let mut cursor = node.walk();
    for child in node.children(&mut cursor) {
        snapshot(child, result);
    }
}

#[test]
fn examples_and_fixed_entry_points() {
    parse(include_str!("../examples/showcase.not"), false);
    parse(include_str!("../examples/signatures.notc"), true);
    parse(include_str!("../examples/current.notc"), true);
    let prose = "fn panel(title?: String) -> Content; let x = 1;";
    assert!(text_of(&parse(prose, false), prose, "identifier").is_empty());
    assert!(
        parser(true)
            .parse("let x = 1;", None)
            .unwrap()
            .root_node()
            .has_error()
    );
}
#[test]
fn queries_compile_for_both_languages() {
    for (code, queries) in [
        (
            false,
            [
                notist::HIGHLIGHTS_QUERY,
                notist::FOLDS_QUERY,
                notist::INDENTS_QUERY,
                notist::OUTLINE_QUERY,
                notist::BRACKETS_QUERY,
            ],
        ),
        (
            true,
            [
                notist::CODE_HIGHLIGHTS_QUERY,
                notist::CODE_FOLDS_QUERY,
                notist::CODE_INDENTS_QUERY,
                notist::CODE_OUTLINE_QUERY,
                notist::CODE_BRACKETS_QUERY,
            ],
        ),
    ] {
        let language = if code {
            notist::CODE_LANGUAGE
        } else {
            notist::LANGUAGE
        }
        .into();
        for query in queries {
            Query::new(&language, query).unwrap();
        }
    }
}
#[test]
fn current_imports_paths_methods_loops_and_constraints() {
    let source = r#"import "lib.notc": panel as box, label,;
import "common.notc": *;
fn link<T: Type>(target: T) -> InlineContent where T == Path || T == String;
fn positive<n: Int>() -> Int where n > 0 { n }
fn asset(directory: Path = p".") -> Path { directory.join("img").join("chart.svg") }
fn size(xs: Array<Int>) -> Int { make_array().len() + (1, 2).len() + xs.len() }
fn labels(xs: Array<String>) -> InlineContent {
    for label in xs [#text(text: label)]
}
fn blocks(xs: Array<Int>) -> BlockContent {
    for n in xs { let text = str(n); [ #text(text: text) ] }
}
"#;
    let tree = parse(source, true);
    assert_eq!(text_of(&tree, source, "code_import").len(), 2);
    assert_eq!(
        text_of(&tree, source, "import_member"),
        ["panel as box", "label"]
    );
    assert_eq!(
        text_of(&tree, source, "where_clause"),
        ["where T == Path || T == String", "where n > 0"]
    );
    assert_eq!(text_of(&tree, source, "path_literal"), [r#"p".""#]);
    assert_eq!(text_of(&tree, source, "for_expression").len(), 2);
    let mut loops = Vec::new();
    nodes(tree.root_node(), "for_expression", &mut loops);
    for node in loops {
        let subject = node.child_by_field_name("subject").unwrap();
        assert_eq!(&source[subject.byte_range()], "xs");
    }
    assert!(text_of(&tree, source, "member_expression").contains(&"make_array().len"));
    assert!(text_of(&tree, source, "member_expression").contains(&"(1, 2).len"));
}

#[test]
fn markup_paths_member_imports_and_for() {
    let source = "#import \"lib.notc\": panel as box, label,;\n#import \"common.notc\": *;\n@(asset: p\"./img.svg\")#link(p\"./guide.not\")[guide]\n#for x in (\"a\", \"b\") [#text(text: x)]\n#for x in make_array() { [#text(text: x)] }\n";
    let tree = parse(source, false);
    assert_eq!(text_of(&tree, source, "import_declaration").len(), 2);
    assert_eq!(text_of(&tree, source, "markup_for_expression").len(), 2);
    assert_eq!(
        text_of(&tree, source, "path_literal"),
        ["p\"./img.svg\"", "p\"./guide.not\""]
    );
}

#[test]
fn constraints_and_paths_recover_at_the_next_declaration() {
    for condition in ["", "T ==", "T ||", "("] {
        let source =
            format!("fn broken<T: Type>() -> Int where {condition};\nfn healthy() -> Int {{ 1 }}");
        let tree = parser(true).parse(&source, None).unwrap();
        assert!(tree.root_node().has_error(), "{source}");
        assert!(
            text_of(&tree, &source, "function_declaration").contains(&"fn healthy() -> Int { 1 }")
        );
    }
    let source = "fn path() -> Path { p\"unfinished\n}\nfn healthy() -> Int { 1 }";
    let tree = parse(source, true);
    assert_eq!(text_of(&tree, source, "path_literal"), ["p\"unfinished"]);
    assert_eq!(text_of(&tree, source, "function_declaration").len(), 2);
    for keyword in ["where", "for", "in"] {
        let source = format!("fn {keyword}() -> Int {{ 1 }}");
        assert!(
            parser(true)
                .parse(&source, None)
                .unwrap()
                .root_node()
                .has_error()
        );
    }
}

#[test]
fn loop_subjects_leave_content_bodies_to_the_loop() {
    for subject in [
        "xs",
        "make_array()",
        "make_array().values()",
        "(choose()[input])",
    ] {
        let source = format!("fn f() -> InlineContent {{ for x in {subject} [#text(text: x)] }}");
        let tree = parse(&source, true);
        let mut loops = Vec::new();
        nodes(tree.root_node(), "for_expression", &mut loops);
        assert_eq!(loops.len(), 1);
        let node = loops[0].child_by_field_name("subject").unwrap();
        assert_eq!(&source[node.byte_range()], subject);
    }
}

#[test]
fn expression_precedence_and_trivia() {
    let source = "fn f(n: Int) -> Int { let iffoo = 1; -n + 2 * 3 - 4 / 2 }";
    let tree = parse(source, true);
    assert_eq!(
        text_of(&tree, source, "binary_expression"),
        ["-n + 2 * 3 - 4 / 2", "-n + 2 * 3", "2 * 3", "4 / 2"]
    );
    assert_eq!(text_of(&tree, source, "unary_expression"), ["-n"]);
    for expression in [
        "n<=2",
        "n >= 2",
        "n /* nested /* x */ y */ + 2",
        "n + /* c */ 2",
        "lib.f (1)",
        "lib.f\n(1)",
        "lib.f<enabled: (n > 0)>(1)",
        "if n == 0 { 1 } else if n == 1 { 2 } else { 3 }",
    ] {
        parse(&format!("fn f(n: Int) -> Int {{ {expression} }}"), true);
    }
}
#[test]
fn current_code_language() {
    let source = r#"import "lib.notc" as lib;
import "other.notc";
fn math<C: Type = InlineContent>(text: String) -> C;
fn mark<C: Type>[children: C] -> C {
    let n = comptime lib.count(1 + 2 * 3);
    if C == InlineContent { [#emph[#children]] }
    else { [ #children ] }
}
fn classify(n: Int) -> Int {
    match n { 0 => 1, _ => -n + 2, }
}
fn compare(n: Int) -> Bool { n < 2 && n >= 0 || !false }
fn generic -> Int { lib.value<Int>(n: (3 > 2)) }
"#;
    let tree = parse(source, true);
    assert_eq!(text_of(&tree, source, "code_import").len(), 2);
    assert_eq!(text_of(&tree, source, "function_declaration").len(), 5);
    assert_eq!(text_of(&tree, source, "comptime_parameters").len(), 2);
    assert_eq!(text_of(&tree, source, "if_expression").len(), 1);
    assert_eq!(text_of(&tree, source, "match_arm").len(), 2);
}
#[test]
fn current_markup_imports_and_expressions() {
    let source = "#import \"lib.notc\" as lib;\n#important() #lib.math<InlineContent>(text: lib.label(1 + 2))[#children] #[removed]";
    let tree = parse(source, false);
    assert_eq!(text_of(&tree, source, "import_declaration").len(), 1);
    assert_eq!(text_of(&tree, source, "call").len(), 2);
    assert_eq!(text_of(&tree, source, "reference"), ["#children"]);
}
#[test]
fn calls_enforce_adjacency() {
    let source = "#a.b(foo: -2, \"key\": true)[*body*] #foo[body] #[body] #fn()\n#bare #foo (1) #foo. b(1) #foo. # (1) #foo(\"x\") [text]";
    let tree = parse(source, false);
    assert_eq!(
        text_of(&tree, source, "call"),
        [
            "#a.b(foo: -2, \"key\": true)[*body*]",
            "#foo[body]",
            "#fn()",
            "#foo(\"x\")"
        ]
    );
    assert_eq!(text_of(&tree, source, "reference"), ["#bare", "#foo"]);
    assert_eq!(text_of(&tree, source, "number"), ["2"]);
}
#[test]
fn annotation_dicts_and_boolean_keys() {
    let source = "@!(title: \"Demo\") @(true: false, false: true, tags: (\"a\",))#badge(true: true, false: false)";
    let tree = parse(source, false);
    assert_eq!(text_of(&tree, source, "annotation").len(), 2);
    assert_eq!(
        text_of(&tree, source, "boolean"),
        ["false", "true", "true", "false"]
    );
}
#[test]
fn all_string_and_collection_forms() {
    let source = "#f((), (,), (:), (42), (1,), (1, 2,), (a: (b: true)), -0.25, \"escaped\\n\\\"\", r###\"raw \"##\"###, \"\"\"\nmultiline\n\"\"\", r#\"\"\"\nraw \\n\n\"\"\"#)";
    let tree = parse(source, false);
    assert_eq!(text_of(&tree, source, "string").len(), 4);
    assert_eq!(text_of(&tree, source, "unit"), ["()"]);
    assert_eq!(text_of(&tree, source, "array"), ["(,)", "(1,)", "(1, 2,)"]);
    assert_eq!(text_of(&tree, source, "group"), ["(42)"]);
    assert_eq!(text_of(&tree, source, "dict").len(), 3);
}
#[test]
fn block_boundaries_and_opaque_payloads() {
    let source = "= *Title*\r\n== Next\r\n- One\r\n  + Two\r\nword - inline\r\n---\r\n`#bad() $ @ _` $ x^2 $\r\n```rust\r\n#bad()\r\n```` closing\r\nAfter\r\n";
    let tree = parse(source, false);
    assert_eq!(text_of(&tree, source, "heading").len(), 2);
    assert_eq!(text_of(&tree, source, "list_marker"), ["-", "+"]);
    assert_eq!(text_of(&tree, source, "divider"), ["---"]);
    assert_eq!(text_of(&tree, source, "raw_inline"), ["`#bad() $ @ _`"]);
    assert_eq!(text_of(&tree, source, "raw_content"), ["#bad()\r\n"]);
    assert!(text_of(&tree, source, "call").is_empty());
}
#[test]
fn styles_nest_without_crossing_blank_lines_or_raw() {
    let source = "*strong _inner_ ~strike~*\n*soft\nbreak*\nfoo_bar_baz\nctx.block_indent `strip_continuation_indent`\n\n*unmatched\n\nother*\n\\*escaped\\* \\_ \\~ \\a \\\nnext";
    let tree = parse(source, false);
    assert_eq!(
        text_of(&tree, source, "strong"),
        ["*strong _inner_ ~strike~*", "*soft\nbreak*"]
    );
    assert_eq!(text_of(&tree, source, "emphasis"), ["_inner_", "_bar_"]);
    assert_eq!(text_of(&tree, source, "strike"), ["~strike~"]);
    assert_eq!(text_of(&tree, source, "explicit_break").len(), 1);
    assert_eq!(text_of(&tree, source, "escape").len(), 4);
}
#[test]
fn empty_styles_and_math_stay_literal_and_failed_embed_preserves_wiki() {
    let source = "** __ ~~ $$$ ![[page.not]] ![ordinary] [empty]() [[]]";
    let tree = parse(source, false);
    for kind in ["strong", "emphasis", "strike", "math", "embed"] {
        assert!(text_of(&tree, source, kind).is_empty(), "{kind}");
    }
    assert_eq!(text_of(&tree, source, "wikilink"), ["[[page.not]]", "[[]]"]);
    assert_eq!(text_of(&tree, source, "link").len(), 1);
}

#[test]
fn incomplete_markup_falls_back_to_text() {
    for source in [
        "#bare",
        "#foo::",
        "#foo(1",
        "#foo(1\n\n2)",
        "[ordinary]",
        "[[unfinished",
        "[text](unfinished",
        "*unfinished",
        "_unfinished",
        "~unfinished",
        "`unfinished",
        "$unfinished",
        "$ x$",
        "$$",
        "=plain",
        "-plain",
    ] {
        parse(source, false);
    }
}
#[test]
fn links_embeds_wikilinks_and_comments() {
    let source = "[a *strong*](https://example.com/a_(b) \"title (here)\") ![alt](image.png) [[dir/page.not#id|_label_]]\nhttps://example.com // comment\n/* outer /* nested */ end */";
    let tree = parse(source, false);
    assert_eq!(text_of(&tree, source, "link").len(), 1);
    assert_eq!(text_of(&tree, source, "embed").len(), 1);
    assert_eq!(text_of(&tree, source, "wikilink").len(), 1);
    assert_eq!(text_of(&tree, source, "link_target"), ["dir/page.not#id"]);
    assert_eq!(
        text_of(&tree, source, "comment"),
        ["// comment", "/* outer /* nested */ end */"]
    );
}
#[test]
fn tables_validate_width_and_shield_calls() {
    for source in [
        "| A | B |\n| :--- | ---: |\n| x | y |\n\nAfter",
        "A | B\n--- | ---\nx | y\n",
        "A | B\n--- | ---\n#f(\"a|b\")[hello] | y\n",
    ] {
        assert_eq!(text_of(&parse(source, false), source, "table").len(), 1);
    }
    for source in ["A | B\n---\n", "| A | B |\n| --- | nope |\n"] {
        assert!(text_of(&parse(source, false), source, "table").is_empty());
    }
}
#[test]
fn signatures_unicode_hyphens_generics_and_adjacent_arrows() {
    let source = "fn 中文-name(值?: Array<crate.类型>, rows: Array<Array<Int>> = ((1,),),) [children: InlineContent] -> Content<值>;\nfn fnfoo()->Content;\nfn trailing-()->Content;";
    let tree = parse(source, true);
    assert_eq!(text_of(&tree, source, "function_declaration").len(), 3);
    assert!(text_of(&tree, source, "identifier").contains(&"trailing-"));
    for source in [
        "fn fn()->Content;",
        "fn f(fn: String)->Content;",
        "fn f(x: fn)->Content;",
        "fn f(x?: Int = 1)->Content;",
        "fn f(x: Array<>)->Content;",
        "use x;",
        "fn f(x: Int = 1 + 2)->Content;",
    ] {
        assert!(
            parser(true)
                .parse(source, None)
                .unwrap()
                .root_node()
                .has_error(),
            "{source}"
        );
    }
}
#[test]
fn broken_signature_preserves_next_function() {
    for broken in [
        "fn broken(x: Array<);",
        "fn broken(x: Int = (1,;",
        "fn broken(x: String) -> ;",
        "fn broken(x: Int\n",
        "let old = 1;",
    ] {
        let source = format!("{broken}\nfn healthy()->Content;\n");
        let tree = parser(true).parse(&source, None).unwrap();
        assert!(tree.root_node().has_error());
        assert!(
            text_of(&tree, &source, "function_declaration").contains(&"fn healthy()->Content;"),
            "{source}: {}",
            tree.root_node().to_sexp()
        );
    }
}
#[test]
fn highlights_code_without_coloring_prose() {
    let source = "true 123 String fn plain\n#pkg.badge(label: \"text\", flag: true, n: 2)";
    let tree = parse(source, false);
    let query = Query::new(&notist::LANGUAGE.into(), notist::HIGHLIGHTS_QUERY).unwrap();
    let mut cursor = QueryCursor::new();
    let mut captures = cursor.captures(&query, tree.root_node(), source.as_bytes());
    let mut results = Vec::new();
    while let Some((m, i)) = captures.next() {
        let capture = m.captures[*i];
        results.push((
            query.capture_names()[capture.index as usize],
            &source[capture.node.byte_range()],
        ));
    }
    assert!(results.contains(&("function.call", "pkg.badge")));
    assert!(results.contains(&("constant.builtin", "true")));
    assert!(
        results
            .iter()
            .all(|(_, text)| *text != "123" && *text != "String" && *text != "fn")
    );
}
#[test]
fn incremental_edits_match_fresh_trees_and_ranges() {
    for (code, original) in [
        (false, include_str!("../examples/showcase.not")),
        (true, include_str!("../examples/signatures.notc")),
        (true, include_str!("../examples/functions.notc")),
        (true, include_str!("../examples/current.notc")),
        (
            false,
            "#for x in (1, 2) [#text(text: str(x))]\n#link(p\"./guide.not\")[guide]\n",
        ),
        (false, "= CRLF\r\n*text*\r\n```\r\nopaque\r\n```\r\n"),
        (true, "fn 空(值: String = \"\"\"\n多行\n\"\"\")->Content;\n"),
    ] {
        for (offset, ch) in original.char_indices() {
            let mut parser = parser(code);
            let mut tree = parser.parse(original, None).unwrap();
            let mut source = original.to_owned();
            let end = offset + ch.len_utf8();
            let point = |s: &str, byte: usize| {
                let prefix = &s[..byte];
                Point::new(
                    prefix.bytes().filter(|b| *b == b'\n').count(),
                    prefix.rfind('\n').map_or(byte, |i| byte - i - 1),
                )
            };
            tree.edit(&InputEdit {
                start_byte: offset,
                old_end_byte: end,
                new_end_byte: offset,
                start_position: point(&source, offset),
                old_end_position: point(&source, end),
                new_end_position: point(&source, offset),
            });
            source.replace_range(offset..end, "");
            let edited = parser.parse(&source, Some(&tree)).unwrap();
            let fresh = parser.parse(&source, None).unwrap();
            let mut left = Vec::new();
            let mut right = Vec::new();
            snapshot(edited.root_node(), &mut left);
            snapshot(fresh.root_node(), &mut right);
            assert_eq!(left, right, "delete {ch:?} at {offset} ({code})");
            let mut edited = edited;
            edited.edit(&InputEdit {
                start_byte: offset,
                old_end_byte: offset,
                new_end_byte: end,
                start_position: point(&source, offset),
                old_end_position: point(&source, offset),
                new_end_position: point(original, end),
            });
            let restored = parser.parse(original, Some(&edited)).unwrap();
            let fresh = parser.parse(original, None).unwrap();
            left.clear();
            right.clear();
            snapshot(restored.root_node(), &mut left);
            snapshot(fresh.root_node(), &mut right);
            assert_eq!(left, right, "restore {ch:?} at {offset} ({code})");
        }
    }
}
