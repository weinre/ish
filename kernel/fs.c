#include "debug.h"
#include <string.h>
#include <sys/stat.h>
#include "kernel/calls.h"
#include "kernel/errno.h"
#include "kernel/task.h"
#include "kernel/fs.h"
#include "fs/fd.h"
#include "fs/path.h"
#include "fs/dev.h"

static struct fd *at_fd(fd_t f) {
    if (f == AT_FDCWD_)
        return AT_PWD;
    return f_get(f);
}

static void apply_umask(mode_t_ *mode) {
    struct fs_info *fs = current->fs;
    lock(&fs->lock);
    *mode &= ~fs->umask;
    unlock(&fs->lock);
}

// TODO ENAMETOOLONG

dword_t sys_access(addr_t path_addr, dword_t mode) {
    return sys_faccessat(AT_FDCWD_, path_addr, mode, 0);
}
// TODO: Something about flags
dword_t sys_faccessat(fd_t at_f, addr_t path_addr, mode_t_ mode, dword_t flags) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    STRACE("faccessat(%d, \"%s\", 0x%x, %d)", at_f, path, mode, flags);
    return generic_accessat(at, path, mode);
}

fd_t sys_openat(fd_t at_f, addr_t path_addr, dword_t flags, mode_t_ mode) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("openat(%d, \"%s\", 0x%x, 0x%x)", at_f, path, flags, mode);

    if (flags & O_CREAT_)
        apply_umask(&mode);

    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    struct fd *fd = generic_openat(at, path, flags, mode);
    if (IS_ERR(fd))
        return PTR_ERR(fd);
    return f_install(fd, flags);
}

fd_t sys_open(addr_t path_addr, dword_t flags, mode_t_ mode) {
    return sys_openat(AT_FDCWD_, path_addr, flags, mode);
}

dword_t sys_readlinkat(fd_t at_f, addr_t path_addr, addr_t buf_addr, dword_t bufsize) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("readlinkat(%d, \"%s\", %#x, %#x)", at_f, path, buf_addr, bufsize);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    char buf[bufsize];
    ssize_t size = generic_readlinkat(at, path, buf, bufsize);
    if (size >= 0) {
        STRACE(" \"%.*s\"", size, buf);
        if (user_write(buf_addr, buf, size))
            return _EFAULT;
    }
    return size;
}

dword_t sys_readlink(addr_t path_addr, addr_t buf_addr, dword_t bufsize) {
    return sys_readlinkat(AT_FDCWD_, path_addr, buf_addr, bufsize);
}

dword_t sys_linkat(fd_t src_at_f, addr_t src_addr, fd_t dst_at_f, addr_t dst_addr) {
    char src[MAX_PATH];
    if (user_read_string(src_addr, src, sizeof(src)))
        return _EFAULT;
    char dst[MAX_PATH];
    if (user_read_string(dst_addr, dst, sizeof(dst)))
        return _EFAULT;
    STRACE("linkat(%d, \"%s\", %d, \"%s\")", src_at_f, src, dst_at_f, dst);
    struct fd *src_at = at_fd(src_at_f);
    if (src_at == NULL)
        return _EBADF;
    struct fd *dst_at = at_fd(dst_at_f);
    if (dst_at == NULL)
        return _EBADF;
    return generic_linkat(src_at, src, dst_at, dst);
}

dword_t sys_link(addr_t src_addr, addr_t dst_addr) {
    return sys_linkat(AT_FDCWD_, src_addr, AT_FDCWD_, dst_addr);
}

#define AT_REMOVEDIR_ 0x200
dword_t sys_unlinkat(fd_t at_f, addr_t path_addr, int_t flags) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("unlinkat(%d, \"%s\", %d)", at_f, path, flags);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    if (flags & AT_REMOVEDIR_)
        return generic_rmdirat(at, path);
    else
        return generic_unlinkat(at, path);
}

dword_t sys_unlink(addr_t path_addr) {
    return sys_unlinkat(AT_FDCWD_, path_addr, 0);
}

