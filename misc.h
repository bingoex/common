#ifndef _MISC_H_
#define _MISC_H_

#include <sys/time.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#define MAXPATHLEN 1024

namespace common
{
	namespace base 
	{
		class CMisc {
			public:
				static unsigned getip(const char *ifname);
				static int check_process_exist(pid_t pid);

				static inline int64_t time_diff(const struct timeval &tv1, const struct timeval &tv2) 
				{
					return ((int64_t)(tv1.tv_sec - tv2.tv_sec)) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000;
				}

				static inline void * realloc_safe(void *ptr, size_t size)
				{
					void *new_ptr = realloc(ptr, size);
					//TODO
					if (NULL == new_ptr) {
						free(ptr);
					}
					return new_ptr;
				}
		};
	}
}

#endif 
