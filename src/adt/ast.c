/**
 * @author
 * Håkon Ludvig Øisund <haakon@oisund.no>
 * 
 * @implements ast.h
 * 
 * @brief Abstract Syntax Tree (AST) 
 */

#include "ast.h"
#include "printing.h"
#include "set.h"
#include "index.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

node_t* create_tree_node(void* item, nodetype_t nodetype) {
    node_t* node = malloc(sizeof(node_t));
    if (node == NULL) {
        pr_error("Could not allocate memory to tree node\n");
        return NULL;
    }

    node->item = item;
    node->type = nodetype;
    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;

    return node;
}

ast_t* ast_create() {
    ast_t* ast = malloc(sizeof(ast_t));
    if (ast == NULL) {
        pr_error("Could not allocate memory to ast\n");
        return NULL;
    }
    
    ast->root = NULL;

    return ast;
}

void rec_destroy_node(node_t* node) {
    if (node == NULL)
        return;

    rec_destroy_node(node->left);
    rec_destroy_node(node->right);
    free(node);
}

void ast_destroy(ast_t* ast) {
    if (ast == NULL) 
        return;
    
    // Delete all nodes
    rec_destroy_node(ast->root);

    free(ast);
    ast = NULL;
}

// Declare parse functions
node_t* parse_term(list_iter_t* iter, char* errmsg);

uint8_t ast_parse(ast_t* ast, list_t* query_tokens, char* errmsg) {
    if (list_length(query_tokens) == 1) {
        char* word = list_popfirst(query_tokens);
        list_addfirst(query_tokens, word); // add it back immediatly so memory is freed when `list_destroy` is called later
        if (is_operator(word)) {
            pr_error("Not a valid query\n");
            snprintf(errmsg, LINE_MAX, "Expected word to search for");
            return 0;
        }
        node_t* top = create_tree_node(word, WORD);
        ast->root = top;
        return 1;
    }

    list_iter_t* iterator = list_createiter(query_tokens);
    node_t* root = parse_term(iterator, errmsg);
    list_destroyiter(iterator);
    if (root == NULL) {
        return 0;
    }
    ast->root = root;
    return 1;
}

nodetype_t str_to_nodetype(char* str) {
    if (strcmp(str, "&!") == 0) return ANDNOT;
    else if (strcmp(str, "&&") == 0) return AND;
    else if (strcmp(str, "||") == 0) return OR;
    
    return WORD;
}

char* nodetype_to_str(nodetype_t op) {
    switch (op)
    {
    case ANDNOT:
        return "&!";
    case AND:
        return "&&";
    case OR:
        return "||";
    default:
        return "";
    }
}

node_t* parse_term(list_iter_t* iter, char* errmsg) {
    node_t* left = NULL;
    nodetype_t op = INVALID;
    node_t* right = NULL;
    char* cur = NULL;
    while ((cur = list_next(iter))) {
        if (strcmp(cur, ")") == 0) 
            break;
        if (is_operator(cur)) {
            if (op) {
                snprintf(errmsg, LINE_MAX, "Too many operators in term");
                break;
            }
            if (!left) {
                snprintf(errmsg, LINE_MAX, "Expected term before %s", cur);
                break;
            }
            if (left && right) {
                snprintf(errmsg, LINE_MAX, "Expected operator between terms");
                break;
            }
            op = str_to_nodetype(cur);
            continue;
        }
        if (!left) {
            if (strcmp(cur, "(") == 0) {
                left = parse_term(iter, errmsg);
                if (left == NULL) break;
            } else {
                left = create_tree_node(cur, WORD);
            }
            continue;
        }
        if (!right) {
            if (strcmp(cur, "(") == 0) {
                right = parse_term(iter, errmsg);
                if (right == NULL) break;
            } else {
                right = create_tree_node(cur, WORD);
            }
            continue;
        }
    }
    uint8_t failure = 0;
    if (!left) {
        snprintf(errmsg, LINE_MAX, "Term cannot be empty");
        failure = 1;
    } else if (!op) {
        snprintf(errmsg, LINE_MAX, 
            "Expected operator after %s", (char*)left->item
        );
        failure = 1;
    } else if (!right) {
        snprintf(errmsg, LINE_MAX, 
            "Expected term after %s", nodetype_to_str(op)
        );
        failure = 1;
    }
    if (failure) {
        free(left);
        free(right);
        return NULL;
    }
    
    node_t* parent = create_tree_node(NULL, op);
    parent->left = left;
    left->parent = parent;
    parent->right = right;
    right->parent = parent;

    return parent;
}

