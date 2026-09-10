#pragma once

int json_get_string(const char *json, const char *key, char *out, int outlen);
int json_get_u32(const char *json, const char *key, unsigned long *out);
int json_get_u64(const char *json, const char *key, unsigned long long *out);
char *json_dup_string(const char *json, const char *key);
int json_escape(const char *in, char *out, int outlen);
int json_escape_len(const char *in);
