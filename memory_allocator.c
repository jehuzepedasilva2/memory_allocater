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
int* memory;
// key: block size, val: block start
Node* unallocated_blocks = NULL; 
// key: block start, val: block size
Node* block_starts = NULL;

// for tree structure
Node* put_block(Node* node, int key, int val);
Node* find_block_helper(Node* node, int key, int* min_diff, Node* result);
Node* delete_block(Node* node, int key, int val);
Node* get_inorder_succesor(Node* node);
void print_tree_helper(Node* node, const char* prefix, int is_left);
void print_tree(Node* node);
// for main allocator program
int init_program();
int init_memory();
void list();
int* alloc(int n);
void print_list_literal();
int encode_block(int start, int length);
void decode_block(int value, int* start, int* length);
int dealloc(int start_byte);

//=================== TREE DATA STRUCTURE ==================
/*
    Node tree: 
    This will keep track of the free memory blocks keyed by size [block_size, block_size]:
            [8, 2]
            /    \
        [3, 5]  [10, 10]
        /   \
    [2, 0] [6, 2]
    For brevity I will keep the tree BST unbalanced, could add it later

    Starts:
    Similar to the tree above, but it will tree ordered by block starts.
*/

Node* put_block(Node* node, int key, int val) {
    if (node == NULL) {
        Node* new_node = malloc(sizeof(Node));
        if (new_node == NULL) {
            return NULL;
        }
        new_node->key = key;
        new_node->val = val;
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

Node* find_block(Node* node, int key_size) {
    int min_diff = INT_MAX;
    return find_block_helper(node, key_size, &min_diff, NULL);
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
            // inorder succesor is the leftmost child of the right subtree
            Node* inorder_succesor = get_inorder_succesor(node->right);
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

// go left all way
Node* get_inorder_succesor(Node* node) {
    if (node->left == NULL) {
        return node;
    }
    return get_inorder_succesor(node->left);
}

/*
    For space optimilaty we can merge adjacent blocks.
    Example:
                (size:2, start:8)
                /             \
        (size:2, start:3) (size:3, start:0)

        Memory (size 10) for tree above (U = Node, A = allocated)
        bytes:    0  1  2   3  4   5  6  7   8  9
        memory:  [U][U][U] [U][U] [A][A][A] [U][U]
        With this tree, if we wanted a block of size 5, we wouldn't be able to get bytes 0-5 
        since its not in the tree so we need a function that can merge nodes that can create 
        a bigger block when merged
    
    I need to:
    1. Identify when nodes are adjacent blocks
        - compare the start_block1 to the end_block2 of another (end_block2 should be start_block1 + 1)
        - I can either scan the whole tree or, keep a second tree sorted by start (easiest)?
    2. Remove the indivual nodes from the tree
    3. Repeat this process
*/

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
    memory = malloc(MEMORY_SIZE * sizeof(int));
    if (memory == NULL) {
        return 1;
    }
    memset(memory, -1, MEMORY_SIZE * sizeof(int));
    // initialize the tree
    unallocated_blocks = put_block(unallocated_blocks, MEMORY_SIZE, 0);
    block_starts = put_block(block_starts, 0, MEMORY_SIZE);
    return 0;
}

int init_program() {
    int memory_status = init_memory();
    if (memory_status == 1) {
        printf("Something went wrong allocating main memory\n");      
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
                printf("\nEnsure number of bytes is < %d and > 0 and that byte %d is not already allocated!\n\n", MEMORY_SIZE, n);
            } else {
                printf("\nSuccessfully allocated!\nstart byte: %d, end byte: %d\n\n", *status, *(status+1));
                free(status);
            }

        } else if (user_choice == 3) {
            int start_byte;
			printf("Enter the start byte (must be the start of a block):\n> ");
			scanf("%d", &start_byte);
			printf("\n");
			int status = dealloc(start_byte);

			if (status == 1) {
				printf("byte %d is not the start of a block, byte was never allocated, or out of bounds!\n", start_byte);
			} else {
				printf("Sucess!\nThe block starting at %d was deallocated\n\n", start_byte);
			}

        } else if (user_choice == 990) {
            print_tree(unallocated_blocks);
            printf("\n");
        } else if (user_choice == 991) {
            print_tree(block_starts);
            printf("\n");
        } else if (user_choice == 992) {
            print_list_literal();
            printf("\n");
        }
    }
    return 1;
}