/* TREE ITERATOR */

struct tree_iterator {
    ast_t* ast;
    list_t* stack;
};

uint8_t is_operator(void* value) {
    if (strstr(value, "&&") != NULL) return 1;
    else if (strstr(value, "&!") != NULL) return 1;
    else if (strstr(value, "||")) return 1;

    return 0;
}

void ast_move_left(tree_iterator_t* iter, node_t* start) {
    node_t* current = start;

    while (current) {
        list_addfirst(iter->stack, current);
        
        current = current->left;
    }
}

tree_iterator_t* ast_createiter(ast_t* ast) {
    tree_iterator_t* iter = malloc(sizeof(tree_iterator_t));
    if (iter == NULL) {
        pr_error("Could not allocate memory for iterator\n");
        return NULL;
    }

    iter->ast = ast;
    iter->stack = list_create((cmp_fn) strcmp);
    ast_move_left(iter, ast->root);

    return iter;
}

void ast_destroyiter(tree_iterator_t* iter) {
    if (iter == NULL) return;

    list_destroy(iter->stack, NULL);
    free(iter);
    iter = NULL;
}

uint8_t ast_hasnext(tree_iterator_t* iter) {
    return list_length(iter->stack) != 0;
}

void* ast_next(tree_iterator_t* iter) {
    if (!ast_hasnext(iter)) 
        return NULL;
    node_t* cur = list_popfirst(iter->stack);

    if (cur->right) {
        ast_move_left(iter, cur->right);
    }

    return cur->item;
}


/* THE FOLLOWING IS GENERATED BY CHATGPT */

void print_tree_recursive(const node_t *node, const char *prefix, int is_last) {
    if (node == NULL) {
        return;
    }

    printf("%s", prefix);
    printf("%s", is_last ? "└── " : "├── ");
    switch (node->type)
    {
    case WORD:
        printf("%s\n", (char*)node->item);
        break;
    case ANDNOT:
        printf("&!\n");
        break;
    case AND:
        printf("&&\n");
        break;
    case OR:
        printf("||\n");
        break;
    default:
        break;
    }

    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_last ? "    " : "│   ");

    int has_left = (node->left != NULL);
    int has_right = (node->right != NULL);

    if (has_left && has_right) {
        print_tree_recursive(node->left, new_prefix, 0);
        print_tree_recursive(node->right, new_prefix, 1);
    } else if (has_left) {
        print_tree_recursive(node->left, new_prefix, 1);
    } else if (has_right) {
        print_tree_recursive(node->right, new_prefix, 1);
    }
}

void print_tree(const ast_t* ast) {    
    node_t* root = ast->root;
    if (root == NULL) {
        printf("(empty tree)\n");
        return;
    }

    switch (ast->root->type)
    {
    case WORD:
        printf("%s\n", (char*)ast->root->item);
        break;
    case ANDNOT:
        printf("&!\n");
        break;
    case AND:
        printf("&&\n");
        break;
    case OR:
        printf("||\n");
        break;
    default:
        break;
    }

    if (root->left && root->right) {
        print_tree_recursive(root->left, "", 0);
        print_tree_recursive(root->right, "", 1);
    } else if (root->left) {
        print_tree_recursive(root->left, "", 1);
    } else if (root->right) {
        print_tree_recursive(root->right, "", 1);
    }
}