// Include the shared Notist literal scanner under the names selected by the
// owning Markdown scanner. Its private state/helpers do not collide upstream.
#define NOTIST_SCANNER_STORAGE static inline
#define Scanner NotistScanner
#include "../src/scanner.c"
#undef Scanner
#undef NOTIST_SCANNER_STORAGE
#undef tree_sitter_notist_external_scanner_create
#undef tree_sitter_notist_external_scanner_destroy
#undef tree_sitter_notist_external_scanner_serialize
#undef tree_sitter_notist_external_scanner_deserialize
#undef tree_sitter_notist_external_scanner_scan

// Match Rust char::is_whitespace for Markdown content flank classification.
static bool notist_md_space(int32_t c) {
  return (c >= 0x09 && c <= 0x0d) || c == 0x20 || c == 0x85 ||
    c == 0xa0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200a) ||
    c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}

// After the opening bracket, consume a Markdown body to its outer closing
// bracket. Code spans, fenced code and math shield brackets, as in notist-md.
static bool notist_md_body(TSLexer *l, bool mark_payload, bool *block) {
  unsigned depth = 0;
  bool first_space = notist_md_space(l->lookahead), previous_space = first_space;
  bool line_prefix = l->get_column(l) == 0;
  while (!l->eof(l)) {
    int32_t c = l->lookahead;
    if (c == ']' && !depth) {
      if (mark_payload) l->mark_end(l);
      *block = first_space && previous_space;
      return true;
    }
    if (c == '\\') {
      advance_notist(l);
      if (!l->eof(l)) advance_notist(l);
      previous_space = false; line_prefix = false;
      continue;
    }
    if (c == '`' || (c == '~' && line_prefix)) {
      unsigned run = 0;
      while (l->lookahead == c) { advance_notist(l); run++; }
      if (run >= 3 && line_prefix) {
        while (!l->eof(l) && !newline(l->lookahead)) advance_notist(l);
        if (!l->eof(l)) eat_newline(l);
        bool closed = false;
        while (!l->eof(l)) {
          while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
          unsigned count = 0;
          while (l->lookahead == c) { advance_notist(l); count++; }
          if (count >= run) {
            while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
            if (l->eof(l) || newline(l->lookahead)) { closed = true; break; }
          }
          while (!l->eof(l) && !newline(l->lookahead)) advance_notist(l);
          if (!l->eof(l)) eat_newline(l);
        }
        if (!closed) break;
      } else if (c == '`') {
        bool closed = false;
        while (!l->eof(l)) {
          unsigned count = 0;
          while (l->lookahead == '`') { advance_notist(l); count++; }
          if (count == run) { closed = true; break; }
          if (!count) advance_notist(l);
        }
        if (!closed) break;
      }
      previous_space = false; line_prefix = false;
      continue;
    }
    if (c == '$') {
      advance_notist(l);
      if (!notist_md_space(l->lookahead)) {
        bool last_space = false;
        while (!l->eof(l)) {
          int32_t next = l->lookahead; advance_notist(l);
          if (next == '\\' && !l->eof(l)) { advance_notist(l); last_space = false; }
          else if (next == '$' && !last_space) break;
          else last_space = notist_md_space(next);
        }
      }
      previous_space = false; line_prefix = false;
      continue;
    }
    if (c == '[') depth++;
    else if (c == ']') depth--;
    previous_space = notist_md_space(c);
    if (newline(c)) { eat_newline(l); line_prefix = true; }
    else { advance_notist(l); line_prefix &= space(c); }
  }
  if (mark_payload) l->mark_end(l);
  *block = first_space && previous_space;
  return false;
}

// Probe a standalone Notist construct after its initial # or @. No token end
// is changed on a failed probe; the Markdown lexer can take the same source.
static inline bool notist_md_construct_tail(TSLexer *l, int32_t marker) {
  if (marker == '@') {
    if (l->lookahead == '!') advance_notist(l);
    return l->lookahead == '(' && balanced(l, '(', ')', true);
  }
  bool args = false;
  {
    bool is_import = false;
    if (!call_path_info(l, &is_import)) return false;
    if (is_import && space(l->lookahead)) {
      while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
      if (l->lookahead != '"' || !string_tail(l, 0)) return false;
      while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
      if (l->lookahead == 'a') {
        advance_notist(l);
        if (l->lookahead != 's') return false;
        advance_notist(l);
        if (!space(l->lookahead)) return false;
        while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
        if (!call_path(l)) return false;
        while (l->lookahead == ' ' || l->lookahead == '\t') advance_notist(l);
      }
      if (l->lookahead != ';') return false;
      advance_notist(l);
      return true;
    }
    if (l->lookahead == '<') {
      if (!balanced(l, '<', '>', true)) return false;
      args = true;
    }
    if (l->lookahead == '(') {
      if (!balanced(l, '(', ')', true)) return false;
      args = true;
    }
  }
  if (l->lookahead == '[') {
    advance_notist(l);
    bool block = false;
    if (!notist_md_body(l, false, &block)) return false;
    advance_notist(l);
    return true;
  }
  return args;
}
