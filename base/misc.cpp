#include "misc.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>

#include <netinet/in.h>       /* for sockaddr_in */  
using namespace common::base;

unsigned CMisc::getip(const char *ifname) 
{
	struct ifaddrs *ifa = NULL, *ifList;
	uint32_t dwIp = 0;

	if (strncmp(ifname, "all", 4))
		return 0U;

	if (getifaddrs(&ifList) < 0) return -1;

	for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr->sa_family == AF_INET) {
			if ((ifname == NULL && !strcmp(ifa->ifa_name, "eth1"))
					|| (ifname != NULL  && !strcmp(ifa->ifa_name, ifname))) {
				dwIp  = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
			}
		}
	}

	freeifaddrs(ifList);

	return dwIp;
}

int CMisc::check_process_exist(pid_t pid)
{
	int ret = kill(pid, 0);

	if (ret == 0 || errno != ESRCH) 
		return 1;

	return 0;
}
