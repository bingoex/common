#include "serverbase.h"



#define C_PUB_HEAD_SIZE 8
#define C_HEAD_SIZE C_PUB_HEAD_SIZE
#define C_SAFE_AREA_LEN 4


#define C_SAFE_AREA_VAL 0x58464E41 //"ANFX"  X == 0x58

CShmMQ::CShmMQ(): shmkey_(0), shmsize_(0), shmid_(0),
	shmmem_(NULL), pstat_(NULL), head_(NULL), tail_(NULL), block_(NULL), blocksize_(0)
{
	msg_timeout = 0;
}

CShmMQ::~CShmMQ()
{
	fini();
}

int CShmMQ::getmemory(int shmkey, int shmsize)
{
	shmsize += sizeof(Q_STATINFO);

	if ((shmid_ = shmget(shmkey, shmsize, 0)) != -1) {
		shmmem_ = shmat(shmid_, NULL, 0);

		if (likely(shmmem_ != MAP_FAILED)) {
			pstat_ = (Q_STATINFO *)shmmem_;
			shmmem_ = (void *)((unsigned long)shmmem_ + sizeof(Q_STATINFO));
			return 0;
		}
		else {
			fprintf(stderr, "Shm map failed key %d size %d", shmkey_, shmsize_);
			return COMMU_ERR_SHMMAP;
		}
	}
	else {
		shmid_ =shmget(shmkey, shmsize, IPC_CREAT | 0666);

		if (likely(shmid_ != -1)) {
			shmmem_ = shmat(shmid_, NULL, 0);

			if (likely(shmmem_ != MAP_FAILED)) {
				pstat_ = (Q_STATINFO *)shmmem_;
				shmmem_ = (void *)((unsigned long)shmmem_ + sizeof(Q_STATINFO));
				return 1;
			}
			else {
				fprintf(stderr, "Shm map failed key %d size %d", shmkey_, shmsize_);
				return COMMU_ERR_SHMMAP;
			}
		}
		else {
			fprintf(stderr, "Shm create failed key %d size %d", shmkey_, shmsize_);
			return COMMU_ERR_SHMNEW;
		}
	}
}

void CShmMQ::fini()
{
	if (pstat_)
		shmdt((const void *)pstat_);
}

int CShmMQ::init(int shmkey, int shmsize)
{
	fini();

	shmkey_ = shmkey;
	shmsize_ = shmsize;

	int ret = 0;

	if ((ret = getmemory(shmkey_, shmsize_)) > 0) {
		memset(shmmem_, 0x0, sizeof(unsigned) * 2);
	}
	else if (ret < 0) {
		switch(ret) {
			case COMMU_ERR_SHMMAP:
				break;
			case COMMU_ERR_SHMNEW:
				break;
			default:
				fprintf(stderr, "CShmMQ::getmemory() return %d, can not be recognised\n", ret);
		}
		return ret
	}

	head_ = (unsigned *)shmmem_;
	tail_ = head + 1;
	pid_= (pid_t *)(tail_ + 1);
	block_ = (char *)(pid_ + 1);
	blocksize_ = shmsize_ - sizeof(unsigned) * 2 - sizeof(pid_t);

	*pid_ = 0;

	return 0;
}

int CShmMQ::clear()
{
	if (!shmmem_) {
		fprintf(stderr, "shmmem_ is NULL\n");
		return COMMU_ERR_SHM_NULL;
	}

	*pid = getpid();
	*head_ = 0;
	*tail_ = 0;
	*pid_ = 0;

	pstat_->msg_count = 0;

	return 0;
}

