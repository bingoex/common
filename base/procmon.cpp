#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#include "procmon.h"
#include "singleton.h"
#include "log.h"
#include "serverbase.h"

#define GROUP_UNSED -1

using namespace common::singleton;
using namespace common::base::procmon;


CMQCommu::CMQCommu():mqid_(0)
{
}

CMQCommu::CMQCommu(int mqkey):mqid_(0)
{
	init((void *)&mqkey);
}

CMQCommu::~CMQCommu()
{
}

int CMQCommu::init(void *args)
{
	int mqkey = *((int *)args);
	mqid_ = msgget(mqkey, IPC_CREAT | 0666);

	if (mqid_ < 0) {
		LOG("message queue create failed mqkey(%d) ret %d", mqkey, mqid_);
		printf("message queue create failed mqkey(%d) ret %d\n", mqkey, mqid_);
		exit(-1);
	}

	return 0;
}

void CMQCommu::fini()
{
	if (mqid_ > 0) {
		msgctl(mqid_, IPC_RMID, NULL);
	}
}

int CMQCommu::recv(ProcMonMsg *msg, long msgtype) {
	int ret = 0;

	do {
		ret = msgrcv(mqid_, (void *)msg, sizeof(ProcMonMsg) - sizeof(long), msgtype, IPC_NOWAIT);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

int CMQCommu::send(ProcMonMsg *msg)
{
	int ret = 0;
	msg->timestamp_ = time(NULL);

	while((ret = msgsnd(mqid_, (const void *)msg, msg->msglen_, IPC_NOWAIT) != 0) && (errno == EINTR));

	return ret;
}



#define ADJUST_PROC_DELAY 15
#define ADJUST_PROC_CYCLE 1200
#define MIN_NOTIFY_TIME_CYCLE 30

CProcMonSrv::CProcMonSrv(): commu_(NULL)
{
	cur_group_ = 0;
	memset(proc_groups_, 0x0, sizeof(ProcGroupObj) * MAX_PROC_GROUP_NUM);

	for (int i = 0; i < MAX_PROC_GROUP_NUM; i++)
		proc_groups_[i].curprocnum_ = GROUP_UNSED;

	msg_[1].msglen_ = (long)(((ProcMonMsg *)0)->msgcontent_) + sizeof(ProcInfo) - sizeof(long);
	msg_[1].srctype_ = (MSG_VERSION << 1) | MSG_SRC_SERVER;
}

CProcMonSrv::~CProcMonSrv()
{
	ProcGroupObj * group;
	ProcObj * proc = NULL;
	ProcObj * prev = NULL;

	for (int i = 0; i < cur_group_; i++) {
		group = &proc_groups_[i];

		if (group->curprocnum_ != GROUP_UNSED) {
			for (int j = 0; j < BUCKET_SIZE; j++) {
				prev = NULL;
				list_for_each_entry(proc, &group->bucket_[j], list_)
				{
					if (likely(prev)) {
						delete prev;
					}

					prev = proc;
				}

				if (likely(prev))
					delete prev;
			}
		}
	}

	if (likely(commu_)){
		commu_->fini();
		delete commu_;
	}
}

















int main() 
{
	return 0;
}
