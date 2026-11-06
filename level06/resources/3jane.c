#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned int id;
    unsigned int clearance;
    char         label[16];
    char         name[32];
} vault_entry_t;              // sizeof == 0x38 (56)

#define NUM_ENTRIES 4

static vault_entry_t vault[NUM_ENTRIES];
static unsigned int  access_log_idx;

static const unsigned int clearances[NUM_ENTRIES] = { /* raw values, see note below */ };
static const char *labels[NUM_ENTRIES] = { "ALPHA", "BETA", "GAMMA", "DELTA" };

static void init_vault(void)
{
    for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
        vault[i].id        = i;
        vault[i].clearance = clearances[i];
        strncpy(vault[i].label, labels[i], 15);
        snprintf(vault[i].name, sizeof(vault[i].name), "entry_%04x", i * 0x1337);
    }
}

static void print_banner(void)
{
    printf("\033[0;37m");
    puts("  [VILLA STRAYLIGHT] Access restricted.");
    puts("  [3JANE] Convince me.");
    printf("\033[0m");
}

static void log_request(void)
{
    char tag[72];

    printf("[3JANE] Request ID: ");
    fflush(stdout);

    fgets(tag, 0x40, stdin);
    tag[strcspn(tag, "\n")] = '\0';

    printf("[3JANE] Logging: ");
    printf(tag);                // format string bug: user input as the format itself
    putchar('\n');

    access_log_idx++;
}

static void authenticate(void)
{
    char buf[136];

    printf("[3JANE] Access code: ");
    fflush(stdout);

    gets(buf);                  // unbounded: classic stack overflow

    if (strncmp(buf, "STRAYLIGHT_", 11) == 0) {
        printf("[3JANE] Access granted. Clearance level: %u\n", vault[0].clearance);
    } else {
        puts("[3JANE] Access denied.");
    }
}

static void display_vault(void)
{
    puts("[3JANE] Vault status:");
    for (unsigned int i = 0; i < NUM_ENTRIES; i++) {
        printf("  [%u] %s clearance=%u\n", vault[i].id, vault[i].label, vault[i].clearance);
    }
}

int main(void)
{
    init_vault();
    print_banner();
    log_request();
    authenticate();
    display_vault();
    return 0;
}