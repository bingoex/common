#ifndef _BASE_SHMCOMMU_H_
#define _BASE_SHMCOMMU_H_

#include "atomic.h"
#include "commu.h"
#include "singleton.h"

#define LOCK_TYPE_NONE 0x0
#define LOCK_TYPE_PRODUCER 0x1
#define LOCK_TYPE_COMSUMER 0x2

#define COMMU_ERR_SHMGET -101
#define COMMU_ERR_SHMNEW -102
#define COMMU_ERR_SHMMAP -103

#define COMMU_ERR_FILEOPEN -111
#define COMMU_ERR_FILEMAP -112
#define COMMU_ERR_MQFULL -121
#define COMMU_ERR_MQEMPTY -122
#define COMMU_ERR_OTFBUFF -123
#define COMMU_ERR_SHM_NULL -124
#define COMMU_ERR_CHKFAIL -126
#define COMMU_ERR_MSG_TIMEOUT -127
#define COMMU_ERR_FLOCK -201

using namespace common::base::commu;
using namespace common::singleton;

namespace common{
	namespace base {
		namespace commu {
			namespace shmcommu{

				typedef struct tagMQStat {
					unsigned usedlen_;
					unsigned freelen_;
					unsigned totallen_;
					unsigned shmkey_;
					unsigned shmid_;
					unsigned shmsize_;
				} TMQStat;

				typedef struct qstatinfo {
					atomic_t msg_count;
					atomic_t process_count;
					atomic_t flag;
					atomic_t curflow;
				} Q_STATINFO;

				class CShmMQ
				{
					public:
						CShmMQ();
						~CShmMQ();
						int init(int shmkey, int shmsize);
						int clear();
						int enqueue(const void *data, unsigned data_len, unsigned flow);
						int dequeue(void * buf, unsigned buf_size, unsigned &data_len, unsigned &flow);

						inline void * memory()
						{
							return shmmem_;
						}

						inline void getstat(TMQStat &mq_stat)
						{
							unsigned head = *head_;
							unsigned tail = *tail_;

							mq_stat.usedlen_ = (tail >= head) ? tail - head : tail + blocksize_ - head;
							mq_stat.freelen_ = head > tail ? head - tail: head + blocksize_ - tail;
							mq_stat.totallen_ =blocksize_;
							mq_stat.shmkey_ = shmkey_;
							mq_stat.shmid_ = shmid_;
							mq_stat.shmsize_ = shmsize_;
						}

						inline bool __attribute__((always_inline)) isEmpty()
						{
							return pstat_->msg_count == 0;
						}
					protected:
						int shmkey_;
						int shmsize_;
						int shmid_;
						void *shmmem_;

						Q_STATINFO * pstat_;

						volatile unsigned *head_;
						volatile unsigned *tail_;
						volatile pid_t * pid;

						char * block_;
						unsigned blocksize_;

						bool do_check(unsigned head, unsigned tail);
						int getmemory(int shmkey, int shmsize);
						void fini();
					public:
						unsigned msg_timeout;
				};


				class CShmProducer
				{
					public:
						CShmProducer();
						virtual ~CShmProducer();
						virtual int init(int shmkey, int shmsize);
						virtual int clear();

						virtual void set_notify(int fd)
						{
							notify_fd_ = fd;
						}

						virtual int produce(const void *data, unsigned data_len, unsigned flow);

						virtual void getstat(TMQStat &mq_stat) 
						{
							mq_->getstat(mq_stat);
						}

						virtual void fini();

					public:
						CShmMQ * mq_;
						int notify_fd_;
				};

				class CShmProducerL : public CShmProducer
				{
					public:
						CShmProducerL();
						~CShmProducerL();
						int init(int shmkey, int shmsize);
						int clear();
						int produce(const void * data, unsigned data_len, unsigned flow);
					protected:
						int mfd_;
						void fini();
				};

				
				class CShmComsumer
				{
					public:
						CShmComsumer();
						virtual ~CShmComsumer();
						virtual int init(int shmkey, int shmsize);
						virtual int clear();
						virtual inline int comsume(void *buf, unsigned buf_size, unsigned &data_len, unsigned &flow);

						virtual void getstat(TMQStat &mq_stat) 
						{
							mq_->getstat(mq_stat);
						}

						virtual void fini();
					public:
						CShmMQ * mq_;
				};

				class CShmComsumerL : public CShmComsumer
				{
					public:
						CShmComsumerL();
						~CShmComsumerL();

						int init(int shmkey, int shmsize);
						int clear();
					protected:
						int mfd_;
						inline int comsume(void *buf, unsigned buf_size, unsigned &data_len, unsigned &flow);
						void fini();
				};

				typedef struct {
					int shmkey_producer_;
					int shmsize_producer_;
					int shmkey_comsumer_;
					int shmsize_comsumer_;

					int locktype;
					int maxpkg_;
					unsigned msg_timeout;
					int groupid;
					int notifyfd_;
				} ShmCommuConf;

				class CShmCommu : public CCommu
				{
					public:
						CShmCommu();
						~CShmCommu();
						int init(const void * config);
						int clear();
						int poll(bool block = false);
						int sendto(unsigned flow, void *arg1/*data blob*/, void *arg2/**/);
						int ctrl(unsigned flow, ctrl_type type, void *arg1, void *arg2);
						int get_msg_count();
					protected:
						CShmProducer * producer_;
						CShmComsumer * comsumer_;
						unsigned buff_size_;
						blob_type buff_blob_;
						int locktype_;
						unsigned msg_timeout;
						void fini();
				};







			}
		}
	}
}



#endif
