#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int id;
    unsigned int type;
    unsigned int flags;
    char         name[16];
    void       (*handler)(void);
} node_t;                       // sizeof == 0x28 (40)

#define MAX_NODES 12

static node_t       nodes[MAX_NODES];
static unsigned int node_count;

static void default_handler(void)
{
    puts("[NEUROMANCER] Default handler invoked.");
}

static void init_nodes(void)
{
    for (unsigned int i = 0; i < MAX_NODES; i++) {
        nodes[i].id    = i;
        nodes[i].type  = i % 3;
        nodes[i].flags = 0;
        snprintf(nodes[i].name, sizeof(nodes[i].name), "NODE_%04x", i * 0x1a2b);
        nodes[i].handler = default_handler;
    }
    node_count = MAX_NODES;
}

static void print_banner(void)
{
    printf("\033[0;35m");
    puts("  [NEUROMANCER] I am the land of the dead.");
    puts("  [NEUROMANCER] You should not be here.");
    printf("\033[0m");
}

static void show_node(unsigned int idx)
{
    if (idx < node_count) {
        printf("[NEUROMANCER] Node %u: %s type=%u flags=%08x\n",
            nodes[idx].id, nodes[idx].name, nodes[idx].type, nodes[idx].flags);
    }
}

static void info_leak(void)
{
    printf("[NEUROMANCER] puts@libc: %p\n", puts);
}

static void console_handshake(void)
{
    char tag[72];

    printf("[NEUROMANCER] Console handle: ");
    fflush(stdout);

    if (fgets(tag, 0x40, stdin) != NULL) {
        printf("[NEUROMANCER] Trace echo: ");
        printf(tag);           // format string bug: user input used as the format itself
    }
}

static void process_input(void)
{
    char buf[136];

    printf("[NEUROMANCER] Interface: ");
    fflush(stdout);

    gets(buf);                 // unbounded: classic stack overflow

    if (strncmp(buf, "NODE:", 5) == 0) {
        unsigned long idx = strtoul(buf + 5, NULL, 10);
        show_node((unsigned int)idx);
    } else if (strncmp(buf, "LIST", 4) == 0) {
        for (unsigned int i = 0; i < node_count; i++)
            show_node(i);
    } else {
        puts("[NEUROMANCER] Unknown command.");
    }
}

static void handle_command(void)
{
    info_leak();
    console_handshake();
    process_input();
}

int main(void)
{
    init_nodes();
    print_banner();
    handle_command();
    return 0;
}