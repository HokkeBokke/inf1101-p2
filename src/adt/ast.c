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

typedef struct node node_t;
struct node {
    void* item;
    nodetype_t type;
    node_t* parent;
    node_t* left;
    node_t* right;
};

struct ast {
    node_t* root;
};

// Declare parse functions
node_t* parse_query(list_t* query_tokens);
node_t* parse_andterm(list_t* query_tokens);
node_t* parse_orterm(list_t* query_tokens);
node_t* parse_term(list_t* query_tokens);

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

ast_t* ast_create(list_t* query_tokens) {
    ast_t* ast = malloc(sizeof(ast_t));
    if (ast == NULL) {
        pr_error("Could not allocate memory to ast\n");
        return NULL;
    }
    
    ast->root = NULL;

    return ast;
}

void ast_parse(ast_t* ast, list_t* query_tokens) {
    node_t* root = parse_query(query_tokens);
    ast->root = root;
}

/**
 * @brief Splits a list into two at the first occurance of a delimiter
 * 
 * @returns 0, or a negative integer if an error occured
 */
int split_list_with_delimit(
    list_t* original_list, 
    char* delimit, 
    list_t* lhs_list, 
    list_t* rhs_list
) {
    // Try creating lists if they are not already created
    if (lhs_list == NULL) {
        lhs_list = list_create((cmp_fn) strcmp);
        if (lhs_list == NULL) {
            pr_error("Could not create list\n");
            return -1;
        }
    }
    if (rhs_list == NULL) {
        rhs_list = list_create((cmp_fn) strcmp);
        if (rhs_list == NULL) {
            pr_error("Could not create lists\n");
            return -1;
        }
    }

    list_iter_t* iter = list_createiter(original_list);
    if (iter == NULL) {
        pr_error("Could not create iterator\n");
        return -2;
    }

    int lhs_done = 0;
    // Create seperate lists for left hand side and right hand side
    char* term = NULL;
    while ((term = list_next(iter))) {
        if (lhs_done) {
            list_addlast(rhs_list, term);
            continue;
        }
        if (strcmp(term, delimit) == 0) {
            lhs_done = 1;
            continue;
        }
        
        list_addlast(lhs_list, term);
    }

    // Clean up
    list_destroyiter(iter);

    return 0;
}

node_t* parse_term(list_t* query_tokens) {
    if (list_contains(query_tokens, "(") && list_contains(query_tokens, ")")) {
        node_t* query_root = parse_query(query_tokens);
        return query_root;
    }

    list_iter_t* iter = list_createiter(query_tokens);
    char* term = NULL;
    while ((term = list_next(iter))) {
        if (strcmp(term, "(") == 0 || strcmp(term, ")") == 0) {
            continue;
        }

        list_destroyiter(iter);
        node_t* node = create_tree_node(term, TERM);
        return node;
    }

    list_destroyiter(iter);
    return NULL;
}

node_t* parse_orterm(list_t* query_tokens) {
    if (!list_contains(query_tokens, "||")) {
        node_t* term_root = parse_term(query_tokens);
        return term_root;
    }

    list_t* left = list_create((cmp_fn) strcmp);
    list_t* right = list_create((cmp_fn) strcmp);
    split_list_with_delimit(query_tokens, "||", left, right);

    node_t* root = create_tree_node(NULL, OR);
    node_t* left_branch = parse_term(left);
    left_branch->parent = root;
    node_t* right_branch = parse_orterm(right);
    right_branch->parent = root;

    root->left = left_branch;
    root->right = right_branch;

    // Clean up
    list_destroy(left, NULL);
    list_destroy(right, NULL);

    return root;
}

node_t* parse_andterm(list_t* query_tokens) {
    if (!list_contains(query_tokens, "&&")) {
        node_t* andterm_root = parse_orterm(query_tokens);
        return andterm_root;
    }

    list_t* left = list_create((cmp_fn) strcmp);
    list_t* right = list_create((cmp_fn) strcmp);
    split_list_with_delimit(query_tokens, "&&", left, right);

    node_t* root = create_tree_node(NULL, AND);
    node_t* left_branch = parse_orterm(left);
    left_branch->parent = root;
    node_t* right_branch = parse_andterm(right);
    right_branch->parent = root;

    root->left = left_branch;
    root->right = right_branch;

    // Clean up
    list_destroy(left, NULL);
    list_destroy(right, NULL);

    return root;
}

node_t* parse_query(list_t* query_tokens) {
    if (!list_contains(query_tokens, "&!")) {
        node_t* term_root = parse_andterm(query_tokens);
        return term_root;
    }

    list_t* left = list_create((cmp_fn) strcmp);
    list_t* right = list_create((cmp_fn) strcmp);
    split_list_with_delimit(query_tokens, "&!", left, right);

    node_t* root = create_tree_node(NULL, ANDNOT);
    node_t* left_branch = parse_andterm(left);
    left_branch->parent = root;
    node_t* right_branch = parse_query(right);
    right_branch->parent = root;

    root->left = left_branch;
    root->right = right_branch;

    // Clean up
    list_destroy(left, NULL);
    list_destroy(right, NULL);

    return root;
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
    case TERM:
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
    case TERM:
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