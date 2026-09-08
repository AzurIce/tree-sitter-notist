#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum TokenType {
    ESCAPED_INLINE_OPEN,
    ESCAPED_MULTILINE_OPEN,
    RAW_INLINE_OPEN,
    RAW_MULTILINE_OPEN,
    STRING_CONTENT,
    ESCAPE_SEQUENCE,
    STRING_CLOSE,
    INLINE_RAW,
    FENCE_OPEN,
    FENCE_INFO,
    FENCE_CONTENT,
    FENCE_CLOSE,
    LINE_COMMENT,
    BLOCK_COMMENT,
    TEXT_CHUNK,
    STRONG_OPEN,
    STRONG_CLOSE,
    EMPHASIS_OPEN,
    EMPHASIS_CLOSE,
    UNDERLINE_OPEN,
    UNDERLINE_CLOSE,
    STRIKE_OPEN,
    STRIKE_CLOSE,
    MATH_OPEN,
    MATH_CONTENT,
    MATH_CLOSE,
    MATH_BLOCK_OPEN,
    MATH_BLOCK_CONTENT,
    MATH_BLOCK_CLOSE,
    TARGET_OPEN,
    HEADING_MARKER,
    LIST_MARKER,
    ENUM_MARKER,
    TASK_MARKER,
    RULE_MARKER,
    PIPE,
    TABLE_DELIMITER_ROW,
    OR_OPERATOR,
    AND_OPERATOR,
    COMPARISON_OPERATOR,
    ADDITIVE_OPERATOR,
    MULTIPLICATIVE_OPERATOR,
    ELSE_KEYWORD,
};

typedef enum {
    MODE_NONE,
    MODE_ESCAPED_INLINE,
    MODE_ESCAPED_MULTILINE,
    MODE_RAW_INLINE,
    MODE_RAW_MULTILINE,
    MODE_FENCE,
    MODE_MATH_BLOCK,
    MODE_MATH_INLINE,
} Mode;

typedef struct {
    uint32_t delimiter_length;
    Mode mode;
    bool fence_info_allowed;
    uint8_t inline_stack[16];
    uint8_t inline_stack_length;
} Scanner;

enum InlineDelimiter {
    INLINE_STRONG,
    INLINE_EMPHASIS,
    INLINE_UNDERLINE,
    INLINE_STRIKE,
};

static bool scan_inline_delimiter(TSLexer *lexer, char delimiter, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        if (lexer->lookahead != delimiter) {
            return false;
        }
        lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    return true;
}

static bool has_inline_close(TSLexer *lexer, char delimiter, uint8_t length) {
    bool has_content = false;
    while (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') {
        if (lexer->lookahead == '\\') {
            lexer->advance(lexer, false);
            if (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') {
                lexer->advance(lexer, false);
                has_content = true;
            }
            continue;
        }
        if (lexer->lookahead == delimiter) {
            uint8_t found = 0;
            while (found < length && lexer->lookahead == delimiter) {
                lexer->advance(lexer, false);
                found++;
            }
            return found == length && has_content;
        }
        lexer->advance(lexer, false);
        has_content = true;
    }
    return false;
}

/// From just after a `<` already consumed and marked: is there a same-line
/// `>` reachable through at least one body character? Backslash escapes the
/// next character (`\<`, `\>`, `\\`); control characters terminate the
/// scan, mirroring the authoritative parser. Pure lookahead: advances the
/// lexer but the caller's `mark_end` decides the token end.
static bool has_target_close(TSLexer *lexer) {
    bool has_content = false;
    while (!lexer->eof(lexer)) {
        if (lexer->lookahead < 0x20) {
            return false;
        }
        if (lexer->lookahead == '\\') {
            lexer->advance(lexer, false);
            if (lexer->eof(lexer) || lexer->lookahead < 0x20) {
                return false;
            }
            lexer->advance(lexer, false);
            has_content = true;
            continue;
        }
        if (lexer->lookahead == '>') {
            return has_content;
        }
        lexer->advance(lexer, false);
        has_content = true;
    }
    return false;
}

/// From just after an opening `$` (already consumed and marked): does the
/// same line hold a valid math close? The first body character must be
/// non-whitespace and not `$`; a close is an unescaped `$` preceded by a
/// non-whitespace character (a `$` preceded by whitespace is content, the
/// scan continues); a `\` pairs with the next character (so `\$` never
/// terminates and stays in the payload). Pure lookahead: advances the lexer
/// but the caller's `mark_end` decides the token end.
static bool has_math_close(TSLexer *lexer) {
    if (lexer->eof(lexer) || lexer->lookahead == '\r' || lexer->lookahead == '\n' ||
        lexer->lookahead == '$' || lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        return false;
    }
    int32_t previous = 0;
    while (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') {
        if (lexer->lookahead == '\\') {
            lexer->advance(lexer, false);
            if (lexer->eof(lexer) || lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                return false;
            }
            previous = lexer->lookahead;
            lexer->advance(lexer, false);
            continue;
        }
        if (lexer->lookahead == '$' && previous != ' ' && previous != '\t') {
            return true;
        }
        previous = lexer->lookahead;
        lexer->advance(lexer, false);
    }
    return false;
}

