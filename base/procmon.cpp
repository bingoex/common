#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

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

bool CProcMonSrv::do_check()
{
	ProcObj * proc = NULL;
	ProcGroupObj * group = NULL;
	int i, j;

	for (i = 0; i < cur_group_; i++) {
		group = &proc_groups_[i];
		if (group->curprocnum_ != GROUP_UNSED) {
			//TODO
			check_group(&group->groupinfo_, group->curprocnum_);

			//TODO check if need group reload
			GroupInfo * reload_group = &group->groupinfo_;
			if (reload_group->reload_ > 0) {
				// hort reload
				if (reload_group->reload_ == PROC_RELOAD_WAIT_NEW) {
					if (time(NULL) - reload_group->reload_time > 60) {
						LOG("reload new proc start failed, kill cur group %d", i);
						kill_group(reload_group->groupid_, SIGUSR1);
						reload_group->reload_ = 0;
					} else {
						LOG("wait for new proc start...");
					}

					continue;
				}

				for(j = 0; j < BUCKET_SIZE; j++) {
					list_for_each_entry(proc, &group->bucket_[j], list_)
					{
						do_fork(reload_group->basepath_, reload_group->exefile_, reload_group->etcfile_, 1, 
								reload_group->group_type_, reload_group->affinity_);
					}
				}
				reload_group->reload_ = PROC_RELOAD_WAIT_NEW;
			}
			else {
				for (j = 0; j < BUCKET_SIZE; j++) {
					list_for_each_entry(proc, &group->bucket_[j], list_)
					{
						if (unlikely(check_proc(&group->groupinfo_, &proc->procinfo_))) {
							//some proc has bean deleted , go back to list head & travel again
							j--;
							break;
						}
					}
				}
			}
		}
	}

	return true;
}

