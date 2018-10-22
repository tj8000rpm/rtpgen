/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <time.h>

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
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>

static volatile bool force_quit;

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 10 /* TX drain every ~10us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];
/* ethernet addresses of peer ports */
static struct ether_addr l2fwd_ports_eth_peer_addr[RTE_MAX_ETHPORTS];

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

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.txmode = {
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

struct orig_rtp_hdr {
        uint16_t flags;
        uint16_t sequence;
        uint32_t timestamp;
        uint32_t ssrc;
} __attribute__((__packed__));

#define ORIG_RTP_MAX_SESSIONS 50000
struct orig_rtp_config {
	bool enabled;
	uint8_t portid;
	uint32_t ip_d_addr;
	uint32_t ip_s_addr;
	uint16_t udp_d_port;
	uint16_t udp_s_port;
	uint16_t rtp_timestamp;
	uint16_t rtp_sequence;
	uint16_t rtp_ssrc;
};

struct orig_rtp_config *rtp_configs_per_port[RTE_MAX_ETHPORTS];

#define ORIG_RTP_TX_INTERVAL_US 20000 /* TX rtp packet every ~20ms = ~20000us */
static unsigned int orig_sessions = 1;

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

// baseパケットの生成
static struct rte_mbuf *
orig_precreate_const_hdrs(struct rte_mbuf *m, uint16_t portid, uint16_t sessionid){

	struct ether_hdr *eth;
	struct ipv4_hdr *ip;
	struct udp_hdr *udp;
	uint16_t len;
	struct orig_rtp_config *rtp_conf;

	rtp_conf=rtp_configs_per_port[portid];

	len=m->data_len;
	// init UDP headers;
	udp=(struct udp_hdr *)rte_pktmbuf_prepend(m,sizeof(struct udp_hdr));
	udp->src_port=rtp_conf[sessionid].udp_s_port;
	udp->src_port=rtp_conf[sessionid].udp_d_port;
	udp->dgram_len=rte_be_to_cpu_16(len+sizeof(struct udp_hdr));
	udp->dgram_cksum=0;

	len=m->pkt_len;
	// init IP headers;
	ip=(struct ipv4_hdr *)rte_pktmbuf_prepend(m,sizeof(struct ipv4_hdr));
	ip->version_ihl=0x45;
	ip->type_of_service=0x05;
	ip->total_length=rte_be_to_cpu_16(len+sizeof(struct ipv4_hdr));
	ip->packet_id=0;
	ip->fragment_offset=rte_be_to_cpu_16(0x4000);
	ip->time_to_live=64;
	ip->next_proto_id=17;
	ip->hdr_checksum=0;
	ip->src_addr=rtp_conf[sessionid].ip_s_addr;
	ip->dst_addr=rtp_conf[sessionid].ip_d_addr;

	// init Ether headers;
	// mac-addr-copying
	eth=(struct ether_hdr *)rte_pktmbuf_prepend(m,sizeof(struct ether_hdr));
	ether_addr_copy(&l2fwd_ports_eth_peer_addr[portid], &eth->d_addr);
	ether_addr_copy(&l2fwd_ports_eth_addr[portid], &eth->s_addr);
	eth->ether_type = rte_be_to_cpu_16(ETHER_TYPE_IPv4);
	
	return m;
}

// 送信タイミングでのpacketの生成
static struct rte_mbuf *
orig_create_newpacket(uint16_t portid, uint16_t sessionid){
	uint16_t PAYLOAD_LEN = 160;
	struct orig_rtp_hdr *rtp;
	struct rte_mbuf *m;
	char *payload;
	int i;
	uint8_t rtp_version;
	uint8_t rtp_padding;
	uint8_t rtp_extention;
	uint8_t rtp_ccrc_count;
	uint8_t rtp_marker;
	uint8_t rtp_payload_type;
	uint32_t rtp_sequence;
	uint32_t rtp_timestamp;
	uint32_t rtp_ssrc;
	struct orig_rtp_config *rtp_conf;
	
	rtp_conf=rtp_configs_per_port[portid];

	// create mbuf as m
	m=rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);

	// add RTP headers;
	rtp=rte_pktmbuf_mtod(m,struct orig_rtp_hdr *);
	//rtp=(struct orig_rtp_hdr *)rte_pktmbuf_append(m,sizeof(struct orig_rtp_hdr));
	rtp_version=2;
	rtp_padding=0;
	rtp_extention=0;
	rtp_ccrc_count=0;
	rtp_marker=0;
	rtp_payload_type=0;
	rtp_sequence=rtp_conf[sessionid].rtp_sequence;
	rtp_timestamp=rtp_conf[sessionid].rtp_timestamp;
	rtp_ssrc=rtp_conf[sessionid].rtp_ssrc;
	rtp->flags=rte_be_to_cpu_16((rtp_version     &0x3 )<<14|
	                            (rtp_padding     &0x1 )<<13|
	                            (rtp_extention   &0x1 )<<12|
	                            (rtp_ccrc_count  &0xf )<<8|
	                            (rtp_marker      &0x1 )<<7|
	                            (rtp_payload_type&0x7f));
	rtp->sequence=rte_be_to_cpu_16(rtp_sequence);
	rtp->timestamp=rte_be_to_cpu_32(rtp_timestamp);
	rtp->ssrc=rte_be_to_cpu_32(rtp_ssrc);

	m->pkt_len =sizeof(struct orig_rtp_hdr);
	m->data_len=sizeof(struct orig_rtp_hdr);

	// definePayloads;
	payload=(char *)rte_pktmbuf_append(m,sizeof(char)*PAYLOAD_LEN);
	//payload=rte_pktmbuf_mtod(m, char *);
	for(i=0;i<PAYLOAD_LEN;i++)
		payload[i]=i&0xff;

	rtp_conf[sessionid].rtp_timestamp+=PAYLOAD_LEN;
	rtp_conf[sessionid].rtp_sequence+=1;

	return m;
}

