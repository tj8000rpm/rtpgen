/*-
 *   Based on l2fwd sample on 17.11
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

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

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

#include "ipc_pack.pb-c.h"

static volatile bool force_quit;

#define RTE_LOGTYPE_RTPGEN RTE_LOGTYPE_USER1

#define NB_MBUF   8192
#define MAX_NB_MBUF 57344

//#define MAX_PKT_BURST 32
#define MAX_PKT_BURST 512
#define BURST_TX_DRAIN_US 10 /* TX drain every ~5us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 4096

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


/* //////////////////////////////////////////////////////////////////////////////
 *
 * RTPGEN specified varibles
 * 
 *///////////////////////////////////////////////////////////////////////////////

#define RTPGEN_RTP_PT_PCMU 0
#define RTPGEN_MAXFILENAME 256
#define RTPGEN_RTP_MAX_SESSIONS 50000
#define RTPGEN_DEFAULT_RTP_TX_INTERVAL_US 20000 /* TX rtp packet every ~20ms = ~20000us */
#define RTPGEN_DEFAULT_RTP_PAYLOAD_LEN 160
#define RTPGEN_RTP_MAX_PAYLOAD_LEN 800
#define RTPGEN_RTP_PAYLOAD_FILE_MAX_TIME_S 30
#define RTPGEN_RTP_MAX_FILE_LEN RTPGEN_DEFAULT_RTP_PAYLOAD_LEN *\
								   (1000000 / RTPGEN_DEFAULT_RTP_TX_INTERVAL_US) *\
								   RTPGEN_RTP_PAYLOAD_FILE_MAX_TIME_S // u-lawで30秒分

unsigned rtpgen_rtp_tx_interval_us = RTPGEN_DEFAULT_RTP_TX_INTERVAL_US;
unsigned rtpgen_rtp_payload_len    = RTPGEN_DEFAULT_RTP_PAYLOAD_LEN;
uint8_t  rtpgen_rtp_pt             = RTPGEN_RTP_PT_PCMU;

struct rtpgen_rtp_hdr {
	uint16_t flags;
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc;
} __attribute__((__packed__));

struct rtpgen_rtp_config {
	bool enabled;
	uint8_t portid;
	uint32_t ip_dst_addr;
	uint32_t ip_src_addr;
	uint16_t udp_dst_port;
	uint16_t udp_src_port;
	uint32_t rtp_timestamp;
	uint32_t rtp_sequence;
	uint32_t rtp_ssrc;
};

typedef struct rtpgen_ethIpUdpHdr_s {
	struct ether_hdr eth;
	struct ipv4_hdr ip;
	struct udp_hdr udp;
} rtpgen_ethIpUdpHdr_t __attribute__((__packed__));

rtpgen_ethIpUdpHdr_t *constantHeaders[RTE_MAX_ETHPORTS];
struct rtpgen_rtp_config *rtp_configs_per_port[RTE_MAX_ETHPORTS];

static unsigned int rtpgen_sessions = 1;
static unsigned int rtpgen_payload_data_len = RTPGEN_DEFAULT_RTP_PAYLOAD_LEN * 2;
// メモリコピーを行うため。循環時にオーバフローしないように2倍の領域を確保
static uint8_t rtpgen_payload_data[RTPGEN_RTP_MAX_FILE_LEN * 2];

static char rtpgen_payload_filename[RTPGEN_MAXFILENAME];

static unsigned int n_mbuf_pool = NB_MBUF;

static uint8_t use_default_mac_peer = true;

int pt_th=0;
pthread_t *p_socket_main_thread=NULL;
pthread_t socket_main_thread;
pthread_t *p_sub_socket_thread=NULL;
pthread_t sub_socket_thread;

struct socket_info{
    int sock;
	int ssock;
};


void enabling_message(RtpgenIPCmsgV1*, RtpgenIPCmsgV1*);
void thread_loop(struct socket_info*);
void subthread_loop(int*);
/* //////////////////////////////////////////////////////////////////////////////
 *
 * RTPGEN specified varibles end
 *
 *///////////////////////////////////////////////////////////////////////////////

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