dword_t sys_renameat2(fd_t src_at_f, addr_t src_addr, fd_t dst_at_f, addr_t dst_addr, int_t flags) {
    if (flags != 0)
        return _EINVAL;
    char src[MAX_PATH];
    if (user_read_string(src_addr, src, sizeof(src)))
        return _EFAULT;
    char dst[MAX_PATH];
    if (user_read_string(dst_addr, dst, sizeof(dst)))
        return _EFAULT;
    STRACE("renameat(%d, \"%s\", %d, \"%s\")", src_at_f, src, dst_at_f, dst);
    struct fd *src_at = at_fd(src_at_f);
    if (src_at == NULL)
        return _EBADF;
    struct fd *dst_at = at_fd(dst_at_f);
    if (dst_at == NULL)
        return _EBADF;
    return generic_renameat(src_at, src, dst_at, dst);
}

dword_t sys_renameat(fd_t src_at_f, addr_t src_addr, fd_t dst_at_f, addr_t dst_addr) {
    return sys_renameat2(src_at_f, src_addr, dst_at_f, dst_addr, 0);
}

dword_t sys_rename(addr_t src_addr, addr_t dst_addr) {
    return sys_renameat2(AT_FDCWD_, src_addr, AT_FDCWD_, dst_addr, 0);
}

dword_t sys_symlinkat(addr_t target_addr, fd_t at_f, addr_t link_addr) {
    char target[MAX_PATH];
    if (user_read_string(target_addr, target, sizeof(target)))
        return _EFAULT;
    char link[MAX_PATH];
    if (user_read_string(link_addr, link, sizeof(link)))
        return _EFAULT;
    STRACE("symlinkat(\"%s\", %d, \"%s\")", target, at_f, link);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    return generic_symlinkat(target, at, link);
}

dword_t sys_symlink(addr_t target_addr, addr_t link_addr) {
    return sys_symlinkat(target_addr, AT_FDCWD_, link_addr);
}

dword_t sys_mknod(addr_t path_addr, mode_t_ mode, dev_t_ dev) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("mknod(\"%s\", %#x, %#x)", path, mode, dev);
    apply_umask(&mode);
    return generic_mknod(path, mode, dev);
}

dword_t sys_read(fd_t fd_no, addr_t buf_addr, dword_t size) {
    STRACE("read(%d, 0x%x, %d)", fd_no, buf_addr, size);
    char *buf = (char *) malloc(size+1);
    if (buf == NULL)
        return _ENOMEM;
    int_t res = 0;
    struct fd *fd = f_get(fd_no);
    if (fd == NULL || fd->ops->read == NULL) {
        res = _EBADF;
        goto out;
    }
    if (S_ISDIR(fd->type)) {
        res = _EISDIR;
        goto out;
    }
    res = fd->ops->read(fd, buf, size);
    if (res >= 0) {
        buf[res] = '\0';
        STRACE(" \"%.99s\"", buf);
        if (user_write(buf_addr, buf, res))
            res = _EFAULT;
    }
out:
    free(buf);
    return res;
}

dword_t sys_readv(fd_t fd_no, addr_t iovec_addr, dword_t iovec_count) {
    dword_t iovec_size = sizeof(struct iovec_) * iovec_count;
    struct iovec_ *iovecs = malloc(iovec_size);
    if (iovecs == NULL)
        return _ENOMEM;
    int res = 0;
    if (user_read(iovec_addr, iovecs, iovec_size)) {
        res = _EFAULT;
        goto err;
    }
    dword_t count = 0;
    for (unsigned i = 0; i < iovec_count; i++) {
        res = sys_read(fd_no, iovecs[i].base, iovecs[i].len);
        if (res < 0)
            goto err;
        count += res;
        if ((unsigned) res < iovecs[i].len)
            break;
    }
    free(iovecs);
    return count;

err:
    free(iovecs);
    return res;
}

dword_t sys_write(fd_t fd_no, addr_t buf_addr, dword_t size) {
    // FIXME this is a DOS vector
    char *buf = malloc(size + 1);
    if (buf == NULL)
        return _ENOMEM;
    dword_t res = 0;
    if (user_read(buf_addr, buf, size)) {
        res = _EFAULT;
        goto out;
    }
    buf[size] = '\0';
    STRACE("write(%d, \"%.100s\", %d)", fd_no, buf, size);
    struct fd *fd = f_get(fd_no);
    if (fd == NULL || fd->ops->write == NULL) {
        res = _EBADF;
        goto out;
    }
    res = fd->ops->write(fd, buf, size);
out:
    free(buf);
    return res;
}

