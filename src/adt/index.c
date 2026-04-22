/**
 * @implements index.h
 */

/* set log level for prints in this file */
#define LOG_LEVEL LOG_LEVEL_DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h> // for LINE_MAX

#include "printing.h"
#include "index.h"
#include "defs.h"
#include "common.h"
#include "list.h"
#include "map.h"
#include "set.h"
#include "ast.h"

typedef struct doc_info {
    char* doc_name;
    size_t n_terms;
} doc_info_t;

typedef struct term_info {
    doc_info_t* doc;
    size_t frequency;
} term_info_t;

struct index {
    map_t* terms;
    set_t* documented_paths;
};

int compare_doc_info_name(doc_info_t* a, doc_info_t* b) {
    if (!a || !b) return -1;

    return strcmp(a->doc_name, b->doc_name);
}

int compare_term_info_name(term_info_t* a, term_info_t* b) {
    if (!a || !b) return -1;
    if (!a->doc || !b->doc) return -1;

    return strcmp(a->doc->doc_name, b->doc->doc_name);
}

void destroy_term_info(term_info_t* info) {
    free(info);
}

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
    index->documented_paths = set_create((cmp_fn) compare_doc_info_name);

    return index;
}

void free_mapset(set_t* set) {
    set_destroy(set, (free_fn)destroy_term_info);
}

void free_doc_info(doc_info_t* doc) {
    free(doc->doc_name);
    free(doc);
}

void index_destroy(index_t *index) {
    // during development, you can use the following macro to silence "unused variable" errors.
    UNUSED(index);

    map_destroy(index->terms, free, (free_fn)free_mapset);
    set_destroy(index->documented_paths, (free_fn)free_doc_info);
    free(index);
    index = NULL;
}

int index_document(index_t *index, char *doc_name, list_t *terms) {
    /**
     * TODO: Process document, enabling the terms and subsequent document to be found by index_query
     *
     * Note: doc_name and the list of terms is now owned by the index. See the docstring.
     */

    // Check if document has been documented previously
    doc_info_t doc;
    doc.doc_name = doc_name;
    if (set_get(index->documented_paths, &doc)) {
        pr_debug("%s already documented\n", doc_name);
        return 0;
    }
    doc_info_t* doc_info = malloc(sizeof(doc_info_t));
    doc_info->doc_name = doc_name;
    doc_info->n_terms = list_length(terms);
    set_insert(index->documented_paths, doc_info);
    
    list_iter_t* term_iter = list_createiter(terms);
    if (term_iter == NULL)
        return -1;
    
    char* term;
    while ((term = list_next(term_iter))) {
        entry_t* entry = map_get(index->terms, term);
        if (entry == NULL) {
            set_t* appearances = set_create((cmp_fn) compare_term_info_name);
            if (appearances == NULL) {
                list_destroyiter(term_iter);
                return -1;
            }

            char* term_cpy = malloc(strlen(term)+1);
            strcpy(term_cpy, term); // copy term to make `free` easier
            
            term_info_t* term_info = malloc(sizeof(term_info_t));
            if (term_info == NULL) {
                pr_error("Could not allocate memory for term_info\n");
                return -1;
            }
            term_info->doc = doc_info;
            term_info->frequency = 1;

            set_insert(appearances, term_info);
            map_insert(index->terms, term_cpy, appearances);
            continue;
        }
        
        set_t* appearances = entry->val;
        term_info_t search;
        search.doc = doc_info;
        term_info_t* term_info = set_get(appearances, &search);
        // When term is in map, but not registered to the current document:
        if (term_info == NULL) {
            term_info = malloc(sizeof(term_info_t));
            term_info->doc = doc_info;
            term_info->frequency = 1;
            set_insert(appearances, term_info);
            pr_debug("term %s added in %s\n", term, doc_name);
            continue;
        }

        pr_debug("%s appears %zu times in %s\n", term, term_info->frequency, doc_name);
        term_info->frequency += 1;
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
    list_t* results = list_create((cmp_fn) compare_results_by_score);
    set_iter_t* set_iter = set_createiter(evaluated_set);
    term_info_t* cur_doc = NULL;
    while ((cur_doc = set_next(set_iter))) {
        query_result_t* result = malloc(sizeof(query_result_t));
        result->doc_name = cur_doc->doc->doc_name;
        double tf = (double)cur_doc->frequency / (double)cur_doc->doc->n_terms;
        double idf = log(
            (double)set_length(index->documented_paths) / (double)set_length(evaluated_set)
        );
        double tf_idf = tf * idf;
        pr_debug("%f x %f = %f\n", tf, idf, tf_idf);
        result->score = tf_idf;
        list_addlast(results, result);
    }
    list_sort(results);

    // Clean up
    set_destroyiter(set_iter);
    set_destroy(evaluated_set, NULL);
    ast_destroy(ast);

    return results;
}

void index_stat(index_t *index, size_t *n_docs, size_t *n_terms) {
    *n_docs = set_length(index->documented_paths);
    *n_terms = map_length(index->terms);
}
