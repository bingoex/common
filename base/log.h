#ifndef _BASE_LOG_H_
#define _BASE_LOG_H_

#include "cm_log.h"

#define LOG(fmt, args...) \
	do { \
		Log(logfile, LOG_FORMAT_TYPE_TIME, "[%s][%d]:%s() "fmt, \
				__FILE__, __LINE__, __FUNCTION__, ## args);\
	}while(0) \


#endif
