#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD   160
#define GROUP_N   3
#define HDR_LEN   8
#define PKT_LEN   (HDR_LEN + PAYLOAD)
#define TYPE_DATA 0
#define TYPE_FEC  1

static void put_header(unsigned char *buf, uint32_t seq, uint8_t type, uint8_t group_n) {
    uint32_t seq_be = htonl(seq);
    memcpy(buf, &seq_be, 4);
    buf[4] = type;
    buf[5] = group_n;
    buf[6] = 0;
    buf[7] = 0;
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char in_buf[2048];
    unsigned char out_pkt[PKT_LEN];
    unsigned char parity[PAYLOAD];
    memset(parity, 0, PAYLOAD);
    uint32_t group_start = 0;
    int in_group = 0;

    for (;;) {
        ssize_t n = recvfrom(in_fd, in_buf, sizeof in_buf, 0, NULL, NULL);
        if (n < 4 + PAYLOAD) continue;
        uint32_t seq;
        memcpy(&seq, in_buf, 4);
        seq = ntohl(seq);
        unsigned char *payload = in_buf + 4;

        if (in_group == 0) group_start = seq;

        put_header(out_pkt, seq, TYPE_DATA, 0);
        memcpy(out_pkt + HDR_LEN, payload, PAYLOAD);
        sendto(out_fd, out_pkt, PKT_LEN, 0, (struct sockaddr *)&relay, sizeof relay);

        for (int j = 0; j < PAYLOAD; j++) parity[j] ^= payload[j];
        in_group++;

        if (in_group == GROUP_N) {
            put_header(out_pkt, group_start, TYPE_FEC, GROUP_N);
            memcpy(out_pkt + HDR_LEN, parity, PAYLOAD);
            sendto(out_fd, out_pkt, PKT_LEN, 0, (struct sockaddr *)&relay, sizeof relay);
            memset(parity, 0, PAYLOAD);
            in_group = 0;
        }
    }
    return 0;
}