static bool scan_word(TSLexer *lexer, const char *word) {
    for (const char *c = word; *c != '\0'; c++) {
        if (lexer->lookahead != *c) {
            return false;
        }
        lexer->advance(lexer, false);
    }
    return true;
}

// 近似判断标识符字符（Unicode 字母/数字/`-`/`_`）：非 ASCII 一律视为字母。
static bool at_identifier_char(TSLexer *lexer) {
    int32_t c = lexer->lookahead;
    return c == '_' || c == '-' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || c >= 0x80;
}

static bool scan_line_break(TSLexer *lexer) {
    if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);        return true;
    }
    if (lexer->lookahead != '\r') {
        return false;
    }
    lexer->advance(lexer, false);
    if (lexer->lookahead != '\n') {
        return false;
    }
    lexer->advance(lexer, false);
    return true;
}

static bool scan_quotes(TSLexer *lexer, uint32_t count) {
    for (uint32_t index = 0; index < count; index++) {
        if (lexer->lookahead != '"') {
            return false;
        }
        lexer->advance(lexer, false);
    }
    return true;
}

static bool scan_hashes(TSLexer *lexer, uint32_t count) {
    for (uint32_t index = 0; index < count; index++) {
        if (lexer->lookahead != '#') {
            return false;
        }
        lexer->advance(lexer, false);
    }
    return lexer->lookahead != '#';
}

static bool scan_string_close(TSLexer *lexer, const Scanner *scanner) {
    uint32_t quote_count = scanner->mode == MODE_ESCAPED_MULTILINE ||
                                   scanner->mode == MODE_RAW_MULTILINE
                               ? 3
                               : 1;
    if (!scan_quotes(lexer, quote_count)) {
        return false;
    }
    if (scanner->mode == MODE_RAW_INLINE || scanner->mode == MODE_RAW_MULTILINE) {
        return scan_hashes(lexer, scanner->delimiter_length);
    }
    return true;
}

static bool scan_string_close_after_first_quote(TSLexer *lexer, const Scanner *scanner) {
    uint32_t quote_count = scanner->mode == MODE_ESCAPED_MULTILINE ||
                                   scanner->mode == MODE_RAW_MULTILINE
                               ? 3
                               : 1;
    if (quote_count == 3 && !scan_quotes(lexer, 2)) {
        return false;
    }
    if (scanner->mode == MODE_RAW_INLINE || scanner->mode == MODE_RAW_MULTILINE) {
        return scan_hashes(lexer, scanner->delimiter_length);
    }
    return true;
}

static bool scan_inline_raw_body(TSLexer *lexer, uint32_t delimiter_length) {
    while (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') {
        if (lexer->lookahead != '`') {
            lexer->advance(lexer, false);
            continue;
        }

        uint32_t closing_length = 0;
        while (lexer->lookahead == '`') {
            closing_length++;
            lexer->advance(lexer, false);
        }
        if (closing_length == delimiter_length) {
            lexer->mark_end(lexer);
            lexer->result_symbol = INLINE_RAW;
            return true;
        }
    }
    return false;
}

static bool scan_fence_close_at_line_start(TSLexer *lexer, uint32_t minimum_length) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
    }
    if (lexer->lookahead != '`') {
        return false;
    }

    uint32_t length = 0;
    while (lexer->lookahead == '`') {
        length++;
        lexer->advance(lexer, false);
    }
    if (length < minimum_length) {
        return false;
    }
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
    }
    return lexer->eof(lexer) || lexer->lookahead == '\r' || lexer->lookahead == '\n';
}

static bool scan_fence_content(TSLexer *lexer, uint32_t delimiter_length) {
    bool has_content = false;
    bool at_line_start = true;

    while (!lexer->eof(lexer)) {
        if (at_line_start) {
            lexer->mark_end(lexer);
            if (scan_fence_close_at_line_start(lexer, delimiter_length)) {
                if (!has_content) {
                    return false;
                }
                lexer->result_symbol = FENCE_CONTENT;
                return true;
            }
        }

        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            has_content = true;
            at_line_start = true;
        } else if (lexer->lookahead == '\r') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == '\n') {
                lexer->advance(lexer, false);
            }
            lexer->mark_end(lexer);
            has_content = true;
            at_line_start = true;
        } else {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            has_content = true;
            at_line_start = false;
        }
    }
    return false;
}

