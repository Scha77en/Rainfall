#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int id;        // offset 0
    unsigned int hash;      // offset 4 (djb2 of the key)
    char         key[32];   // offset 8
    char         value[32]; // offset 0x28 (40)
} entry_t;                   // sizeof == 0x48 (72)

#define MAX_ENTRIES 16
static entry_t      table[MAX_ENTRIES];
static unsigned int entry_count;

static unsigned int djb2(unsigned char *s)
{
    unsigned int hash = 0x1505;
    while (*s != 0) {
        hash = *s ^ (hash * 0x21);
        s++;
    }
    return hash;
}

static void init_table(void)
{
    memset(table, 0, sizeof(table));
    for (unsigned int i = 0; i < 4; i++) {
        table[i].id = i;
        snprintf(table[i].key,   0x20, "KEY_%04x", i * 0xdead);
        snprintf(table[i].value, 0x20, "VAL_%04x", i * 0xbeef);
        table[i].hash = djb2((unsigned char *)table[i].key);
        entry_count++;
    }
}

static void print_banner(void)
{
    printf("\033[0;36m");
    puts("  [MATRIX] The cyberspace awaits.");
    puts("  [MATRIX] Key-value store v3.1");
    printf("\033[0m");
}

static void lookup(char *key)
{
    unsigned int h = djb2((unsigned char *)key);

    for (unsigned int i = 0; i < entry_count; i++) {
        if (h == table[i].hash && strncmp(table[i].key, key, 0x20) == 0) {
            printf("[MATRIX] Found: %s => %s\n", table[i].key, table[i].value);
            return;
        }
    }
    puts("[MATRIX] Key not found.");
}

static void insert(char *key, char *value)
{
    if (entry_count < MAX_ENTRIES) {
        table[entry_count].id = entry_count;
        strncpy(table[entry_count].key,   key,   0x1f);
        strncpy(table[entry_count].value, value, 0x1f);
        table[entry_count].hash = djb2((unsigned char *)key);
        entry_count++;
        printf("[MATRIX] Inserted: %s\n", key);
    }
}

static void handle_input(void)
{
    char  value_buf[32];
    char  key_buf[32];
    char  cmd_buf[104];   // no stack canary on this function!
    char *sep;

    printf("[MATRIX] Command (GET/SET): ");
    fflush(stdout);
    gets(cmd_buf);                  // unbounded read

    if (strncmp(cmd_buf, "GET:", 4) == 0) {
        strncpy(key_buf, cmd_buf + 4, 0x1f);
        lookup(key_buf);
    } else if (strncmp(cmd_buf, "SET:", 4) == 0) {
        sep = strchr(cmd_buf + 4, ':');
        if (sep != NULL) {
            *sep = '\0';
            strncpy(key_buf,   cmd_buf + 4, 0x1f);
            strncpy(value_buf, sep + 1,     0x1f);
            insert(key_buf, value_buf);
        }
    } else {
        puts("[MATRIX] Unknown command.");
    }
}

int main(void)
{
    init_table();
    print_banner();
    handle_input();
    return 0;
}