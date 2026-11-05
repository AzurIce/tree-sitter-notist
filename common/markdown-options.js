// Fixed extensions matching the Notist Markdown frontend. Do not let process
// environment variables silently change the generated grammar.
const common = require('../vendor/tree-sitter-markdown/common/common');
for (const option of Object.keys(common).filter(key => key.startsWith('EXTENSION_'))) {
  common[option] = false;
}
common.EXTENSION_STRIKETHROUGH = true;
common.EXTENSION_PIPE_TABLE = true;
