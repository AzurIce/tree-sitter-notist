// Code expressions are also used in Markup/Markdown call headers.
const list = ($, item) => seq(item, repeat(seq(repeat($._trivia), ',', repeat($._trivia), item)), optional(seq(repeat($._trivia), ',')));
module.exports = {
  comptime_arguments: $ => seq(alias($._comptime_open, '<'), repeat($._trivia), list($, choice($.named_argument, $._expression)), repeat($._trivia), alias($._comptime_close, '>')),
  _expression: $ => choice($._or, $.if_expression, $.match_expression, $.comptime_expression),
  ...Object.fromEntries([
    ['or', 'and'], ['and', 'equality'], ['equality', 'compare'],
    ['compare', 'sum'], ['sum', 'product'], ['product', 'prefix'],
  ].flatMap(([name, next]) => [
    [`_${name}`, $ => choice($[`_${next}`], alias($[`_binary_${name}`], $.binary_expression))],
    [`_binary_${name}`, $ => seq(field('left', $[`_${name}`]),
      field('operator', alias($[`_${name}_operator`], $.operator)), repeat($._trivia), field('right', $[`_${next}`]))],
  ])),
  _prefix: $ => choice($._postfix, $.unary_expression),
  unary_expression: $ => seq(field('operator', choice('!', '-')), repeat($._trivia), field('operand', $._prefix)),
  _postfix: $ => choice($._value, $.content_block, $.block_expression, $.invoke_expression),
  invoke_expression: $ => seq(field('function', $._postfix), choice(
    seq(field('comptime', $.comptime_arguments), optional(seq(repeat($._trivia), field('arguments', $.arguments))),
      optional(seq(repeat($._trivia), field('body', $.content_block)))),
    seq(field('arguments', $.arguments), optional(seq(repeat($._trivia), field('body', $.content_block)))),
    field('body', $.content_block))),
  block_expression: $ => seq('{', repeat($._trivia), repeat(seq($.let_binding, repeat($._trivia))), optional(seq($._expression, repeat($._trivia))), '}'),
  let_binding: $ => seq('let', repeat($._trivia), field('name', $.identifier), repeat($._trivia), '=', repeat($._trivia), field('value', $._expression), repeat($._trivia), ';'),
  if_expression: $ => seq('if', repeat($._trivia), field('condition', $._expression), repeat($._trivia),
    field('consequence', $.block_expression), repeat($._trivia), 'else', repeat($._trivia), field('alternative', choice($.block_expression, $.if_expression))),
  match_expression: $ => seq('match', repeat($._trivia), field('subject', $._expression), repeat($._trivia), '{', repeat($._trivia),
    optional(list($, $.match_arm)), repeat($._trivia), '}'),
  match_arm: $ => seq(field('pattern', $._expression), repeat($._trivia), '=>', repeat($._trivia), field('value', $._expression)),
  comptime_expression: $ => prec.right(seq('comptime', repeat($._trivia), $._expression)),
  // Collection elements in expression positions may themselves be expressions.
  group: $ => seq('(', repeat($._trivia), $._expression, repeat($._trivia), ')'),
  array: $ => seq('(', repeat($._trivia), choice(',', seq($._expression, repeat($._trivia), ',', repeat($._trivia),
    optional(list($, $._expression)))), repeat($._trivia), ')'),
  dict_entry: $ => seq(field('key', choice($.identifier, $.string)), ':', repeat($._trivia), field('value', $._expression)),
};
