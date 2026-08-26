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
    $.math_close,
    $.heading_marker,
    $.list_marker,
    $.enum_marker,
    $.task_marker,
    $.rule_marker,
    $.pipe,
    $.table_delimiter_row,
    $.block_attributes_open,
    $.module_attributes_open,
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
    // `@[name = value]` — 标识符后随空白时 id 与 property 的分叉。
    [$.id_attribute, $.property_attribute],
    // `if c [a] else ...` — 空白后是 else 则继续，否则 if 结束。
    [$.if_expression],
    // `(x: Int) => ...` versus `(expression)`.
    [$.parameter, $.qualified_name],
    [$.parameters],
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
      optional($.module_attributes),
      repeat(choice($._item, $._line_break)),
    ),

    _item: $ => choice(
      $.heading,
      $.list_item,
      $.enum_item,
      $.task_item,
      $.rule,
      $.table,
      $.block_attributes,
      $.code_block,
      $.embedded_expression,
      $.fenced_raw,
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
      $.strong,
      $.emphasis,
      $.underline,
      $.strike,
      $.inline_math,
      $.escaped_punctuation,
      alias($.text_chunk, $.text),
      $.text,
    ),

    block_attributes: $ => seq(
      $.block_attributes_open,
      optional($._whitespace),
      $.first_attribute,
      repeat(seq(
        optional($._whitespace),
        ",",
        optional($._whitespace),
        field("item", $.attribute_item),
      )),
      optional($._whitespace),
      "]",
    ),

    module_attributes: $ => prec.dynamic(1, seq(
      $.module_attributes_open,
      optional($._whitespace),
      $.attribute_item,
      repeat(seq(
        optional($._whitespace),
        ",",
        optional($._whitespace),
        field("item", $.attribute_item),
      )),
      optional($._whitespace),
      "]",
    )),

    inline_body: $ => prec.right(repeat1($._inline)),

    _inline: $ => choice(
      $.embedded_expression,
      $.code_block,
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

    inline_math: $ => seq(
      field("open", alias($.math_open, $.math_marker)),
      field("body", repeat1(choice(
        alias($.text_chunk, $.math_text),
        alias(token(prec(-2, /[#\[\]`/@,*_~\\{}(|]/)), $.math_text),
      ))),
      field("close", alias($.math_close, $.math_marker)),
    ),

    escaped_punctuation: _ => token(/\\[\x21-\x2f\x3a-\x40\x5b-\x60\x7b-\x7e]/),

    // Target literal `<path[/label]>` — the `<` `>` pair delimits the body;
    // backslash escapes are accepted by the body token.
    target_literal: $ => seq(
      "<",
      field("target", $.target_content),
      ">",
    ),

    target_content: _ => token.immediate(/[^>\n]+/),

    // EmbeddedExpression = "#" EmbeddedCode Attributes?
    // The top-level expression after "#" stops at whitespace: binary/unary
    // operations need parentheses (`#(1 + 2)`), while `#1 + 2` embeds `1`.
    embedded_expression: $ => seq(
      "#",
      field("expression", $._embedded_expression),
      optional(field("attributes", $.attributes)),
    ),

    _embedded_expression: $ => choice(
      $.none,
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
      $.none,
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

    none: _ => "none",
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

    attributes: $ => seq(
      "@",
      field("first", $.first_attribute),
      repeat(seq(",", field("item", $.attribute_item))),
    ),

    first_attribute: $ => choice(
      $.id_attribute,
      $.attribute_item,
    ),

    attribute_item: $ => choice(
      $.tag_attribute,
      $.class_attribute,
      $.property_attribute,
    ),

    id_attribute: $ => field("name", $.identifier),
    tag_attribute: $ => seq("#", field("name", $.identifier)),
    class_attribute: $ => seq(".", field("name", $.identifier)),
    property_attribute: $ => seq(
      field("key", $.identifier),
      optional($._whitespace),
      "=",
      optional($._whitespace),
      field("value", $.attribute_value),
    ),

    // key = value 的 value 只许 String / Int / Bool 字面量。
    attribute_value: $ => choice(
      $.boolean,
      $.integer,
      $.attribute_string,
    ),

    attribute_string: $ => seq(
      '"',
      optional($.attribute_string_content),
      '"',
    ),

    attribute_string_content: $ => repeat1(choice(
      token.immediate(/[^"\\]+/),
      token.immediate(/\\./),
    )),

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
