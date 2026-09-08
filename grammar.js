/**
 * @file Notist grammar for tree-sitter
 * @license MIT OR Apache-2.0
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: "notist",

  extras: _ => [],

  word: $ => $.identifier,

  externals: $ => [
    $.escaped_inline_open,
    $.escaped_multiline_open,
    $.raw_inline_open,
    $.raw_multiline_open,
    $.string_content,
    $.escape_sequence,
    $.string_close,
    $.inline_raw,
    $.fence_open,
    $.fence_info,
    $.fence_content,
    $.fence_close,
    $.line_comment,
    $.block_comment,
    $.text_chunk,
    $.strong_open,
    $.strong_close,
    $.emphasis_open,
    $.emphasis_close,
    $.underline_open,
    $.underline_close,
    $.strike_open,
    $.strike_close,
    $.math_open,
    $.math_content,
    $.math_close,
    $.math_block_open,
    $.math_block_content,
    $.math_block_close,
    $.target_open,
    $.heading_marker,
    $.list_marker,
    $.enum_marker,
    $.task_marker,
    $.rule_marker,
    $.pipe,
    $.table_delimiter_row,
    $.or_operator,
    $.and_operator,
    $.comparison_operator,
    $.additive_operator,
    $.multiplicative_operator,
    $.else_keyword,
  ],

  conflicts: $ => [
    // A line starting with `|` is a table only when a delimiter row follows;
    // otherwise the pipes fall back to plain text.
    [$._item, $.table_row],
    // 分隔行之后遇到换行：继续 body 行还是结束 table，交给 GLR +
    // table_row 的 dynamic precedence。
    [$.table],
    [$.table_row],
    // `if c [a] else ...` — 空白后是 else 则继续，否则 if 结束。
    [$.if_expression],
    // `(x: Int) => ...` versus `(expression)`.
    [$.parameter, $.qualified_name],
    [$.parameters],
    // `()` — Unit 字面量与空参数表（lambda / let 的 `name()`）同形，
    // 由后随 token（`=>` / `->`）判别。
    [$.unit_literal, $.parameters],
    // Array / Dict 字面量与 parameters 同构：trivia 重的重复体交给 GLR。
    [$.array_literal],
    // `name: Type = default` — 空白后是 `=` 则带默认值，否则形参结束。
    [$.parameter],
    [$.arguments],
    [$.import_item],
    // `name: value` versus a positional bare name in arguments.
    [$.qualified_name, $.named_argument],
    // 前导空行后是否跟 @![...]：模块属性解析优先（dynamic precedence）。
    [$.document],
  ],

  rules: {
    document: $ => seq(
      repeat($._line_break),
      // 2026-08-31: 前导 `@!expr` 模块标注可堆叠——模块标注是元数据而非
      // 内容，后续标注仍先于第一个 Item。位置违规由 analyzer 诊断。
      repeat(choice($.module_annotation, $._item, $._line_break)),
    ),

    _item: $ => choice(
      $.heading,
      $.list_item,
      $.enum_item,
      $.task_item,
      $.rule,
      $.table,
      $.annotation,
      $.code_block,
      $.embedded_expression,
      $.fenced_raw,
      $.math_block,
      $.inline_raw,
      $.strong,
      $.emphasis,
      $.underline,
      $.strike,
      $.inline_math,
      $.escaped_punctuation,
      alias($.pipe, $.text),
      alias($.text_chunk, $.text),
      $.text,
    ),

    text: _ => token(prec(-2, /[#\[\]`/@,*_~$\\{}(]/)),

    heading: $ => prec.right(seq(
      field("marker", $.heading_marker),
      optional(field("body", $.inline_body)),
    )),

    list_item: $ => prec.right(seq(
      field("marker", $.list_marker),
      optional(field("body", $.inline_body)),
    )),

    enum_item: $ => prec.right(seq(
      field("marker", $.enum_marker),
      optional(field("body", $.inline_body)),
    )),

    task_item: $ => prec.right(seq(
      field("marker", $.task_marker),
      optional(field("body", $.inline_body)),
    )),

    rule: $ => field("marker", $.rule_marker),

    // Table = header row + delimiter row + body rows; the delimiter row is a
    // single external token so that `|` lines without a delimiter stay text.
    table: $ => seq(
      field("header", $.table_row),
      $._line_break,
      $.table_delimiter_row,
      repeat(seq($._line_break, field("row", $.table_row))),
    ),

    // 每多一行 dynamic precedence 加一：真正的 table 解析总是优于
    // "table 提前结束后把后续 `|` 行按文本解析" 的回退解析。
    table_row: $ => prec.dynamic(1, seq(
      $.pipe,
      repeat1(seq(
        optional(field("cell", $.table_cell)),
        $.pipe,
      )),
    )),

    // cell 是行内 markup 子文档，但 `|` 在 cell 内不做文本回退
    //（否则 cell 会把分隔符吞成文本）；字面 `|` 写作 `\|`。
    // dynamic precedence：吃掉更多 cell 的解析优先于 table 提前在行中结束。
    table_cell: $ => prec.dynamic(1, prec.right(repeat1($._table_inline))),

    _table_inline: $ => choice(
      $.embedded_expression,
      $.code_block,
      $.inline_raw,
      $.inline_math,
      $.strong,
      $.emphasis,
      $.underline,
      $.strike,
      $.escaped_punctuation,
      alias($.text_chunk, $.text),
      $.text,
    ),

    inline_body: $ => prec.right(repeat1($._inline)),

    _inline: $ => choice(
      $.embedded_expression,
      $.code_block,
      $.inline_raw,
      $.inline_math,
      $.strong,
      $.emphasis,
      $.underline,
      $.strike,
      $.escaped_punctuation,
      alias($.pipe, $.text),
      alias($.text_chunk, $.text),
      $.text,
    ),

    strong: $ => seq(
      field("open", alias($.strong_open, $.strong_marker)),
      field("body", repeat1($._strong_item)),
      field("close", alias($.strong_close, $.strong_marker)),
    ),

    _strong_item: $ => choice(
      $.emphasis,
      $.underline,
      $.strike,
      $._strong_atom,
    ),

    _strong_atom: $ => choice(
      $.embedded_expression,
      $.inline_raw,
      $.inline_math,
      $.escaped_punctuation,
      alias($.pipe, $.text),
      alias($.text_chunk, $.text),
      alias(token(prec(-2, /[#\[\]`/@,_~$\\{}(]/)), $.text),
    ),

    emphasis: $ => seq(
      field("open", alias($.emphasis_open, $.emphasis_marker)),
      field("body", repeat1($._emphasis_item)),
      field("close", alias($.emphasis_close, $.emphasis_marker)),
    ),

    _emphasis_item: $ => choice(
      $.strong,
      $.strike,
      $._emphasis_atom,
    ),

    _emphasis_atom: $ => choice(
      $.embedded_expression,
      $.inline_raw,
      $.inline_math,
      $.escaped_punctuation,
      alias($.pipe, $.text),
      alias($.text_chunk, $.text),
      alias(token(prec(-2, /[#\[\]`/@,*~$\\{}(]/)), $.text),
    ),

    underline: $ => seq(
      field("open", alias($.underline_open, $.underline_marker)),
      field("body", repeat1($._underline_item)),
      field("close", alias($.underline_close, $.underline_marker)),
    ),

    _underline_item: $ => choice(
      $.strong,
      $.strike,
      $._emphasis_atom,
    ),

    strike: $ => seq(
      field("open", alias($.strike_open, $.strike_marker)),
      field("body", repeat1($._strike_item)),
      field("close", alias($.strike_close, $.strike_marker)),
    ),

    _strike_item: $ => choice(
      $.strong,
      $.emphasis,
      $.underline,
      $._strike_atom,
    ),

    _strike_atom: $ => choice(
      $.embedded_expression,
      $.inline_raw,
      $.inline_math,
      $.escaped_punctuation,
      alias($.pipe, $.text),
      alias($.text_chunk, $.text),
      alias(token(prec(-2, /[#\[\]`/@,*_$\\{}(]/)), $.text),
    ),

    // 行内数学 `$...$`：`$` 后必须紧跟非空白、非 `$` 才算开符；闭符 `$`
    // 前必须是非空白；内容是 raw text（不解释 markup，`\$` 不闭、原样
    // 保留）。开符 `math_open` 是外部 token：扫描器只在同一行内验证到
    // 合法闭符时才产出，否则 `$` 降级为普通文本，不跨行。
    inline_math: $ => seq(
      field("open", $.math_open),
      optional(field("body", $.math_content)),
      field("close", $.math_close),
    ),

    escaped_punctuation: _ => token(/\\[\x21-\x2f\x3a-\x40\x5b-\x60\x7b-\x7e]/),

    // Target literal `<path[/label]>` — the `<` `>` pair delimits the body.
    // `target_open` is external so the scanner only fires it when the same
    // line actually holds a closing `>`; otherwise `<` degrades to text like
    // every other paired inline delimiter. `\<` `\>` `\\` escape the
    // delimiters and the backslash; no line breaks inside.
    target_literal: $ => seq(
      $.target_open,
      field("target", $.target_content),
      ">",
    ),

    target_content: _ => token.immediate(/(\\[<>\\]|[^>\\\x00-\x1f\x7f])+/),

    // EmbeddedExpression = "#" EmbeddedCode
    // The top-level expression after "#" stops at whitespace: binary/unary
    // operations need parentheses (`#(1 + 2)`), while `#1 + 2` embeds `1`.
    embedded_expression: $ => seq(
      "#",
      field("expression", $._embedded_expression),
    ),

    _embedded_expression: $ => choice(
      $.unit_literal,
      $.boolean,
      $.float,
      $.integer,
      $.string,
      $.content_block,
      $.code_block,
      $.call_expression,
      $.qualified_name,
      $.parenthesized_expression,
      $.if_expression,
      $.lambda,
      $.let_expression,
      $.import_expression,
      $.target_literal,
    ),

    _code_expression: $ => choice(
      $.unit_literal,
      $.boolean,
      $.float,
      $.integer,
      $.string,
      $.content_block,
      $.code_block,
      $.call_expression,
      $.qualified_name,
      $.parenthesized_expression,
      $.unary_expression,
      $.binary_expression,
      $.if_expression,
      $.lambda,
      $.let_expression,
      $.import_expression,
      $.target_literal,
    ),

    // `()` 是 Unit 的字面量与唯一值（2026-08-31 裁决，替代 none 关键字）。
    // 与 Array `(,)`、Dict `(:)` 由括号后首字符判别。
    unit_literal: $ => seq("(", repeat($._code_trivia), ")"),
    boolean: _ => choice("true", "false"),
    integer: _ => token(prec(2, /[0-9]+/)),
    float: _ => token(prec(2, /[0-9]+\.[0-9]+/)),

    string: $ => choice(
      $.escaped_inline_string,
      $.escaped_multiline_string,
      $.raw_inline_string,
      $.raw_multiline_string,
    ),

    escaped_inline_string: $ => seq(
      $.escaped_inline_open,
      repeat(choice($.string_content, $.escape_sequence)),
      $.string_close,
    ),

    escaped_multiline_string: $ => seq(
      $.escaped_multiline_open,
      repeat(choice($.string_content, $.escape_sequence)),
      $.string_close,
    ),

    raw_inline_string: $ => seq(
      $.raw_inline_open,
      repeat($.string_content),
      $.string_close,
    ),

    raw_multiline_string: $ => seq(
      $.raw_multiline_open,
      repeat($.string_content),
      $.string_close,
    ),

    // ContentBlock = "[" Markup "]"
    content_block: $ => seq(
      "[",
      optional(field("body", $.content_body)),
      "]",
    ),

    content_body: $ => repeat1(choice($._item, $._line_break)),

    // CodeBlock = "{" Statements "}"; statements are separated by ";" or
    // line breaks (both are trivia here, so the grammar stays lenient).
    code_block: $ => seq(
      "{",
      repeat(choice(
        $._code_trivia,
        ";",
        $._code_expression,
      )),
      "}",
    ),

    // CallExpression = QualifiedName (Arguments ContentBlock* | ContentBlock+)
    // 尾随块必须紧邻 callee/arguments（无空白），故 call 优先于语句切分。
    call_expression: $ => prec.right(1, seq(
      field("function", $.qualified_name),
      choice(
        seq(
          field("arguments", $.arguments),
          repeat(field("trailing", $.content_block)),
        ),
        repeat1(field("trailing", $.content_block)),
      ),
    )),

    parenthesized_expression: $ => seq(
      "(",
      repeat($._code_trivia),
      $._code_expression,
      repeat($._code_trivia),
      ")",
    ),

    // `..expr`：集合字面量内的展开——Dict 拼接 Dict，Array 拼接 Array。
    spread: $ => seq("..", $._code_expression),

    // not a == b 是 not (a == b)，-a * b 是 (-a) * b。
    unary_expression: $ => choice(
      prec(3, seq(
        "not",
        repeat($._code_trivia),
        field("operand", $._code_expression),
      )),
      prec(7, seq(
        "-",
        repeat($._code_trivia),
        field("operand", $._code_expression),
      )),
    ),

    // 优先级从低到高：or -> and -> 比较/相等 -> 加减 -> 乘除。
    // 二元运算符是 external token（scanner 跳过前导水平空白）：只在二元延续
    // 合法的 parser 状态下产生，避免被 markup 文本栈的 text_chunk 抢先吞掉；
    // 同时保证 `#1 + 2` 的 ` + 2` 仍按文本解析、换行后的 `- item` 不被吞进
    // 上一行的 let 右值。
    binary_expression: $ => choice(
      prec.left(1, seq(
        field("left", $._code_expression),
        field("operator", $.or_operator),
        repeat($._code_trivia),
        field("right", $._code_expression),
      )),
      prec.left(2, seq(
        field("left", $._code_expression),
        field("operator", $.and_operator),
        repeat($._code_trivia),
        field("right", $._code_expression),
      )),
      prec.left(4, seq(
        field("left", $._code_expression),
        field("operator", $.comparison_operator),
        repeat($._code_trivia),
        field("right", $._code_expression),
      )),
      prec.left(5, seq(
        field("left", $._code_expression),
        field("operator", $.additive_operator),
        repeat($._code_trivia),
        field("right", $._code_expression),
      )),
      prec.left(6, seq(
        field("left", $._code_expression),
        field("operator", $.multiplicative_operator),
        repeat($._code_trivia),
        field("right", $._code_expression),
      )),
    ),

    if_expression: $ => seq(
      "if",
      repeat($._code_trivia),
      field("condition", $._code_expression),
      repeat($._code_trivia),
      field("then", choice($.code_block, $.content_block)),
      optional(seq(
        repeat($._code_trivia),
        alias($.else_keyword, "else"),
        repeat($._code_trivia),
        field("else", choice($.if_expression, $.code_block, $.content_block)),
      )),
    ),

    lambda: $ => seq(
      field("parameters", $.parameters),
      repeat($._code_trivia),
      "=>",
      repeat($._code_trivia),
      field("body", $._code_expression),
    ),

    // let name[: Type] = expr | let name(params) -> R = body
    let_expression: $ => seq(
      "let",
      repeat1($._code_trivia),
      field("name", $.identifier),
      optional(seq(
        repeat($._code_trivia),
        ":",
        repeat($._code_trivia),
        field("type", $.type_expression),
      )),
      repeat($._code_trivia),
      choice(
        seq(
          field("parameters", $.parameters),
          repeat($._code_trivia),
          "->",
          repeat($._code_trivia),
          field("return_type", $.type_expression),
          repeat($._code_trivia),
          "=",
          repeat($._code_trivia),
          field("body", $._code_expression),
        ),
        seq(
          "=",
          repeat($._code_trivia),
          field("value", $._code_expression),
        ),
      ),
    ),

    // import <path>::{name, name as alias} — 显式选择器，无 wildcard。
    import_expression: $ => seq(
      "import",
      repeat1($._code_trivia),
      field("path", $.target_literal),
      "::",
      "{",
      repeat($._code_trivia),
      $.import_item,
      repeat(seq(
        repeat($._code_trivia),
        ",",
        repeat($._code_trivia),
        $.import_item,
      )),
      optional(seq(repeat($._code_trivia), ",")),
      repeat($._code_trivia),
      "}",
    ),

    import_item: $ => seq(
      field("name", $.identifier),
      optional(seq(
        repeat1($._code_trivia),
        "as",
        repeat1($._code_trivia),
        field("alias", $.identifier),
      )),
    ),

    // Type = TypeMember ("|" TypeMember)*
    // 紧邻的 `?` 绑定最内层类型。
    type_expression: $ => prec.right(1, seq(
      $.type_member,
      repeat(seq(
        $.union_operator,
        optional($._code_trivia),
        $.type_member,
      )),
    )),

    type_member: $ => prec.right(seq(
      choice(
        $.qualified_name,
        $.function_type,
      ),
      optional("?"),
    )),

    union_operator: _ => token(prec(2, /[ \t]*\|/)),

    function_type: $ => seq(
      "fn",
      repeat($._code_trivia),
      field("parameters", $.parameters),
      repeat($._code_trivia),
      "->",
      repeat($._code_trivia),
      field("return_type", $.type_expression),
    ),

    parameters: $ => seq(
      "(",
      repeat($._code_trivia),
      optional(seq(
        $.parameter,
        repeat(seq(
          repeat($._code_trivia),
          ",",
          repeat($._code_trivia),
          $.parameter,
        )),
        optional(seq(repeat($._code_trivia), ",")),
      )),
      repeat($._code_trivia),
      ")",
    ),

    // 形参带 "=" 的是 named 可省略；类型位置的 "=" 后无默认表达式。
    parameter: $ => seq(
      optional(seq("trailing", repeat1($._code_trivia))),
      field("name", $.identifier),
      repeat($._code_trivia),
      ":",
      repeat($._code_trivia),
      field("type", $.type_expression),
      optional(choice(
        seq(
          repeat($._code_trivia),
          "=",
          repeat($._code_trivia),
          field("default", $._code_expression),
        ),
        seq(repeat($._code_trivia), "="),
      )),
    ),

    qualified_name: $ => seq(
      $.identifier,
      repeat(seq("::", $.identifier)),
    ),

    identifier: _ => /[\p{L}_][\p{L}\p{N}_-]*/,

    arguments: $ => seq(
      "(",
      repeat($._code_trivia),
      optional(seq(
        $.argument,
        repeat(seq(
          repeat($._code_trivia),
          ",",
          repeat($._code_trivia),
          $.argument,
        )),
        optional(seq(repeat($._code_trivia), ",")),
      )),
      repeat($._code_trivia),
      ")",
    ),

    argument: $ => choice(
      $.named_argument,
      $.positional_argument,
    ),

    named_argument: $ => seq(
      field("name", $.identifier),
      repeat($._code_trivia),
      ":",
      repeat($._code_trivia),
      field("value", $._code_expression),
    ),

    positional_argument: $ => field("value", $._code_expression),

    fenced_raw: $ => seq(
      $.fence_open,
      optional(field("language", $.fence_info)),
      $._line_break,
      optional(field("body", $.fence_content)),
      field("close", $.fence_close),
    ),

    // 块级数学：开行与闭行都是"仅含 `$$` 的行"（行首可有空白）；内容为
    // 多行 raw text。开行由扫描器验证（行首空白 + `$$` + 其余仅空白）才
    // 产出 math_block_open；未闭合时错误节点吃到 EOF。开行后的换行与
    // fenced_raw 同款用显式 _line_break 承接，内容行才都从行首起扫。
    math_block: $ => seq(
      field("open", $.math_block_open),
      $._line_break,
      optional(field("body", $.math_block_content)),
      field("close", $.math_block_close),
    ),

    // 标注（2026-08-31 统一数据模型）：`@expr` 绑定其后紧邻的 Item，
    // `@!expr` 在文件开头绑定模块根。载荷是求值为 Dict 的单个表达式：
    // 标识符速记、调用，或 Dict 字面量。无 postfix、无裸键糖、无 fallback。
    annotation: $ => seq("@", field("payload", $._annotation_payload)),

    module_annotation: $ => seq("@!", field("payload", $._annotation_payload)),

    _annotation_payload: $ => choice(
      $.qualified_name,
      $.call_expression,
      $.dict_literal,
    ),

    // `(,)` 空 Array；单元素尾逗号必需：`(a,)`。条目在重复体内必选，
    // 尾逗号由独立 optional 子句承接（与 arguments 同构，避免 GLR 误判）。
    array_literal: $ => seq(
      "(",
      repeat($._code_trivia),
      choice(
        ",",
        seq(
          choice(
            field("element", $._code_expression),
            field("spread", $.spread),
          ),
          repeat(seq(
            repeat($._code_trivia),
            ",",
            repeat($._code_trivia),
            choice(
              field("element", $._code_expression),
              field("spread", $.spread),
            ),
          )),
          optional(seq(repeat($._code_trivia), ",")),
        ),
      ),
      ")",
    ),

    // `(:)` 空 Dict；条目为 `key: value` 或 `..expr` 展开，尾逗号合法。
    dict_literal: $ => seq(
      "(",
      repeat($._code_trivia),
      choice(
        ":",
        seq(
          field("entry", $._dict_entry),
          repeat(seq(
            repeat($._code_trivia),
            ",",
            repeat($._code_trivia),
            field("entry", $._dict_entry),
          )),
          optional(seq(repeat($._code_trivia), ",")),
        ),
      ),
      ")",
    ),

    _dict_entry: $ => choice(
      seq(
        field("key", choice($.identifier, $.string, $.integer, $.boolean)),
        repeat($._code_trivia),
        ":",
        repeat($._code_trivia),
        field("value", $._code_expression),
      ),
      $.dict_spread,
    ),

    dict_spread: $ => seq("..", field("value", $._code_expression)),

    _line_break: _ => /\r?\n/,
    _whitespace: _ => /[ \t\r\n]+/,

    // Code 上下文 trivia：空白与注释（注释只在 Code 上下文合法）。
    _code_trivia: $ => choice(
      $._whitespace,
      $.line_comment,
      $.block_comment,
    ),
  },
});