// 送信タイミングでのpacketの生成
static unsigned
rtpgen_setup_newpacket(uint16_t portid, struct rte_mbuf **pkts,unsigned *activeids, unsigned count){
	struct rtpgen_rtp_hdr *rtp;
	struct rte_mbuf *m;
	uint8_t *payload;
	unsigned payload_idx;
	uint8_t rtp_version;
	uint8_t rtp_padding;
	uint8_t rtp_extention;
	uint8_t rtp_ccrc_count;
	uint8_t rtp_marker;
	unsigned sessionid, i;
	struct rtpgen_rtp_config *rtp_conf;
	
	rtp_conf=rtp_configs_per_port[portid];
	
	for(i=0;i<count;i++){
		sessionid=activeids[i];

		m=pkts[i];
		rte_memcpy((uint8_t *)m->buf_addr + m->data_off,
			   (uint8_t *)&(constantHeaders[portid][sessionid]), sizeof(rtpgen_ethIpUdpHdr_t));
		m->data_len=sizeof(rtpgen_ethIpUdpHdr_t);
		m->pkt_len =sizeof(rtpgen_ethIpUdpHdr_t);

		rtp=(struct rtpgen_rtp_hdr *)rte_pktmbuf_append(m,sizeof(struct rtpgen_rtp_hdr));

		// add RTP headers;
		rtp_version=2;
		rtp_padding=0;
		rtp_extention=0;
		rtp_ccrc_count=0;
		rtp_marker=0;
		rtp->flags=rte_be_to_cpu_16((rtp_version    &0x3 ) << 14 |
		                            (rtp_padding    &0x1 ) << 13 |
		                            (rtp_extention  &0x1 ) << 12 |
		                            (rtp_ccrc_count &0xf ) <<  8 |
		                            (rtp_marker     &0x1 ) <<  7 |
		                            (rtpgen_rtp_pt  &0x7f)        );
		rtp->sequence =rte_be_to_cpu_16(rtp_conf[sessionid].rtp_sequence);
		rtp->timestamp=rte_be_to_cpu_32(rtp_conf[sessionid].rtp_timestamp);
		rtp->ssrc     =rte_be_to_cpu_32(rtp_conf[sessionid].rtp_ssrc);

		// append Payload data;
		payload=(uint8_t *)rte_pktmbuf_append(m,sizeof(uint8_t)*rtpgen_rtp_payload_len);
		payload_idx=rtp_conf[sessionid].rtp_sequence;
		payload_idx*=rtpgen_rtp_payload_len;
		payload_idx%=rtpgen_payload_data_len;

		rte_memcpy((uint8_t *)payload,
		           (uint8_t *)&(rtpgen_payload_data)+payload_idx, rtpgen_rtp_payload_len);

		rtp_conf[sessionid].rtp_timestamp+=rtpgen_rtp_payload_len;
		rtp_conf[sessionid].rtp_sequence+=1;
	}
	
	return count;
}

/* RTPペイロードデータのよみこみ */
static void
rtpgen_set_payload_data(void){
	FILE *fp;
	unsigned i, size;

	// data flush
	memset(rtpgen_payload_data, 0, sizeof(char)*RTPGEN_RTP_MAX_FILE_LEN);

	//rtpgen_payload_data={
	uint8_t default_data[]={
			0x9f,0xff,0x8b,0x91,0x86,0x87,0x8b,0x87, // 8
			0x9f,0x91,0x1f,0x7f,0x0b,0x11,0x06,0x07, // 16
			0x0b,0x07,0x1f,0x11,0x9f,0x7e,0x8b,0x91, // 24
			0x86,0x87,0x8b,0x87,0x9f,0x91,0x1f,0xff, // 32
			0x0b,0x11,0x06,0x07,0x0b,0x07,0x1f,0x11, // 40
			0x9f,0x7f,0x8b,0x91,0x86,0x87,0x8b,0x87, // 48
			0x9f,0x91,0x1f,0x7f,0x0b,0x11,0x06,0x07, // 56
			0x0b,0x07,0x1f,0x11,0x9f,0x7f,0x8b,0x91, // 64
			0x86,0x87,0x8b,0x87,0x9f,0x91,0x1f,0xff, // 72
			0x0b,0x11,0x06,0x07,0x0b,0x07,0x1f,0x11, // 80
			0x9f,0x7f,0x8b,0x91,0x86,0x87,0x8b,0x87, // 88
			0x9f,0x91,0x1f,0x7f,0x0b,0x11,0x06,0x07, // 96
			0x0b,0x07,0x1f,0x11,0x9f,0xff,0x8b,0x91, // 104
			0x86,0x87,0x8b,0x87,0x9f,0x91,0x1f,0xff, // 112
			0x0b,0x11,0x06,0x07,0x0b,0x07,0x1f,0x11, // 120
			0x9f,0xff,0x8b,0x91,0x86,0x87,0x8b,0x87, // 128
			0x9f,0x91,0x1f,0x7f,0x0b,0x11,0x06,0x07, // 136
			0x0b,0x07,0x1f,0x11,0x9f,0xff,0x8b,0x91, // 144
			0x86,0x87,0x8b,0x87,0x9f,0x91,0x1f,0xff, // 152
			0x0b,0x11,0x06,0x07,0x0b,0x07,0x1f,0x11	 // 160
		};
	for(i=0;i<rtpgen_rtp_payload_len+rtpgen_rtp_payload_len;i++){
		rtpgen_payload_data[i]=default_data[i%rtpgen_rtp_payload_len];
	}
	rtpgen_payload_data_len=rtpgen_rtp_payload_len;

	if ( strcmp(rtpgen_payload_filename, "")!= 0) {
		fp=fopen(rtpgen_payload_filename, "rb");
		if(fp!=NULL){
			size=fread(&rtpgen_payload_data, sizeof(uint8_t), RTPGEN_RTP_MAX_FILE_LEN, fp);
			if(size==0)
				return;
			rtpgen_payload_data_len=size;

			for(i=0;i<rtpgen_rtp_payload_len;i++)
				rtpgen_payload_data[rtpgen_payload_data_len+i]=rtpgen_payload_data[i];
			
			fclose(fp);
			printf("loaded from %s\n", rtpgen_payload_filename);
		}else{
			printf("invalid filename - payload data, use default setting\n");
		}
	}
}

