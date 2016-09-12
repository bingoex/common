#ifndef _COMMU_H_
#define _COMMU_H_

#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define SOCK_TYPE_TCP 0x1;
#define SOCK_TYPE_UDP 0x2;
#define SOCK_TYPE_UNIX 0x4;
#define SOCK_TYPE_NOTIFY 0x8;
#define SOCK_MAX_BIND 300;

namespace common{
	namespace base {
		namespace commu {
			//callback type
			typedef enum {
				CB_CONNECTED = 0,
				CB_DISCONNECT,
				CB_RECVDATA,
				CB_RECVERROR,
				CB_RECVNONE,
				CB_SENDDATA,
				CB_SENDERROR,
				CB_SENDDONE,
				CB_HANGUP,
				CB_OVERLOAD,
				CB_TIMEOUT,
			} cb_type;

			typedef int (*cb_func)(unsigned flow, void *arg1, void *arg2);

			//control cmd type
			typedef enum {
				CT_DISCONNECT = 0,
				CT_CLOSE,
				CT_STAT,
			} ctrl_type;

			typedef struct {
				int len;
				char *data; // data buf
				void *owner;
				void *extdata;
			} blob_type;

			typedef struct {
				int fd_;
				int type_; // STREAM DGRAM ...
				unsigned localip_;
				unsigned short localport_;
				unsigned remoteip_;
				unsigned short remoteport_;
				time_t recvtime_; //recv sec
				suseconds_t tv_usec; // recv usec
			} TConnExtInfo;

			class CTCommu 
			{
				public:
					CTCommu() {
						memset(func_list_, 0, sizeof(cb_func) * (CB_TIMEOUT + 1));
						memset(func_args_, 0, sizeof(void *) * (CB_TIMEOUT + 1));
					}

					virtual ~CTCommu() {}

					/*
					 * config the name of config or just a config struct 
					 */
					virtual int init(const void *config) = 0;

					/*
					 * recv data
					 */
					virtual int poll(bool block = false) = 0;

					/*
					 * send data
					 */ 
					virtual int sendto(unsigned flow, void *arg1, void *arg2) = 0;

					/*
					 * send contrl cmd
					 * type : the cmd
					 */
					virtual int ctrl(unsigned flow, ctrl_type type, void *arg1, void *arg2) = 0;

					virtual int reg_cb(cb_type type, cb_func func, void *args = NULL) {
						if (type <= CB_TIMEOUT) {
							func_list_[type] = func;
							func_args_[type] = args;
							return 0;
						} else {
							return -1;
						}
					}

					//clear shm just for proxy
					virtual int clear() = 0;




				protected:
					cb_func func_list_[CB_TIMEOUT + 1];
					void *func_args_[CB_TIMEOUT + 1];

					virtual void fini() = 0;
			};

		}
	}
}

#endif
