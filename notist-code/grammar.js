const markup = require('../grammar');

module.exports = grammar(markup, {
  name: 'notist_code',
  rules: {
    source_file: $ => optional($.code_file),
  },
});
