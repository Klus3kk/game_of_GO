#include "client_net.h"
#include "client_state.h"
#include "client_proto.h"
#include <sys/select.h>
#include <stdio.h>

int ensure_connected(Settings *st) {
    if (net_ready) return 1;

    if (net_connect(&net, "127.0.0.1", 1984) < 0) return 0;
    net_ready = 1;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "NICK %s", st->nickname[0] ? st->nickname : "player");
    net_send_line(&net, cmd);

    net_send_line(&net, "SUB");
    net_send_line(&net, "GAMES");
    return 1;
}

void pump_network(Screen *screen) {
    if (!net_ready) return;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(net.fd, &rfds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int rc = select(net.fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) return;

    if (FD_ISSET(net.fd, &rfds)) {
        int ok = net_recv_into_buffer(&net);
        if (ok <= 0) {
            net_close(&net);
            net_ready = 0;
            return;
        }

        char line[1024];
        while (net_next_line(&net, line, sizeof(line))) {
            parse_server_line(line, screen);
        }
    }
}
