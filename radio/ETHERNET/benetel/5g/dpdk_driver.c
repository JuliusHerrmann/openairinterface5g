/* license: to be defined */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "low.h"

// HEADER DEFINITION
#define ANT_NUM buf[23]
#define PAYLOAD_1 buf[20]
#define PAYLOAD_2 buf[21]
#define ETH_TYPE buf[17]
#define SYMBOL buf[29]
#define SUBFRAME buf[28]
#define FRAME buf[27]

static volatile bool force_quit;

unsigned short iq[712320];
unsigned char iq_swap[1424640];

unsigned short *iq_ptr[14];
unsigned int dl_start = 0, slot_id_ctrl = 0, count_symbol = 0, sf = 0x10;

/* MAC updating enabled by default */
static int mac_updating = 1;
#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1
#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 5 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256
/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;
/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];
/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;
/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];
static unsigned int l2fwd_rx_queue_per_lcore = 1;
#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
        unsigned n_rx_port;
        unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];
static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];
static struct rte_eth_conf port_conf = {
    .rxmode =
        {
            .split_hdr_size = 0,
            .offloads = DEV_RX_OFFLOAD_JUMBO_FRAME,
            .split_hdr_size = 0,
            .max_rx_pkt_len = 9500,
        },
    .txmode =
        {
            .mq_mode = ETH_MQ_TX_NONE,
        },
};
struct rte_mempool * l2fwd_pktmbuf_pool = NULL;
/* Per-port statistics struct */
struct l2fwd_port_statistics {
        uint64_t tx;
        uint64_t rx;
        uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 10; /* default period is 10 seconds */
/* Print out statistics on packets dropped */
static void
print_stats(void)
{
        uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
        unsigned portid;
        total_packets_dropped = 0;
        total_packets_tx = 0;
        total_packets_rx = 0;
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };
                /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);
        printf("\nPort statistics ====================================");
        for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
                /* skip disabled ports */
                if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
                        continue;
                printf("\nStatistics for port %u ------------------------------"
                           "\nPackets sent: %24"PRIu64
                           "\nPackets received: %20"PRIu64
                           "\nPackets dropped: %21"PRIu64,
                           portid,
                           port_statistics[portid].tx,
                           port_statistics[portid].rx,
                           port_statistics[portid].dropped);
                total_packets_dropped += port_statistics[portid].dropped;
                total_packets_tx += port_statistics[portid].tx;
                total_packets_rx += port_statistics[portid].rx;
        }
        printf("\nAggregate statistics ==============================="
                   "\nTotal packets sent: %18"PRIu64
                   "\nTotal packets received: %14"PRIu64
                   "\nTotal packets dropped: %15"PRIu64,
                   total_packets_tx,
                   total_packets_rx,
                   total_packets_dropped);
        printf("\n====================================================\n");
}
static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid, benetel_t *bs)
{
        unsigned char *buf, *buf_tx;
        unsigned int len;
        struct ether_hdr *eth;

        unsigned dst_port;
        int sent, prach_ctrl = 0;
        struct rte_eth_dev_tx_buffer *buffer;

        unsigned char *IQ_ptr;

        ul_packet_t p;

        buf = rte_pktmbuf_mtod(m, unsigned char *);
        // DD Debug
        // rte_memdump(stdout, "Rx Packet",buf ,34);
        eth = rte_pktmbuf_mtod(m, struct ether_hdr *);
        len = rte_pktmbuf_data_len(m);

        IQ_ptr = &buf[34];
        if (portid == 0) {
                // destination mac address (RRU MAC)
                /*eth->d_addr.addr_bytes[0] = 0x02;
                eth->d_addr.addr_bytes[1] = 0x00;
                eth->d_addr.addr_bytes[2] = 0x5e;
                eth->d_addr.addr_bytes[3] = 0x01;
                eth->d_addr.addr_bytes[4] = 0x01;
                eth->d_addr.addr_bytes[5] = 0x01;*/

                eth->d_addr.addr_bytes[0] = buf[6];
                eth->d_addr.addr_bytes[1] = buf[7];
                eth->d_addr.addr_bytes[2] = buf[8];
                eth->d_addr.addr_bytes[3] = buf[9];
                eth->d_addr.addr_bytes[4] = buf[10];
                eth->d_addr.addr_bytes[5] = buf[11];

                // source mac address (DU MAC)
                eth->s_addr.addr_bytes[0] = 0xdd;
                eth->s_addr.addr_bytes[1] = 0xdd;
                eth->s_addr.addr_bytes[2] = 0xdd;
                eth->s_addr.addr_bytes[3] = 0xdd;
                eth->s_addr.addr_bytes[4] = 0xdd;
                eth->s_addr.addr_bytes[5] = 0xdd;
        } else {
                // destination mac address (RRU MAC)
                /*eth->d_addr.addr_bytes[0] = 0x02;
                eth->d_addr.addr_bytes[1] = 0x00;
                eth->d_addr.addr_bytes[2] = 0x5e;
                eth->d_addr.addr_bytes[3] = 0x01;
                eth->d_addr.addr_bytes[4] = 0x01;
                eth->d_addr.addr_bytes[5] = 0x01;*/

                eth->d_addr.addr_bytes[0] = buf[6];
                eth->d_addr.addr_bytes[1] = buf[7];
                eth->d_addr.addr_bytes[2] = buf[8];
                eth->d_addr.addr_bytes[3] = buf[9];
                eth->d_addr.addr_bytes[4] = buf[10];
                eth->d_addr.addr_bytes[5] = buf[11];
                // source mac address (DU MAC)
                eth->s_addr.addr_bytes[0] = 0xdd;
                eth->s_addr.addr_bytes[1] = 0xdd;
                eth->s_addr.addr_bytes[2] = 0xdd;
                eth->s_addr.addr_bytes[3] = 0xdd;
                eth->s_addr.addr_bytes[4] = 0xdd;
                eth->s_addr.addr_bytes[5] = 0xde;
        }
        dst_port = l2fwd_dst_ports[portid];
        // DD Debug
        // printf("dstPort =%d\n", dst_port);
        //  Handshake Condition
        if (buf[18] == 0xAA) // ORAN handshake msgs.
        {
                printf("\n Start UP Request Received\n");
                printf("\n====================================================\n");

                buf[18] = 0xBB; // change to msg2 after receive msg1

                printf(" Start Up Response Sent\n");
                printf("\n====================================================\n");
        }

        // Trigger start send DL packets
        if (PAYLOAD_1 == 0x13 && PAYLOAD_2 == 0xe4 && SYMBOL == 0x44 && ANT_NUM == 0x00 && SUBFRAME == 0x00 && dl_start == 0) {
                printf("\nU-Plane Started\n");
                printf("\n====================================================\n");

                dl_start = 1;
                count_symbol = 28;
        }

        if (PAYLOAD_1 == 0x13 && PAYLOAD_2 == 0xe4) {
                int subframe = SUBFRAME >> 4;
                int slot = ((SUBFRAME & 0x0f) << 2) | ((SYMBOL >> 6) & 0x03);
                p.frame = FRAME;
                p.slot = subframe * 2 + slot;
                p.symbol = SYMBOL & 0x3f;
                p.antenna = ANT_NUM;
                memcpy(p.iq, IQ_ptr, 5088);
                store_ul(bs, &p);
                //     if (p.symbol==0) printf("store ul f.sl.sy %d.%d.%d\n", p.frame, p.slot, p.symbol);
        }

        // U-PLANE UL PROCESSING
        if (PAYLOAD_1 == 0x13 && PAYLOAD_2 == 0xe4 && dl_start == 1) {
                int a = ANT_NUM & 0x01;
                int frame = FRAME;
                int subframe = SUBFRAME >> 4;
                int slot = ((SUBFRAME & 0x0f) << 2) | ((SYMBOL >> 6) & 0x03);
                int symbol = SYMBOL & 0x3f;
                int oai_slot;

                int tx_frame = frame;
                int tx_subframe = subframe;
                int tx_slot = slot;
                int tx_symbol = symbol + 10;

                if (tx_symbol > 13) {
                        tx_symbol -= 14;
                        tx_slot++;
                        if (tx_slot == 2) {
                          tx_slot = 0;
                          tx_subframe++;
                          if (tx_subframe == 10) {
                            tx_subframe = 0;
                            tx_frame = (tx_frame + 1) & 255;
                          }
                        }
                }

                // Mask the symbol bits from UL packet received and apply the shift.
                SYMBOL = ((SYMBOL & 0b00111111) + 10) % 14;

                ANT_NUM = a;
                SUBFRAME = sf;

                if (a == 1)
                        slot_id_ctrl++;

                // Slot id control for DL
                if (slot_id_ctrl > 13) {
                        SYMBOL = SYMBOL | 0b01000000;

                        if (a == 1) {
                          if (slot_id_ctrl > 27) {
                            slot_id_ctrl = 0;
                            sf = sf + 0x10;

                            if (sf > 0x90) {
                              sf = 0;
                            }
                          }
                        }
                }

                /* send actual DL data (if available) */
                oai_slot = tx_subframe * 2 + tx_slot;
                lock_dl_buffer(bs->buffers, oai_slot);
                if (!(bs->buffers->dl_busy[a][oai_slot] & (1 << tx_symbol))) {
                        // printf("%s: warning, DL underflow (antenna %d sl.symbol %d.%d)\n", __FUNCTION__,a, oai_slot, tx_symbol);
                        memset(IQ_ptr, 0, 1272 * 4);
                } else {
                        memcpy(IQ_ptr, bs->buffers->dl[a][oai_slot] + tx_symbol * 1272 * 4, 1272 * 4);
                }
                // printf("DL buffer f sf slot symbol %d %d %d %d (sf %d)\n", tx_frame, tx_subframe, tx_slot, tx_symbol, (int)sf);
                bs->buffers->dl_busy[a][oai_slot] &= ~(1 << tx_symbol);
                unlock_dl_buffer(bs->buffers, oai_slot);
        }

        // U-PLANE PRACH PROCESSING
        else if (PAYLOAD_1 == 0x0d && PAYLOAD_2 == 0x28) {
                if (ANT_NUM == 0) {
                        int subframe = SUBFRAME >> 4;
                        // int slot = ((SUBFRAME & 0x0f) << 2) | ((SYMBOL >> 6) & 0x03);
                        if (subframe == 9) {
                          // printf("store prach f.sf.sl %d.%d.%d %d\n", FRAME, subframe, slot, subframe * 2 + slot);
                          store_prach(bs, FRAME, subframe * 2 + 1, IQ_ptr);
                        }
                }
                rte_pktmbuf_free(m);
                return;
        }

        // Send Packets
        buffer = tx_buffer[dst_port];

        sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);

        if (sent)
                port_statistics[dst_port].tx += sent;
}
/* main processing loop */
static void
l2fwd_main_loop(benetel_t *bs)
{
        struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
        struct rte_mbuf *m;
        int sent;
        unsigned lcore_id;
        uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
        unsigned i, j, portid, nb_rx;
        struct lcore_queue_conf *qconf;
        const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
                        BURST_TX_DRAIN_US;
        struct rte_eth_dev_tx_buffer *buffer;
        prev_tsc = 0;
        timer_tsc = 0;
        lcore_id = rte_lcore_id();
        qconf = &lcore_queue_conf[lcore_id];
        if (qconf->n_rx_port == 0) {
                RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
                return;
        }
        RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);
        for (i = 0; i < qconf->n_rx_port; i++) {
                portid = qconf->rx_port_list[i];
                RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
                        portid);
        }
        while (!force_quit) {
                cur_tsc = rte_rdtsc();
                /*
                 * TX burst queue drain
                 */
                diff_tsc = cur_tsc - prev_tsc;
                if (unlikely(diff_tsc > drain_tsc)) {
                        for (i = 0; i < qconf->n_rx_port; i++) {
                                portid = l2fwd_dst_ports[qconf->rx_port_list[i]];
                                buffer = tx_buffer[portid];
                                sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
                                if (sent)
                                        port_statistics[portid].tx += sent;
                        }
                        /* if timer is enabled */
                        if (timer_period > 0) {
                                /* advance the timer */
                                timer_tsc += diff_tsc;
                                /* if timer has reached its timeout */
                                if (unlikely(timer_tsc >= timer_period)) {
                                        /* do this only on master core */
                                        if (lcore_id == rte_get_master_lcore()) {
                                                print_stats();
                                                /* reset the timer */
                                                timer_tsc = 0;
                                        }
                                }
                        }
                        prev_tsc = cur_tsc;
                }
                /*
                 * Read packet from RX queues
                 */
                for (i = 0; i < qconf->n_rx_port; i++) {
                        portid = qconf->rx_port_list[i];
                        nb_rx = rte_eth_rx_burst(portid, 0,
                                                 pkts_burst, MAX_PKT_BURST);
                        port_statistics[portid].rx += nb_rx;
                        for (j = 0; j < nb_rx; j++) {
                                m = pkts_burst[j];
                                rte_prefetch0(rte_pktmbuf_mtod(m, void *));
                                l2fwd_simple_forward(m, portid, bs);
                        }
                }
        }
}
static int
l2fwd_launch_one_lcore(void *bs)
{
        l2fwd_main_loop(bs);
        return 0;
}
/* display usage */
static void
l2fwd_usage(const char *prgname)
{
        printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
               "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
               "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
                   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n"
                   "  --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default)\n"
                   "      When enabled:\n"
                   "       - The source MAC address is replaced by the TX port MAC address\n"
                   "       - The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID\n",
               prgname);
}
static int
l2fwd_parse_portmask(const char *portmask)
{
        char *end = NULL;
        unsigned long pm;
        /* parse hexadecimal string */
        pm = strtoul(portmask, &end, 16);
        if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
                return -1;
        if (pm == 0)
                return -1;
        return pm;
}
static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
{
        char *end = NULL;
        unsigned long n;
        /* parse hexadecimal string */
        n = strtoul(q_arg, &end, 10);
        if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
                return 0;
        if (n == 0)
                return 0;
        if (n >= MAX_RX_QUEUE_PER_LCORE)
                return 0;
        return n;
}
static int
l2fwd_parse_timer_period(const char *q_arg)
{
        char *end = NULL;
        int n;
        /* parse number string */
        n = strtol(q_arg, &end, 10);
        if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
                return -1;
        if (n >= MAX_TIMER_PERIOD)
                return -1;
        return n;
}
static const char short_options[] =
        "p:"  /* portmask */
        "q:"  /* number of queues */
        "T:"  /* timer period */
        ;
