#pragma once

#include "compat.h"

typedef struct {
	HINTERNET session;
	HINTERNET connect;
	int pr_number;
	unsigned long long last_comment_id;
	char session_id[64];
	char branch[128];
	char default_branch[64];
} gh_ctx_t;

typedef struct {
	unsigned long long id;
	char *body;
} gh_comment_t;

int gh_init(gh_ctx_t *ctx);
void gh_close(gh_ctx_t *ctx);
int gh_http(gh_ctx_t *ctx, const char *method, const char *path, const char *json_body, char **resp, int *status_out);
int gh_bootstrap_pr(gh_ctx_t *ctx);
int gh_post_comment(gh_ctx_t *ctx, const char *body);
int gh_list_comments(gh_ctx_t *ctx, gh_comment_t **out, int *count);
void gh_free_comments(gh_comment_t *comments, int count);
int gh_post_frame(gh_ctx_t *ctx, const unsigned char *data, int datalen);
int gh_wait_frame(gh_ctx_t *ctx, unsigned long long after_id, char *recvBuf, int recvBufMaxLen);