// packetの生成
static void
orig_send_packet(struct rte_mbuf *m, unsigned portid)
{
	int sent;
	struct rte_eth_dev_tx_buffer *buffer;

	// 送信ポートのバッファを取得。
	buffer = tx_buffer[portid];
	sent = rte_eth_tx_buffer(portid, 0, buffer, m);
	// うまいこと言ったらsentに送信パケット数書いておわり。
	if (sent){
		port_statistics[portid].tx += sent;
		//force_quit=true;
	}
}

/* main processing loop */
static void
orig_main_loop(void)
{
	//struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	int sent;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc, orig_rtp_timer_tsc;
	//unsigned i, j, portid, nb_rx;
	unsigned i, portid, sessionid;
	struct lcore_queue_conf *qconf;

	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;

	prev_tsc = 0;
	timer_tsc = 0;
	orig_rtp_timer_tsc = 0;
	const uint64_t orig_rtp_tx_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			ORIG_RTP_TX_INTERVAL_US;

	lcore_id = rte_lcore_id();
	// coreごとのqueue情報の確認
	// LABEL:QCONF-UPDATEで変えた情報をここで参照
	// unsigned n_rx_port;
	// unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	qconf = &lcore_queue_conf[lcore_id];

	// EALオプションから指定されているが、
	// RX_Queueのアサインがされていないlcoreを除外
	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	// debug表示用
	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);

	}

	while (!force_quit) {
		// http://doc.dpdk.org/api-1.6/rte__cycles_8h.html#a16fa7ef140377a37bc66b336dd54ff70
		// Return the number of TSC cycles since boot
		// 
		// uint64_t rte_get_tsc_cycles
		//
		// tsc=> time stamp counter
		// 現在の時刻取得みたいなもの
		// tsc自体はクロックのカウンタのようなので動作周波数により変わる模様
		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		// tscの前回呼び出しからの差分を取得
		diff_tsc = cur_tsc - prev_tsc;
		// unlikelyはマクロ関数
		// CPUにこの条件式がだいたい成立しないことを支持するため。
		// へーーー
		//
		// すなわちこのサイクルがドレイン時間(10us)を超えた場合の処理
		// と、最初の１回めの処理。
		// => 10us秒毎の処理ってこと・・・。
		if (unlikely(diff_tsc > drain_tsc)) {
			for (i = 0; i < qconf->n_rx_port; i++) {

				portid = l2fwd_dst_ports[qconf->rx_port_list[i]];
				buffer = tx_buffer[portid];
				// https://doc.dpdk.org/api/rte__ethdev_8h.html#ace8779bcecccb86c94ce3638f35c9254
				// Send any packets queued up for transmission on a port and HW queue
				// static uint16_t rte_eth_tx_buffer_flush
				//
				// port_id	portid
				// queue_id	0 => 今回送信qは1つなのでidxとして0固定
				// buffer	buffer
				sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
				// sentに送信できたパケット数が入るためstatに追加
				if (sent){
					port_statistics[portid].tx += sent;
				}

			}

			/* if timer is enabled */
			// timer_period default=10, 
			// すなわち何かしら設定があれば必ずチェック
			//
			// で、１０秒に一回statsを更新するための処理なわけだ。
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

			/* 上記要領に習い、50ミリ秒単位でパケットを送信 */
			orig_rtp_timer_tsc += diff_tsc;
			/* if timer has reached  */
			if (unlikely( orig_rtp_timer_tsc > orig_rtp_tx_tsc )){
				
				for (i = 0; i < qconf->n_rx_port; i++) {
					// rxqueueのport_idを取得
					portid = qconf->rx_port_list[i];
					for (sessionid=0; sessionid<orig_sessions; sessionid++){
						m=orig_create_newpacket(portid, sessionid);
						m=orig_precreate_const_hdrs(m, portid, sessionid);
						orig_send_packet(m, portid);
					}
				}
				orig_rtp_timer_tsc=0;
			}

			// tscの今回値を前回値に更新
			prev_tsc = cur_tsc;
		}

	}
}

