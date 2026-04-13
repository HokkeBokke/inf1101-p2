#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "list.h"
#include "common.h"
#include "tokenize.h"
#include "defs.h"

int is_operator_part(int c) {
    switch (c) {
        /* return 1 for all of these, otherwise let `isalnum` decide */
        case '(':
            ATTR_FALLTHROUGH;
        case ')':
            ATTR_FALLTHROUGH;
        case '|':
            ATTR_FALLTHROUGH;
        case '&':
            ATTR_FALLTHROUGH;
        case '!':
            return 1;
        default:
            /* break here as gcc sometimes throws a no-return warning. clang does not. */
            break;
    }
    return 0;
}

int is_valid_query_char(int c) {
    if (is_operator_part(c)) {
        return 1;
    }
    /* not a special char, filter normally as ascii alphanumeric */
    return is_ascii_alnum(c);
}

static list_t *tokenize_query(char *query) {
    list_t *tokens = list_create((cmp_fn) strcmp);
    if (!tokens) {
        return NULL;
    }

    /**
     * 1. Parse the query into a list of tokens
     * This will reduce phrases such as "o-k" to "ok", which is completely fine for searching purposes.
     * In fact, google tends to ignore most special characters, although in more sophisticated manner.
     */
    int status = tokenize_string(query, tokens, 1, is_space_or_par, is_valid_query_char, tolower);
    if (status < 0) {
        list_destroy(tokens, free);
        return NULL;
    }

    return tokens;
}

int main() {
    char* query = "(guinea && pig) &! (island || (new || old))";
    list_t* query_tokens = tokenize_query(query);

    ast_t* ast = ast_create();
    ast_parse(ast, query_tokens);

    print_tree(ast);

    return 0;
}