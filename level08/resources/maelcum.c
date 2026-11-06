#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned int id;
    unsigned int hops;
    char         name[16];
    char         via[16];
} route_t;                      // sizeof == 0x28 (40)

#define MAX_ROUTES 8

static route_t      routing_table[MAX_ROUTES];
static unsigned int route_count;
static char         relay_buf[512];
static unsigned int relay_len;

static void init_routing(void)
{
    memset(routing_table, 0, sizeof(routing_table));
    memset(relay_buf, 0, sizeof(relay_buf));

    strncpy(routing_table[0].name, "ZION", 15);
    strncpy(routing_table[0].via,  "MAELCUM", 15);
    routing_table[0].hops = 1;
    route_count = 1;
}

static void print_banner(void)
{
    printf("\033[0;33m");
    puts("  [ZION] Maelcum speaks.");
    puts("  [MAELCUM] Relay node \xe2\x80\x94 Babylon not.");
    printf("\033[0m");
}

static void add_route(char *name, char *via, unsigned int hops)
{
    if (route_count < MAX_ROUTES) {
        strncpy(routing_table[route_count].name, name, 15);
        strncpy(routing_table[route_count].via,  via, 15);
        routing_table[route_count].hops = hops;
        routing_table[route_count].id   = route_count;
        route_count++;
    }
}

static void relay_status(void)
{
    char tag[72];
    long n;

    printf("[MAELCUM] Tag this relay (who's asking?): ");
    fflush(stdout);

    n = read(0, tag, 0x3f);
    if (n < 0) n = 0;
    tag[n] = '\0';
    tag[strcspn(tag, "\n")] = '\0';

    printf("[MAELCUM] Logging relay tag: ");
    printf(tag);                 // format string bug: user input as the format itself
    putchar('\n');
    fflush(stdout);
}

static void relay_data(void)
{
    char cmd_buf[40];

    printf("[MAELCUM] Send it: ");
    fflush(stdout);

    read(0, cmd_buf, 0x40);      // reads up to 64 bytes into a 40-byte buffer

    if (strncmp(cmd_buf, "RELAY:", 6) == 0) {
        unsigned long n = strtoul(cmd_buf + 6, NULL, 10);
        if (n < 0x200) {
            printf("[MAELCUM] Ready for %u bytes: ", (unsigned int)n);
            fflush(stdout);
            n = read(0, relay_buf, n);
            relay_len = (unsigned int)n;
            printf("[MAELCUM] Relayed %u bytes.\n", (unsigned int)n);
        }
    } else {
        puts("[MAELCUM] Unknown command.");
    }
}

static void print_routes(void)
{
    for (unsigned int i = 0; i < route_count; i++) {
        printf("[MAELCUM] Route %u: %s via %s (%u hops)\n",
            routing_table[i].id, routing_table[i].name,
            routing_table[i].via, routing_table[i].hops);
    }
}

int main(void)
{
    init_routing();
    print_banner();
    add_route("FREESIDE", "GATEWAY_3", 3);
    add_route("VILLA_STRAYLIGHT", "TESSIER_GW", 5);
    relay_status();
    relay_data();
    print_routes();
    return 0;
}