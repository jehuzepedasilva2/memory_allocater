#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

//======== CONSTANTS, PROTOTYPES, GLOBAL VARIABLES ========

// Node for tree data structure
// NOTE TO SELF: If you're going to refrence the same struct within the struct, must add a name
typedef struct Node {
    int key;
    int val; 
    struct Node* left;
    struct Node* right;
} Node;

int MEMORY_SIZE = 10;
// key: block size, val: block start
Node* u_tree_sizes = NULL; 
// key: block start, val: block size
Node* u_tree_starts = NULL;
// key: block start, val: block size
Node* a_tree_starts = NULL;

// for tree structure
Node* put_block(Node* node, int key, int val);
Node* find_block_helper(Node* node, int key, int* min_diff, Node* result);
Node* delete_block(Node* node, int key, int val);
Node* get_succesor_delete(Node* node);
void get_succesor_predecessor(Node* node, int key, Node** result);
void free_tree(Node* node);
void print_tree_helper(Node* node, const char* prefix, int is_left);
void print_tree(Node* node);
// for main allocator program
int init_program();
int init_memory();
void list();
void list_helper(Node* node, int* memory_i);
int* alloc(int n);
int dealloc(int start_byte);
void exit_program();

//=================== TREE DATA STRUCTURE ==================

/*
    u_tree_sizes tree: 
    Keeps track of the unallocated blocks sorted by size [block_size, block_size]:
            [8, 2]
            /    \
        [3, 5]  [10, 10]
        /   \
    [2, 0] [6, 2]
    For brevity I will keep the tree BST unbalanced, could add it later.

    u_tree_starts tree:
    Similar to the tree above, but it is ordered by block starts.

    a_tree_starts tree:
    Keeps track of all the allocated blocks, sorted by start.
*/
Node* put_block(Node* node, int key, int val) {
    if (node == NULL) {
        Node* new_node = malloc(sizeof(Node));
        if (new_node == NULL) {
            return NULL;
        }
        new_node->key = key;
        new_node->val = val;
        new_node->left = NULL;
        new_node->right = NULL;
        return new_node;
    }
    if (key <= node->key) {
        node->left = put_block(node->left, key, val);
    } else {
        node->right = put_block(node->right, key, val);
    }
    return node;
}

Node* find_block_helper(Node* node, int key, int* min_diff, Node* result) {
    // if we found nothing, means there are no free blocks >= size we want
    if (node == NULL) {
        return result;
    }
    // we search right if the current node size is smaller
    if (node->key < key) {
        return find_block_helper(node->right, key, min_diff, result);
    } 
    // if node->key >= key search left, keeping track of the closest to key
    int current_diff = node->key - key;
    if (current_diff < *min_diff) {
        *min_diff = current_diff;
        result = node;
    }
    return find_block_helper(node->left, key, min_diff, result);
}

Node* find_block(Node* node, int key) {
    int min_diff = INT_MAX;
    return find_block_helper(node, key, &min_diff, NULL);
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
Node* delete_block(Node* node, int key, int val) {
    // case 1:
    if (node == NULL) {
        return NULL;
    }
    // once we find the node:
    if (node->key == key && node->val == val) {
        // case 2:
        if (node->left == NULL && node->right == NULL) {
            // free memory and return null
            free(node);
            return NULL;
        // case 3:
        } else if (node->left == NULL) {
            Node* right_node = node->right;
            free(node);
            return right_node;
        } else if (node->right == NULL) {
            Node* left_node = node->left;
            free(node);
            return left_node;
        // case 4
        } else {
            // find the inorder succesor, and replace with this one
            Node* inorder_succesor = get_succesor_delete(node->right);
            node->key = inorder_succesor->key;
            node->val = inorder_succesor->val;
            node->right = delete_block(node->right, inorder_succesor->key, inorder_succesor->val);
        }

    // if current node is smaller than size we want go right
    } else if (node->key < key) {
        node->right = delete_block(node->right, key, val);
    // otherwise search for it in the right tree
    } else {
        node->left = delete_block(node->left, key, val);
    }
    return node;
}

// inorder succesor is the leftmost child of the right subtree
Node* get_succesor_delete(Node* node) {
    if (node->left == NULL) {
        return node;
    }
    return get_succesor_delete(node->left);
}

void get_succesor_predecessor(Node* node, int key, Node** result) {
    if (node == NULL) {
        return;
    }
    if (key > node->key) {
        *(result + 1) = node;
        get_succesor_predecessor(node->right, key, result);
    } else if (key < node->key) {
        *result = node;
        get_succesor_predecessor(node->left, key, result);
    } else {
        return;
    }
}

// free trees recursively with a post order traversal
void free_tree(Node* node) {
    if (node == NULL) {
        return;
    }
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

// ------------------- FOR DEBUGGING TREE ------------------
void print_tree_helper(Node* node, const char* prefix, int is_left) {
    if (node == NULL) {
        return;
    }

    char new_prefix[256];

    if (node->right != NULL) {
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_left ? "│   " : "    ");
        print_tree_helper(node->right, new_prefix, 0);
    }

    printf("%s%s(%d, %d)\n", prefix, is_left ? "└── " : "┌── ", node->key, node->val);

    if (node->left != NULL) {
        snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_left ? "    " : "│   ");
        print_tree_helper(node->left, new_prefix, 1);
    }
}

