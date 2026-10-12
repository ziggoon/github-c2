#pragma once

/*
 * Beacon UDC2 frames match Cobalt-Strike/udc2-vs:
 *   | 4-byte little-endian payload length | payload |
 *
 * sendBuf from Beacon is already a complete frame. recvBuf must be written
 * the same way, including the length prefix. GitHub comments only carry
 * sequential slices of that unmodified frame (base64), because issue
 * comments are size-capped. They are not a second packet format.
 */
#define FRAME_LENGTH     4
#define CHUNK_DATA_MAX   32000
#define PROXY_POLL_MS    2000
#define PROXY_MAX_TRIES  90
