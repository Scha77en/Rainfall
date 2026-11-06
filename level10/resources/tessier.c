#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned int magic;      // 0xcafebabe
    unsigned int size;       // 0x200
    unsigned int level;      // 3
    char         name[16];   // "T-A_VAULT"
    char         secret[256];
} vault_t;                   // sizeof == 0x11c (284)

typedef struct {
    unsigned int id;
    unsigned int access_level;
    unsigned int active;
    char         token[32];
} session_t;                 // sizeof == 0x2c (44)

#define MAX_SESSIONS 4

static vault_t      vault;
static session_t    sessions[MAX_SESSIONS];
static unsigned int session_count;

static void init_vault(void)
{
    vault.magic = 0xcafebabe;
    vault.size  = 0x200;
    vault.level = 3;
    strncpy(vault.name, "T-A_VAULT", 15);
    memset(vault.secret, 0, sizeof(vault.secret));
    strncpy(vault.secret, "TESSIER-ASHPOOL SA \xe2\x80\x94 Final authentication required.", 255);
}

static void init_sessions(void)
{
    memset(sessions, 0, sizeof(sessions));
}

static void print_banner(void)
{
    printf("\033[0;31m");
    puts("  [TESSIER-ASHPOOL SA] This system is beyond you.");
    puts("  [T-A] Final authentication required.");
    printf("\033[0m");
}

static void format_log(void)
{
    char tag[40];

    printf("[T-A] Session tag: ");
    fflush(stdout);

    fgets(tag, 0x20, stdin);
    tag[strcspn(tag, "\n")] = '\0';

    printf("[T-A] Tag accepted: ");
    printf(tag);                // format string bug: user input as the format itself
    putchar('\n');
}

static int create_session(char *token)
{
    if (session_count < MAX_SESSIONS) {
        sessions[session_count].id           = session_count;
        sessions[session_count].access_level = 0;
        sessions[session_count].active       = 1;
        strncpy(sessions[session_count].token, token, 31);
        return session_count++;
    }
    return -1;
}

static void authenticate(void)
{
    char buf[72];
    int  idx;

    printf("[T-A] Authentication token: ");
    fflush(stdout);

    gets(buf);                  // unbounded: classic stack overflow

    idx = create_session(buf);
    if (idx < 0) {
        puts("[T-A] Session limit reached.");
        return;
    }

    if (strncmp(buf, "TA_ROOT_", 8) == 0) {
        sessions[idx].access_level = 0xff;
        printf("[T-A] Root access granted. Session %d.\n", idx);
        printf("[T-A] Vault: %s\n", vault.secret);
    } else {
        printf("[T-A] Access denied. Session %d terminated.\n", idx);
        sessions[idx].active = 0;
    }
}

int main(void)
{
    init_vault();
    init_sessions();
    print_banner();
    format_log();
    authenticate();
    return 0;
}