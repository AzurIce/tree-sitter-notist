#ifndef NOTIST_SCANNER_STORAGE
#define NOTIST_SCANNER_STORAGE
#endif
#include "tree_sitter/parser.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "unicode.h"

// This order is part of the grammar/scanner interface.
enum Token {
  NEWLINE, IDENTIFIER, FN_KEYWORD, STRING, BOOLEAN, TEXT,
  HEADING_MARKER, LIST_MARKER, DIVIDER,
  STRONG_OPEN, STRONG_CLOSE, EMPHASIS_OPEN, EMPHASIS_CLOSE,
  STRIKE_OPEN, STRIKE_CLOSE, CALL_START, BODY_OPEN, BODY_CLOSE,
  WIKI_OPEN, LINK_TARGET, WIKI_PIPE, WIKI_CLOSE,
  LINK_OPEN, EMBED_START, LINK_LABEL_CLOSE, LINK_DESTINATION,
  RAW_INLINE, RAW_FENCE_START, RAW_CONTENT, RAW_FENCE_END, MATH,
  COMMENT, TABLE_START, TABLE_ROW, TABLE_DELIMITER,
  ANNOTATION_START, MODULE_ANNOTATION_START, IMPORT_START, PATH_DOT, ARGUMENTS_OPEN, COMPTIME_OPEN,
  COMPTIME_CLOSE, OR_OPERATOR, AND_OPERATOR, EQUALITY_OPERATOR, COMPARE_OPERATOR, SUM_OPERATOR, PRODUCT_OPERATOR,
  TOKEN_COUNT,
};

typedef struct {
  uint32_t fence;
  uint32_t fence_indent;
  bool previous_space;
  bool line_prefix;
  bool previous_colon;
} Scanner;

NOTIST_SCANNER_STORAGE void *tree_sitter_notist_external_scanner_create(void) { Scanner *s = calloc(1, sizeof(Scanner)); s->line_prefix = true; return s; }
NOTIST_SCANNER_STORAGE void tree_sitter_notist_external_scanner_destroy(void *p) { free(p); }
NOTIST_SCANNER_STORAGE unsigned tree_sitter_notist_external_scanner_serialize(void *p, char *buffer) {
  Scanner *s = p;
  // Explicit byte encoding avoids platform padding and uninitialized bytes.
  for (unsigned i = 0; i < 4; i++) {
    buffer[i] = (char)(s->fence >> (8 * i));
    buffer[4 + i] = (char)(s->fence_indent >> (8 * i));
  }
  buffer[8] = s->previous_space;
  buffer[9] = s->line_prefix;
  buffer[10] = s->previous_colon;
  return 11;
}
NOTIST_SCANNER_STORAGE void tree_sitter_notist_external_scanner_deserialize(void *p, const char *buffer, unsigned length) {
  Scanner *s = p;
  memset(s, 0, sizeof(*s));
  s->line_prefix = true;
  if (length != 11) return;
  for (unsigned i = 0; i < 4; i++) {
    s->fence |= (uint32_t)(uint8_t)buffer[i] << (8 * i);
    s->fence_indent |= (uint32_t)(uint8_t)buffer[4 + i] << (8 * i);
  }
  s->previous_space = buffer[8] != 0;
  s->line_prefix = buffer[9] != 0;
  s->previous_colon = buffer[10] != 0;
}

static void advance_notist(TSLexer *l) { l->advance(l, false); }
static bool newline(int32_t c) { return c == '\r' || c == '\n'; }
static bool space(int32_t c) { return c == ' ' || c == '\t' || newline(c); }
static bool ident_start(int32_t c) { return c == '_' || IN_RANGES(c, alphabetic_ranges); }
static bool ident_continue(int32_t c) { return ident_start(c) || c == '-' || IN_RANGES(c, number_ranges); }
static void eat_newline(TSLexer *l) {
  int32_t c = l->lookahead; advance_notist(l);
  if (c == '\r' && l->lookahead == '\n') advance_notist(l);
}
static bool emit(Scanner *s, TSLexer *l, enum Token token, bool last_space) {
  s->previous_space = last_space;
  if (token != TEXT) s->previous_colon = false;
  if (token == NEWLINE) s->line_prefix = true;
  else if (token == TEXT) s->line_prefix &= last_space;
  else s->line_prefix = false;
  l->result_symbol = token;
  return true;
}
static bool finish(Scanner *s, TSLexer *l, enum Token token, bool last_space) {
  l->mark_end(l);
  return emit(s, l, token, last_space);
}