void print_tree(Node* node) {
    print_tree_helper(node, "", 1);
}

/*
=================== MAIN PROGRAM =======================
*/

int main() {
    init_program();
    return 0;
}

int init_memory() {
    u_tree_sizes = put_block(u_tree_sizes, MEMORY_SIZE, 0);
    u_tree_starts = put_block(u_tree_starts, 0, MEMORY_SIZE);
    // error check
    if (u_tree_sizes == NULL || u_tree_starts == 0) {
        return 1;
    }
    return 0;
}

int init_program() {
    int err = init_memory();
    if (err == 1) {
        printf("Something went wrong :(!");
        return 0;
    }

    int user_choice = 0;

    while (user_choice != 4) {
        printf("Select one of following:\n");
        printf("[1] LIST\n[2] ALLOCATE\n[3] DEALLOCATE\n[4] EXIT\n> ");
        scanf("%d", &user_choice);
        printf("\n");

        if (user_choice == 1) {
            list();
        } else if (user_choice == 2) {
            printf("Enter the number of bytes to allocate\n> ");
            int n;
            scanf("%d", &n);

            int* status = alloc(n);
            if (status == NULL) {
                printf("\nEnsure number of bytes is greater than 0 and less %d!\n\n", MEMORY_SIZE);
            } else {
                printf("\nSuccessfully allocated!\nstart byte: %d, end byte: %d\n\n", *status, *(status+1));
                // free the memory
                free(status);
            }
        } else if (user_choice == 3) {
            int start_byte;
			printf("Enter the start byte (must be the start of a block):\n> ");
			scanf("%d", &start_byte);
			printf("\n");
			int status = dealloc(start_byte);

			if (status == 1) {
				printf("Byte %d is not the start of a block or was never allocated!\n\n", start_byte);
			} else if (status == 2) {
                printf("Outside of memory limits!\n\n");
            } else {
				printf("Sucess!\nThe block starting at %d was deallocated\n\n", start_byte);
			}
        } else if (user_choice == 4) {
            exit_program();
        // for debugging trees
        } else if (user_choice == 5) {
            printf("Unallocated tree sorted by block sizes:\n");
            print_tree(u_tree_sizes);
            printf("\n");

            printf("Unallocated tree sorted by block starts:\n");
            print_tree(u_tree_starts);
            printf("\n");

            printf("Allocated tree sorted by block starts:\n");
            print_tree(a_tree_starts);
            printf("\n");
        } else if (user_choice == 990) {
            print_tree(u_tree_sizes);
            printf("\n");
        } else if (user_choice == 991) {
            print_tree(u_tree_starts);
            printf("\n");
        } else if (user_choice == 992) {
            print_tree(a_tree_starts);
            printf("\n");
        } else {
            printf("Please enter an option from the menu!\n\n");
        }
    }
    return 1;
}

void list() {
    printf("U = unallocated, A = allocated\n");
    if (a_tree_starts == NULL) {
        printf("[U:0-%d]\n\n", MEMORY_SIZE-1);
        return;
    }
    int memory_i = 0;
    list_helper(a_tree_starts, &memory_i);
    printf("[U:%d-%d]", memory_i, MEMORY_SIZE-1);
    printf("\n\n");
}

/*
    Traverse the tree sorted by starts, keep track of the memory index and if that memory
    index is less than the current node, we know theres an allocated block starting at 
    memory index, all the way up to the start-1 of current node, we can print that block out then 
    print the free block the node represents and update memory index to be the start of the 
    current nodes block plus the size.
    Example:
        (0, 4)
            \
            (7, 2)
*/
void list_helper(Node* node, int* memory_i) {
    if (node == NULL) {
        return;
    }
    list_helper(node->left, memory_i);
    if (*memory_i < MEMORY_SIZE && *memory_i < node->key) {
        printf("[U:%d-%d]", *memory_i, node->key-1);
    }
    printf("[A:%d-%d]", node->key, node->key + node->val - 1);
    *memory_i = node->key + node->val;
    list_helper(node->right, memory_i);
}

