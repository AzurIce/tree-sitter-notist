const shared = require('../grammar');
module.exports = grammar(shared, {
  name: 'notist_code',
  rules: { source_file: $ => optional($.module) },
});
