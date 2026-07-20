#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD    160
#define GROUP_N    3
#define HDR_LEN    8
#define PKT_LEN    (HDR_LEN + PAYLOAD)
#define TYPE_DATA  0
#define TYPE_FEC   1
#define MAX_GROUPS 20000

typedef struct {
    uint8_t received_mask;
    uint8_t forwarded_mask;
    uint8_t have_fec;
    uint8_t count;
    unsigned char xor_buf[PAYLOAD];
    unsigned char fec_buf[PAYLOAD];
} group_t;

static group_t groups[MAX_GROUPS];

static void get_header(const unsigned char *buf, uint32_t *seq, uint8_t *type, uint8_t *group_n) {
    uint32_t seq_be;
    memcpy(&seq_be, buf, 4);
    *seq = ntohl(seq_be);
    *type = buf[4];
    *group_n = buf[5];
}

static void forward(int out_fd, struct sockaddr_in *player, uint32_t seq, const unsigned char *payload) {
    unsigned char pkt[4 + PAYLOAD];
    uint32_t seq_be = htonl(seq);
    memcpy(pkt, &seq_be, 4);
    memcpy(pkt + 4, payload, PAYLOAD);
    sendto(out_fd, pkt, sizeof pkt, 0, (struct sockaddr *)player, sizeof *player);
}

static void try_reconstruct(group_t *g, uint32_t group_start, int out_fd, struct sockaddr_in *player) {
    if (!g->have_fec || g->count != GROUP_N - 1) return;
    for (int i = 0; i < GROUP_N; i++) {
        if (!(g->received_mask & (1 << i))) {
            unsigned char recon[PAYLOAD];
            for (int j = 0; j < PAYLOAD; j++) recon[j] = g->fec_buf[j] ^ g->xor_buf[j];
            uint32_t missing_seq = group_start + i;
            if (!(g->forwarded_mask & (1 << i))) {
                forward(out_fd, player, missing_seq, recon);
                g->forwarded_mask |= (1 << i);
            }
            break;
        }
    }
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(groups, 0, sizeof groups);
    unsigned char buf[2048];

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < PKT_LEN) continue;
        uint32_t seq; uint8_t type, group_n;
        get_header(buf, &seq, &type, &group_n);
        unsigned char *payload = buf + HDR_LEN;

        if (type == TYPE_DATA) {
            uint32_t group_id = seq / GROUP_N;
            uint32_t group_start = group_id * GROUP_N;
            int offset = seq - group_start;
            group_t *g = &groups[group_id % MAX_GROUPS];

            if (!(g->received_mask & (1 << offset))) {
                g->received_mask |= (1 << offset);
                for (int j = 0; j < PAYLOAD; j++) g->xor_buf[j] ^= payload[j];
                g->count++;
            }
            if (!(g->forwarded_mask & (1 << offset))) {
                forward(out_fd, &player, seq, payload);
                g->forwarded_mask |= (1 << offset);
            }
            try_reconstruct(g, group_start, out_fd, &player);

        } else if (type == TYPE_FEC) {
            uint32_t group_start = seq;
            uint32_t group_id = group_start / GROUP_N;
            group_t *g = &groups[group_id % MAX_GROUPS];
            if (!g->have_fec) {
                memcpy(g->fec_buf, payload, PAYLOAD);
                g->have_fec = 1;
            }
            try_reconstruct(g, group_start, out_fd, &player);
        }
    }
    return 0;
}
