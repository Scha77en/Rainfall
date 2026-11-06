#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OPERATOR_LEN 64
#define MAX_RETRIES 3
#define DIAGNOSTIC_VERSION "2.1.4"

typedef struct {
    unsigned int flags;
    unsigned int retries;
    char version[16];
    char last_op[OPERATOR_LEN];
} diag_ctx_t;

static diag_ctx_t ctx;

static void __attribute__((noinline)) maintenance_exec(long code)
{
    if (code == 0xdeadbeefL) {
        setreuid(geteuid(), geteuid());
        execl("/bin/sh", "sh", NULL);
    }
}

static void init_ctx(void)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.flags = 0x1;
    ctx.retries = MAX_RETRIES;
    strncpy(ctx.version, DIAGNOSTIC_VERSION, sizeof(ctx.version) - 1);
}

static void print_header(void)
{
    printf("\033[0;33m");
    printf("  [ONO-SENDAI VII] Cyberspace deck online.\n");
    printf("  [ONO-SENDAI VII] Diagnostic subsystem v%s\n", ctx.version);
    printf("  [ONO-SENDAI VII] Waiting for operator ID:\033[0m ");
    fflush(stdout);
}

static void run_diagnostic(void)
{
    char op_id[OPERATOR_LEN];

    print_header();
    read(STDIN_FILENO, op_id, 256);

    strncpy(ctx.last_op, op_id, OPERATOR_LEN - 1);

    if (strncmp(op_id, "ONO_", 4) == 0) {
        printf("[ONO-SENDAI VII] Operator recognized. Running diagnostics...\n");
    } else {
        printf("[ONO-SENDAI VII] Unknown operator. Logging attempt.\n");
        ctx.retries--;
        if (ctx.retries == 0) {
            printf("[ONO-SENDAI VII] Max retries exceeded. Shutting down.\n");
            exit(1);
        }
    }
}

int main(void)
{
    init_ctx();
    run_diagnostic();
    return 0;
}