/* main processing loop */
static void
rtpgen_main_loop(void){
	int sent;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	uint64_t rtpgen_rtp_timer_tsc,
		 rtpgen_prev_tsc;
	unsigned i, j, portid, sessionid, cur_sessionid;
	struct lcore_queue_conf *qconf;
	unsigned active_sessions[RTPGEN_RTP_MAX_SESSIONS];
	unsigned int nb_active_sessions = 0;

	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;

	struct rte_mbuf *tx_bursts[RTPGEN_RTP_MAX_SESSIONS];

	prev_tsc = 0;
	timer_tsc = 0;
	rtpgen_rtp_timer_tsc = 0;
	const uint64_t rtpgen_rtp_tx_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			(rtpgen_rtp_tx_interval_us - 2*BURST_TX_DRAIN_US);

	lcore_id = rte_lcore_id();
	// coreごとのqueue情報の確認
	// LABEL:QCONF-UPDATEで変えた情報をここで参照
	// unsigned n_rx_port;
	// unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	qconf = &lcore_queue_conf[lcore_id];

	// EALオプションから指定されているが、
	// RX_Queueのアサインがされていないlcoreを除外
	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, RTPGEN, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, RTPGEN, "entering main loop on lcore %u\n", lcore_id);

	// debug表示用
	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, RTPGEN, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);

	}

	cur_sessionid=0;
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
		// CPUにこの条件式がだいたい成立しないことを指示するため。
		//
		// すなわちこのサイクルがドレイン時間(10us)を超えた場合の処理
		// と、最初の１回めの処理。
		// => 10us秒毎の処理ってこと・・・。
		//if (diff_tsc > drain_tsc) {
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
			// で、n秒に一回statsを更新するための処理なわけ。
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

			/* advance the timer */
			rtpgen_rtp_timer_tsc += diff_tsc;

			/* 上記要領に習い、パケット化周期単位でパケットを送信 */
			// rtpgen_rtp_tx_interval_us = pkt化周期
			/* if timer has reached  */
			if (unlikely( rtpgen_rtp_timer_tsc >= rtpgen_rtp_tx_tsc )){
				for (i = 0; i < qconf->n_rx_port; i++) {
					// rxqueueのport_idを取得
					portid = qconf->rx_port_list[i];
					if(cur_sessionid == 0){
						nb_active_sessions=0;
						for(j=0;j<rtpgen_sessions;j++)
							if(rtp_configs_per_port[portid][j].enabled)
								active_sessions[nb_active_sessions++]=j;
						if(rte_pktmbuf_alloc_bulk(l2fwd_pktmbuf_pool, tx_bursts, nb_active_sessions)!=0)
							continue;
						
						rtpgen_prev_tsc=cur_tsc;
						nb_active_sessions=rtpgen_setup_newpacket(portid, tx_bursts, active_sessions, nb_active_sessions);
					}
				
					for (sessionid=cur_sessionid;
						sessionid<cur_sessionid+32 && sessionid<nb_active_sessions;
						sessionid++){
						sent = rte_eth_tx_buffer(portid, 0, tx_buffer[portid], tx_bursts[sessionid]);
						if (sent)
							port_statistics[portid].tx += sent;
					}
					if(sessionid >= nb_active_sessions){
						rtpgen_rtp_timer_tsc=cur_tsc-rtpgen_prev_tsc;
						cur_sessionid=0;
					}else{
						cur_sessionid=sessionid;
					}
				}
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
rtpgen_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	rtpgen_main_loop();
	return 0;
}

