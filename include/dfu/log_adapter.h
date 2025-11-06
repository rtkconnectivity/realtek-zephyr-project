#ifndef __LOG_ADAPTER_H__
#define __LOG_ADAPTER_H__

/* Log type definition */
typedef enum
{
    TYPE_UPPERSTACK_RESET       = 0,
    TYPE_UPPERSTACK_FORMAT      = 1,
    TYPE_UPPERSTACK_MESSAGE     = 2,
    TYPE_UPPERSTACK_BINARY      = 3,
    TYPE_UPPERSTACK_STRING      = 4,
    TYPE_UPPERSTACK_BDADDR1     = 5,
    TYPE_UPPERSTACK_BDADDR2     = 6,
    TYPE_UPPERSTACK_RAMDATA1    = 7,
    TYPE_UPPERSTACK_RAMDATA2    = 8,
    TYPE_UPPERSTACK_RAMDATA3    = 9,
    TYPE_UPPERSTACK_RAMDATA4    = 10,
    TYPE_UPPERSTACK_RAMDATA5    = 11,
    TYPE_UPPERSTACK_RAMDATA6    = 12,
    TYPE_UPPERSTACK_RAMDATA7    = 13,
    TYPE_UPPERSTACK_RAMDATA8    = 14,

    TYPE_PLATFORM_DBG_DIRECT    = 16,

    TYPE_RTL8752H               = 37,
    TYPE_RTL87X2G               = 38,
} T_LOG_TYPE;

/* Log type current ic used */
#define LOG_TYPE                (TYPE_RTL8752H)

/* Log subtype definition */
typedef enum
{
    SUBTYPE_DIRECT                  = 0x00,

    SUBTYPE_FORMAT                  = 0x10,
    SUBTYPE_DOWN_MESSAGE            = 0x11,
    SUBTYPE_UP_MESSAGE              = 0x12,

    SUBTYPE_DOWN_SNOOP              = 0x20,
    SUBTYPE_UP_SNOOP                = 0x28,

    SUBTYPE_BDADDR                  = 0x30,

    SUBTYPE_STRING                  = 0x40,

    SUBTYPE_BINARY                  = 0x50,

    SUBTYPE_INDEX                   = 0x60,
    SUBTYPE_STACK_INDEX             = 0x61,

    SUBTYPE_TEXT                    = 0x70,
} T_LOG_SUBTYPE;

#define COMBINE_TRACE_INFO(type, subtype, module, level)  (uint32_t)(((type)<<24) | ((subtype)<<16) | ((module)<<8) | (level))
void log_direct(uint32_t info, const char *fmt, ...);
#define log_direct_retarget     log_direct
#define DBG_DIRECT(...)     do {\
        log_direct_retarget(COMBINE_TRACE_INFO(LOG_TYPE, SUBTYPE_DIRECT, 0, 0), __VA_ARGS__);\
    } while (0)

/* APP module */
#define APP_PRINT_INFO0 LOG_INF
#define APP_PRINT_INFO1 LOG_INF
#define APP_PRINT_INFO2 LOG_INF
#define APP_PRINT_INFO3 LOG_INF
#define APP_PRINT_INFO4 LOG_INF
#define APP_PRINT_INFO5 LOG_INF
#define APP_PRINT_INFO6 LOG_INF

#define APP_PRINT_ERROR0 LOG_ERR
#define APP_PRINT_ERROR1 LOG_ERR
#define APP_PRINT_ERROR2 LOG_ERR
#define APP_PRINT_ERROR3 LOG_ERR
#define APP_PRINT_ERROR4 LOG_ERR
#define APP_PRINT_ERROR5 LOG_ERR
#define APP_PRINT_ERROR6 LOG_ERR

#define APP_PRINT_TRACE0 LOG_INF
#define APP_PRINT_TRACE1 LOG_INF
#define APP_PRINT_TRACE2 LOG_INF
#define APP_PRINT_TRACE3 LOG_INF
#define APP_PRINT_TRACE4 LOG_INF
#define APP_PRINT_TRACE5 LOG_INF

/* DFU module */
#define DFU_PRINT_INFO0 LOG_INF
#define DFU_PRINT_INFO1 LOG_INF
#define DFU_PRINT_INFO2 LOG_INF
#define DFU_PRINT_INFO3 LOG_INF
#define DFU_PRINT_INFO4 LOG_INF
#define DFU_PRINT_INFO5 LOG_INF
#define DFU_PRINT_INFO6 LOG_INF

#define DFU_PRINT_ERROR0 LOG_ERR
#define DFU_PRINT_ERROR1 LOG_ERR
#define DFU_PRINT_ERROR2 LOG_ERR
#define DFU_PRINT_ERROR3 LOG_ERR
#define DFU_PRINT_ERROR4 LOG_ERR

#define DFU_PRINT_TRACE0 LOG_DBG
#define DFU_PRINT_TRACE1 LOG_DBG
#define DFU_PRINT_TRACE2 LOG_DBG
#define DFU_PRINT_TRACE3 LOG_DBG
#define DFU_PRINT_TRACE4 LOG_DBG

#endif /* __LOG_ADAPTER_H__ */
