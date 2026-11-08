/* log.c - In-memory log. No C library calls. */
#include "log.h"
#include <stdarg.h>

void log_init(Log *log) {
    for (int32_t i = 0; i < LOG_LINES; i++) {
        log->lines[i][0] = '\0';
    }
    log->total = 0;
}

/* Writes the decimal text of value at out, never going past end.
 * Returns the new write position. Dividing by the constant 10 compiles to a
 * multiplication, so no software division routine is needed on the GBA. */
static char *put_int(char *out, char *end, int32_t value) {
    char digits[12];
    int32_t count = 0;
    uint32_t magnitude;

    if (value < 0) {
        if (out < end) {
            *out++ = '-';
        }
        magnitude = (uint32_t)0 - (uint32_t)value; /* safe for INT32_MIN */
    } else {
        magnitude = (uint32_t)value;
    }

    do {
        digits[count++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u);

    while (count > 0 && out < end) {
        *out++ = digits[--count];
    }
    return out;
}

void log_addf(Log *log, const char *fmt, ...) {
    char *line = log->lines[log->total % LOG_LINES];
    char *out = line;
    char *end = line + (LOG_LINE_LEN - 1); /* leave room for the '\0' */
    va_list args;

    va_start(args, fmt);
    while (*fmt != '\0') {
        if (*fmt == '%' && (fmt[1] == 's' || fmt[1] == 'd' || fmt[1] == '%')) {
            char spec = fmt[1];
            fmt += 2;
            if (spec == 's') {
                const char *text = va_arg(args, const char *);
                if (text == NULL) {
                    text = "(null)";
                }
                while (*text != '\0' && out < end) {
                    *out++ = *text++;
                }
            } else if (spec == 'd') {
                out = put_int(out, end, (int32_t)va_arg(args, int));
            } else if (out < end) {
                *out++ = '%';
            }
        } else {
            if (out < end) {
                *out++ = *fmt;
            }
            fmt++;
        }
    }
    va_end(args);

    *out = '\0';
    log->total++;
}

int32_t log_count(const Log *log) {
    return (log->total < LOG_LINES) ? (int32_t)log->total : (int32_t)LOG_LINES;
}

const char *log_get(const Log *log, int32_t index) {
    int32_t count = log_count(log);
    uint32_t first;

    if (index < 0 || index >= count) {
        return NULL;
    }
    first = log->total - (uint32_t)count;
    return log->lines[(first + (uint32_t)index) % LOG_LINES];
}
