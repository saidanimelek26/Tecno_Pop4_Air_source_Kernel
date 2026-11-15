#ifndef __FF_ERROR_H__
#define __FF_ERROR_H__

#include "ff_log.h"

#define FF_CHECK_ERR(expr)                                             \
	do {                                                           \
		int __e = (expr);                                      \
		if (__e) {                                             \
			FF_LOGE("'%s'.", ff_err_strerror(__e));        \
			return (__e);                                  \
		}                                                      \
	} while (0)

#define FF_CHECK_PTR(ptr)                                              \
	do {                                                           \
		if ((ptr) == NULL) {                                   \
			FF_CHECK_ERR(FF_ERR_NULL_PTR);                 \
		}                                                      \
	} while (0)

typedef enum {
	FF_SUCCESS      =  0,
	FF_ERR_INTERNAL = -1,
	FF_ERR_NOENT    = -2,
	FF_ERR_INTR     = -4,
	FF_ERR_IO       = -5,
	FF_ERR_AGAIN    = -11,
	FF_ERR_NOMEM    = -12,
	FF_ERR_BUSY     = -16,
	FF_ERR_BAD_PARAMS   = -200,
	FF_ERR_NULL_PTR     = -201,
	FF_ERR_BUF_OVERFLOW = -202,
	FF_ERR_BAD_PROTOCOL = -203,
	FF_ERR_SENSOR_SIZE  = -204,
	FF_ERR_NULL_DEVICE  = -205,
	FF_ERR_DEAD_DEVICE  = -206,
	FF_ERR_REACH_LIMIT  = -207,
	FF_ERR_REE_TEMPLATE = -208,
	FF_ERR_NOT_TRUSTED  = -209,
} ff_err_t;

const char *ff_err_strerror(int err);

#endif
