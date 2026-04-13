/**
 * @implements index.h
 */

/* set log level for prints in this file */
#define LOG_LEVEL LOG_LEVEL_DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h> // for LINE_MAX

#include "printing.h"
#include "index.h"
#include "defs.h"
#include "common.h"
#include "list.h"
#include "map.h"
#include "set.h"
#include "ast.h"

/*

    hashmap:
    term1: (doc1, doc2, ...)

*/

struct index {
    map_t* terms;
    size_t number_of_documents_indexed;
};

/**
 * You may utilize this for lists of query results, or write your own comparison function.
 */
ATTR_MAYBE_UNUSED
int compare_results_by_score(query_result_t *a, query_result_t *b) {
    if (a->score > b->score) {
        return -1;
    }
    if (a->score < b->score) {
        return 1;
    }
    return 0;
}

/**
 * @brief debug / helper to print a list of strings with a description.
 * Can safely be removed, but could be useful for debugging/development.
 *
 * Remove this function from your finished program once you are done
 */
ATTR_MAYBE_UNUSED
static void print_list_of_strings(const char *descr, list_t *tokens) {
    if (LOG_LEVEL <= LOG_LEVEL_INFO) {
        return;
    }

    list_iter_t *tokens_iter = list_createiter(tokens);
    if (!tokens_iter) {
        /* this is not a critical function, so just print an error and return. */
        pr_error("Failed to create iterator\n");
        return;
    }

    pr_info("\n%s: [", descr);
    while (list_hasnext(tokens_iter)) {
        char *token = (char *) list_next(tokens_iter);
        pr_info("\"%s\"%s", token, list_hasnext(tokens_iter) ? ", " : "]");
    }
    pr_info("\n");

    list_destroyiter(tokens_iter);
}

index_t *index_create() {
    index_t *index = malloc(sizeof(index_t));
    if (index == NULL) {
        pr_error("Failed to allocate memory for index\n");
        return NULL;
    }

    /**
     * TODO: Allocate, initialize and set up nescessary structures
     */
    index->terms = map_create((cmp_fn) strcmp, hash_string_fnv1a64);
    index->number_of_documents_indexed = 0;

    return index;
}

void index_destroy(index_t *index) {
    // during development, you can use the following macro to silence "unused variable" errors.
    UNUSED(index);

    /**
     * TODO: Free all memory associated with the index
     */
}

int index_document(index_t *index, char *doc_name, list_t *terms) {
    /**
     * TODO: Process document, enabling the terms and subsequent document to be found by index_query
     *
     * Note: doc_name and the list of terms is now owned by the index. See the docstring.
     */

    // TODO: Check if document has been documented previously
    
    
    list_iter_t* term_iter = list_createiter(terms);
    if (term_iter == NULL)
        return -1;
    
    index->number_of_documents_indexed++;
    char* term;
    while ((term = list_next(term_iter))) {
        entry_t* entry = map_get(index->terms, term);
        set_t* appearance_set = NULL;
        if (entry == NULL) {
            appearance_set = set_create((cmp_fn) strcmp);
            if (appearance_set == NULL) {
                list_destroyiter(term_iter);
                return -1;
            }
            
            set_insert(appearance_set, doc_name);
            map_insert(index->terms, term, appearance_set);
            continue;
        }
        // If term is already in the map:
        appearance_set = entry->val;
        set_insert(appearance_set, doc_name);
    }
    list_destroyiter(term_iter);

    return 0;
}

list_t *index_query(index_t *index, list_t *query_tokens, char *errmsg) {
    print_list_of_strings("query", query_tokens); // remove this if you like

    /**
     * TODO: perform the search, and return:
     * query is invalid => write reasoning to errmsg and return NULL
     * query is valid   => return list with any results (empty if none)
     *
     * Tip: `snprintf(errmsg, LINE_MAX, "...")` is a handy way to write to the error message buffer as you
     * would do with a typical `printf`. `snprintf` does not print anything, rather writing your message to
     * the buffer.
     */

    ast_t* ast = ast_create();
    ast_parse(ast, query_tokens);

    tree_iterator_t* tree_iter = ast_createiter(ast);
    if (tree_iter == NULL) {
        pr_error("Could not allocate memory for tree iterator\n");
        return NULL;
    }
    // Fetch results from map
    set_t* doc_appearances = NULL;
    if (doc_appearances == NULL) {
        pr_error("Could not allocate memory for set\n");
        return NULL;
    }
    


    // Construct a list of `query_result_t` objects

    // Clean up


    return NULL; // TODO: return list of query_result_t objects instead
}

void index_stat(index_t *index, size_t *n_docs, size_t *n_terms) {
    /**
     * TODO: fix this
     */
    *n_docs = index->number_of_documents_indexed;
    *n_terms = map_length(index->terms);
}
