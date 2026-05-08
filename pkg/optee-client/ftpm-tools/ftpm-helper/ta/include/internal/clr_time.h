#ifndef __CLR_TIME_H__
#define __CLR_TIME_H__

#include <stdint.h>

typedef int64_t time_t;

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};


struct tm* localtime (const time_t * const	timep);
struct tm *localtime_r (const time_t * const timep, struct tm * tmp);

struct tm *gmtime_r (const time_t * const timep,  struct tm * tmp);
struct tm *gmtime (const time_t * const timep);

time_t mktime(struct tm * const	tmp);

#endif
