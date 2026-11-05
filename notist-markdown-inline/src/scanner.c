#define tree_sitter_notist_external_scanner_create notist_md_inline_literal_create
#define tree_sitter_notist_external_scanner_destroy notist_md_inline_literal_destroy
#define tree_sitter_notist_external_scanner_serialize notist_md_inline_literal_serialize
#define tree_sitter_notist_external_scanner_deserialize notist_md_inline_literal_deserialize
#define tree_sitter_notist_external_scanner_scan notist_md_inline_literal_scan
#include "../../common/markdown-scanner.h"

#define MARKDOWN_INLINE_SCANNER_STORAGE static
#define tree_sitter_markdown_inline_external_scanner_create notist_md_inline_upstream_create
#define tree_sitter_markdown_inline_external_scanner_destroy notist_md_inline_upstream_destroy
#define tree_sitter_markdown_inline_external_scanner_serialize notist_md_inline_upstream_serialize
#define tree_sitter_markdown_inline_external_scanner_deserialize notist_md_inline_upstream_deserialize
#define tree_sitter_markdown_inline_external_scanner_scan notist_md_inline_upstream_scan
#include "../../vendor/tree-sitter-markdown/tree-sitter-markdown-inline/src/scanner.c"

// The first fifteen external symbols belong to the pinned upstream grammar.
enum MarkdownToken {
  MD_IDENTIFIER = 15, MD_STRING, MD_BOOLEAN, MD_CALL_START,
  MD_BODY_OPEN, MD_BODY_CLOSE, MD_COMMENT, MD_ANNOTATION_START,
  MD_MODULE_ANNOTATION_START, MD_BLOCK_CONTENT, MD_INLINE_CONTENT, MD_MATH, MD_IMPORT_START,
  MD_PATH_DOT, MD_ARGUMENTS_OPEN, MD_COMPTIME_OPEN,
  MD_COMPTIME_CLOSE, MD_OR_OPERATOR, MD_AND_OPERATOR, MD_EQUALITY_OPERATOR,
  MD_COMPARE_OPERATOR, MD_SUM_OPERATOR, MD_PRODUCT_OPERATOR,
};
typedef struct { void *markdown; void *notist; } CombinedScanner;
void *tree_sitter_notist_markdown_inline_external_scanner_create(void) {
  CombinedScanner *s = malloc(sizeof(*s));
  s->markdown = notist_md_inline_upstream_create();
  s->notist = notist_md_inline_literal_create();
  return s;
}
void tree_sitter_notist_markdown_inline_external_scanner_destroy(void *p) {
  CombinedScanner *s = p;
  notist_md_inline_upstream_destroy(s->markdown);
  notist_md_inline_literal_destroy(s->notist);
  free(s);
}
unsigned tree_sitter_notist_markdown_inline_external_scanner_serialize(void *p, char *buffer) {
  CombinedScanner *s = p;
  unsigned length = notist_md_inline_literal_serialize(s->notist, buffer + 1);
  buffer[0] = (char)length;
  return 1 + length + notist_md_inline_upstream_serialize(s->markdown, buffer + 1 + length);
}
void tree_sitter_notist_markdown_inline_external_scanner_deserialize(void *p, const char *buffer, unsigned length) {
  CombinedScanner *s = p;
  unsigned notist_length = length ? (uint8_t)buffer[0] : 0;
  if (length < notist_length + 1) {
    notist_md_inline_literal_deserialize(s->notist, NULL, 0);
    notist_md_inline_upstream_deserialize(s->markdown, NULL, 0);
    return;
  }
  notist_md_inline_literal_deserialize(s->notist, buffer + 1, notist_length);
  notist_md_inline_upstream_deserialize(s->markdown, buffer + 1 + notist_length, length - 1 - notist_length);
}
bool tree_sitter_notist_markdown_inline_external_scanner_scan(void *p, TSLexer *l, const bool *valid) {
  CombinedScanner *s = p;
  // Upstream's sentinel marks recovery. Let it close its own branches.
  if (valid[ERROR] || valid[TRIGGER_ERROR]) return notist_md_inline_upstream_scan(s->markdown, l, valid);
  if ((valid[MD_BLOCK_CONTENT] || valid[MD_INLINE_CONTENT]) && l->lookahead != ']' && !l->eof(l)) {
    bool block = false;
    notist_md_body(l, true, &block);
    l->result_symbol = block ? MD_BLOCK_CONTENT : MD_INLINE_CONTENT;
    return true;
  }
  bool symbols[TOKEN_COUNT] = {false};
  const enum Token original[] = {IDENTIFIER, STRING, BOOLEAN, CALL_START,
    BODY_OPEN, BODY_CLOSE, COMMENT, ANNOTATION_START, MODULE_ANNOTATION_START};
  for (unsigned i = 0; i < sizeof(original) / sizeof(original[0]); i++) symbols[original[i]] = valid[MD_IDENTIFIER + i];
  symbols[MATH] = valid[MD_MATH];
  symbols[IMPORT_START] = valid[MD_IMPORT_START];
  symbols[PATH_DOT] = valid[MD_PATH_DOT];
  symbols[ARGUMENTS_OPEN] = valid[MD_ARGUMENTS_OPEN];
  symbols[COMPTIME_OPEN] = valid[MD_COMPTIME_OPEN];
  for (unsigned i = 0; i < 7; i++) symbols[COMPTIME_CLOSE + i] = valid[MD_COMPTIME_CLOSE + i];
  bool literal = (valid[MD_IDENTIFIER] || valid[MD_STRING] || valid[MD_BOOLEAN]) && ident_start(l->lookahead);
   bool custom = literal || (valid[MD_STRING] && l->lookahead == '"') ||
     valid[MD_OR_OPERATOR] || valid[MD_AND_OPERATOR] || valid[MD_EQUALITY_OPERATOR] ||
     valid[MD_COMPARE_OPERATOR] || valid[MD_SUM_OPERATOR] || valid[MD_PRODUCT_OPERATOR] ||
     (valid[MD_COMPTIME_CLOSE] && l->lookahead == '>') ||
     (valid[MD_PATH_DOT] && l->lookahead == '.') ||
     (valid[MD_ARGUMENTS_OPEN] && l->lookahead == '(') ||
     (valid[MD_COMPTIME_OPEN] && l->lookahead == '<') ||
    (valid[MD_CALL_START] && l->lookahead == '#') ||
    ((valid[MD_ANNOTATION_START] || valid[MD_MODULE_ANNOTATION_START]) && l->lookahead == '@') ||
    (valid[MD_BODY_OPEN] && l->lookahead == '[') ||
    (valid[MD_BODY_CLOSE] && l->lookahead == ']') ||
    (valid[MD_COMMENT] && l->lookahead == '/') ||
    (valid[MD_MATH] && l->lookahead == '$');
  if (custom) {
    if (!notist_md_inline_literal_scan(s->notist, l, symbols)) return false;
    if (l->result_symbol == MATH) l->result_symbol = MD_MATH;
    else if (l->result_symbol == IMPORT_START) l->result_symbol = MD_IMPORT_START;
    else if (l->result_symbol == PATH_DOT) l->result_symbol = MD_PATH_DOT;
    else if (l->result_symbol == ARGUMENTS_OPEN) l->result_symbol = MD_ARGUMENTS_OPEN;
    else if (l->result_symbol == COMPTIME_OPEN) l->result_symbol = MD_COMPTIME_OPEN;
    else if (l->result_symbol >= COMPTIME_CLOSE && l->result_symbol <= PRODUCT_OPERATOR)
      l->result_symbol = MD_COMPTIME_CLOSE + l->result_symbol - COMPTIME_CLOSE;
    else for (unsigned i = 0; i < sizeof(original) / sizeof(original[0]); i++) {
      if (l->result_symbol == original[i]) { l->result_symbol = MD_IDENTIFIER + i; break; }
    }
    return true;
  }
  return notist_md_inline_upstream_scan(s->markdown, l, valid);
}