/* display usage */
static void
rtpgen_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
	       "  -T PERIOD: statistics will be refreshed each PERIOD seconds\n"
		   "             (0 to disable, 10 default, 86400 maximum)\n"
	       "  -s SESSIONS: number of rtp sessions\n"
	       "  -M PORT0_MACPEER,PORT1_MACPEER,...,PORTn_MACPEER: Peer MAC address for each port\n"
	       "  -b BUFFER: number of mbuf pool size(%d default, %d maximum)\n"
	       "  -f FILE: select rtp payload file(expected no header rtp file)\n"
	       "  -P SIZE: rtp payload size(%d default, %d maximum)\n"
	       "  -t PAYLOAD_TYPE: rtp payload type(%d(PCMU) default)\n",
	       prgname, NB_MBUF, MAX_NB_MBUF, RTPGEN_DEFAULT_RTP_PAYLOAD_LEN,
		   RTPGEN_RTP_MAX_PAYLOAD_LEN, RTPGEN_RTP_PT_PCMU);
}

static int
rtpgen_parse_portmask(const char *portmask)
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


static int
rtpgen_parse_number(const char *q_arg, int lower, int upper, int err)
{
	char *end = NULL;
	int n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return err;
	if (n <= lower)
		return err;
	if (n > upper)
		return err;

	return n;
}

static int
rtpgen_parse_macaddr(char *q_arg) {

	uint32_t n=0;
	char *end = NULL;
	int i, port, s_ptr;
	char *tmp, *tmp2;
	char c;

	for(s_ptr = 0, port = 0, i = 0; i < 256; i++){
		c = q_arg[i];
		if(c==',' || c=='\0'){
			if(port>=RTE_MAX_ETHPORTS) return 0;

			q_arg[i] = '\0';
			tmp = q_arg + s_ptr;
			s_ptr = i+1;

			for(i = 0; i < 6; i++){
				tmp2 = strtok(tmp, ":");
				if(tmp2 == NULL) return 0;

				n = strtoul(tmp2, &end, 16);
				if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
					return 0;
				if (n > 0xff)
					return 0;
				l2fwd_ports_eth_peer_addr[port].addr_bytes[i]=n;

				tmp = NULL;
			}
			port++;

			if(c=='\0') break;
		}
	}
	return 1;
}

static void rtpgen_parse_filename(const char *q_arg, char *filename) {
	strcpy(filename, q_arg);
}


static const char short_options[] =
	"p:"  /* portmask */
	"q:"  /* number of queues */
	"T:"  /* timer period */
	"s:"  /* number of sessions */
	"f:"  /* payload data use file */
	"b:"  /* number of mbuf pool */
	"P:"  /* payload size */
	"t:"  /* payload type */
	"M:"  /* MAC Address peers */
	;

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
};

