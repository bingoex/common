#ifndef _BASE_PROCMON_H_
#define _BASE_PROCMON_H_

#include <time.h>
#include <map>

#include "list.h"
#include "cm_log.h"

#define MAX_FILEPATH_LEN 128
#define BUCKET_SIZE 10
#define MAX_PROC_GROUP_NUM 256
#define MAX_MSG_BUFF 100
#define MSG_EXPIRE_TIME 120
#define MSG_VERSION 0x01

#define EXCEPTION_STARTBIT 20
#define EXCEPTION_TYPE(msg_srctype__) (msg_srctype__ >> EXCEPTION_STARTBIT)

#define MSG_ID_SERVER 0X01
#define MSG_SRC_SERVER 0x00
#define MSG_SRC_CLIENT 0x01
#define DEDAULT_MQ_KEY 0x700100 //7340288

#define PROCMON_EVENT_PROCDEAD 1
#define PROCMON_EVENT_OVERLOAD (1<<1)
#define PROCMON_EVENT_LOWSRATE (1<<2)
#define PROCMON_EVENT_LATENCY  (1<<3)
#define PROCMON_EVENT_OTFMEM   (1<<4)
#define PROCMON_EVENT_PROCDOWN (1<<5)
#define PROCMON_EVENT_PROCUP   (1<<6)
#define PROCMON_EVENT_FORKFAIL (1<<7)
#define PROCMON_EVENT_MQKEYCONFIGERROR (1<<8)
#define PROCMON_EVENT_TASKPAUSE (1<<9)
#define PROCMON_EVENT_QSTATNULL (1<<10)


#define PROCMON_CMD_KILL 0x1
#define PROCMON_CMD_LOAD 0x2
#define PROCMON_CMD_FORK 0x4

#define PROCMON_STATUS_OK	0x0
#define PROCMON_STATUS_OVERLOAD 1
#define PROCMON_STATUS_LOWSRATE (1<<1)
#define PROCMON_STATUS_LATENCY  (1<<2)
#define PROCMON_STATUS_OTFMEM   (1<<3)



#define PROC_RELOAD_NORMAL 0
#define PROC_RELOAD_START 1
#define PROC_RELOAD_WAIT_NEW 2

using namespace common::base;

namespace common {
	namespace base {
		namespace procmon {

			typedef struct  {
				int groupid_;
				time_t adjust_proc_time;
				char basepath_[MAX_FILEPATH_LEN];
				char exefile_[MAX_FILEPATH_LEN];
				char etcfile_[MAX_FILEPATH_LEN];
				int exitsignal_;
				unsigned maxprocnum_;
				unsigned minprocnum_;
				unsigned heartbeat_;
				char mpfile_[MAX_FILEPATH_LEN];
				void * q_recv_pstat;
				unsigned int type;
				unsigned int sendkey;
				unsigned int sendkeysize;
				unsigned int recvkey;
				unsigned int recvkeysize;
				int mqkey;
				char reserve_[8];
				unsigned group_type_;
				unsigned affinity_;
				unsigned reload_;
				time_t reload_time;
			} GroupInfo;

			typedef struct {
				int groupid_;
				int procid_;
				time_t timestamp_;
				char reserve_[8];
			} ProcInfo;

			typedef struct {
				int groupid_;
				int procid_;
				unsigned cmd_;
				int arg1_;
				int arg2_;
				char reserve_[8];
			} ProcEvent;

			typedef struct {
				long msgtype_;
				long msglen_;
				long srctype_;
				time_t timestamp_;
				char msgcontent_[MAX_MSG_BUFF];
			} ProcMonMsg;

			class CCommu
			{
				public:
					CCommu() {}
					virtual ~CCommu() {}

					virtual int init(void *args) = 0;

					virtual void fini() = 0;

					virtual int recv(ProcMonMsg *msg, long msgtype = 1) = 0;

					virtual int send(ProcMonMsg *msg) = 0;
			};

			class CMQCommu: public CCommu
			{
				public:
					CMQCommu();
					CMQCommu(int mqkey);
					~CMQCommu();
					int init(void *args);
					void fini();
					int recv(ProcMonMsg *msg, long msgtype = 0);
					int send(ProcMonMsg *msg);
					void setlog(LogFile * logfile);
				protected:
					int mqid_;
			};

			typedef struct {
				GroupInfo groupinfo_;
				list_head_t bucket_[BUCKET_SIZE];
				int curprocnum_;
				int errprocnum_;
			} ProcGroupObj;

			typedef struct {
				ProcInfo procinfo_;
				int status_;
				list_head_t list_;
			} ProcObj;

			typedef void (*monsrc_cb) (const GroupInfo * groupinfo,
									   const ProcInfo * procinfo,
									   int event,
									   void *arg);


			class CProcMonSrv
			{
				public:
					CProcMonSrv();
					virtual ~CProcMonSrv();

					void set_commu(CCommu * conmmu);

					void set_log(LogFile * logflie);

					int add_group(const GroupInfo* groupinfo);

					int mod_group(int groupid, const GroupInfo* groupinfo);

					void run();

					void stat(char *buf, int *buf_len);

					void killall(int signp);

					void kill_group(int grp_id, int signo);

					int dump_pid_list(char *buf, int len);

				protected:
					CCommu * commu_;

					ProcGroupObj proc_groups_[MAX_PROC_GROUP_NUM];
					int cur_group_;
					ProcMonMsg msg_[2];

					std::map<int, std::map<int, int> > reload_tag;
					
					bool do_recv(long msgtype);
					bool do_check();

					ProcGroupObj * find_group(int groupid);

					int add_proc(int groupid, const ProcInfo * procinfo);
					ProcObj * find_proc(int groupid, int procid);
					void del_proc(int groupid, int procid);
					
					void segv_wait(ProcInfo * procinfo);
					void process_exception(void);

					virtual void check_group(GroupInfo *group, int curprocnumd);
					virtual bool check_proc(GroupInfo *group, ProcInfo *proc);
					virtual bool do_event(int event, void *arg1, void *arg2);
					virtual void do_fork(const char *basepath, const char *exefile, const char * etcfile,
										 int num, unsigned group_type, unsigned mask);

					virtual void do_kill(int procid, int signo = SIGKILL);

					virtual void do_order(int groupid, int procid, int eventno, int cmd, int arg1 = 0, int arg2 = 0);
					bool check_groupbusy(int groupid);
					int set_affinity(const uint64_t mask);
			};

			class CProcMonCli
			{
				public:
					CProcMonCli();
					virtual ~CProcMonCli();

					void set_commu(CCommu * conmmu);

					void run();
					void exception_report(int signo);

					ProcMonMsg msg_[2];
				protected:
					CCommu * commu_;
			};

#define CLI_SEND_INFO(cli) ((ProcInfo*)(cli)->msg_[0].msgcontent_)
#define CLI_RECV_INFO(cli) ((ProcEvent*)(cli)->msg_[1].msgcontent_)





		}
	}
}

#endif

