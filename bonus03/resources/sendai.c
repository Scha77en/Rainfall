#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int id;              // offset 0
    unsigned int port;            // offset 4
    unsigned int connected;       // offset 8
    char         hostname[32];    // offset 0xc (12)
    // 4 bytes padding (alignment to offset 0x30)
    void       (*disconnect)(unsigned int id); // offset 0x30 (48)
} conn_t;                          // sizeof == 0x38 (56)

#define MAX_CONNS 8
static conn_t conns[MAX_CONNS];

static void disconnect_default(unsigned int id)
{
    printf("[SENDAI] Connection %u closed.\n", id);
}

static void init_connections(void)
{
    for (unsigned int i = 0; i < MAX_CONNS; i++) {
        conns[i].id         = i;
        conns[i].connected  = 0;
        conns[i].disconnect = disconnect_default;
        snprintf(conns[i].hostname, 0x20, "node-%04x.sprawl.net", i * 0x1337);
        conns[i].port = i * 0xd + 0x400;
    }
}

static void print_banner(void)
{
    printf("\033[0;34m");
    puts("  [SENDAI] Connection broker online.");
    puts("  [SENDAI] Ono-Sendai routing layer v5.0");
    printf("\033[0m");
}

static void info_leak(void)
{
    printf("[SENDAI] disconnect@binary: %p\n", disconnect_default);
}

static void handle_command(void)
{
    char cmd_buf[64];
    char data_buf[136];

    info_leak();

    while (1) {
        printf("[SENDAI] Command: ");
        fflush(stdout);
        if (fgets(cmd_buf, sizeof(cmd_buf), stdin) == NULL)
            break;
        cmd_buf[strcspn(cmd_buf, "\n")] = '\0';

        if (strncmp(cmd_buf, "CONNECT:", 8) == 0) {
            unsigned int n = strtoul(cmd_buf + 8, NULL, 10);
            if (n < MAX_CONNS) {
                conns[n].connected = 1;
                printf("[SENDAI] Connected to %s:%u\n", conns[n].hostname, conns[n].port);
            }
        } else if (strncmp(cmd_buf, "SEND:", 5) == 0) {
            unsigned int n = strtoul(cmd_buf + 5, NULL, 10);
            if (n < MAX_CONNS && conns[n].connected != 0) {
                printf("[SENDAI] Data for conn %u: ", n);
                fflush(stdout);
                gets(data_buf);               // unbounded read!
                printf("[SENDAI] Sent: ");
                printf(data_buf);             // format string bug
                putchar('\n');
            }
        } else if (strncmp(cmd_buf, "DISCONNECT:", 11) == 0) {
            unsigned int n = strtoul(cmd_buf + 11, NULL, 10);
            if (n < MAX_CONNS) {
                conns[n].disconnect(n);
            }
        } else if (strncmp(cmd_buf, "QUIT", 4) == 0) {
            break;
        } else {
            puts("[SENDAI] Unknown command.");
        }
    }
}

int main(void)
{
    init_connections();
    print_banner();
    handle_command();
    return 0;
}