/*
    alloc: allocates N bytes of contiguous space if available 
        args: int n
        return: NULLL on failure
                on success it returns the range or memory allocated in the following format (start-byte,end-byte)

    When called, search thorugh the binary tree structure, find a block that fits the size requested best.
    if the size of the found block is greater than n, we split, delete the old node we found, then create a new node with
    the unused bytes and insert it back into the tree.
    Example:
        On first call tree looks like (with memory size of 10):
                            [10, 0]
        - call alloc(3) => gets the [10, 0] block
        - calculate leftover byest and create a new node => [7, 3]
          (lefover node starts at previous nodes start + n, size is previous size - n)
        - delete the old node
        - add the leftover block back to the tree
        - update allocated tree
*/
int* alloc(int n) {
      if (n < 1 || n > MEMORY_SIZE) {
        return NULL;
    }
    // find a block that fits
    Node* block_by_size = find_block(u_tree_sizes, n);
    if (block_by_size == NULL) {
        return NULL;
    }
    int size = block_by_size->key, start = block_by_size->val; 
    // create a new node, and add to the tree
    int leftover_block_size = size - n;
    int leftover_block_start = start + n;
    if (leftover_block_size > 0) {
        // put left over block in both trees
        u_tree_sizes = put_block(u_tree_sizes, leftover_block_size, leftover_block_start);
        u_tree_starts = put_block(u_tree_starts, leftover_block_start, leftover_block_size);
    }
    // delete the old node and update both trees
    u_tree_sizes = delete_block(u_tree_sizes, size, start);
    u_tree_starts = delete_block(u_tree_starts, start, size);
    // update allocated tree
    a_tree_starts = put_block(a_tree_starts, start, n);
    // set up return;
    int* start_end = malloc(sizeof(int) * 2);
    *start_end = start;
    *(start_end + 1) = start + n - 1;
    return start_end;
}

/*
    dealloc: deallocates memory allocated using alloc return
        args: start-byte
        return: (error) if no block allocated at the start byte 
                (ok) otherwise
    
    When this is called, get the start byte and length from a_tree_starts

    For space optimilaty we can merge adjacent blocks.
    Example:
                (start:8, size:2)
                /          
         (start:0, size:3)
                    \
                (start:3, size:2)

        Memory (size 10) for tree above (U = Node, A = allocated)
        bytes:    0  1  2   3  4   5  6  7   8  9
        memory:  [U][U][U] [U][U] [A][A][A] [U][U]
        With this tree, if we wanted a block of size 5, we wouldn't be able to get bytes 0-5 
        since its not in the tree so we need a function that can merge nodes that can create 
        a bigger block when merged

        - create a third tree where the nodes are sorted by start bytes.
        - when something is deallocated;
            1. create the node for that block
            2. insert back into the u_trees and delete from a_tree
            3. find the newly inserted blocks inorder predecessor and successor and check that:
                - predecessors_start + predecessors_size == new_block_start (previous unallocated block is adjacent)
                - new_block_start + size == successors_start (next unallocated block is adjcent)
                - we can then merge and create a new node with (predecessors_start, successors_start + successors_start_size)
                - delete predecessor and successor and the new_block and inser merged_block
*/
int dealloc(int start_byte) {
    if (start_byte < 0 || start_byte > MEMORY_SIZE) {
        return 2;
    }
    Node* allocated_block = find_block(a_tree_starts, start_byte);
    if (allocated_block == NULL) {
        return 1;
    }
    int start = allocated_block->key, size = allocated_block->val;
    // delete block from allocated
    a_tree_starts = delete_block(a_tree_starts, start, size);  
    // get inorder predecessor and successor from the newly created unallocated block
    Node** result = malloc(sizeof(Node*) * 2);
    get_succesor_predecessor(u_tree_starts, start, result);
    Node* predecessor = *(result + 1); 
    Node* successor = *(result + 0);
    // compare them to the new block
    // compare new_block with the previous free block
    int merged_start = start;
    int merged_size = size;
    // check predecessor
    if (predecessor != NULL && predecessor->key + predecessor->val == start) {
        int predecessor_key = predecessor->key, predecessor_val = predecessor->val;
        merged_start = predecessor_key;
        merged_size += predecessor_val;
        // remove predecessor from both u_trees
        u_tree_starts = delete_block(u_tree_starts, predecessor_key, predecessor_val);
        u_tree_sizes = delete_block(u_tree_sizes, predecessor_val, predecessor_key);
    }
    // check successor
    if (successor != NULL && start + size == successor->key) {
        int successor_key = successor->key, successor_val = successor->val;
        merged_size += successor_val;
        // remove successor from both u_trees
        u_tree_starts = delete_block(u_tree_starts, successor_key, successor_val);
        u_tree_sizes = delete_block(u_tree_sizes, successor_val, successor_key);
    }
    // insert merged node or insert the free block into both u_trees
    u_tree_starts = put_block(u_tree_starts, merged_start, merged_size);
    u_tree_sizes = put_block(u_tree_sizes, merged_size, merged_start);
    // free result
    free(result);
    return 0;
}

// teminate the program, free allocated memory
void exit_program() {
    free_tree(u_tree_sizes);
    free_tree(u_tree_starts);
    free_tree(a_tree_starts);
    return;
}