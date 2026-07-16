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
};

typedef enum {
    MODE_NONE,
    MODE_ESCAPED_INLINE,
    MODE_ESCAPED_MULTILINE,
    MODE_RAW_INLINE,
    MODE_RAW_MULTILINE,
    MODE_FENCE,
} Mode;

typedef struct {
    uint32_t delimiter_length;
    Mode mode;
    bool fence_info_allowed;
} Scanner;

static bool scan_line_break(TSLexer *lexer) {
    if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        return true;
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
    return 2 + sizeof(scanner->delimiter_length);
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

    if (length >= 2 + sizeof(scanner->delimiter_length)) {
        scanner->mode = (Mode)buffer[0];
        scanner->fence_info_allowed = buffer[1] != 0;
        memcpy(&scanner->delimiter_length, buffer + 2, sizeof(scanner->delimiter_length));
    }
}

bool tree_sitter_notist_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    Scanner *scanner = payload;

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