/* Parse the argument given in the command line of the application */
static int
rtpgen_parse_args(int argc, char **argv)
{
	int opt, ret, timer_secs, macret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, short_options,
				  NULL, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = rtpgen_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				rtpgen_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = rtpgen_parse_number(optarg, 0, MAX_RX_QUEUE_PER_LCORE, 0);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				rtpgen_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_secs = rtpgen_parse_number(optarg, -1, MAX_TIMER_PERIOD, -1);
			if (timer_secs < 0) {
				printf("invalid timer period\n");
				rtpgen_usage(prgname);
				return -1;
			}
			timer_period = timer_secs;
			break;

		/* number of mbuf pool */
		case 'b':
			n_mbuf_pool = rtpgen_parse_number(optarg, 0, MAX_NB_MBUF, 0);
			if (rtpgen_sessions == 0) {
				printf("invalid mbuf pool size\n");
				rtpgen_usage(prgname);
				return -1;
			}
			break;

		/* number of sessions */
		case 's':
			rtpgen_sessions = rtpgen_parse_number(optarg, 0, RTPGEN_RTP_MAX_SESSIONS, 0);
			if (rtpgen_sessions == 0) {
				printf("invalid session number\n");
				rtpgen_usage(prgname);
				return -1;
			}
			break;

		/* payload data */
		case 'f':
			rtpgen_parse_filename(optarg, rtpgen_payload_filename);
			if ( strcmp(rtpgen_payload_filename, "")== 0) {
				printf("invalid filename\n");
				rtpgen_usage(prgname);
				return -1;
			}
			break;

		/* payload size */
		case 'P':
			rtpgen_rtp_payload_len = rtpgen_parse_number(optarg, 0, RTPGEN_RTP_MAX_PAYLOAD_LEN, 0);
			if (rtpgen_rtp_payload_len == 0) {
				printf("invalid payload size\n");
				rtpgen_usage(prgname);
				return -1;
			}
			break;

		/* payload type */
		case 't':
			rtpgen_rtp_pt = (uint8_t)rtpgen_parse_number(optarg, -1, 127, 255);
			if (rtpgen_rtp_pt == 255) {
				printf("invalid payload type\n");
				rtpgen_usage(prgname);
				return -1;
			}
			break;

		/* Mac peer address */
		case 'M':
			macret = rtpgen_parse_macaddr(optarg);
			if (macret == 0) {
				printf("invalid mac address format\n");
				rtpgen_usage(prgname);
				return -1;
			}
			use_default_mac_peer=false;
			break;


		/* long options */
		case 0:
			break;

		default:
			rtpgen_usage(prgname);
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

uint32_t api_call_read_resources(RtpConfigV1 *ret, struct rtpgen_rtp_config *data);
uint32_t api_sub_call_write(RtpConfigV1 *source,
                            struct rtpgen_rtp_config *target,
                            rtpgen_ethIpUdpHdr_t *header);
void api_rest_rtp_config(RtpConfigV1 *source, struct rtpgen_rtp_config *target);

uint32_t api_call_read_resources(RtpConfigV1 *ret, struct rtpgen_rtp_config *data){
	if(!ret){
		return RTPGEN_IPCMSG_V1__RESPONSE__ERROR_SERVER_ERROR;
	}
	ret->has_enabled       = true; ret->enabled       = data->enabled;
	ret->has_ip_dst_addr   = true; ret->ip_dst_addr   = data->ip_dst_addr;
	ret->has_ip_src_addr   = true; ret->ip_src_addr   = data->ip_src_addr;
	ret->has_udp_dst_port  = true; ret->udp_dst_port  = data->udp_dst_port;
	ret->has_udp_src_port  = true; ret->udp_src_port  = data->udp_src_port;
	ret->has_rtp_timestamp = true; ret->rtp_timestamp = data->rtp_timestamp;
	ret->has_rtp_sequence  = true; ret->rtp_sequence  = data->rtp_sequence;
	ret->has_rtp_ssrc      = true; ret->rtp_ssrc      = data->rtp_ssrc;
	return RTPGEN_IPCMSG_V1__RESPONSE__SUCCESS;
}

uint32_t api_sub_call_write(RtpConfigV1 *source,
                            struct rtpgen_rtp_config *target,
                            rtpgen_ethIpUdpHdr_t *header){
	if(!target){
		return RTPGEN_IPCMSG_V1__RESPONSE__ERROR_SERVER_ERROR;
	}
	if(source){
		if(source->has_ip_dst_addr  ){
			target->ip_dst_addr  =                  source->ip_dst_addr;
			header->ip.dst_addr  = rte_be_to_cpu_32(source->ip_dst_addr);
		}
		if(source->has_ip_src_addr  ){
			target->ip_src_addr  =                  source->ip_src_addr;
			header->ip.src_addr  = rte_be_to_cpu_32(source->ip_src_addr);
		}
		if(source->has_udp_dst_port ){
			target->udp_dst_port =                  source->udp_dst_port;
			header->udp.dst_port = rte_be_to_cpu_16(source->udp_dst_port);
		}
		if(source->has_udp_src_port ){
			target->udp_src_port =                  source->udp_src_port;
			header->udp.src_port = rte_be_to_cpu_16(source->udp_src_port);
		}
		if(source->has_rtp_timestamp) target->rtp_timestamp = source->rtp_timestamp;
		if(source->has_rtp_sequence ) target->rtp_sequence  = source->rtp_sequence;
		if(source->has_rtp_ssrc     ) target->rtp_ssrc      = source->rtp_ssrc;
	}
	return RTPGEN_IPCMSG_V1__RESPONSE__SUCCESS;
}

void api_rest_rtp_config(RtpConfigV1 *source, struct rtpgen_rtp_config *target){
	uint32_t s_time=rand();
	/* 音源の長さに合わせて必ず音源ファイルの先頭になるようにオフセットする */
	if(!source || !source->has_rtp_timestamp)
		target->rtp_timestamp = s_time-s_time%rtpgen_payload_data_len;

	if(!source || !source->has_rtp_sequence ) target->rtp_sequence  = rand();
	if(!source || !source->has_rtp_ssrc     ) target->rtp_ssrc      = rand();
}

void enabling_message(RtpgenIPCmsgV1 *request_msg, RtpgenIPCmsgV1 *response_msg){
	struct rtpgen_rtp_config *data;
	rtpgen_ethIpUdpHdr_t *hdr;
	uint32_t selector;
	uint32_t portid;
	response_msg->has_response_code=1;
	response_msg->response_code=RTPGEN_IPCMSG_V1__RESPONSE__SUCCESS;

	/*  validation message */
	if(! request_msg->has_request_code ||
	   ! request_msg->has_portid ||
	   ! request_msg->has_id_selector){
		response_msg->response_code=RTPGEN_IPCMSG_V1__RESPONSE__ERROR_BAD_REQUEST;
		return;
	}
	selector = request_msg->id_selector;
 	portid   = request_msg->portid;

	if(selector>=rtpgen_sessions){
		response_msg->has_id_selector=true;
		response_msg->id_selector    =rtpgen_sessions-1;
		response_msg->response_code=RTPGEN_IPCMSG_V1__RESPONSE__ERROR_NOT_FOUND;
		return;
	}
	if(portid >= RTE_MAX_ETHPORTS){
		response_msg->has_portid=true;
		response_msg->portid    =RTE_MAX_ETHPORTS-1;
		response_msg->response_code=RTPGEN_IPCMSG_V1__RESPONSE__ERROR_NOT_FOUND;
		return;
	}
	if((l2fwd_enabled_port_mask & (1 << portid)) == 0){
		response_msg->has_portid=true;
		response_msg->portid    =portid;
		response_msg->response_code=RTPGEN_IPCMSG_V1__RESPONSE__ERROR_FORBIDDEN;
		return;
	}
		
	data=&(rtp_configs_per_port[portid][selector]);
	hdr=&(constantHeaders[portid][selector]);

	if(data == NULL){
		response_msg->response_code=RTPGEN_IPCMSG_V1__RESPONSE__ERROR_SERVER_ERROR;
		return;
	}

	switch(request_msg->request_code){
		case RTPGEN_IPCMSG_V1__REQUEST__CREATE:
			if(data->enabled){ // すでに存在する場合はエラー
				response_msg->response_code=
					RTPGEN_IPCMSG_V1__RESPONSE__ERROR_CONFLICT;
				return;
			}
			response_msg->response_code=
				api_sub_call_write(request_msg->rtp_config, data, hdr);
			api_rest_rtp_config(request_msg->rtp_config, data);
			data->enabled = true;
			break;
		case RTPGEN_IPCMSG_V1__REQUEST__READ:
			if(!data->enabled){ // 存在しない場合はエラー
				response_msg->response_code=
					RTPGEN_IPCMSG_V1__RESPONSE__ERROR_NOT_FOUND;
				return;
			}
			response_msg->response_code=
				api_call_read_resources(response_msg->rtp_config, data);
			break;
		case RTPGEN_IPCMSG_V1__REQUEST__UPDATE:
			if(!data->enabled){ // 存在しない場合はエラー
				response_msg->response_code=
					RTPGEN_IPCMSG_V1__RESPONSE__ERROR_NOT_FOUND;
				return;
			}
			response_msg->response_code=
				api_sub_call_write(request_msg->rtp_config, data, hdr);
			break;
		case RTPGEN_IPCMSG_V1__REQUEST__DELETE:
			if(!data->enabled){ // 存在しない場合はエラー
				response_msg->response_code=
					RTPGEN_IPCMSG_V1__RESPONSE__ERROR_NOT_FOUND;
				return;
			}
			data->enabled=false; // 無効化
			break;
		default:
			response_msg->response_code = 
				RTPGEN_IPCMSG_V1__RESPONSE__ERROR_BAD_REQUEST;
			return;
	}
	response_msg->has_portid      = true; response_msg->portid     =portid;
	response_msg->has_id_selector = true; response_msg->id_selector=selector;
}

void subthread_loop(int *p_sock){
	int sock=*p_sock;
	uint8_t buf[1024];
	int len = 0;
	int send_buffer_len = 0;
	void *send_buffer            = NULL;
	RtpgenIPCmsgV1 *pack         = NULL;
	RtpgenIPCmsgV1 *response_msg = NULL;

	while(true){
		memset(buf,0,sizeof(uint8_t)*1024);
		len = read(sock, buf, sizeof(buf));
		if(len <= 0){
			printf("connection remote closed.\n");
			break;
		}
		// recived data
		pack=rtpgen_ipcmsg_v1__unpack(NULL, len, buf);

		if(pack == NULL){
			printf("protobuf read error\n");
			break;
		}

		// create response data
		response_msg=(RtpgenIPCmsgV1 *)malloc(sizeof(RtpgenIPCmsgV1));
		if(response_msg == NULL){
			printf("memmory allocation error\n");
			break;
		}

		// initialized response data
		rtpgen_ipcmsg_v1__init(response_msg);

		// update response data rtp_config field
		response_msg->rtp_config=(RtpConfigV1 *)malloc(sizeof(RtpConfigV1));
		if(response_msg->rtp_config == NULL){
			printf("memmory allocation error\n");
			break;
		}
		rtp_config_v1__init(response_msg->rtp_config);

		// IPCメッセージの内容を処理
		enabling_message(pack, response_msg);
		if(response_msg==NULL){
			printf("error exit\n");
			break;
		}
		
		// レスポンスメッセージを
		send_buffer_len=rtpgen_ipcmsg_v1__get_packed_size(response_msg);
		send_buffer=malloc(send_buffer_len);
		if(send_buffer == NULL){
			printf("memmory allocation error\n");
			break;
		}

		rtpgen_ipcmsg_v1__pack(response_msg, send_buffer);
		write(sock, send_buffer, send_buffer_len);

		rtpgen_ipcmsg_v1__free_unpacked(pack, NULL);

		if(response_msg != NULL){
			if(response_msg->rtp_config != NULL)
				free(response_msg->rtp_config);
			response_msg->rtp_config=NULL;
			free(response_msg);
		}
		response_msg=NULL;
		if(send_buffer != NULL)
			free(send_buffer);
		send_buffer=NULL;
	}

	if(response_msg != NULL){
		if(response_msg->rtp_config != NULL)
			free(response_msg->rtp_config);
		free(response_msg);
	}
	if(send_buffer != NULL)
		free(send_buffer);

	printf("sub_socket closed\n");
	pt_th--;
	close(sock);
}

void thread_loop(struct socket_info *info){
	int sock;
	int ssock;
	struct sockaddr_in client;
	unsigned client_addr_len;

	sock=info->sock;
	printf("socket opened\n");

	client_addr_len=sizeof(client);
	listen(sock, 1-1);
	while(!force_quit){
		ssock = accept(sock, (struct sockaddr *)&client, &client_addr_len);
		info->ssock=ssock;
		if(ssock > 0){
			if(pt_th >= 1){
				printf("MAX-Sessions : Connection closed.\n");
				close(ssock);
			}else{
				pthread_create(&sub_socket_thread,NULL,(void *)subthread_loop,(void *)&ssock);
				p_sub_socket_thread=&sub_socket_thread;
				pt_th++;
			}
		}
   } 
   pthread_join(sub_socket_thread,NULL);
	p_sub_socket_thread=NULL;
   close(ssock);
}


static void
signal_handler(int signum) {
	//int ret;
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
		printf("SIGNAl interupt... SIG:%d\n",signum);
		if(p_sub_socket_thread!=NULL){
			pthread_cancel(sub_socket_thread);
			pthread_kill(sub_socket_thread, SIGTERM);
		}
		if(p_socket_main_thread!=NULL){
			pthread_cancel(socket_main_thread);
			pthread_kill(socket_main_thread, SIGTERM);
		}
		printf("thread closed...\n");
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
	unsigned i, len;
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;
	struct rtpgen_rtp_config *tmp_set;

	srand((unsigned)time(NULL));

	struct sockaddr_in addr;
	
	struct socket_info sinfo;
	int sock = -1;
	int optyes = 1;
	
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
	ret = rtpgen_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid RTPGEN arguments\n");

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
	//l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
	printf("--- %d\n",(unsigned)(rtpgen_sessions*1.15));
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", n_mbuf_pool,
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

	/* RTPペイロードのデータを初期化 */
	rtpgen_set_payload_data();

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

		// Set a destination mac addr
		if(use_default_mac_peer){
			l2fwd_ports_eth_peer_addr[portid].addr_bytes[0]=0x20;
			l2fwd_ports_eth_peer_addr[portid].addr_bytes[1]=0x00;
			l2fwd_ports_eth_peer_addr[portid].addr_bytes[2]=0x00;
			l2fwd_ports_eth_peer_addr[portid].addr_bytes[3]=0x00;
			l2fwd_ports_eth_peer_addr[portid].addr_bytes[4]=0x00;
			l2fwd_ports_eth_peer_addr[portid].addr_bytes[5]=0xff & portid;
		}

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
				//origcb,
				&port_statistics[portid].dropped);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 portid);

		/* 各ポート毎のセッション情報を初期化 */
		rtp_configs_per_port[portid]=
			(struct rtpgen_rtp_config *)malloc(
				sizeof(struct rtpgen_rtp_config)*RTPGEN_RTP_MAX_SESSIONS);
		constantHeaders[portid]=
			(rtpgen_ethIpUdpHdr_t *)malloc(
				sizeof(rtpgen_ethIpUdpHdr_t)*RTPGEN_RTP_MAX_SESSIONS);

		tmp_set=rtp_configs_per_port[portid];

		for(i=0; i<rtpgen_sessions; i++){
			tmp_set[i].enabled=false;
			tmp_set[i].portid=portid;
			tmp_set[i].rtp_timestamp=0;
			tmp_set[i].rtp_sequence=0;
			tmp_set[i].rtp_ssrc=rand();
			tmp_set[i].ip_dst_addr=IPv4(127,0,0,1);
			tmp_set[i].ip_src_addr=IPv4(127,0,0,1);
			tmp_set[i].udp_dst_port=6000+1000*portid+i;
			tmp_set[i].udp_src_port=8000+1000*portid+i;
			
			len=rtpgen_rtp_payload_len + sizeof(struct rtpgen_rtp_hdr);
			len+=sizeof(struct udp_hdr);
			constantHeaders[portid][i].udp.src_port=rte_be_to_cpu_16(8000+1000*portid+i);
			constantHeaders[portid][i].udp.dst_port=rte_be_to_cpu_16(6000+1000*portid+i);
			constantHeaders[portid][i].udp.dgram_len=rte_be_to_cpu_16(len);
			constantHeaders[portid][i].udp.dgram_cksum=0;

			len+=sizeof(struct ipv4_hdr);
			constantHeaders[portid][i].ip.dst_addr=rte_be_to_cpu_32(IPv4(127,0,0,1));
			constantHeaders[portid][i].ip.src_addr=rte_be_to_cpu_32(IPv4(127,0,0,1));
			constantHeaders[portid][i].ip.version_ihl=0x45;
			constantHeaders[portid][i].ip.type_of_service=0x05;
			constantHeaders[portid][i].ip.total_length=rte_be_to_cpu_16(len);
			constantHeaders[portid][i].ip.packet_id=0;
			constantHeaders[portid][i].ip.fragment_offset=rte_be_to_cpu_16(0x4000);
			constantHeaders[portid][i].ip.time_to_live=64;
			constantHeaders[portid][i].ip.next_proto_id=17;
			constantHeaders[portid][i].ip.hdr_checksum=0;

			ether_addr_copy(&l2fwd_ports_eth_peer_addr[portid], &(constantHeaders[portid][i].eth.d_addr));
			ether_addr_copy(&l2fwd_ports_eth_addr[portid], &(constantHeaders[portid][i].eth.s_addr));
			constantHeaders[portid][i].eth.ether_type = rte_be_to_cpu_16(ETHER_TYPE_IPv4);
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


	/* Bind controllers endpoint */
	sock = socket(AF_INET,  SOCK_STREAM, 0);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optyes, sizeof(int));
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optyes, sizeof(int));

	sinfo.sock=sock;
	sinfo.ssock=0;

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7700);

	bind(sock, (struct sockaddr *)&addr, sizeof(addr));

	printf("- open %d\n",sinfo.sock);
	
	ret = pthread_create(&socket_main_thread,NULL,(void *)thread_loop,(void *)&sinfo);
	p_socket_main_thread=&socket_main_thread;
	if (ret != 0){
		rte_exit(EXIT_FAILURE,
			"Could not bind controllers tcp port(0.0.0.0:7700).\n");
	}

	ret = 0;
	/* launch per-lcore init on every lcore */
	// main loopを呼び出し
	// http://doc.dpdk.org/api/rte__launch_8h.html#a2f78fc845135fe22c1ba1c870954b60a
	// Launch a function on all lcores.
	// int rte_eal_mp_remote_launch	
	// 
	// f		rtpgen_launch_one_lcore
	// arg		NULL => rtpgen_launch_one_lcoreの引数
	// call_master	CALL_MASTER => たぶん自分自身からよんだってこと?
	//
	// EAL側オプションで指定したcoremaskでonになってるものだけ
	// lcore数分fを実行していく。
	rte_eal_mp_remote_launch(rtpgen_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		// http://doc.dpdk.org/api/rte__launch_8h.html#a1282dc7cd7e6793afab3ef29239ddf54
		// Wait until an lcore finishes its job.
		// int rte_eal_wait_lcore
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}

	// add controller tcpport function
	if(p_sub_socket_thread)
    	ret = pthread_join(sub_socket_thread,NULL);
	if(p_socket_main_thread)
		ret = pthread_join(socket_main_thread,NULL);

	printf("the program will be terminating...\n");
	if(sinfo.ssock!=0){
		shutdown(sinfo.ssock,0);
		close(sinfo.ssock);
	}
	if(sinfo.sock!=0){
		close(sock);
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
		free(constantHeaders[portid]);
		printf(" Done\n");
	}
	printf("Bye...\n");

	return ret;
}
