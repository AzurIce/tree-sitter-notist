#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum TokenType {
    RAW_OPEN,
    RAW_CONTENT,
    RAW_CLOSE,
    BACKTICK_RAW,
};

typedef struct {
    uint32_t raw_delimiter_length;
    bool raw_active;
} Scanner;

static bool scan_raw_close(TSLexer *lexer, uint32_t delimiter_length) {
    if (lexer->lookahead != ']') {
        return false;
    }

    lexer->advance(lexer, false);
    for (uint32_t index = 0; index < delimiter_length; index++) {
        if (lexer->lookahead != '!') {
            return false;
        }
        lexer->advance(lexer, false);
    }
    return true;
}

static bool scan_backtick_raw(TSLexer *lexer) {
    if (lexer->lookahead != '`') {
        return false;
    }

    uint32_t delimiter_length = 0;
    while (lexer->lookahead == '`') {
        delimiter_length++;
        lexer->advance(lexer, false);
    }

    while (!lexer->eof(lexer)) {
        if (lexer->lookahead != '`') {
            lexer->advance(lexer, false);
            continue;
        }

        uint32_t closing_length = 0;
        while (lexer->lookahead == '`') {
            closing_length++;
            lexer->advance(lexer, false);
        }
        if (closing_length >= delimiter_length) {
            lexer->mark_end(lexer);
            lexer->result_symbol = BACKTICK_RAW;
            return true;
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
    buffer[0] = scanner->raw_active ? 1 : 0;
    memcpy(buffer + 1, &scanner->raw_delimiter_length, sizeof(scanner->raw_delimiter_length));
    return 1 + sizeof(scanner->raw_delimiter_length);
}

void tree_sitter_notist_external_scanner_deserialize(
    void *payload,
    const char *buffer,
    unsigned length
) {
    Scanner *scanner = payload;
    scanner->raw_active = false;
    scanner->raw_delimiter_length = 0;

    if (length >= 1 + sizeof(scanner->raw_delimiter_length)) {
        scanner->raw_active = buffer[0] != 0;
        memcpy(
            &scanner->raw_delimiter_length,
            buffer + 1,
            sizeof(scanner->raw_delimiter_length)
        );
    }
}

bool tree_sitter_notist_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    Scanner *scanner = payload;

    if (!scanner->raw_active && valid_symbols[BACKTICK_RAW] && lexer->lookahead == '`') {
        return scan_backtick_raw(lexer);
    }

    if (!scanner->raw_active && valid_symbols[RAW_OPEN] && lexer->lookahead == '!') {
        uint32_t delimiter_length = 0;
        while (lexer->lookahead == '!') {
            delimiter_length++;
            lexer->advance(lexer, false);
        }
        if (lexer->lookahead != '[') {
            return false;
        }

        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        scanner->raw_active = true;
        scanner->raw_delimiter_length = delimiter_length;
        lexer->result_symbol = RAW_OPEN;
        return true;
    }

    if (!scanner->raw_active) {
        return false;
    }

    if (valid_symbols[RAW_CLOSE] && lexer->lookahead == ']') {
        if (scan_raw_close(lexer, scanner->raw_delimiter_length)) {
            lexer->mark_end(lexer);
            scanner->raw_active = false;
            scanner->raw_delimiter_length = 0;
            lexer->result_symbol = RAW_CLOSE;
            return true;
        }
        return false;
    }

    if (!valid_symbols[RAW_CONTENT]) {
        return false;
    }

    bool has_content = false;
    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == ']') {
            lexer->mark_end(lexer);
            if (scan_raw_close(lexer, scanner->raw_delimiter_length)) {
                if (!has_content) {
                    return false;
                }
                lexer->result_symbol = RAW_CONTENT;
                return true;
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
        lexer->result_symbol = RAW_CONTENT;
        return true;
    }
    return false;
}
