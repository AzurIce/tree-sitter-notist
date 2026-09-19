#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// The external scanner covers everything the internal lexer cannot decide
// on its own, because tree-sitter resolves lexical competition by match
// length first: the markup text chunk would otherwise swallow every shorter
// token that shares a position with it.
//
//  - LINE_BREAK / HEADING_MARKER: markup is line-sensitive (titles end at
//    newlines; `= ` runs only open sections at column 0).
//  - IMMEDIATE_CALL_OPEN / IMMEDIATE_FIELD_DOT / IMMEDIATE_CONTENT_OPEN:
//    postfix operators of a `#` interpolation must be adjacent to the
//    expression; non-adjacent `(`/`.`/`[` belong to the following text.
//    The scanner is invoked before the internal lexer and before extras
//    are skipped, so "adjacent" is simply "the scanner sees the character".
//
// No state needs to survive between tokens.

enum TokenType {
  LINE_BREAK,
  HEADING_MARKER,
  IMMEDIATE_CALL_OPEN,
  IMMEDIATE_FIELD_DOT,
  IMMEDIATE_CONTENT_OPEN,
  RAW,
  MATH,
  COMMENT,
  LIST_MARKER,
  AUTOLINK,
};

void *tree_sitter_notist_external_scanner_create(void) { return NULL; }

void tree_sitter_notist_external_scanner_destroy(void *payload) {
  (void)payload;
}

void tree_sitter_notist_external_scanner_reset(void *payload) {
  (void)payload;
}

unsigned tree_sitter_notist_external_scanner_serialize(
    void *payload,
    char *buffer
) {
  (void)payload;
  (void)buffer;
  return 0;
}

void tree_sitter_notist_external_scanner_deserialize(
    void *payload,
    const char *buffer,
    unsigned length
) {
  (void)payload;
  (void)buffer;
  (void)length;
}

static bool is_word_start(int32_t c) {
  return c == '_' ||
         (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         (c >= 0x80); // treat any multibyte lead as a word character
}

static bool emit(TSLexer *lexer, enum TokenType symbol) {
  lexer->mark_end(lexer);
  lexer->result_symbol = symbol;
  return true;
}

static bool scan_markup(TSLexer *lexer, const bool *valid, bool line_start) {
  if (valid[AUTOLINK] && lexer->lookahead == 'h') {
    const char *prefix = "http";
    while (*prefix) {
      if (lexer->lookahead != *prefix++) return false;
      lexer->advance(lexer, false);
    }
    if (lexer->lookahead == 's') lexer->advance(lexer, false);
    prefix = "://";
    while (*prefix) {
      if (lexer->lookahead != *prefix++) return false;
      lexer->advance(lexer, false);
    }
    bool content = false;
    unsigned parens = 0, brackets = 0;
    while (!lexer->eof(lexer)) {
      int32_t c = lexer->lookahead;
      if (c <= ' ' || c == '<' || c == '>') break;
      if (c == '(') parens++;
      if (c == '[') brackets++;
      if (c == ')' && !parens) break;
      if (c == ']' && !brackets) break;
      if (c == ')') parens--;
      if (c == ']') brackets--;
      lexer->advance(lexer, false);
      if (c != '.' && c != ',' && c != ';' && c != '!' && c != '?') {
        lexer->mark_end(lexer);
        content = true;
      }
    }
    if (!content) return false;
    lexer->result_symbol = AUTOLINK;
    return true;
  }
  if (valid[RAW] && lexer->lookahead == '`') {
    unsigned fence = 0;
    while (lexer->lookahead == '`') { lexer->advance(lexer, false); fence++; }
    while (fence != 2 && !lexer->eof(lexer)) {
      unsigned ticks = 0;
      while (lexer->lookahead == '`') { lexer->advance(lexer, false); ticks++; }
      if (ticks == fence) break;
      if (!ticks) lexer->advance(lexer, false);
    }
    return emit(lexer, RAW);
  }
  if (valid[MATH] && lexer->lookahead == '$') {
    lexer->advance(lexer, false);
    bool quoted = false;
    while (!lexer->eof(lexer)) {
      int32_t c = lexer->lookahead; lexer->advance(lexer, false);
      if (c == '\\' && !lexer->eof(lexer)) lexer->advance(lexer, false);
      else if (c == '"') quoted = !quoted;
      else if (c == '$' && !quoted) break;
    }
    return emit(lexer, MATH);
  }
  if (lexer->lookahead == '/') {
    lexer->advance(lexer, false);
    if (valid[COMMENT] && lexer->lookahead == '/') {
      while (!lexer->eof(lexer) && lexer->lookahead != '\n') lexer->advance(lexer, false);
      return emit(lexer, COMMENT);
    }
    if (valid[COMMENT] && lexer->lookahead == '*') {
      lexer->advance(lexer, false);
      unsigned nesting = 1;
      while (nesting && !lexer->eof(lexer)) {
        int32_t c = lexer->lookahead; lexer->advance(lexer, false);
        if (c == '/' && lexer->lookahead == '*') { lexer->advance(lexer, false); nesting++; }
        else if (c == '*' && lexer->lookahead == '/') { lexer->advance(lexer, false); nesting--; }
      }
      return emit(lexer, COMMENT);
    }
    if (valid[LIST_MARKER] && line_start && (lexer->lookahead == ' ' || lexer->lookahead == '\t')) return emit(lexer, LIST_MARKER);
    return false;
  }
  if (valid[LIST_MARKER] && line_start) {
    int32_t c = lexer->lookahead;
    if (c == '-' || c == '+') lexer->advance(lexer, false);
    else if (c >= '0' && c <= '9') {
      do { lexer->advance(lexer, false); } while (lexer->lookahead >= '0' && lexer->lookahead <= '9');
      if (lexer->lookahead != '.') return false;
      lexer->advance(lexer, false);
    } else return false;
    if (lexer->lookahead == ' ' || lexer->lookahead == '\t') return emit(lexer, LIST_MARKER);
  }
  return false;
}

bool tree_sitter_notist_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
  (void)payload;

  if (valid_symbols[LINE_BREAK] && lexer->lookahead == '\n') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = LINE_BREAK;
    return true;
  }

  if (valid_symbols[HEADING_MARKER] && lexer->get_column(lexer) == 0 &&
      lexer->lookahead == '=') {
    lexer->advance(lexer, false);
    while (lexer->lookahead == '=') {
      lexer->advance(lexer, false);
    }
    // A heading needs one space after the `=` run; `=` without a following
    // space is plain text and is left to the internal lexer.
    if (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      lexer->result_symbol = HEADING_MARKER;
      return true;
    }
  }

  if (valid_symbols[IMMEDIATE_CALL_OPEN] && lexer->lookahead == '(') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = IMMEDIATE_CALL_OPEN;
    return true;
  }

  // A field access keeps `.` and the field name glued together; if the very
  // next character cannot start a name, the dot stays text.
  if (valid_symbols[IMMEDIATE_FIELD_DOT] && lexer->lookahead == '.') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '_' || lexer->lookahead >= 0x80 ||
        (lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
        (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z')) {
      lexer->mark_end(lexer);
      lexer->result_symbol = IMMEDIATE_FIELD_DOT;
      return true;
    }
  }

  if (valid_symbols[IMMEDIATE_CONTENT_OPEN] && lexer->lookahead == '[') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    lexer->result_symbol = IMMEDIATE_CONTENT_OPEN;
    return true;
  }

  bool line_start = lexer->get_column(lexer) == 0;
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\r') lexer->advance(lexer, true);
  return scan_markup(lexer, valid_symbols, line_start);
}
