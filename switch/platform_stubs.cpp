#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

extern "C" {

int execlp(const char*, const char*, ...)
{
	errno = ENOSYS;
	return -1;
}

FILE* popen(const char*, const char*)
{
	errno = ENOSYS;
	return nullptr;
}

int pclose(FILE*)
{
	errno = ENOSYS;
	return -1;
}

DIR* fdopendir(int)
{
	errno = ENOSYS;
	return nullptr;
}

}