dword_t sys_writev(fd_t fd_no, addr_t iovec_addr, dword_t iovec_count) {
    dword_t iovec_size = sizeof(struct iovec_) * iovec_count;
    struct iovec_ *iovecs = malloc(iovec_size);
    if (iovecs == NULL)
        return _ENOMEM;
    int res = 0;
    if (user_read(iovec_addr, iovecs, iovec_size)) {
        res = _EFAULT;
        goto err;
    }
    dword_t count = 0;
    for (unsigned i = 0; i < iovec_count; i++) {
        res = sys_write(fd_no, iovecs[i].base, iovecs[i].len);
        if (res < 0)
            goto err;
        count += res;
        if ((unsigned) res < iovecs[i].len)
            break;
    }
    free(iovecs);
    return count;

err:
    free(iovecs);
    return res;
}

dword_t sys__llseek(fd_t f, dword_t off_high, dword_t off_low, addr_t res_addr, dword_t whence) {
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    if (!fd->ops->lseek)
        return _ESPIPE;
    lock(&fd->lock);
    off_t_ off = ((off_t_) off_high << 32) | off_low;
    off_t_ res = fd->ops->lseek(fd, off, whence);
    unlock(&fd->lock);
    if (res < 0)
        return res;
    if (user_put(res_addr, res))
        return _EFAULT;
    return 0;
}

dword_t sys_lseek(fd_t f, dword_t off, dword_t whence) {
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    lock(&fd->lock);
    off_t res = fd->ops->lseek(fd, off, whence);
    unlock(&fd->lock);
    if ((dword_t) res != res)
        return _EOVERFLOW;
    return res;
}

dword_t sys_pread(fd_t f, addr_t buf_addr, dword_t size, off_t_ off) {
    STRACE("pread(%d, 0x%x, %d, %d)", f, buf_addr, size, off);
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    char *buf = malloc(size+1);
    if (buf == NULL)
        return _EFAULT;
    lock(&fd->lock);
    int_t res = fd->ops->lseek(fd, off, LSEEK_SET);
    if (res < 0)
        goto out;
    res = fd->ops->read(fd, buf, size);
    if (res >= 0) {
        if (user_write(buf_addr, buf, res))
            res = _EFAULT;
    }
out:
    unlock(&fd->lock);
    free(buf);
    return res;
}

static int fd_ioctl(struct fd *fd, dword_t cmd, dword_t arg) {
    ssize_t size = -1;
    if (fd->ops->ioctl_size)
        size = fd->ops->ioctl_size(cmd);
    if (size < 0) {
        printk("unknown ioctl %x\n", cmd);
        return _ENOTTY;
    }
    if (size == 0)
        return fd->ops->ioctl(fd, cmd, (void *) (long) arg);

    // praying that this won't break
    char buf[size];
    if (user_read(arg, buf, size))
        return _EFAULT;
    int res = fd->ops->ioctl(fd, cmd, buf);
    if (res < 0)
        return res;
    if (user_write(arg, buf, size))
        return _EFAULT;
    return res;
}

static int set_nonblock(struct fd *fd, addr_t nb_addr) {
    dword_t nonblock;
    if (user_get(nb_addr, nonblock))
        return _EFAULT;
    int flags = fd_getflags(fd);
    if (nonblock)
        flags |= O_NONBLOCK_;
    else
        flags &= ~O_NONBLOCK_;
    return fd_setflags(fd, flags);
}

dword_t sys_ioctl(fd_t f, dword_t cmd, dword_t arg) {
    STRACE("ioctl(%d, 0x%x, 0x%x)", f, cmd, arg);
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;

    switch (cmd) {
        case FIONBIO_:
            return set_nonblock(fd, arg);
        default:
            return fd_ioctl(fd, cmd, arg);
    }
    return 0;
}

dword_t sys_getcwd(addr_t buf_addr, dword_t size) {
    STRACE("getcwd(%#x, %#x)", buf_addr, size);
    lock(&current->fs->lock);
    struct fd *wd = current->fs->pwd;
    char pwd[MAX_PATH + 1];
    int err = generic_getpath(wd, pwd);
    unlock(&current->fs->lock);
    if (err < 0)
        return err;

    if (strlen(pwd) + 1 > size)
        return _ERANGE;
    size = strlen(pwd) + 1;
    char *buf = malloc(size);
    if (buf == NULL)
        return _ENOMEM;
    strcpy(buf, pwd);
    STRACE(" \"%.*s\"", size, buf);
    dword_t res = size;
    if (user_write(buf_addr, buf, size))
        res = _EFAULT;
    free(buf);
    return res;
}

