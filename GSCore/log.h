/* log.h - In-memory log (a ring buffer of the last LOG_LINES lines).
 *
 * The log is temporary: it lives inside the Engine and disappears when the
 * program ends. It has its own tiny formatter because the GBA has no printf.
 */
#ifndef LOG_H
#define LOG_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

void log_init(Log *log);

/* Appends one line. Supported conversions: %s (string), %d (int), %%.
 * Anything else is copied as plain text. Lines longer than LOG_LINE_LEN - 1
 * characters are cut. */
void log_addf(Log *log, const char *fmt, ...);

/* Number of lines currently stored (at most LOG_LINES). */
int32_t log_count(const Log *log);

/* Line 0 is the oldest stored line. Returns NULL if index is out of range. */
const char *log_get(const Log *log, int32_t index);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
