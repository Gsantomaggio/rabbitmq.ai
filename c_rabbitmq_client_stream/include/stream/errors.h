#ifndef STREAM_ERRORS_H
#define STREAM_ERRORS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Protocol-level response codes ------------------------------------------------ */
#define RESPONSE_CODE_OK                           ((uint16_t)0x0001)
#define RESPONSE_CODE_STREAM_DOES_NOT_EXIST        ((uint16_t)0x0002)
#define RESPONSE_CODE_SUBSCRIPTION_ID_EXISTS       ((uint16_t)0x0003)
#define RESPONSE_CODE_SUBSCRIPTION_ID_NOT_EXIST    ((uint16_t)0x0004)
#define RESPONSE_CODE_STREAM_ALREADY_EXISTS        ((uint16_t)0x0005)
#define RESPONSE_CODE_STREAM_NOT_AVAILABLE         ((uint16_t)0x0006)
#define RESPONSE_CODE_SASL_MECHANISM_NOT_SUPPORTED ((uint16_t)0x0007)
#define RESPONSE_CODE_AUTH_FAILURE                 ((uint16_t)0x0008)
#define RESPONSE_CODE_SASL_ERROR                   ((uint16_t)0x0009)
#define RESPONSE_CODE_SASL_CHALLENGE               ((uint16_t)0x000a)
#define RESPONSE_CODE_SASL_AUTH_FAILURE_LOOPBACK   ((uint16_t)0x000b)
#define RESPONSE_CODE_VIRTUAL_HOST_ACCESS_FAILURE  ((uint16_t)0x000c)
#define RESPONSE_CODE_UNKNOWN_FRAME                ((uint16_t)0x000d)
#define RESPONSE_CODE_FRAME_TOO_LARGE              ((uint16_t)0x000e)
#define RESPONSE_CODE_INTERNAL_ERROR               ((uint16_t)0x000f)
#define RESPONSE_CODE_ACCESS_REFUSED               ((uint16_t)0x0010)
#define RESPONSE_CODE_PRECONDITION_FAILED          ((uint16_t)0x0011)
#define RESPONSE_CODE_PUBLISHER_DOES_NOT_EXIST     ((uint16_t)0x0012)
#define RESPONSE_CODE_NO_OFFSET                    ((uint16_t)0x0013)

/* Client-level error categories ------------------------------------------------ */
typedef enum {
    STREAM_ERR_NONE           = 0,
    STREAM_ERR_CONNECTION     = 1,
    STREAM_ERR_AUTHENTICATION = 2,
    STREAM_ERR_STREAM_OP      = 3,
    STREAM_ERR_PROTOCOL       = 4,
    STREAM_ERR_OOM            = 5,
} stream_err_kind_t;

typedef struct {
    stream_err_kind_t kind;
    uint16_t          response_code; /* protocol response code (0 for non-protocol errors) */
    char              message[256];
} stream_err_t;

static inline stream_err_t stream_err_ok(void) {
    stream_err_t e;
    memset(&e, 0, sizeof(e));
    return e;
}

static inline int stream_err_is_ok(stream_err_t e) {
    return e.kind == STREAM_ERR_NONE;
}

static inline stream_err_t stream_err_make(stream_err_kind_t kind,
                                           uint16_t          response_code,
                                           const char*       msg)
{
    stream_err_t e;
    e.kind          = kind;
    e.response_code = response_code;
    snprintf(e.message, sizeof(e.message), "%s", msg);
    return e;
}

#define STREAM_ERR_CONNECTION_MSG(msg) \
    stream_err_make(STREAM_ERR_CONNECTION, 0, "connection error: " msg)
#define STREAM_ERR_AUTH(code, msg) \
    stream_err_make(STREAM_ERR_AUTHENTICATION, (code), "authentication error: " msg)
#define STREAM_ERR_STREAM(code, msg) \
    stream_err_make(STREAM_ERR_STREAM_OP, (code), "stream error: " msg)
#define STREAM_ERR_PROTOCOL_MSG(msg) \
    stream_err_make(STREAM_ERR_PROTOCOL, 0, "protocol error: " msg)
#define STREAM_ERR_OOM_MSG() \
    stream_err_make(STREAM_ERR_OOM, 0, "out of memory")

#endif /* STREAM_ERRORS_H */
