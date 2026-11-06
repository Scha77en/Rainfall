#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned int id;         // offset 0
    unsigned int flags;      // offset 4 (always 0, unused)
    char         key[32];    // offset 8
    char         value[64];  // offset 0x28 (40)
} var_t;                      // sizeof == 0x68 (104)

#define MAX_VARS 8
static var_t        vault[MAX_VARS];
static unsigned int vault_count;

extern char **environ;

static void print_banner(void)
{
    printf("\033[0;36m");
    puts("  [MATRIX] The environment holds secrets.");
    puts("  [ENVIRON] Variable vault v1.0");
    printf("\033[0m");
}

static void store_var(char *key, char *value)
{
    if (vault_count < MAX_VARS) {
        vault[vault_count].id    = vault_count;
        vault[vault_count].flags = 0;
        strncpy(vault[vault_count].key,   key,   0x1f);
        strncpy(vault[vault_count].value, value, 0x3f);
        vault_count++;
    }
}

static void init_vault(void)
{
    char *v;

    v = getenv("PATH");
    store_var("PATH", v ? v : "/usr/bin");

    v = getenv("HOME");
    store_var("HOME", v ? v : "/root");

    v = getenv("TERM");
    store_var("TERM", v ? v : "xterm");
}

static void info_leak(void)
{
    printf("[ENVIRON] environ@: %p\n", environ);
    printf("[ENVIRON] vault@:   %p\n", vault);
}

static void lookup(char *key)
{
    for (unsigned int i = 0; i < vault_count; i++) {
        if (strncmp(vault[i].key, key, 0x20) == 0) {
            printf("[ENVIRON] %s=%s\n", vault[i].key, vault[i].value);
            return;
        }
    }
    puts("[ENVIRON] Key not found.");
}

static void trace_query(void)
{
    char tag[72];

    printf("[ENVIRON] Trace tag: ");
    fflush(stdout);

    if (fgets(tag, 0x40, stdin) != NULL) {     // bounded read, 64 into 72 — safe
        printf("[ENVIRON] Resolving ");
        printf(tag);                            // format string bug
        fflush(stdout);
    }
}

static void handle_input(void)
{
    char cmd_buf[88];

    info_leak();
    printf("[ENVIRON] Query: ");
    fflush(stdout);
    gets(cmd_buf);                              // unbounded read!

    if (strncmp(cmd_buf, "GET:", 4) == 0) {
        lookup(cmd_buf + 4);
    } else if (strncmp(cmd_buf, "LIST", 4) == 0) {
        for (unsigned int i = 0; i < vault_count; i++) {
            printf("[ENVIRON] [%u] %s=%s\n", vault[i].id, vault[i].key, vault[i].value);
        }
    } else {
        puts("[ENVIRON] Unknown command.");
    }
}

int main(void)
{
    init_vault();
    print_banner();
    trace_query();
    handle_input();
    return 0;
}