#pragma once

int b64_encoded_len(int n);
int b64_encode(const unsigned char *in, int inlen, char *out, int outlen);
int b64_decode(const char *in, int inlen, unsigned char *out, int outlen);
