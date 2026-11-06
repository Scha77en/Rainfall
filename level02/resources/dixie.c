#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 64
#define MAX_RECORDS 16

typedef struct {
    unsigned int id;
    unsigned int checksum;
    char data[BUF_SIZE];
} record_t;

static record_t records[MAX_RECORDS];
static unsigned int record_count = 0;
static size_t in_len = 0;
static int in_ch = 0;

static void init_records(void)
{
    memset(records, 0, sizeof(records));
}

static unsigned int compute_checksum(const char *data, size_t len)
{
    unsigned int sum = 0x5a5a5a5a;
    for (size_t i = 0; i < len; i++)
        sum ^= ((unsigned int)data[i] << (i % 24));
    return sum;
}

static void print_banner(void)
{
    printf("\033[0;35m");
    printf("  [FLATLINE] The Dixie Flatline lives again.\n");
    printf("  [FLATLINE] ROM construct v3.0 — memory interface ready.\n");
    printf("\033[0m");
}

static void commit_record(const char *buf, size_t len)
{
    records[record_count].id = record_count;
    records[record_count].checksum = compute_checksum(buf, len);
    memcpy(records[record_count].data, buf, BUF_SIZE);
    record_count++;

    printf("[FLATLINE] Record %u stored. Checksum: %08x\n",
        record_count - 1, records[record_count - 1].checksum);
}

static void store_record(void)
{
    char buf[BUF_SIZE];

    if (record_count >= MAX_RECORDS) {
        printf("[FLATLINE] Record buffer full.\n");
        return;
    }

    printf("[FLATLINE] Ready: ");
    fflush(stdout);

    /* Off-by-one: the index runs 0..BUF_SIZE inclusive (<=), so a full
     * BUF_SIZE-byte read still lets the trailing pass write buf[BUF_SIZE].
     * buf is store_record's only local (counter/char are file-scope), so
     * gcc lays it out at rbp-BUF_SIZE with no padding: buf[BUF_SIZE] is the
     * low byte of the saved RBP slot.  One byte too far == one wrong move. */
    in_len = 0;
    while (in_len <= BUF_SIZE) {
        in_ch = read(0, &buf[in_len], 1);
        if (in_ch <= 0)
            break;
        in_len++;
    }

    commit_record(buf, in_len < BUF_SIZE ? in_len : BUF_SIZE);
}

static void dump_records(void)
{
    for (unsigned int i = 0; i < record_count; i++) {
        printf("[FLATLINE] Record %u: %.64s (checksum: %08x)\n",
            records[i].id, records[i].data, records[i].checksum);
    }
}

int main(void)
{
    char session[16] = "dixie";

    init_records();
    print_banner();
    store_record();
    dump_records();
    printf("\033[0;35m  [FLATLINE] %s session closed.\033[0m\n", session);
    return 0;
}
