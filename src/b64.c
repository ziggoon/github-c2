#include "b64.h"
#include "compat.h"

static const char B64_ALPH[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int b64_encoded_len(int n)
{
	return ((n + 2) / 3) * 4;
}

int b64_encode(const unsigned char *in, int inlen, char *out, int outlen)
{
	int i = 0;
	int o = 0;
	int need = b64_encoded_len(inlen);

	if (!in || !out || outlen < need + 1) {
		return -1;
	}
	while (i < inlen) {
		unsigned int v = ((unsigned int)in[i]) << 16;
		if (i + 1 < inlen) {
			v |= ((unsigned int)in[i + 1]) << 8;
		}
		if (i + 2 < inlen) {
			v |= (unsigned int)in[i + 2];
		}
		out[o++] = B64_ALPH[(v >> 18) & 63];
		out[o++] = B64_ALPH[(v >> 12) & 63];
		out[o++] = (i + 1 < inlen) ? B64_ALPH[(v >> 6) & 63] : '=';
		out[o++] = (i + 2 < inlen) ? B64_ALPH[v & 63] : '=';
		i += 3;
	}
	out[o] = 0;
	return o;
}

static int b64_val(char c)
{
	if (c >= 'A' && c <= 'Z') {
		return c - 'A';
	}
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 26;
	}
	if (c >= '0' && c <= '9') {
		return c - '0' + 52;
	}
	if (c == '+') {
		return 62;
	}
	if (c == '/') {
		return 63;
	}
	return -1;
}

int b64_decode(const char *in, int inlen, unsigned char *out, int outlen)
{
	int i = 0;
	int o = 0;

	if (!in || !out) {
		return -1;
	}
	while (i < inlen) {
		int a, b, c, d;
		unsigned int v;

		while (i < inlen && (in[i] == '\n' || in[i] == '\r' || in[i] == ' ' || in[i] == '\t')) {
			i++;
		}
		if (i >= inlen) {
			break;
		}
		a = b64_val(in[i++]);
		while (i < inlen && (in[i] == '\n' || in[i] == '\r' || in[i] == ' ' || in[i] == '\t')) {
			i++;
		}
		b = (i < inlen) ? b64_val(in[i++]) : -1;
		while (i < inlen && (in[i] == '\n' || in[i] == '\r' || in[i] == ' ' || in[i] == '\t')) {
			i++;
		}
		if (i < inlen && in[i] == '=') {
			c = -2;
			i++;
		} else {
			c = (i < inlen) ? b64_val(in[i++]) : -1;
		}
		while (i < inlen && (in[i] == '\n' || in[i] == '\r' || in[i] == ' ' || in[i] == '\t')) {
			i++;
		}
		if (i < inlen && in[i] == '=') {
			d = -2;
			i++;
		} else {
			d = (i < inlen) ? b64_val(in[i++]) : -1;
		}
		if (a < 0 || b < 0) {
			return -1;
		}
		v = ((unsigned int)a << 18) | ((unsigned int)b << 12);
		if (c >= 0) {
			v |= ((unsigned int)c << 6);
		}
		if (d >= 0) {
			v |= (unsigned int)d;
		}
		if (o >= outlen) {
			return -1;
		}
		out[o++] = (unsigned char)((v >> 16) & 0xff);
		if (c >= 0) {
			if (o >= outlen) {
				return -1;
			}
			out[o++] = (unsigned char)((v >> 8) & 0xff);
		}
		if (d >= 0) {
			if (o >= outlen) {
				return -1;
			}
			out[o++] = (unsigned char)(v & 0xff);
		}
	}
	return o;
}