static struct fd *open_dir(const char *path) {
    struct statbuf stat;
    int err = generic_statat(AT_PWD, path, &stat, true);
    if (err < 0)
        return ERR_PTR(err);
    if (!(stat.mode & S_IFDIR))
        return ERR_PTR(_ENOTDIR);

    return generic_open(path, O_RDONLY_, 0);
}

void fs_chdir(struct fs_info *fs, struct fd *fd) {
    lock(&fs->lock);
    fd_close(fs->pwd);
    fs->pwd = fd;
    unlock(&fs->lock);
}

dword_t sys_chdir(addr_t path_addr) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("chdir(\"%s\")", path);

    struct fd *dir = open_dir(path);
    if (IS_ERR(dir))
        return PTR_ERR(dir);
    fs_chdir(current->fs, dir);
    return 0;
}

dword_t sys_fchdir(fd_t f) {
    STRACE("fchdir(%d)", f);
    struct fd *dir = f_get(f);
    if (dir == NULL)
        return _EBADF;
    dir->refcount++;
    fs_chdir(current->fs, dir);
    return 0;
}

dword_t sys_chroot(addr_t path_addr) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("chroot(\"%s\")", path);

    struct fd *dir = open_dir(path);
    if (IS_ERR(dir))
        return PTR_ERR(dir);
    lock(&current->fs->lock);
    fd_close(current->fs->root);
    current->fs->root = dir;
    unlock(&current->fs->lock);
    return 0;
}

dword_t sys_umask(dword_t mask) {
    STRACE("umask(0%o)", mask);
    struct fs_info *fs = current->fs;
    lock(&fs->lock);
    mode_t_ old_umask = fs->umask;
    fs->umask = ((mode_t_) mask) & 0777;
    unlock(&fs->lock);
    return old_umask;
}

static int mount_statfs(struct mount *mount, struct statfsbuf *stat) {
    int err = 0;
    if (mount->fs->statfs)
        err = mount->fs->statfs(mount, stat);
    if (stat->type == 0)
        stat->type = mount->fs->magic;
    return err;
}

static int_t statfs_mount(struct mount *mount, addr_t buf_addr) {
    struct statfsbuf buf;
    int err = mount_statfs(mount, &buf);
    if (err < 0)
        return err;
    struct statfs_ out_buf = {
        .type = buf.type,
        .bsize = buf.bsize,
        .blocks = buf.blocks,
        .bfree = buf.bfree,
        .bavail = buf.bavail,
        .files = buf.files,
        .ffree = buf.ffree,
        .fsid = buf.fsid,
        .namelen = buf.namelen,
        .frsize = buf.frsize,
        .flags = buf.flags,
    };
    if (user_put(buf_addr, out_buf))
        return _EFAULT;
    return 0;
}

static int_t statfs64_mount(struct mount *mount, addr_t buf_addr) {
    struct statfsbuf buf;
    int err = mount_statfs(mount, &buf);
    if (err < 0)
        return err;
    struct statfs64_ out_buf = {
        .type = buf.type,
        .bsize = buf.bsize,
        .blocks = buf.blocks,
        .bfree = buf.bfree,
        .bavail = buf.bavail,
        .files = buf.files,
        .ffree = buf.ffree,
        .fsid = buf.fsid,
        .namelen = buf.namelen,
        .frsize = buf.frsize,
        .flags = buf.flags,
    };
    if (user_put(buf_addr, out_buf))
        return _EFAULT;
    return 0;
}

dword_t sys_statfs(addr_t path_addr, addr_t buf_addr) {
    char path_raw[MAX_PATH];
    if (user_read_string(path_addr, path_raw, sizeof(path_raw)))
        return _EFAULT;
    STRACE("statfs(\"%s\", %#x)", path_raw, buf_addr);
    char path[MAX_PATH];
    int err = path_normalize(AT_PWD, path_raw, path, false);
    if (err < 0)
        return err;
    struct mount *mount = mount_find(path);
    err = statfs_mount(mount, buf_addr);
    mount_release(mount);
    return err;
}

