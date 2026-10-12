#include "compat.h"

void *xmalloc(size_t n)
{
	if (n == 0) {
		n = 1;
	}
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
}

void xfree(void *p)
{
	if (p) {
		HeapFree(GetProcessHeap(), 0, p);
	}
}

int ascii_to_wide(const char *src, wchar_t *dst, int dst_wchars)
{
	int i = 0;

	if (!src || !dst || dst_wchars <= 0) {
		return 0;
	}
	while (src[i] && i < dst_wchars - 1) {
		dst[i] = (wchar_t)(unsigned char)src[i];
		i++;
	}
	dst[i] = 0;
	return i;
}

void skip_ws(const char **pp)
{
	const char *p = *pp;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
		p++;
	}
	*pp = p;
}

int random_bytes(void *buf, unsigned long n)
{
	if (!buf || n == 0) {
		return -1;
	}
	if (!SystemFunction036(buf, n)) {
		return -1;
	}
	return 0;
}

void bytes_to_hex(const unsigned char *in, int n, char *out)
{
	static const char hex[] = "0123456789abcdef";
	int i;

	for (i = 0; i < n; i++) {
		out[i * 2] = hex[(in[i] >> 4) & 0xf];
		out[i * 2 + 1] = hex[in[i] & 0xf];
	}
	out[n * 2] = 0;
}
