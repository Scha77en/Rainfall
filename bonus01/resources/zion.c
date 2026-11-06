#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned int id;          // offset 0
    unsigned int size;        // offset 4
    char         label[16];   // offset 8
    void       (*on_free)(void *self); // offset 0x18 (24)
} block_t;                     // sizeof == 0x20 (32)

#define MAX_BLOCKS 8

static block_t     *blocks[MAX_BLOCKS];
static unsigned int block_count;

static void default_on_free(void *self)
{
    (void)self;   // no-op: just the stack-canary prologue/epilogue in the binary
}

static void print_banner(void)
{
    printf("\033[0;32m");
    puts("  [ZION] The station endures.");
    puts("  [ZION] Memory allocator v1.1");
    printf("\033[0m");
}

static void alloc_block(unsigned int size, char *label)
{
    block_t *meta;
    void    *buf;

    if (block_count < MAX_BLOCKS) {
        meta = malloc(sizeof(block_t));   // 0x20
        buf  = malloc(size);

        if (meta == NULL || buf == NULL) {
            free(meta);
            free(buf);
        } else {
            meta->id      = block_count;
            meta->size    = size;
            meta->on_free = default_on_free;
            strncpy(meta->label, label, 15);

            blocks[block_count] = meta;
            block_count++;

            printf("[ZION] Block %u allocated (%u bytes) label=%s\n",
                   meta->id, size, meta->label);
            printf("[ZION] Data: ");
            fflush(stdout);

            read(0, buf, size + 0x40);    // reads up to size+64 bytes into a `size`-byte buffer!
            free(buf);
        }
    }
}

static void free_block(unsigned int idx)
{
    if (idx < block_count && blocks[idx] != NULL) {
        printf("[ZION] Freeing block %u\n", idx);
        blocks[idx]->on_free(blocks[idx]);   // calls the (possibly corrupted) function pointer
        free(blocks[idx]);
        blocks[idx] = NULL;
    }
}

static void list_blocks(void)
{
    for (unsigned int i = 0; i < block_count; i++) {
        if (blocks[i] != NULL) {
            printf("[ZION] Block %u: label=%s size=%u\n",
                   blocks[i]->id, blocks[i]->label, blocks[i]->size);
        }
    }
}

int main(void)
{
    uid_t euid = geteuid();
    uid_t ruid = geteuid();
    setreuid(ruid, euid);

    print_banner();
    alloc_block(0x40, "ALPHA");
    alloc_block(0x40, "BETA");
    list_blocks();
    free_block(0);
    free_block(1);
    return 0;
}