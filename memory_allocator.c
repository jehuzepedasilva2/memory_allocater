#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//=================== CONSTANTS ==========================
int MEMORY_SIZE = 1000;
/*
=================== TREE DATA STRUCTURE ==================
This will keep track of the free memory blocks keyed by size [block_size, block_size]:
        [8, 2]
        /    \
    [3, 5]  [10, 10]
    /   \
[2, 0] [6, 2]
For brevity I will keep the tree BST unbalanced, could add it later
*/

// NOTE TO SELF: If you're going to refrence the same struct within the struct, must add a name
typedef struct AvailableNode {
    int key_block_size;
    int val_block_start;
    struct AvailableNode* left;
    struct AvailableNode* right;
} AvailableNode;

AvailableNode* put_block(AvailableNode* node, int key_size, int val_start);
AvailableNode* find_block(AvailableNode* node, int key_size, int* min_diff, AvailableNode* result);
AvailableNode* delete_block(AvailableNode* node, int key_size, int val_start);
AvailableNode* get_inorder_succesor(AvailableNode* node);
// for debugging
void pretty_print_helper(AvailableNode* node, const char* prefix, int is_left);

AvailableNode* put_block(AvailableNode* node, int key_size, int val_start) {
    if (node == NULL) {
        AvailableNode* new_node = malloc(sizeof(AvailableNode));
        if (new_node == NULL) {
            return NULL;
        }
        new_node->key_block_size = key_size;
        new_node->val_block_start = val_start;
        // clean up the allocated memory for key_val pair
        return new_node;
    }
    if (key_size <= node->key_block_size) {
        node->left = put_block(node->left, key_size, val_start);
    } else {
        node->right = put_block(node->right, key_size, val_start);
    }
    return node;
}

AvailableNode* find_block(AvailableNode* node, int key_size, int* min_diff, AvailableNode* result) {
    // if we found nothing, means there are no free blocks >= size we want
    if (node == NULL) {
        return result;
    }
    // we search right if the current node size is smaller
    if (node->key_block_size < key_size) {
        return find_block(node->right, key_size, min_diff, result);
    } 
    // search left otherwise, and find the closest thing
    int current_diff = node->key_block_size - key_size;
    if (current_diff < *min_diff) {
        *min_diff = current_diff;
        result = node;
    }
    return find_block(node->left, key_size, min_diff, result);
}

/*
               8
            /    \
          3      10
        /   \      \    
       2     6     13
    We have a few cases:
    1. The tree is empty or the node doesnt exist;
        - In thi case we just return null
    2. We are deleting a leaf
        - return null to the parent pointer
    3. Node has only 1 child
        - Replace parent node with its child
    4. Node has two children (trickiest)
        - Replace node with inorder successor, and delete that node recursively
*/
AvailableNode* delete_block(AvailableNode* node, int key_size, int val_start) {
    // case 1:
    if (node == NULL) {
        return NULL;
    }
    // once we find the node:
    if (node->key_block_size == key_size && node->val_block_start == val_start) {
        // case 2:
        if (node->left == NULL && node->right == NULL) {
            // free memory and return null
            free(node);
            return NULL;
        // case 3:
        } else if (node->left == NULL) {
            AvailableNode* right_node = node->right;
            free(node);
            return right_node;
        } else if (node->right == NULL) {
            AvailableNode* left_node = node->left;
            free(node);
            return left_node;
        // case 4
        } else {
            // find the inorder succesor, and replace with this one
            // inorder succesor is the leftmost child of the right subtree
            AvailableNode* inorder_succesor = get_inorder_succesor(node->right);
            node->key_block_size = inorder_succesor->key_block_size;
            node->val_block_start = inorder_succesor->val_block_start;
            node->right = delete_block(node->right, inorder_succesor->key_block_size, inorder_succesor->val_block_start);
        }

    // if current node is smaller than size we want go right
    } else if (node->key_block_size < key_size) {
        node->right = delete_block(node->right, key_size, val_start);
    // otherwise search for it in the right tree
    } else {
        node->left = delete_block(node->left, key_size, val_start);
    }
    return node;
}

// go left all way
AvailableNode* get_inorder_succesor(AvailableNode* node) {
    if (node->left == NULL) {
        return node;
    }
    return get_inorder_succesor(node->left);
}

// ------------------- FOR DEBUGGING -----------------------
void pretty_print_helper (AvailableNode* node, const char* prefix, int is_left) {
    if (node == NULL) {
        return;
    }

    char new_prefix[256];

    // print right subtree
    if (node->right != NULL) {
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_left ? "│   " : "    ");
        pretty_print_helper(node->right, new_prefix, 0);
    }

    // print current node
    printf("%s%s(%d, %d)\n", prefix, is_left ? "└── " : "┌── ", node->key_block_size, node->val_block_start);

    // print left subtree
    if (node->left != NULL) {
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_left ? "    " : "│   ");
        pretty_print_helper(node->left, new_prefix, 1);
    }
}

/*
=================== MAIN PROGRAM =======================
*/
int init_program();
int init_memory();
void list();

char* memory;
AvailableNode* free_blocks = NULL;

int main() {
    init_program();
    return 0;
}

int init_memory() {
    memory = malloc(sizeof(char) * MEMORY_SIZE);
    if (memory == NULL) {
        return 0;
    }
    memset(memory, '0', MEMORY_SIZE);
    // initialize the tree
    free_blocks = put_block(free_blocks, MEMORY_SIZE, 0);
    return 1;
}

int init_program() {
    int memory_status = init_memory();
    if (memory_status == 0) {
        printf("Something went wrong allocating main memory\n");      
    }

    int user_choice = 0;

    while (user_choice != 4) {
        printf("Select one of following:\n");
        printf("[1] LIST\n[2] ALLOCATE\n[3] DEALLOCATE\n[4] EXIT\n");
        printf("> ");
        scanf("%d", &user_choice);
        printf("\n");

        if (user_choice == 1) {
            list();
        }
    }

    return 1;
}

void list() {
    printf("Memory layout: [F = free, A = allocated]\n");
    printf("\n\n");
}