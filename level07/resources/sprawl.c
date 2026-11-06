#include <stdio.h>

typedef struct {
    unsigned int   tag;          // must be 0xdeadbeef
    unsigned short count;
    unsigned short block_size;
} header_t;

typedef struct {
    unsigned int   tag;
    unsigned short count;
    unsigned short block_size;
    unsigned int   checksum;
    unsigned int   _pad;         // never written, always 0 (.bss)
    unsigned long  reserved;     // explicitly zeroed
} packet_t;                      // sizeof == 24 (0x18)

#define MAX_PACKETS 32            // 0x20

static packet_t     packets[MAX_PACKETS];
static unsigned int packet_count;

static void print_banner(void)
{
    printf("\033[0;36m");
    puts("  [THE SPRAWL] BostonAtlantaNewYork online.");
    puts("  [SPRAWL] Packet relay node v4.2");
    printf("\033[0m");
}

static unsigned int calc_checksum(const unsigned char *buf, unsigned long len)
{
    unsigned int sum = 0;
    for (unsigned long i = 0; i < len; i++)
        sum += buf[i];
    return sum;
}

static void route_tag(void)
{
    char tag[136];

    printf("[SPRAWL] Route tag: ");
    fflush(stdout);

    if (fgets(tag, 0x80, stdin) != NULL) {
        printf("[SPRAWL] Routing via ");
        printf(tag);           // format string bug: user data as the format itself
        fflush(stdout);
    }
}

static int read_header(header_t *hdr)
{
    printf("[SPRAWL] Header (hex): ");
    fflush(stdout);

    if (scanf("%x %hx %hx", &hdr->tag, &hdr->count, &hdr->block_size) != 3)
        return -1;

    if (hdr->tag != 0xdeadbeef)
        return -1;

    return 0;
}

static void process_packet(void)
{
    header_t      hdr;
    unsigned char buf[72];
    unsigned short size_check;
    unsigned int   checksum;

    if (read_header(&hdr) < 0) {
        puts("[SPRAWL] Invalid header.");
        return;
    }

    size_check = hdr.block_size * hdr.count;   // truncated to 16 bits on assignment
    if (size_check >= 0x41) {
        puts("[SPRAWL] Frame too large for relay buffer.");
        return;
    }

    printf("[SPRAWL] Transmit (%u bytes): ", size_check);
    fflush(stdout);
    getchar();

    fread(buf, 1, hdr.count, stdin);    // <-- reads hdr.count bytes, NOT size_check!

    unsigned short csum_len = size_check > 0x40 ? 0x40 : size_check;
    checksum = calc_checksum(buf, csum_len);

    if (packet_count < MAX_PACKETS) {
        packets[packet_count].tag        = hdr.tag;
        packets[packet_count].count      = hdr.count;
        packets[packet_count].block_size = hdr.block_size;
        packets[packet_count].checksum   = checksum;
        packets[packet_count].reserved   = 0;
        packet_count++;
    }

    printf("[SPRAWL] Packet %u received. Checksum: %08x\n", packet_count - 1, checksum);
}

static void dump_packets(void)
{
    for (unsigned int i = 0; i < packet_count; i++) {
        printf("[SPRAWL] Packet %u: count=%u block_size=%u checksum=%08x\n",
            i, packets[i].count, packets[i].block_size, packets[i].checksum);
    }
}

int main(void)
{
    print_banner();
    route_tag();
    process_packet();
    dump_packets();
    return 0;
}