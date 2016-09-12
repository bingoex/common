#include "notify.h"

#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

using namespace common::base::notify;
using namespace common::base;

int g_anf_shm_fifo = false;

char *CNotify::get_fifo_path(int key)
{
#define FIFO_SHM_DIR "/dev/shm/anf_notify"
	int32_t ret = 0;
	static char fifo_path[128] = {0};

	snprintf(fifo_path, sizeof(fifo_path), ".notify_%d", key);

	//TODO
	if (g_anf_shm_fifo) {
		ret = mkdir(FIFO_SHM_DIR, 0777);
		if (ret < 0 && errno != EEXIST) {
			printf("create fifo shm directory /dev/shm/anf_notify failed %s", strerror(errno));
			return fifo_path;
		}

		//TODO os don't support pwdtok
		//uint32_t pwd_key  = pwdtok(key);
		uint32_t pwd_key = 1;

		snprintf(fifo_path, sizeof(fifo_path), FIFO_SHM_DIR".notify_0x%x_%u", pwd_key, (uint32_t)key);
	}

	return fifo_path;
}

int CNotify::notify_init(int key, char *fifoname, size_t fifoname_size)
{
	if (key <= 0)
		return -1;

	int netfd = -1;
	char *path = get_fifo_path(key);

	if (fifoname != NULL) {
		strncpy(fifoname, path, fifoname_size -1);
	}

	if (mkfifo(path, 0666) < 0) {
		if (errno != EEXIST) {
			printf("create fifo[%s] error[%d]", path, errno);
			return -2;
		}
	}

	netfd = open(path, O_RDWR | O_NONBLOCK, 0666);
	if (netfd == -1) {
		printf("open notify fifo[%s] error[%d]", path, errno);
		return -3;
	}

	return netfd;
}

int CNotify::notify_send(int fd)
{
	return write(fd, "x", 1);
}

int CNotify::notify_recv(int fd) 
{
	char buf[64];
	return read(fd, buf, sizeof(buf));
}
