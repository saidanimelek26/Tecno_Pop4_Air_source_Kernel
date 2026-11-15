#ifndef __FF_LOGGING_H__
#define __FF_LOGGING_H__

#define LOG_TAG "ff_ctl"

typedef enum {
	FF_LOG_LEVEL_ALL = 0,
	FF_LOG_LEVEL_VBS = 1,
	FF_LOG_LEVEL_DBG = 2,
	FF_LOG_LEVEL_INF = 3,
	FF_LOG_LEVEL_WRN = 4,
	FF_LOG_LEVEL_ERR = 5,
} ff_log_level_t;

#ifndef __FF_EARLY_LOG_LEVEL
#define __FF_EARLY_LOG_LEVEL FF_LOG_LEVEL_ALL
#endif

extern ff_log_level_t g_log_level;

#define FF_LOGV(...)                                               \
	do {                                                       \
		if (g_log_level <= FF_LOG_LEVEL_VBS) {             \
			ff_log_printf(FF_LOG_LEVEL_VBS, LOG_TAG,   \
				      __VA_ARGS__);                \
		}                                                  \
	} while (0)

#define FF_LOGD(...)                                               \
	do {                                                       \
		if (g_log_level <= FF_LOG_LEVEL_DBG) {             \
			ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG,   \
				      __VA_ARGS__);                \
		}                                                  \
	} while (0)

#define FF_LOGI(...)                                               \
	do {                                                       \
		if (g_log_level <= FF_LOG_LEVEL_INF) {             \
			ff_log_printf(FF_LOG_LEVEL_INF, LOG_TAG,   \
				      __VA_ARGS__);                \
		}                                                  \
	} while (0)

#define FF_LOGW(...)                                               \
	do {                                                       \
		if (g_log_level <= FF_LOG_LEVEL_WRN) {             \
			ff_log_printf(FF_LOG_LEVEL_WRN, LOG_TAG,   \
				      __VA_ARGS__);                \
		}                                                  \
	} while (0)

#define FF_LOGE(format, ...)                                       \
	do {                                                       \
		if (g_log_level <= FF_LOG_LEVEL_ERR) {             \
			const char *__fname__ = __FILE__;          \
			const char *s = __fname__;                 \
			do {                                       \
				if (*s == '/')                     \
					__fname__ = s + 1;         \
			} while (*s++);                            \
			ff_log_printf(FF_LOG_LEVEL_ERR, LOG_TAG,   \
				      "error at %s[%s:%d]: " format, \
				      __func__, __fname__,         \
				      __LINE__, ##__VA_ARGS__);    \
		}                                                  \
	} while (0)

int ff_log_printf(ff_log_level_t level, const char *tag, const char *fmt, ...);

#endif
