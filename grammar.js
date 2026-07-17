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
  ],

  rules: {
    document: $ => repeat($._item),

    _item: $ => choice(
      $.wiki_reference,
      $.embedded_expression,
      $.fenced_raw,
      $.inline_raw,
      $.text,
    ),

    text: _ => token(prec(-1, /[^#\[\]`]+|[#\[\]`]/)),

    wiki_reference: $ => seq(
      "[[",
      field("target", $.wiki_target),
      "]]",
    ),

    wiki_target: _ => token.immediate(/[^\]\n]+/),

    // EmbeddedExpression = "#" CodeExpression Attributes?
    embedded_expression: $ => seq(
      "#",
      field("expression", $._code_expression),
      optional(field("attributes", $.attributes)),
    ),

    _code_expression: $ => choice(
      $.none,
      $.boolean,
      $.float,
      $.integer,
      $.string,
      $.content_block,
      $.call_expression,
      $.parenthesized_expression,
    ),

    none: _ => "none",
    boolean: _ => choice("true", "false"),
    integer: _ => token(prec(2, /-?[0-9]+/)),
    float: _ => token(prec(2, /-?(?:[0-9]+\.[0-9]*|[0-9]*\.[0-9]+)/)),

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

    content_body: $ => repeat1($._item),

    // CallExpression = QualifiedName (Arguments ContentBlock* | ContentBlock+)
    call_expression: $ => seq(
      field("function", $.qualified_name),
      choice(
        seq(
          field("arguments", $.arguments),
          repeat(field("trailing", $.content_block)),
        ),
        repeat1(field("trailing", $.content_block)),
      ),
    ),

    parenthesized_expression: $ => seq(
      "(",
      optional($._whitespace),
      $._code_expression,
      optional($._whitespace),
      ")",
    ),

    qualified_name: $ => seq(
      $.identifier,
      repeat(seq("::", $.identifier)),
    ),

    identifier: _ => /[\p{L}\p{N}_-]+/,

    arguments: $ => seq(
      "(",
      optional($._whitespace),
      optional(seq(
        $.argument,
        repeat(seq(
          optional($._whitespace),
          ",",
          optional($._whitespace),
          $.argument,
        )),
        optional(seq(optional($._whitespace), ",")),
      )),
      optional($._whitespace),
      ")",
    ),

    argument: $ => choice(
      $.named_argument,
      $.positional_argument,
    ),

    named_argument: $ => seq(
      field("name", $.identifier),
      optional($._whitespace),
      "=",
      optional($._whitespace),
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
      "=",
      field("value", $.attribute_value),
    ),

    attribute_value: $ => choice(
      $.identifier,
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
  },
});
