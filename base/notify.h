#ifndef _NOTIFY_H_
#define _NOTIFY_H_

#include <stddef.h>

extern int g_anf_shm_fifo;

namespace common {
	namespace base {
		namespace notify {
			class CNotify 
			{
				public:
					static char *get_fifo_path(int key);

					/*
					 * fifoname :output params
					 */
					static int notify_init(int key, char *fifoname = NULL, size_t fifoname_size = 0);

					static int notify_send(int fd);

					static int notify_recv(int fd);
			};
		}
	}
}


#endif
