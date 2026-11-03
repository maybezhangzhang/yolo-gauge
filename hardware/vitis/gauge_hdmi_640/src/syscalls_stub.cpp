#include <sys/stat.h>
#include <errno.h>
extern "C" {
int _write(int fd, const char *buf, int len) { (void)fd; (void)buf; return len; }
int _read(int fd, char *buf, int len) { (void)fd; (void)buf; (void)len; return 0; }
int _open(const char *p, int f, int m) { (void)p; (void)f; (void)m; return -1; }
int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, struct stat *s) { (void)fd; if (s) s->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int o, int w) { (void)fd; (void)o; (void)w; return 0; }
int _getpid(void) { return 1; }
int _kill(int p, int s) { (void)p; (void)s; errno = EINVAL; return -1; }
void _exit(int c) { (void)c; for(;;){} }
}
