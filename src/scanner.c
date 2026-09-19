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
    if (lexer->lookahead == ' ') {
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

  return false;
}
