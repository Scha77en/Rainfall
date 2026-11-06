#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned int id;              // offset 0
    unsigned int active;          // offset 4
    char         name[32];        // offset 8
    unsigned char data[64];       // offset 0x28 (40)
    void       (*execute)(void *self_data); // offset 0x68 (104)
} construct_t;                     // sizeof == 0x70 (112)

#define MAX_CONSTRUCTS 4

static construct_t *constructs[MAX_CONSTRUCTS];
static unsigned int construct_count;

static void default_execute(construct_t *self)
{
    printf("[FLATLINE] Executing construct %u: %s\n", self->id, self->name);
}

static void print_banner(void)
{
    printf("\033[0;35m");
    puts("  [FLATLINE] ROM construct system.");
    puts("  [FLATLINE] Construct manager v2.0");
    printf("\033[0m");
}

static construct_t *new_construct(char *name)
{
    construct_t *c;

    if (construct_count >= MAX_CONSTRUCTS)
        return NULL;

    c = malloc(sizeof(construct_t));   // 0x70
    if (c == NULL)
        return NULL;

    c->id      = construct_count;
    c->active  = 1;
    c->execute = default_execute;
    strncpy(c->name, name, 0x1f);
    memset(c->data, 0, sizeof(c->data));

    constructs[construct_count] = c;
    construct_count++;

    printf("[FLATLINE] Construct %u created: %s\n", c->id, c->name);
    return c;
}

static void delete_construct(unsigned int idx)
{
    if (idx < construct_count && constructs[idx] != NULL) {
        printf("[FLATLINE] Deleting construct %u\n", idx);
        free(constructs[idx]);
        // BUG: constructs[idx] is never cleared -> dangling pointer left behind
    }
}

static void update_construct(unsigned int idx)
{
    if (idx < construct_count && constructs[idx] != NULL) {
        printf("[FLATLINE] Update data for construct %u: ", idx);
        fflush(stdout);
        read(0, constructs[idx]->data, 0xa0);   // 160 bytes into a 64-byte field!
        printf("[FLATLINE] Construct %u updated.\n", idx);
    }
}

static void run_construct(unsigned int idx)
{
    if (idx < construct_count && constructs[idx] != NULL) {
        constructs[idx]->execute(constructs[idx]->data);
    }
}

int main(void)
{
    uid_t euid = geteuid();
    uid_t ruid = geteuid();
    setreuid(ruid, euid);

    print_banner();
    new_construct("DIXIE");
    new_construct("WINTERMUTE");
    delete_construct(0);
    update_construct(0);
    run_construct(0);
    return 0;
}