// Strings are shared by call literals and module defaults. A failed inline
// string ends at its physical line, so it cannot swallow the next declaration.
static bool string_tail(TSLexer *l, unsigned hashes) {
  if (l->lookahead != '"') return false;
  advance_notist(l);
  unsigned quotes = 1;
  if (l->lookahead == '"') {
    advance_notist(l);
    if (l->lookahead != '"') {
      if (!hashes) return true; // empty ordinary string
      for (unsigned i = 0; i < hashes; i++) {
        if (l->lookahead != '#') return false;
        advance_notist(l);
      }
      return true;
    }
    advance_notist(l); quotes = 3;
  }
  while (!l->eof(l) && (quotes == 3 || !newline(l->lookahead))) {
    if (!hashes && l->lookahead == '\\') {
      advance_notist(l);
      if (!l->eof(l) && (quotes == 3 || !newline(l->lookahead))) advance_notist(l);
      continue;
    }
    if (l->lookahead == '"') {
      unsigned count = 0;
      while (count < quotes && l->lookahead == '"') { advance_notist(l); count++; }
      if (count != quotes) continue;
      unsigned matched = 0;
      while (matched < hashes && l->lookahead == '#') { advance_notist(l); matched++; }
      if (matched == hashes) return true;
      continue;
    }
    advance_notist(l);
  }
  // Editors keep the unterminated region opaque; semantic diagnostics belong
  // to Notist's reference parser, not the highlighting grammar.
  return true;
}

static bool scan_comment(TSLexer *l) {
  // Caller has consumed the first slash.
  if (l->lookahead == '/') {
    while (!l->eof(l) && !newline(l->lookahead)) advance_notist(l);
    return true;
  }
  if (l->lookahead != '*') return false;
  advance_notist(l);
  unsigned depth = 1;
  while (depth && !l->eof(l)) {
    int32_t c = l->lookahead; advance_notist(l);
    if (c == '/' && l->lookahead == '*') { advance_notist(l); depth++; }
    else if (c == '*' && l->lookahead == '/') { advance_notist(l); depth--; }
  }
  return true;
}

// Probe a balanced call slot. Strings, escapes and nested comments shield
// punctuation. Argument probes stop at blank lines; content may span blocks.
static bool balanced(TSLexer *l, int32_t open, int32_t close, bool blocks) {
  unsigned depth = 0;
  while (!l->eof(l)) {
    int32_t c = l->lookahead;
    if (open == '<' && c == '(') {
      if (!balanced(l, '(', ')', blocks)) return false;
      continue;
    }
    if (c == '"') { string_tail(l, 0); continue; }
    if (c == 'r') {
      advance_notist(l);
      unsigned hashes = 0;
      while (l->lookahead == '#') { advance_notist(l); hashes++; }
      if (hashes && l->lookahead == '"') string_tail(l, hashes);
      continue;
    }
    if (c == '/') {
      advance_notist(l);
      scan_comment(l);
      continue;
    }
    if (c == '\\') {
      advance_notist(l);
      if (!l->eof(l)) advance_notist(l);
      continue;
    }
    if (newline(c)) {
      eat_newline(l);
      if (!blocks) {
        while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
        if (newline(l->lookahead) || l->lookahead == '=' || l->lookahead == '`') return false;
      }
      continue;
    }
    advance_notist(l);
    if (c == open) depth++;
    if (c == close && --depth == 0) return true;
  }
  return false;
}

static bool call_path_info(TSLexer *l, bool *is_import) {
  if (!ident_start(l->lookahead)) return false;
  char name[8] = {0}; unsigned count = 0;
  bool first = true;
  for (;;) {
    do {
      if (first && count < 7) name[count++] = (char)(l->lookahead < 128 ? l->lookahead : '?');
      advance_notist(l);
    } while (ident_continue(l->lookahead));
    if (l->lookahead != '.') {
      if (is_import) *is_import = first && count == 6 && !strcmp(name, "import");
      return true;
    }
    first = false;
    advance_notist(l);
    if (!ident_start(l->lookahead)) return false;
  }
}
static bool call_path(TSLexer *l) { return call_path_info(l, NULL); }

