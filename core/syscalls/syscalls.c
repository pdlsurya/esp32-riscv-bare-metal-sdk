#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <reent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "usb_serial.h"

int _write(int fd, const void *ptr, size_t len)
{
    if ((fd != 1) && (fd != 2))
    {
        errno = EBADF;
        return -1;
    }

    return serial_write((const char *)ptr, len);
}

int _read(int fd, void *ptr, size_t len)
{
    char *rx_buf;
    size_t read_len;

    if (fd != 0)
    {
        errno = EBADF;
        return -1;
    }
    if ((ptr == NULL) || (len == 0U))
    {
        return 0;
    }

    rx_buf = serial_read_string();
    read_len = strlen(rx_buf);
    if (read_len > len)
    {
        read_len = len;
    }

    memcpy(ptr, rx_buf, read_len);
    return (int)read_len;
}

int _close(int fd)
{
    if ((fd < 0) || (fd > 2))
    {
        errno = EBADF;
        return -1;
    }

    return 0;
}

int _fstat(int fd, struct stat *st)
{
    if (((fd < 0) || (fd > 2)) || (st == NULL))
    {
        errno = EBADF;
        return -1;
    }

    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd)
{
    if ((fd >= 0) && (fd <= 2))
    {
        return 1;
    }

    errno = EBADF;
    return 0;
}

off_t _lseek(int fd, off_t offset, int whence)
{
    (void)offset;
    (void)whence;

    if ((fd < 0) || (fd > 2))
    {
        errno = EBADF;
        return (off_t)-1;
    }

    errno = ESPIPE;
    return (off_t)-1;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

struct _reent *__getreent(void)
{
    return _impure_ptr;
}
