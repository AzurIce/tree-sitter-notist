/**
 * @file Tree-sitter grammar for Notist: the `.not` markup frontend and the
 * `.notc` code module format, tracking the reference parser in
 * `crates/notist-syntax/src/lib.rs` of the Notist repository.
 *
 * This grammar has a fixed Markup entry point. notist-code/grammar.js
 * inherits the shared rules and selects the Code entry point.
 *
 * Editor-level divergences from the reference parser, kept deliberately:
 * - sections are flat siblings (level lives on the marker) instead of nested
 *   by heading level, matching common markup grammars;
 * - section bodies keep preceding annotations in lexical order; attachment
 *   to the following heading is performed by the Notist frontend, so fold
 *   ranges may include that heading's leading Item annotations;
 * - sections may not open inside `*`/`_` styled spans (the marker degrades
 *   to text there);
 * - horizontal whitespace between markup nodes is lexed as trivia, so text
 *   runs never start or end with spaces they do not need.
 * @author AzurIce
 * @license MIT OR Apache-2.0
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const IDENTIFIER = /[\p{L}_][\p{L}\p{N}_]*/;
const IMMEDIATE_IDENTIFIER = token.immediate(/[\p{L}_][\p{L}\p{N}_]*/);

module.exports = grammar({
  name: 'notist',

  extras: $ => [
    /[ \t\r\n]+/,
    $.comment,
  ],

  externals: $ => [
    $._line_break,
    $.heading_marker,
    $._immediate_call_open,
    $._immediate_field_dot,
    $._immediate_content_open,
    $.raw,
    $.math,
    $.comment,
    $.list_marker,
    $.autolink,
    $.table_start,
    $.table_delimiter_row,
    $.table_row_start,
    $.pipe,
    $.emphasis_marker,
  ],

  word: $ => $.identifier,

  conflicts: $ => [
    [$.content_block, $.bracketed_text],
    [$.parenthesized_expression, $.list_literal],
    [$.parenthesized_expression, $.lambda],
    [$.list_literal, $.lambda],
    [$.dict_literal, $.lambda],
    [$.type, $.qualified_name],
    [$.parameter, $.qualified_name],
    [$.if_expression, $.text],
    [$._primary_expression, $.text],
    [$.qualified_name],
    [$.qualified_name, $.text],
    [$._primary_expression, $._markup_item],
  ],

  rules: {
    source_file: $ => optional($.markup_file),

    // ============================================================ files

    // Leading newlines are tolerated so a code module may open with a
    // blank line (the corpus runner also feeds every case a leading `\n`).
    code_file: $ => seq(repeat($._line_break), repeat1($._statement)),

    markup_file: $ => repeat1(choice($._markup_item, $.section)),

    // ======================================================== statements

    _statement: $ =>
      choice($.let_statement, $.use_statement, $.wasm_statement, $.expression_statement),

    let_statement: $ => prec.dynamic(1, seq('let', field('name', $.identifier), optional(seq(':', field('type', $.type))), '=', field('value', $.expression), ';')),
    use_statement: $ => prec.dynamic(1, seq('use', $._use_tree, ';')),
    wasm_statement: $ => prec.dynamic(1, seq('wasm', field('path', $.string), ';')),
    expression_statement: $ => prec.dynamic(1, seq(field('value', $.expression), ';')),

    _use_tree: $ =>
      prec.right(seq(
        repeat(seq($._use_segment, '::')),
        choice($.use_group, $.use_glob, $.use_leaf),
      )),

    use_group: $ => seq(
      '{',
      optional(seq(
        $._use_tree,
        repeat(seq(',', $._use_tree)),
        optional(','),
      )),
      '}',
    ),

    use_glob: _ => '*',

    use_leaf: $ => seq(
      $._use_segment,
      optional(seq('as', field('alias', $.identifier))),
    ),

    _use_segment: $ => $.identifier,

    // ====================================================== expressions

    expression: $ => choice($.binary_expression, $._code_postfix_expression),

    binary_expression: $ => choice(
      ...[
        ['==', 1], ['!=', 1], ['<', 1], ['>', 1], ['<=', 1], ['>=', 1],
        ['+', 2], ['-', 2],
        ['*', 3], ['/', 3],
      ].map(([operator, precedence]) =>
        prec.left(precedence, seq(
          field('left', $.expression),
          field('operator', operator),
          field('right', $.expression),
        )),
      ),
    ),

    unary_expression: $ => prec(4, seq('-', field('operand', $.expression))),

    if_expression: $ => seq(
      'if',
      field('condition', $.expression),
      '{',
      field('consequence', $.expression),
      '}',
      'else',
      '{',
      field('alternative', $.expression),
      '}',
    ),

    lambda: $ => seq(
      field('parameters', $.parameters),
      optional(seq('->', field('return_type', $.type))),
      '=>',
      field('body', $.expression),
    ),

    parameters: $ => seq(
      '(',
      optional(seq(
        $.parameter,
        repeat(seq(',', $.parameter)),
        optional(','),
      )),
      ')',
    ),

    parameter: $ => seq(
      field('name', $.identifier),
      optional(seq(':', field('type', $.type))),
      optional(seq('=', field('default', $.expression))),
    ),

    type: $ => seq(field('name', $.identifier), optional('?')),

    // Parenthesized groups: `(e)` keeps the expression, `(a, b)` and `(a,)`
    // are lists, `(k: v)` dicts, with `()` for Unit, `(,)` for an empty list,
    // and `(:)` for an empty dict. `(params) => e` is a lambda.
    // GLR separates them on the next token.
    parenthesized_expression: $ => seq('(', $.expression, ')'),
    list_literal: $ => seq('(', $.expression, ',', optional(seq(commaSep1($.expression), optional(','))), ')'),
    dict_literal: $ => seq('(', commaSep1($.dict_entry), optional(','), ')'),
    dict_entry: $ => seq(
      field('key', choice($.identifier, $.string)),
      ':',
      field('value', $.expression),
    ),
    unit_literal: _ => seq('(', ')'),
    empty_list: _ => seq('(', ',', ')'),
    empty_dict: _ => seq('(', ':', ')'),

    qualified_name: $ => seq($.identifier, repeat(seq('::', $.identifier))),
    item_target: $ => seq(
      field('module', $.qualified_name),
      repeat1(seq('::', field('label', $._label_string))),
    ),

    _code_postfix_expression: $ =>
      choice($._primary_expression, $.call_expression, $.field_access),

    call_expression: $ => prec.left(1, seq(
      field('function', $._code_postfix_expression),
      choice(
        field('arguments', $.arguments),
        field('content', $.content_block),
      ),
    )),

    field_access: $ => prec.left(1, seq(
      field('object', $._code_postfix_expression),
      '.',
      field('field', alias(IMMEDIATE_IDENTIFIER, $.identifier)),
    )),

    arguments: $ => seq('(', optional($._argument_list), ')'),

    _argument_list: $ => seq(
      choice($.named_argument, $.expression),
      repeat(seq(',', choice($.named_argument, $.expression))),
      optional(','),
    ),

    named_argument: $ => seq(
      field('name', $.identifier),
      ':',
      field('value', $.expression),
    ),

    // ========================================================= markup

    _primary_expression: $ => choice(
      $.qualified_name,
      $.item_target,
      $.string,
      $.integer,
      'true',
      'false',
      $.content_block,
      $.if_expression,
      $.unary_expression,
      $.lambda,
      $.parenthesized_expression,
      $.list_literal,
      $.dict_literal,
      $.unit_literal,
      $.empty_list,
      $.empty_dict,
    ),

    // Markup item sets per context: titles stop at newlines, styled spans
    // exclude their own delimiter, section bodies exclude sections.
    _markup_item: $ => choice(
      $._line_break,
      $.text,
      $.escape,
      $.wikilink,
      $.strong,
      $.emphasis,
      $.bracketed_text,
      $.annotation,
      $.raw,
      $.math,
      $.autolink,
      $.list_marker,
      $.table,
      $.interpolation,
      $.declaration,
    ),

    _inline_item: $ => prec(1, choice(
      $.text,
      $.escape,
      $.wikilink,
      $.strong,
      $.emphasis,
      $.bracketed_text,
      $.annotation,
      $.raw,
      $.math,
      $.autolink,
      $.interpolation,
      $.declaration,
    )),

    _strong_item: $ => choice(
      $._line_break,
      $.text,
      $.escape,
      $.wikilink,
      $.emphasis,
      $.bracketed_text,
      $.annotation,
      $.raw,
      $.math,
      $.autolink,
      $.interpolation,
      $.declaration,
    ),

    _emphasis_item: $ => choice(
      $._line_break,
      $.text,
      $.escape,
      $.wikilink,
      $.strong,
      $.bracketed_text,
      $.annotation,
      $.raw,
      $.math,
      $.autolink,
      $.interpolation,
      $.declaration,
    ),

    // Interpolation admits a primary expression whose postfix operators
    // (`(...)`, `.name`, trailing `[...]`) must be adjacent, mirroring the
    // reference lexer; anything non-adjacent falls back to markup text.
    // The adjacency tokens are external: the scanner runs before the
    // internal lexer, so the longer text chunk cannot outbid them.
    interpolation: $ => seq('#', field('expression', $._interpolation_expression)),

    annotation: $ => seq('@', optional(token.immediate('!')), field('value', $._interpolation_expression)),

    _interpolation_expression: $ =>
      choice($._primary_expression, $.interpolation_call, $.interpolation_field, $.interpolation_content),

    interpolation_call: $ => prec.left(seq(
      field('function', $._interpolation_expression),
      $._immediate_call_open,
      optional($._argument_list),
      ')',
    )),

    interpolation_field: $ => prec.left(seq(
      field('object', $._interpolation_expression),
      $._immediate_field_dot,
      field('field', alias(IMMEDIATE_IDENTIFIER, $.identifier)),
    )),

    interpolation_content: $ => prec.left(seq(
      field('function', $._interpolation_expression),
      $._immediate_content_open,
      repeat(choice($._markup_item, $.section)),
      ']',
    )),

    declaration: $ => seq('#', choice(
      $.declaration_let,
      $.declaration_use,
      $.declaration_wasm,
    )),
    declaration_let: $ => seq('let', field('name', $.identifier), optional(seq(':', field('type', $.type))), '=', field('value', $.expression), ';'),
    declaration_use: $ => seq('use', $._use_tree, ';'),
    declaration_wasm: $ => seq('wasm', field('path', $.string), ';'),

    content_block: $ => seq('[', repeat(choice($._markup_item, $.section)), ']'),
    bracketed_text: $ => seq('[', repeat(choice($._markup_item, $.section)), ']'),

    section: $ => prec.right(1, seq(
      $.heading_marker,
      optional(field('title', $.title)),
      optional(field('body', $.section_body)),
    )),

    title: $ => prec.right(repeat1($._inline_item)),
    section_body: $ => prec.right(repeat1($._markup_item)),

    table: $ => seq(
      $.table_start,
      repeat1(seq(optional($.table_cell), $.pipe)),
      $._line_break,
      $.table_delimiter_row,
      repeat($.table_row),
    ),
    table_row: $ => prec.right(seq($.table_row_start, repeat1(seq(optional($.table_cell), $.pipe)))),
    table_cell: $ => repeat1($._inline_item),

    strong: $ => prec.right(1, seq('*', repeat($._strong_item), '*')),
    emphasis: $ => prec.right(1, seq($.emphasis_marker, repeat($._emphasis_item), $.emphasis_marker)),

    wikilink: $ => seq(
      '[[',
      field('module', $.wikilink_module),
      repeat(seq('::', field('label', $._label_string))),
      ']]',
    ),
    wikilink_module: $ => $.qualified_name,
    _label_string: $ => alias(token(prec(1, /"(\\.|[^"\\\n])+"/)), $.string),

    // Prose words have their own node, so keyword and number highlighting
    // cannot accidentally apply to code-shaped text in Markup.
    text: $ => choice(
      $._chunk,
      prec.right(1, repeat1($.text_word)),
    ),

    // An underscore can delimit emphasis at a word boundary. Keep interior
    // underscores in prose words, but leave leading/trailing ones to the
    // emphasis rule (matching the reference Markup parser).
    text_word: _ => /[\p{L}\p{N}]+(?:_[\p{L}\p{N}]+)*/,

    _chunk: _ => token(/[^\p{L}\p{N}_\s#@\[\]*\\`$]/),

    escape: _ => token(prec(1, /\\(?:u\{[0-9A-Fa-f]+\}|[\s\S]?)/)),

    // ======================================================= lexicals

    identifier: _ => IDENTIFIER,
    integer: _ => /[0-9]+/,
    string: _ => token(prec(1, /"(\\.|[^"\\\n])*"/)),

  },
});

function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}