/// 块级数学的 bare `$$` 行：行首空白 + `$$` + 其余仅空白到行尾。
/// 与 fence close 同款：探测会消费字符但不 mark_end（调用方先前的
/// mark_end 定界 content，随后的 mark_end 定界 close token）。
static bool scan_math_bare_line(TSLexer *lexer) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
    }
    if (lexer->lookahead != '$') {
        return false;
    }
    lexer->advance(lexer, false);
    if (lexer->lookahead != '$') {
        return false;
    }
    lexer->advance(lexer, false);
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
    }
    return lexer->eof(lexer) || lexer->lookahead == '\r' || lexer->lookahead == '\n';
}

static bool scan_math_block_content(TSLexer *lexer) {
    bool has_content = false;
    bool at_line_start = true;

    while (!lexer->eof(lexer)) {
        if (at_line_start) {
            lexer->mark_end(lexer);
            if (scan_math_bare_line(lexer)) {
                if (!has_content) {
                    return false;
                }
                lexer->result_symbol = MATH_BLOCK_CONTENT;
                return true;
            }
        }

        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            has_content = true;
            at_line_start = true;
        } else if (lexer->lookahead == '\r') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == '\n') {
                lexer->advance(lexer, false);
            }
            lexer->mark_end(lexer);
            has_content = true;
            at_line_start = true;
        } else {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            has_content = true;
            at_line_start = false;
        }
    }
    return false;
}

void *tree_sitter_notist_external_scanner_create(void) {
    return calloc(1, sizeof(Scanner));
}

void tree_sitter_notist_external_scanner_destroy(void *payload) {
    free(payload);
}

unsigned tree_sitter_notist_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *scanner = payload;
    buffer[0] = (char)scanner->mode;
    buffer[1] = scanner->fence_info_allowed ? 1 : 0;
    memcpy(buffer + 2, &scanner->delimiter_length, sizeof(scanner->delimiter_length));
    unsigned offset = 2 + sizeof(scanner->delimiter_length);
    buffer[offset++] = (char)scanner->inline_stack_length;
    memcpy(buffer + offset, scanner->inline_stack, scanner->inline_stack_length);
    return offset + scanner->inline_stack_length;
}

void tree_sitter_notist_external_scanner_deserialize(
    void *payload,
    const char *buffer,
    unsigned length
) {
    Scanner *scanner = payload;
    scanner->mode = MODE_NONE;
    scanner->delimiter_length = 0;
    scanner->fence_info_allowed = false;
    scanner->inline_stack_length = 0;

    if (length >= 2 + sizeof(scanner->delimiter_length)) {
        scanner->mode = (Mode)buffer[0];
        scanner->fence_info_allowed = buffer[1] != 0;
        memcpy(&scanner->delimiter_length, buffer + 2, sizeof(scanner->delimiter_length));
        unsigned offset = 2 + sizeof(scanner->delimiter_length);
        if (length > offset) {
            scanner->inline_stack_length = (uint8_t)buffer[offset++];
            if (scanner->inline_stack_length > sizeof(scanner->inline_stack) ||
                length < offset + scanner->inline_stack_length) {
                scanner->inline_stack_length = 0;
            } else {
                memcpy(
                    scanner->inline_stack,
                    buffer + offset,
                    scanner->inline_stack_length
                );
            }
        }
    }
}