static bool style_mate(TSLexer *l, int32_t marker) {
  if (space(l->lookahead) || l->eof(l) || l->lookahead == marker) return false;
  bool previous_space = false;
  unsigned nesting = 0;
  while (!l->eof(l)) {
    int32_t c = l->lookahead;
    if (c == '\\') {
      advance_notist(l);
      if (newline(l->lookahead)) return false;
      if (!l->eof(l)) advance_notist(l);
      previous_space = false;
      continue;
    }
    if (c == '`') {
      unsigned ticks = 0;
      while (l->lookahead == '`') { advance_notist(l); ticks++; }
      bool closed = false;
      while (!l->eof(l) && !newline(l->lookahead)) {
        unsigned run = 0;
        while (l->lookahead == '`') { advance_notist(l); run++; }
        if (run == ticks) { closed = true; break; }
        if (!run) advance_notist(l);
      }
      if (!closed) return false;
      previous_space = false;
      continue;
    }
    if (newline(c)) {
      eat_newline(l);
      while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
      if (newline(l->lookahead) || l->lookahead == '=' || l->lookahead == '`' ||
          l->lookahead == '@' || l->lookahead == '-' || l->lookahead == '+') return false;
      previous_space = true;
      continue;
    }
    advance_notist(l);
    if (c == marker) {
      if (!previous_space) {
        if (!nesting) return true;
        nesting--;
      } else if (!space(l->lookahead)) nesting++;
    }
    previous_space = space(c);
  }
  return false;
}

// The opaque link destination includes its parentheses and preserves quoted
// titles, escaped punctuation, and balanced nested parentheses.
static bool destination(TSLexer *l) {
  if (l->lookahead != '(') return false;
  advance_notist(l);
  unsigned depth = 1;
  int32_t quote = 0;
  bool previous_space = false;
  while (!l->eof(l) && !newline(l->lookahead)) {
    int32_t c = l->lookahead; advance_notist(l);
    if (c == '\\') {
      if (l->eof(l) || newline(l->lookahead)) return false;
      advance_notist(l); previous_space = false; continue;
    }
    if (quote) { if (c == quote) quote = 0; }
    else if (previous_space && (c == '"' || c == '\'')) quote = c;
    else if (c == '(') depth++;
    else if (c == ')' && --depth == 0) return true;
    previous_space = space(c);
  }
  return false;
}
static bool link_frame(TSLexer *l) {
  // First '[' was already consumed.
  unsigned depth = 1;
  while (!l->eof(l)) {
    int32_t c = l->lookahead;
    if (newline(c)) {
      eat_newline(l);
      while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
      if (newline(l->lookahead) || l->lookahead == '=') return false;
      continue;
    }
    advance_notist(l);
    if (c == '\\') {
      if (newline(l->lookahead)) return false;
      if (!l->eof(l)) advance_notist(l);
    } else if (c == '[') depth++;
    else if (c == ']' && --depth == 0) return destination(l);
  }
  return false;
}

// Scan a logical table line, shielding strings/comments/call slots. Rows are
// intentionally opaque editor nodes; Notist owns cell splitting and lowering.
static bool table_line(TSLexer *l, bool alignment, unsigned *cells, bool *pipe, int32_t first) {
  unsigned separators = 0, nonempty = first && !space(first) && first != '|';
  bool leading = !first || space(first), trailing_pipe = first == '|';
  *pipe |= first == '|';
  bool dash = false, colon_after = false, colon_before = false;
  while (!l->eof(l) && !newline(l->lookahead)) {
    int32_t c = l->lookahead;
    if (c == '|') {
      if (alignment && !leading && !dash) return false;
      if (leading) leading = false;
      else separators++;
      advance_notist(l); trailing_pipe = true; *pipe = true;
      dash = colon_after = colon_before = false;
      continue;
    }
    if (c == ' ' || c == '\t') { advance_notist(l); continue; }
    trailing_pipe = false;
    leading = false; nonempty++;
    if (alignment) {
      if (c == '-') { if (colon_after) return false; dash = true; }
      else if (c == ':') {
        if (dash) { if (colon_after) return false; colon_after = true; }
        else { if (colon_before) return false; colon_before = true; }
      } else return false;
      advance_notist(l);
      continue;
    }
    if (c == '\\') { advance_notist(l); if (!l->eof(l) && !newline(l->lookahead)) advance_notist(l); }
    else if (c == '/') { advance_notist(l); scan_comment(l); }
    else if (c == '#') {
      advance_notist(l);
      if (call_path(l)) {
        if (l->lookahead == '<') balanced(l, '<', '>', false);
        if (l->lookahead == '(') balanced(l, '(', ')', false);
        if (l->lookahead == '[') balanced(l, '[', ']', true);
      }
    } else advance_notist(l);
  }
  if (!nonempty || (alignment && !trailing_pipe && !dash)) return false;
  *cells = separators + 1 - (trailing_pipe ? 1 : 0);
  return *cells > 0;
}

