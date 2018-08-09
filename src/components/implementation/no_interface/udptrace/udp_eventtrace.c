#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <rk_libc_override.h>
#include <rk.h>
#include <rk_inv.h>
#include <memmgr.h>
#include <schedinit.h>
#include <event_trace.h>
#include <sched.h>
#include <cos_time.h>

static struct sockaddr_in soutput, sinput;
static int fd, fdr;

#define CLIBUF_SZ 16
#define TRACE_FLUSH_SLEEP_US 10000

int
udp_writeout(unsigned char *buf, unsigned int sz)
{
	struct sockaddr sa;
	socklen_t len = sizeof(struct sockaddr);

	if (sendto(fd, buf, sz, 0, (struct sockaddr*)&soutput, sizeof(soutput)) != sz) {
		printc("sendto");
		assert(0);
	}

	return sz;
}

static int
udp_event_trace_init(void)
{
	struct sockaddr client_sa;
	socklen_t client_salen = sizeof(struct sockaddr);
	char buf[CLIBUF_SZ] = { 0 };

	printc("entering event trace loop\n");

	soutput.sin_family      = AF_INET;
	soutput.sin_port        = htons(EVENT_TRACE_OUTPORT);
	soutput.sin_addr.s_addr = htonl(INADDR_ANY);
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		printc("Error Establishing socket\n");
		return -1;
	}
	if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		printc("Error Establishing receive socket\n");
		return -1;
	}

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(EVENT_TRACE_INPORT);
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		printc("binding receive socket\n");
		return -1;
	}

	/* wait for a client to connect */
	if (recvfrom(fdr, buf, CLIBUF_SZ, 0, &client_sa, &client_salen) != CLIBUF_SZ) {
		printc("recvfrom");
		return -1;
	}
	soutput.sin_addr.s_addr = ((struct sockaddr_in*)&client_sa)->sin_addr.s_addr;

	printc("Recvfrom success: [%s]\nStarting to flush out events!\n", buf);
}

static int
udp_event_trace_loop(void)
{
	/*
	 * NOTE: if no client connects, we don't go past recvfrom and
	 *       so ring buffer will start to drop events
	 */

	while (1) {
		cycles_t abs_sleep = 0;

		event_flush();
		rdtscll(abs_sleep);

		/* perhaps sleep for a bit? */
		sched_thd_block_timeout(0, abs_sleep + time_usec2cyc(TRACE_FLUSH_SLEEP_US));
	}

	return -1;
}

void
cos_init(void)
{
	printc("Welcome to the udp event trace component\n");

	rk_libcmod_init();

	udp_event_trace_init();
#ifdef EVENT_TRACE_REMOTE
	event_trace_client_init(udp_writeout);
#else
	assert(0);
#endif
	schedinit_child();

	udp_event_trace_loop();
}