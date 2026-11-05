require('../common/markdown-options');
const markdown = require('../vendor/tree-sitter-markdown/tree-sitter-markdown-inline/grammar');
const shared = require('../common/notist');
// Each upstream inline context (including emphasis and link labels) accepts
// Notist constructs. Code spans and link destinations remain opaque.
const contexts = {};
for (const link of ['', '_no_link']) {
  for (const delimiter of ['', '_no_star', '_no_underscore', '_no_tilde']) {
    contexts[`_inline_element${delimiter}${link}`] = ($, previous) =>
      choice($.call, $.reference, $.import_declaration, $.annotation, $.math, previous);
  }
}
module.exports = grammar(markdown, {
  name: 'notist_markdown_inline',
  extras: ($, previous) => [...previous, $.comment],
  externals: ($, previous) => [...previous,
    $.identifier, $.string, $.boolean,
    $._call_start, $._body_open, $._body_close,
    $.comment, $._annotation_start, $._module_annotation_start,
    $.markdown_block_content, $.markdown_inline_content, $.math,
    $._import_start, $._path_dot, $._arguments_open, $._comptime_open,
    $._comptime_close, $._or_operator, $._and_operator, $._equality_operator,
    $._compare_operator, $._sum_operator, $._product_operator,
  ],
  conflicts: ($, previous) => [...previous, [$.dict], [$.array], [$.arguments], ...require('../common/code-conflicts')($)],
  rules: {
    ...Object.fromEntries(Object.entries(shared).filter(([name]) => name !== '_whitespace')),
    ...require('../common/code'),
    ...contexts,
    // Preserve upstream whitespace bookkeeping outside Notist literal contexts.
    _trivia: $ => choice(/[ \t]+/, $._newline_token),
    content_block: $ => seq(alias($._body_open, '['),
      optional(choice($.markdown_block_content, $.markdown_inline_content)),
      alias($._body_close, ']')),
  },
});