static bool scan_raw_region(Scanner *s, TSLexer *l, const bool *valid) {
  // Scan ahead while keeping the token end immediately before a closing line.
  bool content = false;
  for (;;) {
    l->mark_end(l);
    unsigned indent = 0;
    while (l->lookahead == ' ' || l->lookahead == '\t') {
      indent = l->lookahead == '\t' ? (indent / 4 + 1) * 4 : indent + 1;
      advance_notist(l);
    }
    unsigned ticks = 0;
    while (l->lookahead == '`') { advance_notist(l); ticks++; }
    if (indent == s->fence_indent && ticks >= s->fence) {
      if (content && valid[RAW_CONTENT]) return emit(s, l, RAW_CONTENT, true);
      if (!content && valid[RAW_FENCE_END]) {
        while (!l->eof(l) && !newline(l->lookahead)) advance_notist(l);
        s->fence = 0;
        return finish(s, l, RAW_FENCE_END, false);
      }
      return false;
    }
    while (!l->eof(l) && !newline(l->lookahead)) advance_notist(l);
    if (l->eof(l)) {
      if (!valid[RAW_CONTENT]) return false;
      return finish(s, l, RAW_CONTENT, false);
    }
    eat_newline(l); content = true;
  }
}

NOTIST_SCANNER_STORAGE bool tree_sitter_notist_external_scanner_scan(void *payload, TSLexer *l, const bool *valid) {
  Scanner *s = payload;
  // All symbols are valid during error recovery. Do not emit a zero-width
  // table start or let an old fence state consume a repaired declaration.
  bool recovery[TOKEN_COUNT] = {false};
  if (valid[FN_KEYWORD] && valid[TEXT]) {
#ifdef NOTIST_CODE
    recovery[NEWLINE] = recovery[IDENTIFIER] = recovery[FN_KEYWORD] = true;
    recovery[STRING] = recovery[BOOLEAN] = recovery[COMMENT] = true;
    valid = recovery;
#else
    (void)recovery;
    return false;
#endif
  }
  if (s->fence && (valid[RAW_CONTENT] || valid[RAW_FENCE_END])) return scan_raw_region(s, l, valid);
  if (l->eof(l)) return false;
  int32_t c = l->lookahead;
  bool was_colon = s->previous_colon;
  unsigned column = l->get_column(l);
  // Probe the gap together with an infix operator/call suffix. Keeping it in
  // this token avoids a GLR branch per precedence level on every space.
  bool gap = false;
  bool expression_tail = valid[OR_OPERATOR] || valid[AND_OPERATOR] || valid[EQUALITY_OPERATOR] ||
    valid[COMPARE_OPERATOR] || valid[SUM_OPERATOR] || valid[PRODUCT_OPERATOR];
  if (expression_tail) {
    while (space(l->lookahead)) { l->advance(l, true); gap = true; }
    c = l->lookahead;
    if (c == '/' && valid[COMMENT]) {
      advance_notist(l); l->mark_end(l);
      if (scan_comment(l)) return finish(s, l, COMMENT, false);
      if (valid[PRODUCT_OPERATOR]) return emit(s, l, PRODUCT_OPERATOR, false);
      return false;
    }
    if (valid[COMPTIME_CLOSE] && c == '>') { advance_notist(l); return finish(s, l, COMPTIME_CLOSE, false); }
    enum Token op = TOKEN_COUNT;
    if (c == '+' || c == '-') op = SUM_OPERATOR;
    else if (c == '*' || c == '%') op = PRODUCT_OPERATOR;
    else if (c == '<' || c == '>') op = COMPARE_OPERATOR;
    else if (c == '=' || c == '!') op = EQUALITY_OPERATOR;
    else if (c == '|') op = OR_OPERATOR;
    else if (c == '&') op = AND_OPERATOR;
    if (op != TOKEN_COUNT && valid[op]) {
      advance_notist(l);
      if (c == '<' && !gap && valid[COMPTIME_OPEN] && l->lookahead != '=') return finish(s, l, COMPTIME_OPEN, false);
      if (op == EQUALITY_OPERATOR) { if (l->lookahead != '=') return false; advance_notist(l); }
      else if (op == OR_OPERATOR || op == AND_OPERATOR) { if (l->lookahead != c) return false; advance_notist(l); }
      else if (op == COMPARE_OPERATOR && l->lookahead == '=') advance_notist(l);
      return finish(s, l, op, false);
    }
    if (gap && !((valid[ARGUMENTS_OPEN] && c == '(') || (valid[BODY_OPEN] && c == '['))) return false;
  }
  if (valid[NEWLINE] && newline(c)) { eat_newline(l); return finish(s, l, NEWLINE, true); }
  if (valid[PATH_DOT] && c == '.') { advance_notist(l); return finish(s, l, PATH_DOT, false); }
  if (valid[ARGUMENTS_OPEN] && c == '(') { advance_notist(l); return finish(s, l, ARGUMENTS_OPEN, false); }
  if (valid[COMPTIME_OPEN] && c == '<') { advance_notist(l); return finish(s, l, COMPTIME_OPEN, false); }
  if (valid[COMPTIME_CLOSE] && c == '>') { advance_notist(l); return finish(s, l, COMPTIME_CLOSE, false); }
  if (valid[LINK_TARGET]) {
    bool any = false;
    while (!l->eof(l) && !newline(l->lookahead) && l->lookahead != ']' && l->lookahead != '|') {
      advance_notist(l); any = true;
    }
    if (any) return finish(s, l, LINK_TARGET, false);
  }
  if (valid[LINK_DESTINATION] && c == '(' && destination(l)) return finish(s, l, LINK_DESTINATION, false);
  if (valid[TABLE_DELIMITER]) {
    unsigned cells = 0; bool pipe = false;
    if (table_line(l, true, &cells, &pipe, 0)) return finish(s, l, TABLE_DELIMITER, false);
    return false;
  }
  if (valid[TABLE_ROW] && !newline(c) && c != ']' && c != '=' && c != '`' && c != '@') {
    advance_notist(l); l->mark_end(l);
    unsigned cells = 0; bool pipe = false;
    if (table_line(l, false, &cells, &pipe, c) && pipe) return finish(s, l, TABLE_ROW, false);
    return valid[TEXT] && emit(s, l, TEXT, space(c));
  }
  if ((valid[IDENTIFIER] || valid[BOOLEAN] || valid[STRING]) && ident_start(c)) {
    char name[16] = {0}; unsigned count = 0;
    do {
      c = l->lookahead;
      // Raw strings take priority over the name `r`.
      if (count == 1 && name[0] == 'r' && c == '#' && valid[STRING]) break;
      advance_notist(l);
      if (count < 15 && c < 128) name[count] = (char)c;
      count++;
#ifdef NOTIST_CODE
      if (l->lookahead == '-') {
        l->mark_end(l); advance_notist(l);
        if (l->lookahead == '>') {
          if (count == 2 && strcmp(name, "fn") == 0)
            return valid[FN_KEYWORD] && emit(s, l, FN_KEYWORD, false);
          return valid[IDENTIFIER] && emit(s, l, IDENTIFIER, false);
        }
        if (count < 15) name[count] = '-';
        count++;
      }
#endif
    } while (ident_continue(l->lookahead));
    if (!valid[TEXT] && count < 15 &&
        (!strcmp(name, "import") || !strcmp(name, "as") || !strcmp(name, "let") ||
         !strcmp(name, "if") || !strcmp(name, "else") || !strcmp(name, "match") || !strcmp(name, "comptime"))) return false;
    if (count == 1 && name[0] == 'r' && l->lookahead == '#' && valid[STRING]) {
      unsigned hashes = 0;
      while (l->lookahead == '#') { advance_notist(l); hashes++; }
      if (string_tail(l, hashes)) return finish(s, l, STRING, false);
      return false;
    }
#ifdef NOTIST_CODE
    if (count == 2 && strcmp(name, "fn") == 0) {
      return valid[FN_KEYWORD] && finish(s, l, FN_KEYWORD, false);
    }
#endif
    if (valid[BOOLEAN] && !(valid[IDENTIFIER] && l->lookahead == ':') && ((count == 4 && strcmp(name, "true") == 0) ||
                          (count == 5 && strcmp(name, "false") == 0))) return finish(s, l, BOOLEAN, false);
    return valid[IDENTIFIER] && finish(s, l, IDENTIFIER, false);
  }
  if (valid[FN_KEYWORD] && c == 'f') {
    advance_notist(l);
    if (l->lookahead != 'n') return false;
    advance_notist(l);
    if (ident_continue(l->lookahead)) return false;
    return finish(s, l, FN_KEYWORD, false);
  }
  if (valid[STRING] && c == '"' && string_tail(l, 0)) return finish(s, l, STRING, false);

  s->previous_colon = c == ':';
  // Every speculative markup probe retains a short fallback token end.
  if (valid[TEXT]) { advance_notist(l); l->mark_end(l); }
  else if (c == '#' || c == '@' || c == '[' || c == ']' || c == '*' || c == '_' ||
           c == '~' || c == '`' || c == '$' || c == '/' || c == '=' || c == '-' || c == '+' || c == '!' || c == '|') {
    advance_notist(l); l->mark_end(l);
  } else return false;

  if (c == '\\' && (newline(l->lookahead) ||
      (l->lookahead < 128 && strchr("=*_~\\[]()|#$@!+`-", l->lookahead)))) return false;
  if (valid[TABLE_START] && column == 0 &&
      !(c < 128 && strchr("#@[]*_~`$/=+-!\\", c)) && !space(c)) {
    unsigned header_cells = 0, delimiter_cells = 0;
    bool header_pipe = false, delimiter_pipe = false;
    if (table_line(l, false, &header_cells, &header_pipe, c) && newline(l->lookahead)) {
      eat_newline(l);
      if (table_line(l, true, &delimiter_cells, &delimiter_pipe, 0) &&
          header_cells == delimiter_cells && (header_pipe || delimiter_pipe))
        return emit(s, l, TABLE_START, false);
    }
    return valid[TEXT] && emit(s, l, TEXT, false);
  }
#ifdef NOTIST_CODE
  was_colon = false;
#endif
  if (c == '/' && !was_colon && valid[COMMENT] && scan_comment(l)) return finish(s, l, COMMENT, false);
  if (c == '[' && valid[BODY_OPEN]) return emit(s, l, BODY_OPEN, false);
  if (c == ']' && valid[WIKI_CLOSE] && l->lookahead == ']') { advance_notist(l); return finish(s, l, WIKI_CLOSE, false); }
  if (c == ']' && valid[BODY_CLOSE]) return emit(s, l, BODY_CLOSE, false);
  if (c == ']' && valid[LINK_LABEL_CLOSE]) return emit(s, l, LINK_LABEL_CLOSE, false);
  if (c == '|' && valid[WIKI_PIPE]) return emit(s, l, WIKI_PIPE, false);
  if (c == '#') {
    bool is_import = false;
    if ((valid[CALL_START] || valid[IMPORT_START]) && call_path_info(l, &is_import)) {
      if (valid[IMPORT_START] && column == 0 && is_import && space(l->lookahead)) return finish(s, l, IMPORT_START, false);
      if (l->lookahead == '(' && !balanced(l, '(', ')', false)) return valid[TEXT] && emit(s, l, TEXT, false);
      return valid[CALL_START] && emit(s, l, CALL_START, false);
    }
  }
  if (c == '@') {
    if (valid[ANNOTATION_START] && l->lookahead == '(') return emit(s, l, ANNOTATION_START, false);
    if (valid[MODULE_ANNOTATION_START] && l->lookahead == '!') {
      advance_notist(l); l->mark_end(l);
      if (l->lookahead == '(') return emit(s, l, MODULE_ANNOTATION_START, false);
    }
  }
  if (c == '=' && valid[HEADING_MARKER] && column == 0) {
    while (l->lookahead == '=') advance_notist(l);
    if (l->lookahead == ' ' || l->lookahead == '\t') {
      while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
      return finish(s, l, HEADING_MARKER, true);
    }
  }
  if ((c == '-' || c == '+') && valid[LIST_MARKER] && (column == 0 || s->line_prefix)) {
    if (l->lookahead == ' ' || l->lookahead == '\t') return emit(s, l, LIST_MARKER, false);
    if (c == '-' && column == 0 && valid[DIVIDER] && l->lookahead == '-') {
      advance_notist(l);
      if (l->lookahead == '-') {
        advance_notist(l); l->mark_end(l);
        while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
        if (l->eof(l) || newline(l->lookahead)) return emit(s, l, DIVIDER, false);
      }
    }
  }
  if (c == '*' || c == '_' || c == '~') {
    enum Token open = c == '*' ? STRONG_OPEN : c == '_' ? EMPHASIS_OPEN : STRIKE_OPEN;
    enum Token close = c == '*' ? STRONG_CLOSE : c == '_' ? EMPHASIS_CLOSE : STRIKE_CLOSE;
    if (valid[close] && !s->previous_space) return emit(s, l, close, false);
    if (valid[open] && style_mate(l, c)) return emit(s, l, open, false);
  }
  if (c == '[') {
    if (valid[WIKI_OPEN] && l->lookahead == '[') {
      advance_notist(l); l->mark_end(l);
      while (!l->eof(l) && !newline(l->lookahead)) {
        int32_t ch = l->lookahead; advance_notist(l);
        if (ch == ']' && l->lookahead == ']') return emit(s, l, WIKI_OPEN, false);
      }
    } else if (valid[LINK_OPEN] && link_frame(l)) return emit(s, l, LINK_OPEN, false);
  }
  if (c == '!' && valid[EMBED_START] && l->lookahead == '[') {
    advance_notist(l);
    if (link_frame(l)) return emit(s, l, EMBED_START, false);
  }
  if (c == '`' && (valid[RAW_INLINE] || valid[RAW_FENCE_START])) {
    unsigned ticks = 1;
    while (l->lookahead == '`') { advance_notist(l); ticks++; }
    l->mark_end(l);
    if (ticks >= 3 && valid[RAW_FENCE_START] && column == 0) {
      while (!l->eof(l) && !newline(l->lookahead)) advance_notist(l);
      if (!l->eof(l)) eat_newline(l);
      s->fence = ticks; s->fence_indent = column;
      return finish(s, l, RAW_FENCE_START, true);
    }
    while (!l->eof(l) && !newline(l->lookahead)) {
      unsigned run = 0;
      while (l->lookahead == '`') { advance_notist(l); run++; }
      if (run == ticks && valid[RAW_INLINE]) return finish(s, l, RAW_INLINE, false);
      if (!run) advance_notist(l);
    }
  }
  if (c == '$' && valid[MATH]) {
    bool block = space(l->lookahead), previous_space = block, nonempty = false;
    while (!l->eof(l)) {
      int32_t ch = l->lookahead;
      if (ch == '$' && previous_space == block) {
        if (!nonempty) break;
        advance_notist(l); return finish(s, l, MATH, false);
      }
      if (newline(ch)) {
        eat_newline(l);
        while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
        if (newline(l->lookahead) || l->lookahead == '=' || l->lookahead == '`' || l->lookahead == '@') break;
        previous_space = true; continue;
      }
      advance_notist(l);
      if (ch == '\\' && !l->eof(l)) { advance_notist(l); previous_space = false; nonempty = true; continue; }
      previous_space = space(ch); nonempty |= !previous_space;
    }
  }
  // Ordinary prose is never lexed as code. Emit runs up to the next marker;
  // punctuation that failed a probe remains a short literal text token.
  if (valid[TEXT]) {
    if (!(c < 128 && strchr("#@[]*_~`$/=+-!|\\", c)) && !newline(c)) {
      bool last_space = space(c);
      s->line_prefix &= last_space;
      while (!l->eof(l) && !newline(l->lookahead) &&
             !(l->lookahead < 128 && strchr("#@[]*_~`$/=+-!|\\", l->lookahead))) {
        last_space = space(l->lookahead); s->line_prefix &= last_space; s->previous_colon = l->lookahead == ':'; advance_notist(l); l->mark_end(l);
      }
      return emit(s, l, TEXT, last_space);
    }
    return emit(s, l, TEXT, space(c));
  }
  return false;
}