dword_t sys_statfs64(addr_t path_addr, addr_t buf_addr) {
    char path_raw[MAX_PATH];
    if (user_read_string(path_addr, path_raw, sizeof(path_raw)))
        return _EFAULT;
    STRACE("statfs64(\"%s\", %#x)", path_raw, buf_addr);
    char path[MAX_PATH];
    int err = path_normalize(AT_PWD, path_raw, path, false);
    if (err < 0)
        return err;
    struct mount *mount = mount_find(path);
    err = statfs64_mount(mount, buf_addr);
    mount_release(mount);
    return err;
}

dword_t sys_fstatfs(fd_t f, addr_t buf_addr) {
    return statfs_mount(f_get(f)->mount, buf_addr);
}

dword_t sys_fstatfs64(fd_t f, addr_t buf_addr) {
    return statfs64_mount(f_get(f)->mount, buf_addr);
}

dword_t sys_flock(fd_t f, dword_t operation) {
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    return fd->mount->fs->flock(fd, operation);
}

static struct timespec convert_timespec(struct timespec_ t) {
    struct timespec ts;
    ts.tv_sec = t.sec;
    ts.tv_nsec = t.nsec;
    return ts;
}

static struct timespec convert_timeval(struct timeval_ t) {
    struct timespec ts;
    ts.tv_sec = t.sec;
    ts.tv_nsec = t.usec * 1000;
    return ts;
}

static dword_t sys_utime_common(fd_t at_f, addr_t path_addr, struct timespec atime, struct timespec mtime, dword_t flags) {
    char path[MAX_PATH];
    if (path_addr != 0)
        if (user_read_string(path_addr, path, sizeof(path)))
            return _EFAULT;
    STRACE("utimensat(%d, %s, {{%d, %d}, {%d, %d}}, %d)", at_f, path,
            atime.tv_sec, atime.tv_nsec, mtime.tv_sec, mtime.tv_nsec, flags);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;

    bool follow_links = flags & AT_SYMLINK_NOFOLLOW_ ? false : true;
    return generic_utime(at, path_addr != 0 ? path : ".", atime, mtime, follow_links); // TODO implement
}

dword_t sys_utimensat(fd_t at_f, addr_t path_addr, addr_t times_addr, dword_t flags) {
    struct timespec atime;
    struct timespec mtime;
    if (times_addr == 0) {
        atime = mtime = timespec_now();
    } else {
        struct timespec_ times[2];
        if (user_get(times_addr, times))
            return _EFAULT;
        atime = convert_timespec(times[0]);
        mtime = convert_timespec(times[1]);
    }
    return sys_utime_common(at_f, path_addr, atime, mtime, flags);
}

dword_t sys_utimes(addr_t path_addr, addr_t times_addr) {
    struct timespec atime;
    struct timespec mtime;
    if (times_addr == 0) {
        atime = mtime = timespec_now();
    } else {
        struct timeval_ times[2];
        if (user_get(times_addr, times))
            return _EFAULT;
        atime = convert_timeval(times[0]);
        mtime = convert_timeval(times[1]);
    }
    return sys_utime_common(AT_FDCWD_, path_addr, atime, mtime, 0);
}

dword_t sys_fchmod(fd_t f, dword_t mode) {
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    mode &= ~S_IFMT;
    return fd->mount->fs->fsetattr(fd, make_attr(mode, mode));
}

dword_t sys_fchmodat(fd_t at_f, addr_t path_addr, dword_t mode) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("fchmodat(%d, \"%s\", %o)", at_f, path, mode);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    mode &= ~S_IFMT;
    return generic_setattrat(at, path, make_attr(mode, mode), true);
}

dword_t sys_chmod(addr_t path_addr, dword_t mode) {
    return sys_fchmodat(AT_FDCWD_, path_addr, mode);
}

dword_t sys_fchown32(fd_t f, uid_t_ owner, uid_t_ group) {
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    int err;
    if (owner != (uid_t) -1) {
        err = fd->mount->fs->fsetattr(fd, make_attr(uid, owner));
        if (err < 0)
            return err;
    }
    if (group != (uid_t) -1) {
        err = fd->mount->fs->fsetattr(fd, make_attr(gid, group));
        if (err < 0)
            return err;
    }
    return 0;
}

