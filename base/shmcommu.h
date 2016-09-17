#ifndef _BASE_SHMCOMMU_H_
#define _BASE_SHMCOMMU_H_

#include "atomic.h"

namespace common{
	namespace base {
		namespace commu {
			namespace shmcommu{

				typedef struct qstatinfo {
					atomic_t msg_count;
					atomic_t process_count;
					atomic_t flag;
					atomic_t curflow;
				} Q_STATINFO;




			}
		}
	}
}



#endif
