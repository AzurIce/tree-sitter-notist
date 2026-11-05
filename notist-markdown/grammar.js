require('../common/markdown-options');
const markdown = require('../vendor/tree-sitter-markdown/tree-sitter-markdown/grammar');
// Upstream ATX headings require a physical newline; document endings and
// injected call bodies may legitimately end immediately after their title.
const headings = {};
for (let level = 1; level <= 6; level++) {
  headings[`_atx_heading${level}`] = $ => prec(1, seq(
    $[`atx_h${level}_marker`], optional($._atx_heading_content),
    choice($._newline, $._eof)));
}
module.exports = grammar(markdown, {
  name: 'notist_markdown',
  externals: ($, previous) => [...previous, $.notist_block],
  rules: {
    ...headings,
    _block_not_section: ($, previous) => choice(previous,
      seq($.notist_block, choice($._newline, $._eof))),
  },
});