// この関数はEALオプションでEnableになっているコア分だけ
// 呼ばれる。そのコアにQueueが割り振られてるかどうかは
// main_loop側で判断してる。
static int
orig_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	orig_main_loop();
	return 0;
}

/* display usage */
static void
orig_usage(const char *prgname)
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
orig_parse_portmask(const char *portmask)
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
orig_parse_nqueue(const char *q_arg)
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

static unsigned int
orig_parse_nsesisons(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= ORIG_RTP_MAX_SESSIONS)
		return 0;

	return n;
}

static int
orig_parse_timer_period(const char *q_arg)
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
	"s:"  /* number of sessions */
	;

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
};

/* Parse the argument given in the command line of the application */
static int
orig_parse_args(int argc, char **argv)
{
	int opt, ret, timer_secs;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, short_options,
				  NULL, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = orig_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				orig_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = orig_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				orig_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_secs = orig_parse_timer_period(optarg);
			if (timer_secs < 0) {
				printf("invalid timer period\n");
				orig_usage(prgname);
				return -1;
			}
			timer_period = timer_secs;
			break;

		/* number of sessions */
		case 's':
			orig_sessions = orig_parse_nsesisons(optarg);
			if (orig_sessions == 0) {
				printf("invalid session number\n");
				orig_usage(prgname);
				return -1;
			}
			break;


		/* long options */
		case 0:
			break;

		default:
			orig_usage(prgname);
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
check_all_ports_link_status(uint16_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	// 90カウント分試行
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		// 途中で中断が来たとき用ね
		if (force_quit)
			return;
		all_ports_up = 1;
		// 全ポートについて
		for (portid = 0; portid < port_num; portid++) {
			// 途中で中断が来たとき用ね
			if (force_quit)
				return;
			// maskされてないやつだけ
			if ((port_mask & (1 << portid)) == 0)
				continue;
	
			// link -> eth_link struct	
			// https://doc.dpdk.org/api/structrte__eth__link.html	
			// uint32_t 	link_speed
			// uint16_t 	link_duplex: 1   => FULL=1, HALF=0
			// uint16_t 	link_autoneg: 1  => FIXED=0, AUTONEGO=1
			// uint16_t 	link_status: 1   => DOWN=0, UP=1
			memset(&link, 0, sizeof(link));
			// https://doc.dpdk.org/api/rte__ethdev_8h.html#a69fa6ca01c82abb5dc60b402df37f62c
			// Retrieve the status (ON/OFF), 
			// the speed (in Mbps) and 
			// the mode (HALF-DUPLEX or FULL-DUPLEX) of the physical 
			// link of an Ethernet device
			// rte_eth_link_getのノーウェイトバージョン
			//
			// rte_eth_link_get_nowait
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
			// http://doc.dpdk.org/api/rte__cycles_8h.html#af73b844d50b98de74b1a0e1730262f85
			// Wait at least ms milliseconds.
			//
			// static void rte_delay_ms
			// ms	CHECK_INTERVAL => 100ms
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
	}else if(signum == SIGHUP || signum == SIGUSR1) {
		// printf("\n\nReloaded\n\n");
	}
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	int ret;
	uint16_t nb_ports;
	uint16_t nb_ports_available;
	uint16_t portid, last_port;
	unsigned i;
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;
	struct orig_rtp_config *tmp_set;

	srand((unsigned)time(NULL));

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGUSR1, signal_handler);

	/* parse application arguments (after the EAL ones) */
	ret = orig_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

	//printf("MAC updating %s\n", mac_updating ? "enabled" : "disabled");

	/* convert to number of cycles */
	// timer_periodはdefault 10secで秒単位で入ってるので
	// rte_get_timer_hzを掛けあわせて、カウンタ値に変更する
	timer_period *= rte_get_timer_hz();

	/* create the mbuf pool */
	// rte_socket_id() => 稼働中のプログラムのNUMAノードIDを取得する
	//
	// rte_pktmbuf_pool_create =>
	//   （受信したパケットを配置するための）メモリプールを確保して、
	//   プールのポインタを返す
	//   from NTT-Nakamuraさんのスライドから
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());
	
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	// nb_ports_in_mask : portmaskが効いた状態でいくつポートを使用するか
	for (portid = 0; portid < nb_ports; portid++) {
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
		
		rte_eth_dev_info_get(portid, &dev_info);
		// Can get device information about this port like below,
		// but does not use this dev_info nothing: TJ Memo
		// printf("%s\n", dev_info.driver_name);
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		l2fwd_dst_ports[last_port] = last_port;
	}

	rx_lcore_id = 0;
	qconf = NULL;
	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port == // そのコアにあたっているport数
		       l2fwd_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		// LABEL:QCONF-UPDATE
		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, portid);
	}

	nb_ports_available = nb_ports;

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... ", portid);
		fflush(stdout);
		// port_confで定義した設定を各ポートに流していく
		// http://doc.dpdk.org/api/rte__ethdev_8h.html#a1a7d3a20b102fee222541fda50fd87bd
		// port_id: portid
		// nb_rx_queue: 1  => rx_queueのサイズ
		// nb_rx_queue: 1　=> tx_queueのサイズ
		// eth_conf: &port_conf
		//
		// 各ポートに割り当てられるqueueサイズを指定している。
		// queueの数を帰る場合はここを触るひつようがあり。
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, portid);

		// http://doc.dpdk.org/api/rte__ethdev_8h.html#ad31219b87a1733d5b367a7c04c7f7b48
		// 良うわからんが・・・送受信descriptor数の指定・・？
		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, port=%u\n",
				 ret, portid);

		// http://doc.dpdk.org/api-2.2/structether__addr.html
		// 各ポートのmac addrを取得
		rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

		l2fwd_ports_eth_peer_addr[portid].addr_bytes[0]=0x20;
		l2fwd_ports_eth_peer_addr[portid].addr_bytes[1]=0x00;
		l2fwd_ports_eth_peer_addr[portid].addr_bytes[2]=0x00;
		l2fwd_ports_eth_peer_addr[portid].addr_bytes[3]=0x00;
		l2fwd_ports_eth_peer_addr[portid].addr_bytes[4]=0x00;
		l2fwd_ports_eth_peer_addr[portid].addr_bytes[5]=0xff & portid;

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, portid);

		/* init one TX queue on each port */
		fflush(stdout);
		// https://doc.dpdk.org/api/rte__ethdev_8h.html#a796c2f20778984c6f41b271e36bae50e
		// int rte_eth_tx_queue_setup
		// Allocate and set up a transmit queue for an Ethernet device.
		//
		// port_id	portid
		// tx_queue_id	0
		// nb_tx_desc	nb_txd => Default 512
		// socket_id	rte_eth_dev_socket_id(portid) => NUMAノードidを取得
		// tx_conf	NULL
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, portid);

		/* Initialize TX buffers */
		// https://doc.dpdk.org/api/rte__malloc_8h.html#aabfa4bb0c1430eb7be96bf847f61113a
		// void* rte_zmalloc_socket
		//
		// type		"tx_buffer" => useful for debug purposes
		// size_t	RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST) => MACRO/Calculate the size of the tx buffer.
		// 							 MAX_PKT_BURST=>32
		// align	0 => If 0, the return is a pointer that is suitably aligned for any kind of variable 
		// socket 	rte_eth_dev_socket_id(portid) => portidの属するNUMAノード
		//
		// voidポインタ・・・なんにでもcastできるやつ
		tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
				rte_eth_dev_socket_id(portid));
		if (tx_buffer[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
					portid);
		// https://doc.dpdk.org/api/rte__ethdev_8h.html#ac5323444ecbe84309a5fcc03aa3c99cb
		// Initialize default values for buffered transmitting
		// int rte_eth_tx_buffer_init
		//
		// buffer	tx_buffer[portid] => 上で作ったやつ
		// size		MAX_PKT_BURST => 32
      		rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

		// https://doc.dpdk.org/api/rte__ethdev_8h.html#aacd4952d9f45acd463e203c21db9a7bb
		// int rte_eth_tx_buffer_set_err_callback
		// Configure a callback for buffered packets which cannot be sent
		// 
		// buffer	tx_buffer[portid] => 上で初期化したバッファ
		// callback	rte_eth_tx_buffer_count_callback => 
		// userdata	port_statistics[portid].dropped => 
		//
		// https://doc.dpdk.org/api/rte__ethdev_8h.html#ac2e9cb28245ce5e06f6b2caf0506cf60
		// Callback function for tracking unsent buffered packets.
		// void rte_eth_tx_buffer_count_callback
		// 
		// pkts		?
		// unsent	The number of unsent packets in the pkts array => 
		// usedata	rte_eth_tx_buffer_set_err_callbackのuserdata
		//
		// 多分 port_statistics[portid].dropped にunsentだったパケット数を足してく。
		// ていうcallback functionなんだろうなぁ。
		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,
				&port_statistics[portid].dropped);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 portid);

		/* 各ポート毎のセッション情報を初期化 */
		rtp_configs_per_port[portid]=(struct orig_rtp_config *)malloc(sizeof(struct orig_rtp_config)*ORIG_RTP_MAX_SESSIONS);
		tmp_set=rtp_configs_per_port[portid];

		for(i=0; i<orig_sessions; i++){
			tmp_set[i].enabled=true;
			tmp_set[i].portid=portid;
			tmp_set[i].ip_d_addr=rte_be_to_cpu_32(IPv4(192,168,0,(i+5)&0xff));
			tmp_set[i].ip_s_addr=rte_be_to_cpu_32(IPv4(192,168,1,(i+5)&0xff));
			tmp_set[i].udp_d_port=rte_be_to_cpu_16(60000+1000*portid+i);
			tmp_set[i].udp_s_port=rte_be_to_cpu_16(50000+1000*portid+i);
			tmp_set[i].rtp_timestamp=0;
			tmp_set[i].rtp_sequence=0;
			tmp_set[i].rtp_ssrc=rand();
		}

		/* Start device */
		// https://doc.dpdk.org/api/rte__ethdev_8h.html#afdc834c1c52e9fb512301990468ca7c2
		// Start an Ethernet device.
		// int rte_eth_dev_start
		//
		// port_id	portid
		// all basic functions exported by the Ethernet API (link status, receive/transmit, and so on) can be invoked.
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, portid);

		printf("done: \n");

		// https://doc.dpdk.org/api/rte__ethdev_8h.html#a31ce323fcb8456f205e816f3ee791834
		// void rte_eth_promiscuous_enable
		// Enable receipt in promiscuous mode for an Ethernet device.
		//
		// port_id portid
		// プロミスキャスモードを有効か有効化iii
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
		// portの統計情報のストラクチャを0で初期化。
		memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	ret = 0;
	/* launch per-lcore init on every lcore */
	// main loopを呼び出し
	// http://doc.dpdk.org/api/rte__launch_8h.html#a2f78fc845135fe22c1ba1c870954b60a
	// Launch a function on all lcores.
	// int rte_eal_mp_remote_launch	
	// 
	// f		orig_launch_one_lcore
	// arg		NULL => orig_launch_one_lcoreの引数
	// call_master	CALL_MASTER => たぶん自分自身からよんだってこと?
	//
	// EAL側オプションで指定したcoremaskでonになってるものだけ
	// lcore数分fを実行していく。
	rte_eal_mp_remote_launch(orig_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		// http://doc.dpdk.org/api/rte__launch_8h.html#a1282dc7cd7e6793afab3ef29239ddf54
		// Wait until an lcore finishes its job.
		// int rte_eal_wait_lcore
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}

	for (portid = 0; portid < nb_ports; portid++) {
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		// https://doc.dpdk.org/api/rte__ethdev_8h.html#a2f97e9df86fe63e3fcfd4c462afbabcd
		// Stop an Ethernet device. The device can be restarted with a call to rte_eth_dev_start()
		//
		// void rte_eth_dev_stop
		rte_eth_dev_stop(portid);
		// https://doc.dpdk.org/api/rte__ethdev_8h.html#a93eeb672a2f9cd18e338aad10c77687c
		// Close a stopped Ethernet device. The device cannot be restarted! 
		//
		// void rte_eth_dev_close
		rte_eth_dev_close(portid);

		free(rtp_configs_per_port[portid]);
		printf(" Done\n");
	}
	printf("Bye...\n");

	return ret;
}
