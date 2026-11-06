#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_SESSIONS 8
#define LOG_SIZE 512
#define AUTH_TIMEOUT 30

typedef struct {
    int id;
    int active;
    time_t created;
    char token[32];
} session_t;

static session_t sessions[MAX_SESSIONS];
static int session_count = 0;
static char audit_log[LOG_SIZE];

static void init_sessions(void) {
    memset(sessions, 0, sizeof(sessions));
    memset(audit_log, 0, sizeof(audit_log));
    srand(time(NULL));
}

static int create_session(void) {
    if (session_count >= MAX_SESSIONS)
        return -1;
    sessions[session_count].id = session_count;
    sessions[session_count].active = 1;
    sessions[session_count].created = time(NULL);
    snprintf(sessions[session_count].token, 32, "%08x%08x", rand(), rand());
    return session_count++;
}

static void log_attempt(const char *user, int success) {
    snprintf(audit_log, LOG_SIZE, "[%ld] user=%s status=%s",
        time(NULL), user, success ? "OK" : "FAIL");
}

static void print_banner(void) {
    puts("\033[0;36m");
    puts("  ╔══════════════════════════════════════╗");
    puts("  ║   SPRAWL//NET Authentication v2.1   ║");
    puts("  ║      Console cowboy detected.        ║");
    puts("  ╚══════════════════════════════════════╝");
    puts("\033[0m");
}

static int verify_credentials(const char *input) {
    const char valid[] = "zion_access_2077";
    return strncmp(input, valid, strlen(valid)) == 0;
}

static void auth_loop(void) {
    char credentials[64];
    int sid = create_session();

    if (sid < 0) {
        fprintf(stderr, "[-] Session limit reached\n");
        exit(1);
    }

    printf("[SPRAWL//NET] Session %d initialized\n", sid);
    printf("[SPRAWL//NET] Enter credentials: ");
    fflush(stdout);

    gets(credentials);

    log_attempt(credentials, verify_credentials(credentials));

    if (verify_credentials(credentials)) {
        printf("[SPRAWL//NET] Access granted. Token: %s\n",
            sessions[sid].token);
    } else {
        printf("[SPRAWL//NET] Access denied.\n");
        printf("[SPRAWL//NET] Audit: %s\n", audit_log);
    }
}

int main(void) {
    init_sessions();
    print_banner();
    auth_loop();
    return 0;
}
level00@rainfall:~$ clear
level00@rainfall:~$ vim case.c
level00@rainfall:~$ cat case.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_SESSIONS 8
#define LOG_SIZE 512
#define AUTH_TIMEOUT 30

typedef struct {
    int id;
    int active;
    time_t created;
    char token[32];
} session_t;

static session_t sessions[MAX_SESSIONS];
static int session_count = 0;
static char audit_log[LOG_SIZE];

static void init_sessions(void) {
    memset(sessions, 0, sizeof(sessions));
    memset(audit_log, 0, sizeof(audit_log));
    srand(time(NULL));
}

static int create_session(void) {
    if (session_count >= MAX_SESSIONS)
        return -1;
    sessions[session_count].id = session_count;
    sessions[session_count].active = 1;
    sessions[session_count].created = time(NULL);
    snprintf(sessions[session_count].token, 32, "%08x%08x", rand(), rand());
    return session_count++;
}

static void log_attempt(const char *user, int success) {
    snprintf(audit_log, LOG_SIZE, "[%ld] user=%s status=%s",
        time(NULL), user, success ? "OK" : "FAIL");
}

static void print_banner(void) {
    puts("\033[0;36m");
    puts("  ╔══════════════════════════════════════╗");
    puts("  ║   SPRAWL//NET Authentication v2.1   ║");
    puts("  ║      Console cowboy detected.        ║");
    puts("  ╚══════════════════════════════════════╝");
    puts("\033[0m");
}

static int verify_credentials(const char *input) {
    const char valid[] = "zion_access_2077";
    return strncmp(input, valid, strlen(valid)) == 0;
}

static void auth_loop(void) {
    char credentials[64];
    int sid = create_session();

    if (sid < 0) {
        fprintf(stderr, "[-] Session limit reached\n");
        exit(1);
    }

    printf("[SPRAWL//NET] Session %d initialized\n", sid);
    printf("[SPRAWL//NET] Enter credentials: ");
    fflush(stdout);

    gets(credentials);

    log_attempt(credentials, verify_credentials(credentials));

    if (verify_credentials(credentials)) {
        printf("[SPRAWL//NET] Access granted. Token: %s\n",
            sessions[sid].token);
    } else {
        printf("[SPRAWL//NET] Access denied.\n");
        printf("[SPRAWL//NET] Audit: %s\n", audit_log);
    }
}

int main(void) {
    init_sessions();
    print_banner();
    auth_loop();
    return 0;
}