bool tree_sitter_notist_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    Scanner *scanner = payload;
    bool text_has_content = false;

    if (scanner->mode == MODE_NONE && scanner->inline_stack_length > 0) {
        uint8_t top = scanner->inline_stack[scanner->inline_stack_length - 1];
        enum TokenType close_symbol;
        char delimiter;
        uint8_t length;
        switch (top) {
            case INLINE_STRONG:
                close_symbol = STRONG_CLOSE;
                delimiter = '*';
                length = 1;
                break;
            case INLINE_EMPHASIS:
                close_symbol = EMPHASIS_CLOSE;
                delimiter = '_';
                length = 1;
                break;
            case INLINE_UNDERLINE:
                close_symbol = UNDERLINE_CLOSE;
                delimiter = '_';
                length = 2;
                break;
            case INLINE_STRIKE:
                close_symbol = STRIKE_CLOSE;
                delimiter = '~';
                length = 2;
                break;
            }
        if (valid_symbols[close_symbol] && lexer->lookahead == delimiter) {
            if (scan_inline_delimiter(lexer, delimiter, length)) {
                scanner->inline_stack_length--;
                lexer->result_symbol = close_symbol;
                return true;
            }
            return false;
        }
    }

    if (scanner->mode == MODE_NONE && scanner->inline_stack_length < sizeof(scanner->inline_stack)) {
        enum TokenType open_symbol;
        uint8_t delimiter_kind;
        char delimiter;
        uint8_t length;
        bool candidate = false;

        if (lexer->lookahead == '*' && valid_symbols[STRONG_OPEN]) {
            open_symbol = STRONG_OPEN;
            delimiter_kind = INLINE_STRONG;
            delimiter = '*';
            length = 1;
            candidate = true;
        } else if (lexer->lookahead == '_' &&
                   (valid_symbols[UNDERLINE_OPEN] || valid_symbols[EMPHASIS_OPEN])) {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            if (lexer->lookahead == '_' && valid_symbols[UNDERLINE_OPEN]) {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                open_symbol = UNDERLINE_OPEN;
                delimiter_kind = INLINE_UNDERLINE;
                delimiter = '_';
                length = 2;
            } else if (valid_symbols[EMPHASIS_OPEN]) {
                open_symbol = EMPHASIS_OPEN;
                delimiter_kind = INLINE_EMPHASIS;
                delimiter = '_';
                length = 1;
            } else {
                return false;
            }
            candidate = true;
        } else if (lexer->lookahead == '~' && valid_symbols[STRIKE_OPEN]) {
            open_symbol = STRIKE_OPEN;
            delimiter_kind = INLINE_STRIKE;
            delimiter = '~';
            length = 2;
            candidate = true;
        }

        if (candidate) {
            if (delimiter != '_' && !scan_inline_delimiter(lexer, delimiter, length)) {
                return false;
            }
            if (!has_inline_close(lexer, delimiter, length)) {
                return false;
            }
            scanner->inline_stack[scanner->inline_stack_length++] = delimiter_kind;
            lexer->result_symbol = open_symbol;
            return true;
        }
    }

    // Target literal open: fire only when the same line holds an unescaped
    // `>` after at least one body character, so an unterminated `<` degrades
    // to plain text like every other paired inline delimiter. `<` is never a
    // comparison operator in a state that also expects a target.
    if (scanner->mode == MODE_NONE && lexer->lookahead == '<' &&
        valid_symbols[TARGET_OPEN]) {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        if (has_target_close(lexer)) {
            lexer->result_symbol = TARGET_OPEN;
            return true;
        }
        return false;
    }

    if (scanner->mode == MODE_NONE && lexer->get_column(lexer) == 0 &&
        (valid_symbols[HEADING_MARKER] || valid_symbols[LIST_MARKER] ||
         valid_symbols[ENUM_MARKER] || valid_symbols[TASK_MARKER] ||
         valid_symbols[RULE_MARKER] || valid_symbols[TABLE_DELIMITER_ROW] ||
         valid_symbols[MATH_BLOCK_OPEN])) {
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            text_has_content = true;
        }

        // 块级数学开行：仅含 `$$` 的行（行首可有空白）；探测失败时落回
        // 文本，不让 `$$` 拦截普通行。单个 `$` 开头的行顺带尝试行内开符，
        // 避免拦截行内数学。
        if (lexer->lookahead == '$' &&
            (valid_symbols[MATH_BLOCK_OPEN] || valid_symbols[MATH_OPEN])) {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            if (lexer->lookahead == '$' && valid_symbols[MATH_BLOCK_OPEN]) {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                    lexer->advance(lexer, false);
                    lexer->mark_end(lexer);
                }
                if (lexer->eof(lexer) || lexer->lookahead == '\r' ||
                    lexer->lookahead == '\n') {
                    scanner->mode = MODE_MATH_BLOCK;
                    lexer->result_symbol = MATH_BLOCK_OPEN;
                    return true;
                }
                // `$$` 后还有内容：不是 bare 行，整段落回文本（`$` 后紧跟
                // `$` 本就不开行内数学）。
                goto scan_text_chunk;
            }
            if (valid_symbols[MATH_OPEN] && has_math_close(lexer)) {
                scanner->mode = MODE_MATH_INLINE;
                lexer->result_symbol = MATH_OPEN;
                return true;
            }
            goto scan_text_chunk;
        }

        // Heading: 行首 `=` run，数量即 level 无上限；其后必须跟空白或行尾/EOF。
        if (lexer->lookahead == '=' && valid_symbols[HEADING_MARKER]) {
            while (lexer->lookahead == '=') {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
            }
            text_has_content = true;
            if (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                lexer->result_symbol = HEADING_MARKER;
                return true;
            }
            if (lexer->eof(lexer) || lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                lexer->result_symbol = HEADING_MARKER;
                return true;
            }
            goto scan_text_chunk;
        }

        // `-` run：3 个以上且其后只有空白到行尾是 rule，`- ` 是列表/任务，
        // 其余落回普通文本。
        if (lexer->lookahead == '-' &&
            (valid_symbols[LIST_MARKER] || valid_symbols[TASK_MARKER] ||
             valid_symbols[RULE_MARKER])) {
            uint32_t dashes = 0;
            while (lexer->lookahead == '-') {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                dashes++;
            }
            text_has_content = true;
            if (dashes >= 3 && valid_symbols[RULE_MARKER]) {
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                    lexer->advance(lexer, false);
                }
                if (lexer->eof(lexer) || lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                    lexer->mark_end(lexer);
                    lexer->result_symbol = RULE_MARKER;
                    return true;
                }
                goto scan_text_chunk;
            }
            if (dashes == 1 && lexer->lookahead == ' ' &&
                (valid_symbols[LIST_MARKER] || valid_symbols[TASK_MARKER])) {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                if (lexer->lookahead == '[' && valid_symbols[TASK_MARKER]) {
                    lexer->advance(lexer, false);
                    bool valid_state = lexer->lookahead == ' ' || lexer->lookahead == 'x' ||
                                       lexer->lookahead == 'X';
                    if (valid_state) {
                        lexer->advance(lexer, false);
                        if (lexer->lookahead == ']') {
                            lexer->advance(lexer, false);
                            if (lexer->lookahead == ' ') {
                                lexer->advance(lexer, false);
                                lexer->mark_end(lexer);
                                lexer->result_symbol = TASK_MARKER;
                                return true;
                            }
                        }
                    }
                }
                if (valid_symbols[LIST_MARKER]) {
                    lexer->result_symbol = LIST_MARKER;
                    return true;
                }
            }
            goto scan_text_chunk;
        }

        if (lexer->lookahead == '+' && valid_symbols[ENUM_MARKER]) {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            text_has_content = true;
            if (lexer->lookahead == ' ') {
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                lexer->result_symbol = ENUM_MARKER;
                return true;
            }
            goto scan_text_chunk;
        }

        // Table 分隔行：`|` 开头，cell 形如 `:?-+:?`，整行扫描为单个 token。
        if (lexer->lookahead == '|' && valid_symbols[TABLE_DELIMITER_ROW]) {
            lexer->advance(lexer, false);
            bool valid_row = true;
            for (;;) {
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                    lexer->advance(lexer, false);
                }
                if (lexer->lookahead == ':') {
                    lexer->advance(lexer, false);
                }
                uint32_t dashes = 0;
                while (lexer->lookahead == '-') {
                    lexer->advance(lexer, false);
                    dashes++;
                }
                if (dashes == 0) {
                    valid_row = false;
                    break;
                }
                if (lexer->lookahead == ':') {
                    lexer->advance(lexer, false);
                }
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                    lexer->advance(lexer, false);
                }
                if (lexer->lookahead != '|') {
                    valid_row = false;
                    break;
                }
                lexer->advance(lexer, false);
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                    lexer->advance(lexer, false);
                }
                if (lexer->eof(lexer) || lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                    break;
                }
            }
            if (valid_row) {
                lexer->mark_end(lexer);
                lexer->result_symbol = TABLE_DELIMITER_ROW;
                return true;
            }
            goto scan_text_chunk;
        }

        if (text_has_content) {
            goto scan_text_chunk;
        }
    }

    // 行内数学开符：`$` 后必须紧跟非空白、非 `$`，且同一行内存在合法闭符
    //（闭符前非空白、`\$` 不闭）。探测失败时 `$` 降级为普通文本，不跨行。
    if (scanner->mode == MODE_NONE && lexer->lookahead == '$' &&
        valid_symbols[MATH_OPEN]) {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        if (has_math_close(lexer)) {
            scanner->mode = MODE_MATH_INLINE;
            lexer->result_symbol = MATH_OPEN;
            return true;
        }
        return false;
    }

    if (scanner->mode == MODE_NONE && lexer->lookahead == '/' &&
        (valid_symbols[LINE_COMMENT] || valid_symbols[BLOCK_COMMENT])) {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '/' && valid_symbols[LINE_COMMENT]) {
            lexer->advance(lexer, false);
            while (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') {
                lexer->advance(lexer, false);
            }
            lexer->mark_end(lexer);
            lexer->result_symbol = LINE_COMMENT;
            return true;
        }
        if (lexer->lookahead == '*' && valid_symbols[BLOCK_COMMENT]) {
            lexer->advance(lexer, false);
            uint32_t depth = 1;
            while (!lexer->eof(lexer) && depth > 0) {
                if (lexer->lookahead == '/') {
                    lexer->advance(lexer, false);
                    if (lexer->lookahead == '*') {
                        lexer->advance(lexer, false);
                        depth++;
                    }
                    continue;
                }
                if (lexer->lookahead == '*') {
                    lexer->advance(lexer, false);
                    if (lexer->lookahead == '/') {
                        lexer->advance(lexer, false);
                        depth--;
                    }
                    continue;
                }
                lexer->advance(lexer, false);
            }
            lexer->mark_end(lexer);
            lexer->result_symbol = BLOCK_COMMENT;
            return true;
        }
        return false;
    }

    // Code 二元运算符与 else 关键词：只在对应 token 合法的 parser 状态
    //（二元延续 / if 的 else 延续位置）产生，前导水平空白被跳过。这样
    // markup 文本栈的 text_chunk 不会抢先吞掉 ` *` 之类的延续，而纯文本中
    // 的 `a + b` 因 token 不合法仍按普通文本解析。
    if (scanner->mode == MODE_NONE &&
        (valid_symbols[OR_OPERATOR] || valid_symbols[AND_OPERATOR] ||
         valid_symbols[COMPARISON_OPERATOR] || valid_symbols[ADDITIVE_OPERATOR] ||
         valid_symbols[MULTIPLICATIVE_OPERATOR] || valid_symbols[ELSE_KEYWORD])) {
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
            lexer->advance(lexer, true);
        }

        enum TokenType result_symbol = OR_OPERATOR;
        bool matched = false;
        int32_t character = lexer->lookahead;

        if (valid_symbols[COMPARISON_OPERATOR] &&
            (character == '<' || character == '>' || character == '=' || character == '!')) {
            lexer->advance(lexer, false);
            if ((character == '<' || character == '>') && lexer->lookahead != '=') {
                matched = true;
            } else if (lexer->lookahead == '=') {
                lexer->advance(lexer, false);
                matched = true;
            }
            result_symbol = COMPARISON_OPERATOR;
        } else if (valid_symbols[ADDITIVE_OPERATOR] &&
                   (character == '+' || character == '-')) {
            lexer->advance(lexer, false);
            matched = true;
            result_symbol = ADDITIVE_OPERATOR;
        } else if (valid_symbols[MULTIPLICATIVE_OPERATOR] &&
                   (character == '*' || character == '/')) {
            lexer->advance(lexer, false);
            // `//` 与 `/*` 是注释（或 markup 文本），除号不能抢先，
            // 否则 `#let x = 1  // c` 会被解析成 `1 / / c`。
            if (character == '/' &&
                (lexer->lookahead == '/' || lexer->lookahead == '*')) {
                return false;
            }
            matched = true;
            result_symbol = MULTIPLICATIVE_OPERATOR;
        } else if (valid_symbols[OR_OPERATOR] && character == 'o' &&
                   scan_word(lexer, "or") && !at_identifier_char(lexer)) {
            matched = true;
            result_symbol = OR_OPERATOR;
        } else if (valid_symbols[AND_OPERATOR] && character == 'a' &&
                   scan_word(lexer, "and") && !at_identifier_char(lexer)) {
            matched = true;
            result_symbol = AND_OPERATOR;
        } else if (valid_symbols[ELSE_KEYWORD] && character == 'e' &&
                   scan_word(lexer, "else") && !at_identifier_char(lexer)) {
            matched = true;
            result_symbol = ELSE_KEYWORD;
        }

        if (!matched) {
            return false;
        }
        lexer->mark_end(lexer);
        lexer->result_symbol = result_symbol;
        return true;
    }