void list() {
    int i = 0;
    printf("Memory layout: [F = free, A = allocated]\n");
    while (i < MEMORY_SIZE) {
        if (*(memory + i) == -1) {
            int start = i;
            while (i < MEMORY_SIZE && *(memory + i) == -1) i++;
            printf("[F:%d-%d] ", start, i - 1);
        } else {
            int start, size;
            decode_block(*(memory + i), &start, &size); 
            printf("[A:%d-%d] ", start, start + size - 1);
            i = start + size;
        }
    }
    printf("\n\n");
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
        - delete the old node, and free it from memory
        - add the leftover block back to the tree
        - update the memory array

*/
int* alloc(int n) {
      if (n < 1 || n > MEMORY_SIZE) {
        return NULL;
    }
    // find a block that fits
    Node* block = find_block(unallocated_blocks, n);
    if (block == NULL) {
        return NULL;
    }
    int size = block->key, start = block->val; 
    Node* block_a = find_block(block_starts, start);
    // create a new node, and add to the tree
    int leftover_block_size = size - n;
    int leftover_block_start = start + n;
    if (leftover_block_size > 0) {
        unallocated_blocks = put_block(unallocated_blocks, leftover_block_size, leftover_block_start);
        block_starts = put_block(block_starts, leftover_block_start, leftover_block_size);
    }
    // delete the old node and update root
    unallocated_blocks = delete_block(unallocated_blocks, size, start);
    block_starts = delete_block(block_starts, start, size);
    // update memory array
    *(memory + start) = encode_block(start, n);
    for (int i = start+1; i < start + n; i++) {
        *(memory + i) = start;
    }
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
    
    When this is called, get the start byte and length, change the memory to reflect the changes and put
    back into the tree. 
*/
int dealloc(int start_byte) {
    // since the start byte of a block will never directly store the start byte, 
    // if it does, its part of the block, but not the start
    if (start_byte < 0 || start_byte > MEMORY_SIZE || *(memory + start_byte) == start_byte) {
        return 1;
    }
    // decode the values in start byte
    int start, size;
    int value = *(memory + start_byte);
    decode_block(value, &start, &size);
    // change memory
    for (int i = start; i < start + size; i++) {
        *(memory + i) = -1;
    }
    // add back into the tree
    unallocated_blocks = put_block(unallocated_blocks, size, start);  
    block_starts = put_block(block_starts, start, size);  
    return 0;
}

// ------------------- HELPERS -------------------------------

/*
    this encodes the start bytes with the high 16 bits storing the length and the lower 16 bits storing the start
    Example with start = 2 length = 3:
        memory is 0x0000 0000 (32-bits/4-bytes for an int)
        3 << 16 => 0x0003 0000 (shift 16 bits to the left)

        start: 00000000 00000000 00000000 00000010
        0xFFFF:00000000 00000000 11111111 11111111 & (and)
        result:00000000 00000000 00000000 00000010
        2 & 0xFFFF => 0x0000 0002

        start & 0xFFFF: 00000000 00000000 00000000 00000010
        length << 16:   00000011 00000000 00000000 00000000 | (or)
        result:         00000011 00000000 00000000 00000010
       (length << 16) | (start & 0xFFFF) = 0x0003 0002
    
    NOTE: Should be okay as long as start/length don't exeed 16 bits, 
    or are larger than 2^16 = 65536 (okay for this project)
*/
int encode_block(int start_byte, int size) {
    return (size << 16) | (start_byte & 0xFFFF);
}

// reverse what we did
void decode_block(int value, int* start_byte, int* size) {
    *size = (value >> 16) & 0xFFFF;
    *start_byte  = value & 0xFFFF;
}

// ------------------- FOR DEBUGGING MEMORY ------------------
void print_list_literal() {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        printf("| %d", *(memory + i));
    }
}