dword_t sys_fchownat(fd_t at_f, addr_t path_addr, dword_t owner, dword_t group, int flags) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("fchownat(%d, \"%s\", %d, %d, %d)", at_f, path, owner, group, flags);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    int err;
    bool follow_links = flags & AT_SYMLINK_NOFOLLOW_ ? false : true;
    if (owner != (uid_t) -1) {
        err = generic_setattrat(at, path, make_attr(uid, owner), follow_links);
        if (err < 0)
            return err;
    }
    if (group != (uid_t) -1) {
        err = generic_setattrat(at, path, make_attr(gid, group), follow_links);
        if (err < 0)
            return err;
    }
    return 0;
}

dword_t sys_chown32(addr_t path_addr, uid_t_ owner, uid_t_ group) {
    return sys_fchownat(AT_FDCWD_, path_addr, owner, group, 0);
}

dword_t sys_lchown(addr_t path_addr, uid_t_ owner, uid_t_ group) {
    return sys_fchownat(AT_FDCWD_, path_addr, owner, group, AT_SYMLINK_NOFOLLOW_);
}

dword_t sys_truncate64(addr_t path_addr, dword_t size_low, dword_t size_high) {
    off_t_ size = ((off_t_) size_high << 32) | size_low;
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    return generic_setattrat(NULL, path, make_attr(size, size), true);
}

dword_t sys_ftruncate64(fd_t f, dword_t size_low, dword_t size_high) {
    off_t_ size = ((off_t_) size_high << 32) | size_low;
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    return fd->mount->fs->fsetattr(fd, make_attr(size, size));
}

dword_t sys_fallocate(fd_t f, dword_t UNUSED(mode), dword_t offset_low, dword_t offset_high, dword_t len_low, dword_t len_high) {
    off_t_ offset = ((off_t_) offset_high << 32) | offset_low;
    off_t_ len = ((off_t_) len_high << 32) | len_low;
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    struct statbuf statbuf;
    int err = fd->mount->fs->fstat(fd, &statbuf);
    if (err < 0)
        return err;
    if ((uint64_t) offset + (uint64_t) len > statbuf.size)
        return fd->mount->fs->fsetattr(fd, make_attr(size, offset + len));
    return 0;
}

dword_t sys_mkdirat(fd_t at_f, addr_t path_addr, mode_t_ mode) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    STRACE("mkdirat(%d, %s, 0%o)", at_f, path, mode);
    struct fd *at = at_fd(at_f);
    if (at == NULL)
        return _EBADF;
    apply_umask(&mode);
    mode &= 0777;
    return generic_mkdirat(at, path, mode);
}

dword_t sys_mkdir(addr_t path_addr, mode_t_ mode) {
    return sys_mkdirat(AT_FDCWD_, path_addr, mode);
}

dword_t sys_rmdir(addr_t path_addr) {
    char path[MAX_PATH];
    if (user_read_string(path_addr, path, sizeof(path)))
        return _EFAULT;
    return generic_rmdirat(AT_PWD, path);
}

dword_t sys_fsync(fd_t f) {
    struct fd *fd = f_get(f);
    if (fd == NULL)
        return _EBADF;
    int err = 0;
    if (fd->ops->fsync)
        err = fd->ops->fsync(fd);
    return err;
}

// a few stubs
dword_t sys_sendfile(fd_t UNUSED(out_fd), fd_t UNUSED(in_fd), addr_t UNUSED(offset_addr), dword_t UNUSED(count)) {
    return _EINVAL;
}
dword_t sys_sendfile64(fd_t UNUSED(out_fd), fd_t UNUSED(in_fd), addr_t UNUSED(offset_addr), dword_t UNUSED(count)) {
    return _EINVAL;
}
dword_t sys_copy_file_range(fd_t UNUSED(in_fd), addr_t UNUSED(in_off), fd_t UNUSED(out_fd),
        addr_t UNUSED(out_off), dword_t UNUSED(len), uint_t UNUSED(flags)) {
    return _EPERM; // good enough for ruby
}

dword_t sys_xattr_stub(addr_t UNUSED(path_addr), addr_t UNUSED(name_addr),
        addr_t UNUSED(value_addr), dword_t UNUSED(size), dword_t UNUSED(flags)) {
    return _ENOTSUP;
}