scan_text_chunk:
    if (scanner->mode == MODE_NONE && valid_symbols[TEXT_CHUNK]) {
        bool has_content = text_has_content;
        while (!lexer->eof(lexer)) {
            if (lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                break;
            }
            if (lexer->lookahead == '#' || lexer->lookahead == '[' || lexer->lookahead == ']' ||
                lexer->lookahead == '`' || lexer->lookahead == '@' || lexer->lookahead == ',' ||
                lexer->lookahead == '*' || lexer->lookahead == '_' || lexer->lookahead == '~' ||
                lexer->lookahead == '$' || lexer->lookahead == '\\' ||
                lexer->lookahead == '{' || lexer->lookahead == '}' || lexer->lookahead == '|' ||
                lexer->lookahead == '(') {
                break;
            }
            // `//` 与 `/*` 只在 Code 上下文（注释 token 合法）截断文本；
            // Markup 文本流中它们是普通字符。
            if (lexer->lookahead == '/' &&
                (valid_symbols[LINE_COMMENT] || valid_symbols[BLOCK_COMMENT])) {
                lexer->mark_end(lexer);
                lexer->advance(lexer, false);
                if (lexer->lookahead == '/' || lexer->lookahead == '*') {
                    if (has_content) {
                        lexer->result_symbol = TEXT_CHUNK;
                        return true;
                    }
                    return false;
                }
                has_content = true;
                lexer->mark_end(lexer);
                continue;
            }

            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            has_content = true;
        }
        if (has_content) {
            lexer->result_symbol = TEXT_CHUNK;
            return true;
        }
    }

    if (scanner->mode == MODE_NONE && lexer->lookahead == '|' && valid_symbols[PIPE]) {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        lexer->result_symbol = PIPE;
        return true;
    }

    if (scanner->mode == MODE_NONE && lexer->lookahead == '`' &&
        (valid_symbols[FENCE_OPEN] || valid_symbols[INLINE_RAW])) {
        uint32_t length = 0;
        while (lexer->lookahead == '`') {
            length++;
            lexer->advance(lexer, false);
        }
        lexer->mark_end(lexer);
        if (valid_symbols[INLINE_RAW] && scan_inline_raw_body(lexer, length)) {
            return true;
        }
        if (length >= 3 && valid_symbols[FENCE_OPEN]) {
            scanner->mode = MODE_FENCE;
            scanner->delimiter_length = length;
            scanner->fence_info_allowed = true;
            lexer->result_symbol = FENCE_OPEN;
            return true;
        }
        return false;
    }

    if (scanner->mode == MODE_NONE && lexer->lookahead == '"' &&
        (valid_symbols[ESCAPED_INLINE_OPEN] || valid_symbols[ESCAPED_MULTILINE_OPEN])) {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        if (lexer->lookahead == '"') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == '"') {
                lexer->advance(lexer, false);
                if (scan_line_break(lexer) && valid_symbols[ESCAPED_MULTILINE_OPEN]) {
                    lexer->mark_end(lexer);
                    scanner->mode = MODE_ESCAPED_MULTILINE;
                    lexer->result_symbol = ESCAPED_MULTILINE_OPEN;
                    return true;
                }
            }
        }
        if (valid_symbols[ESCAPED_INLINE_OPEN]) {
            scanner->mode = MODE_ESCAPED_INLINE;
            lexer->result_symbol = ESCAPED_INLINE_OPEN;
            return true;
        }
        return false;
    }

    if (scanner->mode == MODE_NONE && lexer->lookahead == 'r' &&
        (valid_symbols[RAW_INLINE_OPEN] || valid_symbols[RAW_MULTILINE_OPEN])) {
        lexer->advance(lexer, false);
        uint32_t hashes = 0;
        while (lexer->lookahead == '#') {
            hashes++;
            lexer->advance(lexer, false);
        }
        if (hashes == 0 || lexer->lookahead != '"') {
            return false;
        }
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        if (lexer->lookahead == '"') {
            lexer->advance(lexer, false);
            if (lexer->lookahead == '"') {
                lexer->advance(lexer, false);
                if (scan_line_break(lexer) && valid_symbols[RAW_MULTILINE_OPEN]) {
                    lexer->mark_end(lexer);
                    scanner->mode = MODE_RAW_MULTILINE;
                    scanner->delimiter_length = hashes;
                    lexer->result_symbol = RAW_MULTILINE_OPEN;
                    return true;
                }
            }
        }
        if (valid_symbols[RAW_INLINE_OPEN]) {
            scanner->mode = MODE_RAW_INLINE;
            scanner->delimiter_length = hashes;
            lexer->result_symbol = RAW_INLINE_OPEN;
            return true;
        }
        return false;
    }

    if (scanner->mode == MODE_FENCE) {
        if (scanner->fence_info_allowed && valid_symbols[FENCE_INFO] &&
            lexer->lookahead != '\r' && lexer->lookahead != '\n') {
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                lexer->advance(lexer, true);
            }
            bool has_info = false;
            while (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') {
                int32_t character = lexer->lookahead;
                lexer->advance(lexer, false);
                if (character != ' ' && character != '\t') {
                    lexer->mark_end(lexer);
                    has_info = true;
                }
            }
            if (!has_info) {
                return false;
            }
            scanner->fence_info_allowed = false;
            lexer->result_symbol = FENCE_INFO;
            return true;
        }
        if (scanner->fence_info_allowed &&
            (lexer->lookahead == '\r' || lexer->lookahead == '\n')) {
            scanner->fence_info_allowed = false;
            return false;
        }
        if (valid_symbols[FENCE_CLOSE] &&
            scan_fence_close_at_line_start(lexer, scanner->delimiter_length)) {
            lexer->mark_end(lexer);
            scanner->mode = MODE_NONE;
            scanner->delimiter_length = 0;
            scanner->fence_info_allowed = false;
            lexer->result_symbol = FENCE_CLOSE;
            return true;
        }
        if (valid_symbols[FENCE_CONTENT]) {
            return scan_fence_content(lexer, scanner->delimiter_length);
        }
        return false;
    }

    if (scanner->mode == MODE_MATH_INLINE) {
        // 闭符：开符时扫描器已验证同行存在合法闭符（闭符前非空白、`\$`
        // 不闭），content 扫描的选点规则与之严格一致，因此这里的 `$` 就是
        // 那个闭符。
        if (valid_symbols[MATH_CLOSE] && lexer->lookahead == '$') {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            scanner->mode = MODE_NONE;
            lexer->result_symbol = MATH_CLOSE;
            return true;
        }
        // 行内数学内容：raw text，逐字保留（`\` 与其后字符成对吞入，`\$`
        // 不终止、原样留在载荷里）；前一个字符是空白时 `$` 不是闭符，继续
        // 作为内容吞入——与 has_math_close 的选点规则严格一致。
        if (valid_symbols[MATH_CONTENT]) {
            bool has_content = false;
            int32_t previous = 0;
            while (!lexer->eof(lexer) && lexer->lookahead != '\r' && lexer->lookahead != '\n') {
                if (lexer->lookahead == '\\') {
                    lexer->advance(lexer, false);
                    if (lexer->eof(lexer) || lexer->lookahead == '\r' ||
                        lexer->lookahead == '\n') {
                        break;
                    }
                    previous = lexer->lookahead;
                    lexer->advance(lexer, false);
                    lexer->mark_end(lexer);
                    has_content = true;
                    continue;
                }
                if (lexer->lookahead == '$' && valid_symbols[MATH_CLOSE] &&
                    previous != ' ' && previous != '\t') {
                    break;
                }
                previous = lexer->lookahead;
                lexer->advance(lexer, false);
                lexer->mark_end(lexer);
                has_content = true;
            }
            if (has_content) {
                lexer->result_symbol = MATH_CONTENT;
                return true;
            }
            return false;
        }
        return false;
    }

    if (scanner->mode == MODE_MATH_BLOCK) {
        // scan_math_bare_line 成功后再 mark_end：close token 覆盖整行
        //（含行首空白与尾随空白，与 fence close 同款）。
        if (valid_symbols[MATH_BLOCK_CLOSE] && scan_math_bare_line(lexer)) {
            lexer->mark_end(lexer);
            scanner->mode = MODE_NONE;
            lexer->result_symbol = MATH_BLOCK_CLOSE;
            return true;
        }
        if (valid_symbols[MATH_BLOCK_CONTENT]) {
            return scan_math_block_content(lexer);
        }
        return false;
    }

    if (scanner->mode == MODE_NONE) {
        return false;
    }

    if (valid_symbols[STRING_CLOSE] && lexer->lookahead == '"') {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        if (scan_string_close_after_first_quote(lexer, scanner)) {
            lexer->mark_end(lexer);
            scanner->mode = MODE_NONE;
            scanner->delimiter_length = 0;
            lexer->result_symbol = STRING_CLOSE;
            return true;
        }
        if (valid_symbols[STRING_CONTENT]) {
            lexer->result_symbol = STRING_CONTENT;
            return true;
        }
        return false;
    }

    bool escaped = scanner->mode == MODE_ESCAPED_INLINE || scanner->mode == MODE_ESCAPED_MULTILINE;
    if (escaped && valid_symbols[ESCAPE_SEQUENCE] && lexer->lookahead == '\\') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '"' || lexer->lookahead == '\\' || lexer->lookahead == 'n' ||
            lexer->lookahead == 'r' || lexer->lookahead == 't') {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            lexer->result_symbol = ESCAPE_SEQUENCE;
            return true;
        }
        return false;
    }

    if (!valid_symbols[STRING_CONTENT]) {
        return false;
    }

    bool has_content = false;
    bool multiline = scanner->mode == MODE_ESCAPED_MULTILINE || scanner->mode == MODE_RAW_MULTILINE;
    while (!lexer->eof(lexer)) {
        if (!multiline && (lexer->lookahead == '\r' || lexer->lookahead == '\n')) {
            break;
        }
        if (escaped && lexer->lookahead == '\\') {
            break;
        }
        if (lexer->lookahead == '"') {
            lexer->mark_end(lexer);
            if (scan_string_close(lexer, scanner)) {
                break;
            }
            has_content = true;
            lexer->mark_end(lexer);
            continue;
        }
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        has_content = true;
    }

    if (has_content) {
        lexer->result_symbol = STRING_CONTENT;
        return true;
    }
    return false;
}