#define CMD_LINE_OPT_MAC_UPDATING "mac-updating"
#define CMD_LINE_OPT_NO_MAC_UPDATING "no-mac-updating"
enum {
        /* long options mapped to a short option */
        /* first long only option value must be >= 256, so that we won't
         * conflict with short options */
        CMD_LINE_OPT_MIN_NUM = 256,
};
static const struct option lgopts[] = {
        { CMD_LINE_OPT_MAC_UPDATING, no_argument, &mac_updating, 1},
        { CMD_LINE_OPT_NO_MAC_UPDATING, no_argument, &mac_updating, 0},
        {NULL, 0, 0, 0}
};
/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
        int opt, ret, timer_secs;
        char **argvopt;
        int option_index;
        char *prgname = argv[0];
        argvopt = argv;
        while ((opt = getopt_long(argc, argvopt, short_options,
                                  lgopts, &option_index)) != EOF) {
                switch (opt) {
                /* portmask */
                case 'p':
                        l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
                        if (l2fwd_enabled_port_mask == 0) {
                                printf("invalid portmask\n");
                                l2fwd_usage(prgname);
                                return -1;
                        }
                        break;
                /* nqueue */
                case 'q':
                        l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
                        if (l2fwd_rx_queue_per_lcore == 0) {
                                printf("invalid queue number\n");
                                l2fwd_usage(prgname);
                                return -1;
                        }
                        break;
                /* timer period */
                case 'T':
                        timer_secs = l2fwd_parse_timer_period(optarg);
                        if (timer_secs < 0) {
                                printf("invalid timer period\n");
                                l2fwd_usage(prgname);
                                return -1;
                        }
                        timer_period = timer_secs;
                        break;
                /* long options */
                case 0:
                        break;
                case 'proc-type':
                        break;
                case 'socket-mem':
                        break;
                case 'file-prefix':
                        l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
                        if (l2fwd_rx_queue_per_lcore == 0) {
                                printf("invalid queue number\n");
                                l2fwd_usage(prgname);
                                return -1;
                        }
                        break;
                /* timer period */
                /* timer period */
                /* timer period */
                default:
                        l2fwd_usage(prgname);
                        return -1;
                }
        }
        if (optind >= 0)
                argv[optind-1] = prgname;
        ret = optind-1;
        optind = 1; /* reset getopt lib */
        return ret;
}
/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
        uint16_t portid;
        uint8_t count, all_ports_up, print_flag = 0;
        struct rte_eth_link link;
        printf("\nChecking link status");
        fflush(stdout);
        for (count = 0; count <= MAX_CHECK_TIME; count++) {
                if (force_quit)
                        return;
                all_ports_up = 1;
                RTE_ETH_FOREACH_DEV(portid) {
                        if (force_quit)
                                return;
                        if ((port_mask & (1 << portid)) == 0)
                                continue;
                        memset(&link, 0, sizeof(link));
                        rte_eth_link_get_nowait(portid, &link);
                        /* print link status if flag set */
                        if (print_flag == 1) {
                                if (link.link_status)
                                        printf(
                                        "Port%d Link Up. Speed %u Mbps - %s\n",
                                                portid, link.link_speed,
                                (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
                                        ("full-duplex") : ("half-duplex\n"));
                                else
                                        printf("Port %d Link Down\n", portid);
                                continue;
                        }
                        /* clear all_ports_up flag if any link down */
                        if (link.link_status == ETH_LINK_DOWN) {
                                all_ports_up = 0;
                                break;
                        }
                }
                /* after finally printing all link status, get out */
                if (print_flag == 1)
                        break;
                if (all_ports_up == 0) {
                        printf(".");
                        fflush(stdout);
                        rte_delay_ms(CHECK_INTERVAL);
                }
                /* set the print_flag if all ports up or timeout */
                if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
                        print_flag = 1;
                        printf("done\n");
                }
        }
}
static void
signal_handler(int signum)
{
        if (signum == SIGINT || signum == SIGTERM) {
                printf("\n\nSignal %d received, preparing to exit...\n",
                                signum);
                force_quit = true;
        }
}
int
dpdk_main(int argc, char **argv, benetel_t *bs)
{
        struct lcore_queue_conf *qconf;
        int ret;
        uint16_t nb_ports;
        uint16_t nb_ports_available = 0;
        uint16_t portid, last_port;
        unsigned lcore_id, rx_lcore_id;
        unsigned nb_ports_in_mask = 0;
        unsigned int nb_lcores = 0;
        unsigned int nb_mbufs;
        /* init EAL */
        ret = rte_eal_init(argc, argv);
        if (ret < 0)
                rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
        argc -= ret;
        argv += ret;
        force_quit = false;
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        /* parse application arguments (after the EAL ones) */
        ret = l2fwd_parse_args(argc, argv);
        if (ret < 0)
                rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");
        printf("MAC updating %s\n", mac_updating ? "enabled" : "disabled");
        /* convert to number of cycles */
        timer_period *= rte_get_timer_hz();
        nb_ports = rte_eth_dev_count_avail();
        if (nb_ports == 0)
                rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
        /* check port mask to possible port mask */
        if (l2fwd_enabled_port_mask & ~((1 << nb_ports) - 1))
                rte_exit(EXIT_FAILURE, "Invalid portmask; possible (0x%x)\n",
                        (1 << nb_ports) - 1);
        /* reset l2fwd_dst_ports */
        for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
                l2fwd_dst_ports[portid] = 0;
        last_port = 0;
        /*
         * Each logical core is assigned a dedicated TX queue on each port.
         */
        RTE_ETH_FOREACH_DEV(portid) {
                /* skip ports that are not enabled */
                if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
                        continue;
                if (nb_ports_in_mask % 2) {
                        l2fwd_dst_ports[portid] = last_port;
                        l2fwd_dst_ports[last_port] = portid;
                }
                else
                        last_port = portid;
                nb_ports_in_mask++;
        }
        if (nb_ports_in_mask % 2) {
                printf("Notice: odd number of ports in portmask.\n");
                l2fwd_dst_ports[last_port] = last_port;
        }
        rx_lcore_id = 0;
        qconf = NULL;
        /* Initialize the port/queue configuration of each logical core */
        RTE_ETH_FOREACH_DEV(portid) {
                /* skip ports that are not enabled */
                if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
                        continue;
                /* get the lcore_id for this port */
                while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
                       lcore_queue_conf[rx_lcore_id].n_rx_port ==
                       l2fwd_rx_queue_per_lcore) {
                        rx_lcore_id++;
                        if (rx_lcore_id >= RTE_MAX_LCORE)
                                rte_exit(EXIT_FAILURE, "Not enough cores\n");
                }
                if (qconf != &lcore_queue_conf[rx_lcore_id]) {
                        /* Assigned a new logical core in the loop above. */
                        qconf = &lcore_queue_conf[rx_lcore_id];
                        nb_lcores++;
                }
                qconf->rx_port_list[qconf->n_rx_port] = portid;
                qconf->n_rx_port++;
                printf("Lcore %u: RX port %u\n", rx_lcore_id, portid);
        }
        nb_mbufs = RTE_MAX(nb_ports * (nb_rxd + nb_txd + MAX_PKT_BURST +
                nb_lcores * MEMPOOL_CACHE_SIZE), 8192U);
        /* create the mbuf pool */
        // DD Debug
        // printf ("Proc Type %d \n", rte_eal_process_type());
        l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
                MEMPOOL_CACHE_SIZE, 0, 9680,
                rte_socket_id());
        if (l2fwd_pktmbuf_pool == NULL)
                rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
        /* Initialise each port */
        RTE_ETH_FOREACH_DEV(portid) {
                struct rte_eth_rxconf rxq_conf;
                struct rte_eth_txconf txq_conf;
                struct rte_eth_conf local_port_conf = port_conf;
                struct rte_eth_dev_info dev_info;
                /* skip ports that are not enabled */
                if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
                        printf("Skipping disabled port %u\n", portid);
                        continue;
                }
                nb_ports_available++;
                /* init port */
                printf("Initializing port %u... ", portid);
                fflush(stdout);
                rte_eth_dev_info_get(portid, &dev_info);
                if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
                        local_port_conf.txmode.offloads |=
                                DEV_TX_OFFLOAD_MBUF_FAST_FREE;
                ret = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
                if (ret < 0)
                        rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
                                  ret, portid);
                ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
                                                       &nb_txd);
                if (ret < 0)
                        rte_exit(EXIT_FAILURE,
                                 "Cannot adjust number of descriptors: err=%d, port=%u\n",
                                 ret, portid);
                rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);
                /* init one RX queue */
                fflush(stdout);
                rxq_conf = dev_info.default_rxconf;
                rxq_conf.offloads = local_port_conf.rxmode.offloads;
                ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
                                             rte_eth_dev_socket_id(portid),
                                             &rxq_conf,
                                             l2fwd_pktmbuf_pool);
                if (ret < 0)
                        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
                                  ret, portid);
                /* init one TX queue on each port */
                fflush(stdout);
                txq_conf = dev_info.default_txconf;
                txq_conf.offloads = local_port_conf.txmode.offloads;
                ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
                                rte_eth_dev_socket_id(portid),
                                &txq_conf);
                if (ret < 0)
                        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
                                ret, portid);
                /* Initialize TX buffers */
                tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
                                RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
                                rte_eth_dev_socket_id(portid));
                if (tx_buffer[portid] == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                                        portid);
                rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);
                ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
                                rte_eth_tx_buffer_count_callback,
                                &port_statistics[portid].dropped);
                if (ret < 0)
                        rte_exit(EXIT_FAILURE,
                        "Cannot set error callback for tx buffer on port %u\n",
                                 portid);
                /* Start device */
                ret = rte_eth_dev_start(portid);
                if (ret < 0)
                        rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
                                  ret, portid);
                printf("done: \n");
                rte_eth_promiscuous_enable(portid);
                printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
                                portid,
                                l2fwd_ports_eth_addr[portid].addr_bytes[0],
                                l2fwd_ports_eth_addr[portid].addr_bytes[1],
                                l2fwd_ports_eth_addr[portid].addr_bytes[2],
                                l2fwd_ports_eth_addr[portid].addr_bytes[3],
                                l2fwd_ports_eth_addr[portid].addr_bytes[4],
                                l2fwd_ports_eth_addr[portid].addr_bytes[5]);
                /* initialize port stats */
                memset(&port_statistics, 0, sizeof(port_statistics));
        }
        if (!nb_ports_available) {
                rte_exit(EXIT_FAILURE,
                        "All available ports are disabled. Please set portmask.\n");
        }
        check_all_ports_link_status(l2fwd_enabled_port_mask);
        ret = 0;
        /* launch per-lcore init on every lcore */
        rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, bs, CALL_MASTER);
        RTE_LCORE_FOREACH_SLAVE(lcore_id) {
                if (rte_eal_wait_lcore(lcore_id) < 0) {
                        ret = -1;
                        break;
                }
        }
        RTE_ETH_FOREACH_DEV(portid) {
                if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
                        continue;
                printf("Closing port %d...", portid);
                rte_eth_dev_stop(portid);
                rte_eth_dev_close(portid);
                printf(" Done\n");
        }
        printf("Bye...\n");
        return ret;
}
