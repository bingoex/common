#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include "shmcommu.h"
#include "procmon.h"
#include "singleton.h"
#include "log.h"
#include "serverbase.h"

#define GROUP_UNSED -1

using namespace std;
using namespace common::singleton;
using namespace common::base::procmon;
using namespace common::base::commu::shmcommu;


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

void CMQCommu::setlog(LogFile * logfile)
{
	this->logfile = logfile;
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

int CProcMonSrv::add_group(const GroupInfo *groupinfo)
{
	//TODO
	if (cur_group_ > MAX_PROC_GROUP_NUM || cur_group_ != groupinfo->groupid_)
		return -1;

	ProcGroupObj * procgroup = &proc_groups_[cur_group_++];
	procgroup->curprocnum_ = 0;
	procgroup->errprocnum_ = 0;
	memcpy(&procgroup->groupinfo_, groupinfo, sizeof(GroupInfo));

	for (int i = 0; i< BUCKET_SIZE; i++)
		INIT_LIST_HEAD(&(procgroup->bucket_[i]));

	return 0;
}

int CProcMonSrv::mod_group(int groupid, const GroupInfo * groupinfo)
{
	if (groupid == cur_group_) {
		add_group(groupinfo);
		return 0;
	}

	ProcGroupObj * procgroup = &proc_groups_[groupid];
	memcpy(&procgroup->groupinfo_, groupinfo, sizeof(GroupInfo));

	return 0;
}

void CProcMonSrv::set_commu(CCommu * commu)
{
	if (commu_)
		delete commu_;

	commu_ = commu;

	do_recv(0);
}

void CProcMonSrv::set_log(LogFile * logfile)
{
	this->logfile = logfile;
}

void CProcMonSrv::run()
{
	do_recv(0);
	do_check();
}

void CProcMonSrv::segv_wait(ProcInfo * procinfo)
{
	//TODO
	// return util the process dead
	return;
}

void CProcMonSrv::process_exception()
{
	// recv the msg
	//  call segv_wait
	//  do event PROCDEAD
}


bool CProcMonSrv::do_recv(long msgtype)
{
	int ret = 0;
	ProcInfo * procinfo = NULL;
	ProcObj * proc = NULL;
	map<int, map<int, int> >::iterator it;

	do {
		ret = commu_->recv(&msg_[0], msgtype);

		if (unlikely(ret > 0)) {

			if ((msg_[0].srctype_ & 0x1) == MSG_SRC_CLIENT) { // from client

				if (EXCEPTION_TYPE(msg_[0].srctype_)) {
					/* TODO handle client exception
					if (_anf_g_exceptionrestart) {
						process_exception();
					}
					coontinue;
					*/
					LOG("process_exception");
				}

				procinfo = (ProcInfo *)(msg_[0].msgcontent_);
				proc = find_proc(procinfo->groupid_, procinfo->procid_);

				// handle old client's heart pkg
				if (likely(proc != NULL)) {
					memcpy(&proc->procinfo_, procinfo, sizeof(ProcInfo));
					LOG("heart groupid %d procid %d ", procinfo->groupid_, procinfo->procid_);
					continue;
				}

				ProcGroupObj * group = find_group(procinfo->groupid_);
				if (NULL == group) {
					LOG("group == NULL groupid %d", procinfo->groupid_);
					continue;
				}

				GroupInfo * reload_group = &group->groupinfo_;
				// handle new client heart pkg
				if (reload_group->reload_ != PROC_RELOAD_WAIT_NEW) {
					LOG("add new client groupid %d procid %d", procinfo->groupid_, procinfo->procid_);
					add_proc(procinfo->groupid_, procinfo);
					continue;
				}

				// TODO handle group reload
				if (reload_group->groupid_ == procinfo->groupid_) {
					if (procinfo->reserve_[0] == 1) {
						continue;
					}
					kill_group(reload_group->groupid_, SIGUSR2);

					reload_group->reload_ = PROC_RELOAD_NORMAL;

					add_proc(procinfo->groupid_, procinfo);
				}

			}
		}
	} while (ret >0);

	return true;
}


bool CProcMonSrv::check_groupbusy(int groupid)
{
	ProcGroupObj *groupobj = &proc_groups_[groupid];
	GroupInfo *group = &(groupobj->groupinfo_);

	if ((Q_STATINFO *)group->q_recv_pstat == NULL) {
		if (group->groupid_ == 0) {
			return false;
		}

		//TODO mac don't support
		//key_t key = pwdtok(group->groupid_ * 2);
		key_t key = group->groupid_ * 2;

		void * pinfo = NULL;
		int shmid_ = -1;
		if ((shmid_ = shmget(key, 0, 0)) < 0) {
			return false;
		}

		pinfo = (void *) shmat(shmid_, NULL, 0);

		if (likely(pinfo != MAP_FAILED)) {
		}else {
			return false;
		}
		group->q_recv_pstat = pinfo;
	}

	Q_STATINFO * q_recv_pstat = (Q_STATINFO *)group->q_recv_pstat;

	int msg_count = atomic_read(&(q_recv_pstat->msg_count));

	if (msg_count < 0) {
		atomic_set(&q_recv_pstat->process_count, 0);
		return false;
	}

	int proc_count = atomic_read(&(q_recv_pstat->process_count));
	
	atomic_set(&q_recv_pstat->process_count, 0);

	if (proc_count < msg_count) {
		return true;
	}

	return false;
}









int main() 
{
	return 0;
}
