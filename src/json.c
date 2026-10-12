#include "json.h"
#include "compat.h"

static const char *find_key(const char *json, const char *key)
{
	size_t klen;
	const char *p;

	if (!json || !key) {
		return NULL;
	}
	klen = strlen(key);
	p = json;
	while ((p = strstr(p, key)) != NULL) {
		if (p > json && *(p - 1) == '"' && p[klen] == '"') {
			const char *q = p + klen + 1;
			skip_ws(&q);
			if (*q == ':') {
				q++;
				skip_ws(&q);
				return q;
			}
		}
		p++;
	}
	return NULL;
}

int json_get_string(const char *json, const char *key, char *out, int outlen)
{
	const char *v;
	int i = 0;

	if (out && outlen > 0) {
		out[0] = 0;
	}
	if (!json || !key || !out || outlen <= 0) {
		return 0;
	}
	v = find_key(json, key);
	if (!v || *v != '"') {
		return 0;
	}
	v++;
	while (*v && i < outlen - 1) {
		if (*v == '\\' && v[1]) {
			v++;
			if (*v == 'n') {
				out[i++] = '\n';
			} else if (*v == 'r') {
				out[i++] = '\r';
			} else if (*v == 't') {
				out[i++] = '\t';
			} else {
				out[i++] = *v;
			}
			v++;
		} else if (*v == '"') {
			break;
		} else {
			out[i++] = *v++;
		}
	}
	out[i] = 0;
	return 1;
}

static int parse_u64(const char *s, unsigned long long *out)
{
	unsigned long long n = 0;

	if (!s || !out || *s < '0' || *s > '9') {
		return 0;
	}
	while (*s >= '0' && *s <= '9') {
		unsigned long long d = (unsigned long long)(*s - '0');

		if (n > (18446744073709551615ULL - d) / 10ULL) {
			return 0;
		}
		n = n * 10ULL + d;
		s++;
	}
	*out = n;
	return 1;
}

int json_get_u32(const char *json, const char *key, unsigned long *out)
{
	unsigned long long n = 0;

	if (!out) {
		return 0;
	}
	*out = 0;
	if (!json_get_u64(json, key, &n) || n > 0xffffffffULL) {
		return 0;
	}
	*out = (unsigned long)n;
	return 1;
}

int json_get_u64(const char *json, const char *key, unsigned long long *out)
{
	const char *v;

	if (!out) {
		return 0;
	}
	*out = 0;
	v = find_key(json, key);
	if (!v) {
		return 0;
	}
	return parse_u64(v, out);
}

char *json_dup_string(const char *json, const char *key)
{
	char *buf;
	int cap = 256;

	for (;;) {
		buf = (char *)xmalloc((size_t)cap);
		if (!buf) {
			return NULL;
		}
		if (!json_get_string(json, key, buf, cap)) {
			xfree(buf);
			return NULL;
		}
		if ((int)strlen(buf) < cap - 1) {
			return buf;
		}
		xfree(buf);
		if (cap >= 262144) {
			return NULL;
		}
		cap *= 2;
	}
}

int json_escape_len(const char *in)
{
	int n = 0;

	if (!in) {
		return 1;
	}
	while (*in) {
		char c = *in++;
		if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
			n += 2;
		} else if ((unsigned char)c < 0x20) {
			n += 6;
		} else {
			n += 1;
		}
	}
	return n + 1;
}

int json_escape(const char *in, char *out, int outlen)
{
	int o = 0;
	static const char hex[] = "0123456789abcdef";

	if (!in || !out || outlen <= 0) {
		return -1;
	}
	while (*in && o < outlen - 1) {
		unsigned char c = (unsigned char)*in++;
		if (c == '"' || c == '\\') {
			if (o + 2 >= outlen) {
				break;
			}
			out[o++] = '\\';
			out[o++] = (char)c;
		} else if (c == '\n') {
			if (o + 2 >= outlen) {
				break;
			}
			out[o++] = '\\';
			out[o++] = 'n';
		} else if (c == '\r') {
			if (o + 2 >= outlen) {
				break;
			}
			out[o++] = '\\';
			out[o++] = 'r';
		} else if (c == '\t') {
			if (o + 2 >= outlen) {
				break;
			}
			out[o++] = '\\';
			out[o++] = 't';
		} else if (c < 0x20) {
			if (o + 6 >= outlen) {
				break;
			}
			out[o++] = '\\';
			out[o++] = 'u';
			out[o++] = '0';
			out[o++] = '0';
			out[o++] = hex[(c >> 4) & 0xf];
			out[o++] = hex[c & 0xf];
		} else {
			out[o++] = (char)c;
		}
	}
	out[o] = 0;
	return o;
}
