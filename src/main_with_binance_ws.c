/*  binance_ws.c
 *
 *  Minimal C program that connects to Binance WebSocket
 *  and prints the first 5 messages from the !ticker@arr stream.
 *
 *  Compile:
 *      gcc -Wall -O2 -o binance_ws binance_ws.c -lwebsockets -lssl -lcrypto
 *
 *  Run:
 *      ./binance_ws
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <libwebsockets.h>

#include <stdio.h>
#include <time.h>

#include "../include/orderbook.h"

void log_ms(const char *fmt, ...) {
    struct timespec ts;
    struct tm timeinfo;
    char time_buffer[64];
    char msg_buffer[1024];  // buffer for user message
    char final_buffer[1200]; // buffer for full log line

    // Get timestamp
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &timeinfo);
    strftime(time_buffer, sizeof(time_buffer), "%Y/%m/%d %H:%M:%S", &timeinfo);
    long subsec = ts.tv_nsec / 100000;  // 4-digit sub-second

    // Format user message
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    // Combine timestamp + message
    snprintf(final_buffer, sizeof(final_buffer), "[%s:%04ld] %s", time_buffer, subsec, msg_buffer);

    // Print all at once
    printf("%s", final_buffer);
}

int
orderbook_update(const char* depth_json) {
//    log_ms("<<< Orderbook update %s\n ", (char *)depth_json);
    OrderBook* ob = parse_orderbook_snapshot(depth_json);
    if (ob) {
        print_orderbook(ob);
        free_orderbook(ob);
        return 0;
    } else {
        printf("Failed to parse order book\n");
        return 1;
    }
}
/* ------------------------------------------------------------------ */
/*  Global state                                                      */
static int msg_count = 0;

/* ------------------------------------------------------------------ */
/*  Callback – called by libwebsockets for every event on the socket. */
static int
callback_binance(struct lws *wsi,
                 enum lws_callback_reasons reason,
                 void *user, void *in, size_t len)
{
    switch (reason) {

    /* The connection is established – send the subscription JSON */
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            log_ms(">>> Connection established\n");
        }
        break;

    /* We received a message – print it */
    case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            /* The payload is in 'in', length 'len' */
            log_ms("<<< %.*s\n", (int)len, (char *)in);
            msg_count++;
            orderbook_update((char *)in);
        }
        break;

    /* The server closed the connection – exit the loop */
    case LWS_CALLBACK_CLIENT_CLOSED:
        printf("Connection closed by server.\n");
        lws_cancel_service(lws_get_context(wsi));
        break;

    /* We are asked to write – send a ping to keep the connection alive */
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        {
            /* Send a ping frame */
            unsigned char ping_buf[LWS_PRE + 1];
            unsigned char *p = &ping_buf[LWS_PRE];
            *p = 0; /* ping payload length 0 */
            log_ms("Sending ping\n");
            lws_write(wsi, p, 1, LWS_WRITE_PING);
        }
        break;

    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  libwebsockets protocol descriptor */
static struct lws_protocols protocols[] = {
    {
        .name = "binance-protocol",
        .callback = callback_binance,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0,
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

/* ------------------------------------------------------------------ */
/*  Signal handler – allow Ctrl‑C to exit cleanly */
static void
sigint_handler(int sig)
{
    (void)sig;
    printf("\nInterrupted – shutting down.\n");
    exit(0);
}


/* ------------------------------------------------------------------ */
int main(void)
{
    signal(SIGINT, sigint_handler);

    //FOr additional debugging
    // lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, NULL);

    log_ms("Connecting to Binance WebSocket...\n");
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;          /* we are a client only */
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "lws_create_context failed\n");
        return 1;
    }

    const char *url = "stream.binance.com";

    /* 2. Connect to Binance WebSocket */
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = "stream.binance.com";
    ccinfo.port = 9443;
    ccinfo.path = "/ws/btcusdt@depth5";
    ccinfo.host = url;
    ccinfo.origin = url;
    ccinfo.protocol = protocols[0].name;
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
    ccinfo.pwsi = NULL;          /* will be set by libwebsockets */

    if (!lws_client_connect_via_info(&ccinfo)) {
        fprintf(stderr, "lws_client_connect_via_info failed\n");
        lws_context_destroy(context);
        return 1;
    }
    log_ms("Connecting to Binance WebSocket...\n");

    /* 3. Service loop – blocks until we exit */
    while (lws_service(context, 1000) >= 0) {
        /* The loop will exit when we call lws_cancel_service() */
   }

    log_ms("something went wrong...\n");

    lws_context_destroy(context);
    return 0;
}