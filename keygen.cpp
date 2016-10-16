#include "keygen.h"
#include "crc32.h"

#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <linux/limits.h>

using namespace platform::commlib;

namespace comm
{
namespace keygen
{


//use pwd & id to generate shm/mq key
key_t pwdtok(int id)
{
    char seed[PATH_MAX] = {0};
    readlink("/proc/self/exe", seed, PATH_MAX);//获取程序全路径
    dirname(seed);//截取全程序路径（去掉程序名）

    uint32_t seed_len = strlen(seed) + sizeof(id);
    memcpy(seed + strlen(seed), &id, sizeof(id));//程序全路径＋id

    CCrc32 generator;
    return generator.Crc32((unsigned char*)seed, seed_len);
}



}
}