int CProcMonSrv::dump_pid_list(char *buf, int len)
{
#define WRITE_BUF(fmt, args...) \
	do { \
		used += snprintf(buf + used, len - used, fmt, ##args); \
	} while(0)

	ProcObj *proc = NULL;
	ProcGroupObj * group = NULL;
	static int mypid = getpid();
	int used = 0;

	WRITE_BUF("%d [ANF_CTRL]\n", mypid);
	for (int i = 0; i < cur_group_; i++){
		group = &proc_groups_[i];
		if (group->curprocnum_ == GROUP_UNSED)
			continue;

		WRITE_BUF("\nGroupId[%d]\n", i);
		for (int j = 0; j < BUCKET_SIZE; j++) {
			list_for_each_entry(proc, &group->bucket_[j], list_)
			{
				WRITE_BUF("%d\n", proc->procinfo_.procid_);
			}
		}
	}

	return used;
}


void CProcMonSrv::killall(int signo)
{
	ProcObj *proc = NULL;
	ProcGroupObj * group = NULL;

	for (int i = 0; i < cur_group_; i++){
		group = &proc_groups_[i];

		if (group->curprocnum_ == GROUP_UNSED)
			continue;

		for (int j = 0; j < BUCKET_SIZE; j++) {
			list_for_each_entry(proc, &group->bucket_[j], list_)
			{
				do_kill(proc->procinfo_.procid_, signo);
			}
		}
	}
}

void CProcMonSrv::kill_group(int grp_id, int signo)
{
	ProcObj *proc = NULL;
	ProcGroupObj * group = NULL;
	map<int , int> proc_value;

	for (int i = 0; i < cur_group_; i++){
		group = &proc_groups_[i];

		if (group->curprocnum_ != GROUP_UNSED && group->groupinfo_.groupid_ == grp_id) {
			for (int j = 0; j < BUCKET_SIZE; j++) {
				list_for_each_entry(proc, &group->bucket_[j], list_)
				{
					proc_value[proc->procinfo_.procid_] = 1;
				}
			}

			break;
		}
	}

	map<int, int>::iterator p;
	for (p = proc_value.begin(); p != proc_value.end(); p++) {
		do_kill(p->first, signo);

		del_proc(grp_id, p->first);
	}
}


int CProcMonSrv::add_proc(int groupid, const ProcInfo * procinfo)
{
	if (groupid >= cur_group_)
		return -1;

	ProcObj * proc = new ProcObj;

	memcpy(&proc->procinfo_, procinfo, sizeof(ProcInfo));
	proc->status_ = PROCMON_STATUS_OK;
	INIT_LIST_HEAD(&proc->list_);

	ProcGroupObj * group = &proc_groups_[groupid];
	list_add(&proc->list_, &group->bucket_[procinfo->procid_ % BUCKET_SIZE]);
	group->curprocnum_++;

	return 0;
}

ProcGroupObj * CProcMonSrv::find_group(int groupid)
{
	ProcGroupObj * group = NULL;

	for (int i = 0; i < cur_group_; i++) {
		group = &proc_groups_[i];

		if (group->groupinfo_.groupid_ == groupid && group->curprocnum_ != GROUP_UNSED) {
			return group;
		}
	}

	return NULL;
}

ProcObj * CProcMonSrv::find_proc(int groupid, int procid)
{
	if (groupid >= cur_group_)
		return NULL;

	ProcGroupObj * group = &proc_groups_[groupid];
	int bucket = procid % BUCKET_SIZE;
	ProcObj * proc = NULL;
	list_for_each_entry(proc, &group->bucket_[bucket], list_)
	{
		if (proc->procinfo_.procid_ == procid)
			return proc;
	}

	return NULL;
}

void CProcMonSrv::del_proc(int groupid, int procid)
{
	if (groupid >= cur_group_)
		return;

	ProcGroupObj * group = &proc_groups_[groupid];
	int bucket = procid % BUCKET_SIZE;
	ProcObj * proc = NULL;
	list_for_each_entry(proc, &group->bucket_[bucket], list_)
	{
		if (proc->procinfo_.procid_ == procid) {
			list_del(&proc->list_);
			delete proc;
			break;
		}
	}

	group->curprocnum_--;
}

void CProcMonSrv::check_group(GroupInfo * group, int curprocnum)
{
	int event = 0;
	int procdiff = 0;
	time_t now = time(NULL);

	if (group->adjust_proc_time + ADJUST_PROC_CYCLE > now ) {
		return;
	}

	if (unlikely((procdiff = (int)group->minprocnum_ - curprocnum) > 0)) {
		LOG("group %d current proc %d, fork %d process", group->groupid_, curprocnum, procdiff);
		event |= PROCMON_EVENT_PROCDOWN;
	} 
	else if (unlikely(procdiff = curprocnum - (int)group->maxprocnum_) > 0) {
		LOG("group %d current proc %d, kill %d process", group->groupid_, curprocnum, procdiff);
		event |= PROCMON_EVENT_PROCUP;
	}
	else {
		if (!check_groupbusy(group->groupid_)) {
			if (((int)group->minprocnum_ < curprocnum)) {
				LOG("group %d is not bus, current proc %d, kill 1 process", group->groupid_, curprocnum);
				event |= PROCMON_EVENT_PROCUP;
				procdiff = 1;
			}
		} else {
			if (((int)group->maxprocnum_ > curprocnum)) {
				LOG("group %d is busy current proc %d, fork 1 process", group->groupid_, curprocnum);
				event |= PROCMON_EVENT_PROCDOWN;
				procdiff = 1;
			}
		}
	}

	if (unlikely(event != 0)) {
		group->adjust_proc_time = now;
		do_event(event, (void *)&procdiff, (void *)group);
	}
}

bool CProcMonSrv::check_proc(GroupInfo * group, ProcInfo * proc)
{
	int event = 0;
	time_t now = time(NULL);

	if (unlikely(proc->timestamp_ < now - (time_t)group->heartbeat_)) {
		LOG("group %d pid %d is dead, no heartbeat", group->groupid_, proc->procid_);
		event |= PROCMON_EVENT_PROCDEAD;
	}

	((ProcObj *)proc)->status_ = PROCMON_STATUS_OK;

	if (unlikely(event != 0)) {
		group->adjust_proc_time = now;
		return do_event(event, (void *)&proc->procid_, (void *)proc);
	} else {
		return false;
	}
}

bool CProcMonSrv::do_event(int event, void *arg1, void *arg2)
{
	//group event
	if (event & PROCMON_EVENT_PROCDOWN) {
		int diff = *((int *)arg1);
		GroupInfo * group = (GroupInfo *)arg2;

		do_fork(group->basepath_, group->exefile_, group->etcfile_, diff, group->group_type_, group->affinity_);
		return false;
	}

	if (event & PROCMON_EVENT_PROCUP) {
		int diff = *((int *)arg1);
		ProcGroupObj * group = (ProcGroupObj *)arg2;
		int groupid = group->groupinfo_.groupid_;
		ProcObj *proc = NULL;
		int procid = 0;
		int count = 0;

		for (int i = 0; i < BUCKET_SIZE; i++) {
			list_for_each_entry(proc, &group->bucket_[i], list_)
			{
				procid = proc->procinfo_.procid_;
				do_kill(procid, SIGUSR1);
				do_recv(procid);
				del_proc(groupid, procid);

				count++;
				i++;
				break;
			}

			if (count >= diff)
				break;
		}
	}

	//proc event
	ProcInfo *proc = (ProcInfo *)arg2;
	((ProcObj *)proc)->status_ = PROCMON_STATUS_OK;

	if (event & PROCMON_EVENT_PROCDEAD) {
		GroupInfo * group = &proc_groups_[proc->groupid_].groupinfo_;
		int signalno = group->exitsignal_;

		//TODO create a coredump file if the worker proccess is Exit
		if ((0 == kill(proc->procid_, 0)) || (errno != ESRCH)) {
			LOG("create a coredump file because the worker proccess is Exit");
		}

		do_kill(proc->procid_, signalno);
		usleep(10000);// TODO
		((ProcGroupObj *)group)->errprocnum_++;
		if (kill(proc->procid_, 0) == -1 && errno == ESRCH) { //if the process be kill then fork
			do_recv(proc->procid_); // del msg in mq
			del_proc(proc->groupid_, proc->procid_);
			do_fork(group->basepath_, group->exefile_, group->etcfile_, 1, group->group_type_, group->affinity_);
			return true;
		} else {
			return false;
		}
	}

	return false;
}

void CProcMonSrv::do_fork(const char *basepath, const char *exefile, const char * etcfile,
		int num, unsigned group_type, unsigned mask) {
	char cmd_buf[256] = {0};
	snprintf(cmd_buf, sizeof(cmd_buf) - 1, "%s/%s %s/%s", basepath, exefile, basepath, etcfile);
	
	if (mask > 0)
		set_affinity(mask);

	for (int i = 0; i < num; i++) {
		system(cmd_buf);
		usleep(12000);
	}
}

int CProcMonSrv::set_affinity(const uint64_t mask)
{
	/* TODO
	unsigned long mask_use = (unsigned long)mask;

	if (sche_setaffinity(getpid(), (uint32_t)sizeof(mask_use), (cpu_set_t *)&mask_use) < 0) {
		return -1;
	}
	*/

	return 0;
}

void CProcMonSrv::do_kill(int procid, int signo)
{
	kill(procid, signo);
}

void CProcMonSrv::do_order(int groupid, int procid, int eventno, int cmd, int arg1, int arg2)
{
	msg_[1].msgtype_ = procid;
	ProcEvent * event = (ProcEvent *)msg_[1].msgcontent_;
	event->groupid_ = groupid;
	event->procid_ = procid;
	event->cmd_ = cmd;
	event->arg1_ = arg1;
	event->arg2_ = arg2;
	commu_->send(&msg_[1]);
}


	


//client

CProcMonCli::CProcMonCli():commu_(NULL)
{
	msg_[0].msgtype_ = getpid();
	msg_[0].msglen_ = (long)(((ProcMonMsg *)0)->msgcontent_) + sizeof(ProcInfo) - sizeof(long);
	msg_[0].srctype_ = (MSG_VERSION << 1) | MSG_SRC_CLIENT;
}

CProcMonCli::~CProcMonCli()
{
	if (commu_)
		delete commu_;
}

void CProcMonCli::set_commu(CCommu * commu){
	if (commu_)
		delete commu_;
	commu_ = commu;
}

void CProcMonCli::run()
{
	commu_->send(&msg_[0]);
}

void CProcMonCli::exception_report(int signo)
{
	//TODO
	int ret;
	if (commu_ == NULL)
		return;

	msg_[0].timestamp_ = time(NULL);
	msg_[0].srctype_ = (signo << EXCEPTION_STARTBIT) | (MSG_VERSION << 1) | MSG_SRC_CLIENT;

	ret = commu_->send(&msg_[0]);
	if (ret < 0)
		return;
	return;
}

void CProcMonCli::set_log(LogFile *logfile) {
	this->logfile = logfile;
}





