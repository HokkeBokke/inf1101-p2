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

void free_mapset(void* set) {
    set_destroy(set, NULL);
}

void index_destroy(index_t *index) {
    // during development, you can use the following macro to silence "unused variable" errors.
    UNUSED(index);

    map_destroy(index->terms, free, free_mapset);
    free(index);
    index = NULL;
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
            char* term_cpy = malloc(strlen(term)+1);
            strcpy(term_cpy, term);
            map_insert(index->terms, term_cpy, appearance_set);
            continue;
        }
        // If term is already in the map:
        appearance_set = entry->val;
        set_insert(appearance_set, doc_name);
    }
    list_destroyiter(term_iter);
    list_destroy(terms, free);

    return 0;
}

set_t* evaluate(index_t* index, node_t* node) {
    if (node->type == INVALID) {
        pr_error("Node type is invalid\n");
        return NULL;
    }
    if (node->type == WORD) {
        char* word = node->item;
        entry_t* entry = map_get(index->terms, word);
        if (entry == NULL) {
            pr_debug("created empty set\n");
            set_t* empty_set = set_create((cmp_fn) strcmp);
            return empty_set;
        }
        // copy set
        set_t* evaluated_set = set_union(entry->val, entry->val); 
        pr_debug("set length: %zu\n", set_length(evaluated_set));
        return evaluated_set;
    }
    set_t* lhs = evaluate(index, node->left);
    set_t* rhs = evaluate(index, node->right);
    set_t* evaluated_set = NULL;
    switch(node->type) {
        case AND:
            evaluated_set = set_intersection(lhs, rhs);
            break;
        case OR:
            evaluated_set = set_union(lhs, rhs);
            break;
        case ANDNOT:
            evaluated_set = set_difference(lhs, rhs);
            break;
        default:
    }
    set_destroy(lhs, NULL);
    set_destroy(rhs, NULL);
    return evaluated_set;
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
    if (!ast_parse(ast, query_tokens, errmsg)) {
        ast_destroy(ast);
        return NULL;
    }

    // Fetch results from map
    set_t* evaluated_set = evaluate(index, ast->root);


    // Construct a list of `query_result_t` objects
    list_t* results = list_create((cmp_fn) strcmp);
    set_iter_t* set_iter = set_createiter(evaluated_set);
    char* cur_doc = NULL;
    while ((cur_doc = set_next(set_iter))) {
        query_result_t* result = malloc(sizeof(query_result_t));
        result->doc_name = cur_doc;
        result->score = 0;
        list_addlast(results, result);
    }

    // Clean up
    set_destroyiter(set_iter);
    set_destroy(evaluated_set, NULL);
    ast_destroy(ast);

    return results;
}

void index_stat(index_t *index, size_t *n_docs, size_t *n_terms) {
    *n_docs = index->number_of_documents_indexed;
    *n_terms = map_length(index->terms);
}
