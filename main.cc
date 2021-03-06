#include <cassert>
#include <cstring>
#include <iostream>
#include <stddef.h>
#if defined(__linux__)
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#endif

#include <fuse.h>
#include <fuse_lowlevel.h>
#include <fuse_opt.h>

#include "filesystem.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

static void ll_destroy(void* userdata) {
    auto fs = reinterpret_cast<FileSystem*>(userdata);
    fs->destroy();
}

static void ll_create(
  fuse_req_t req,
  fuse_ino_t parent,
  const char* name,
  mode_t mode,
  struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    struct fuse_entry_param fe;
    std::memset(&fe, 0, sizeof(fe));

    FileHandle* fh;
    int ret = fs->create(
      parent, name, mode, fi->flags, &fe.attr, &fh, ctx->uid, ctx->gid);
    if (ret == 0) {
        fi->fh = reinterpret_cast<uint64_t>(fh);
        fe.ino = fe.attr.st_ino;
        fe.generation = 0;
        fe.entry_timeout = 1.0;
        fuse_reply_create(req, &fe, fi);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void
ll_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    auto fh = reinterpret_cast<FileHandle*>(fi->fh);

    fs->release(ino, fh);
    fuse_reply_err(req, 0);
}

static void ll_unlink(fuse_req_t req, fuse_ino_t parent, const char* name) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    int ret = fs->unlink(parent, name, ctx->uid, ctx->gid);
    fuse_reply_err(req, -ret);
}

static void ll_forget(fuse_req_t req, fuse_ino_t ino, long unsigned nlookup) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));

    fs->forget(ino, nlookup);
    fuse_reply_none(req);
}

void ll_forget_multi(
  fuse_req_t req, size_t count, struct fuse_forget_data* forgets) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));

    for (size_t i = 0; i < count; i++) {
        const struct fuse_forget_data* f = forgets + i;
        fs->forget(f->ino, f->nlookup);
    }

    fuse_reply_none(req);
}

static void
ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    struct stat st;
    int ret = fs->getattr(ino, &st, ctx->uid, ctx->gid);
    if (ret == 0)
        fuse_reply_attr(req, &st, ret);
    else
        fuse_reply_err(req, -ret);
}

static void ll_lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));

    struct fuse_entry_param fe;
    std::memset(&fe, 0, sizeof(fe));

    int ret = fs->lookup(parent, name, &fe.attr);
    if (ret == 0) {
        fe.ino = fe.attr.st_ino;
        fe.generation = 0;
        fuse_reply_entry(req, &fe);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void
ll_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    int ret = fs->opendir(ino, fi->flags, ctx->uid, ctx->gid);
    if (ret == 0) {
        fuse_reply_open(req, fi);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void ll_readdir(
  fuse_req_t req,
  fuse_ino_t ino,
  size_t size,
  off_t off,
  struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));

    auto buf = std::unique_ptr<char[]>(new char[size]);

    ssize_t ret = fs->readdir(req, ino, buf.get(), size, off);
    if (ret >= 0) {
        fuse_reply_buf(req, buf.get(), (size_t)ret);
    } else {
        int r = (int)ret;
        fuse_reply_err(req, -r);
    }
}

static void
ll_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));

    fs->releasedir(ino);
    fuse_reply_err(req, 0);
}

static void ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    // new files are handled by ll_create
    assert(!(fi->flags & O_CREAT));

    FileHandle* fh;
    int ret = fs->open(ino, fi->flags, &fh, ctx->uid, ctx->gid);
    if (ret == 0) {
        fi->fh = reinterpret_cast<uint64_t>(fh);
        fuse_reply_open(req, fi);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void ll_write_buf(
  fuse_req_t req,
  fuse_ino_t ino,
  struct fuse_bufvec* bufv,
  off_t off,
  struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    auto fh = reinterpret_cast<FileHandle*>(fi->fh);

    ssize_t ret = fs->write_buf(fh, bufv, off);
    if (ret >= 0)
        fuse_reply_write(req, ret);
    else
        fuse_reply_err(req, -ret);
}

static void ll_read(
  fuse_req_t req,
  fuse_ino_t ino,
  size_t size,
  off_t off,
  struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    auto fh = reinterpret_cast<FileHandle*>(fi->fh);

    auto buf = std::unique_ptr<char[]>(new char[size]);

    ssize_t ret = fs->read(fh, off, size, buf.get());
    if (ret >= 0)
        fuse_reply_buf(req, buf.get(), ret);
    else
        fuse_reply_err(req, -ret);
}

static void
ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    struct fuse_entry_param fe;
    std::memset(&fe, 0, sizeof(fe));

    int ret = fs->mkdir(parent, name, mode, &fe.attr, ctx->uid, ctx->gid);
    if (ret == 0) {
        fe.ino = fe.attr.st_ino;
        fe.generation = 0;
        fe.entry_timeout = 1.0;
        fuse_reply_entry(req, &fe);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char* name) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    int ret = fs->rmdir(parent, name, ctx->uid, ctx->gid);
    fuse_reply_err(req, -ret);
}

static void ll_rename(
  fuse_req_t req,
  fuse_ino_t parent,
  const char* name,
  fuse_ino_t newparent,
  const char* newname) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    int ret = fs->rename(parent, name, newparent, newname, ctx->uid, ctx->gid);
    fuse_reply_err(req, -ret);
}

static void ll_setattr(
  fuse_req_t req,
  fuse_ino_t ino,
  struct stat* attr,
  int to_set,
  struct fuse_file_info* fi) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    auto fh = fi ? reinterpret_cast<FileHandle*>(fi->fh) : nullptr;
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    int ret = fs->setattr(ino, fh, attr, to_set, ctx->uid, ctx->gid);
    if (ret == 0)
        fuse_reply_attr(req, attr, 0);
    else
        fuse_reply_err(req, -ret);
}

static void ll_readlink(fuse_req_t req, fuse_ino_t ino) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    char path[PATH_MAX + 1];

    ssize_t ret = fs->readlink(ino, path, sizeof(path) - 1, ctx->uid, ctx->gid);
    if (ret >= 0) {
        path[ret] = '\0';
        fuse_reply_readlink(req, path);
    } else {
        int r = (int)ret;
        fuse_reply_err(req, -r);
    }
}

static void ll_symlink(
  fuse_req_t req, const char* link, fuse_ino_t parent, const char* name) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    struct fuse_entry_param fe;
    std::memset(&fe, 0, sizeof(fe));

    int ret = fs->symlink(link, parent, name, &fe.attr, ctx->uid, ctx->gid);
    if (ret == 0) {
        fe.ino = fe.attr.st_ino;
        fuse_reply_entry(req, &fe);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void ll_fsync(
  fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info* fi) {
    fuse_reply_err(req, 0);
}

static void ll_fsyncdir(
  fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info* fi) {
    fuse_reply_err(req, 0);
}

static void ll_statfs(fuse_req_t req, fuse_ino_t ino) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));

    struct statvfs stbuf;
    std::memset(&stbuf, 0, sizeof(stbuf));

    int ret = fs->statfs(ino, &stbuf);
    if (ret == 0)
        fuse_reply_statfs(req, &stbuf);
    else
        fuse_reply_err(req, -ret);
}

static void ll_link(
  fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char* newname) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    struct fuse_entry_param fe;
    std::memset(&fe, 0, sizeof(fe));

    int ret = fs->link(ino, newparent, newname, &fe.attr, ctx->uid, ctx->gid);
    if (ret == 0) {
        fe.ino = fe.attr.st_ino;
        fuse_reply_entry(req, &fe);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void ll_access(fuse_req_t req, fuse_ino_t ino, int mask) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    int ret = fs->access(ino, mask, ctx->uid, ctx->gid);
    fuse_reply_err(req, -ret);
}

static void ll_mknod(
  fuse_req_t req,
  fuse_ino_t parent,
  const char* name,
  mode_t mode,
  dev_t rdev) {
    auto fs = reinterpret_cast<FileSystem*>(fuse_req_userdata(req));
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    struct fuse_entry_param fe;
    std::memset(&fe, 0, sizeof(fe));

    int ret = fs->mknod(parent, name, mode, rdev, &fe.attr, ctx->uid, ctx->gid);
    if (ret == 0) {
        fe.ino = fe.attr.st_ino;
        fuse_reply_entry(req, &fe);
    } else {
        fuse_reply_err(req, -ret);
    }
}

static void ll_fallocate(
  fuse_req_t req,
  fuse_ino_t ino,
  int mode,
  off_t offset,
  off_t length,
  struct fuse_file_info* fi) {
    fuse_reply_err(req, 0);
}

enum {
    KEY_HELP,
};

struct filesystem_opts {
    size_t size;
    bool debug;
};

#define FS_OPT(t, p, v)                                                        \
    { t, offsetof(struct filesystem_opts, p), v }

static struct fuse_opt fs_fuse_opts[] = {
  FS_OPT("size=%llu", size, 0),
  FS_OPT("-debug", debug, 1),
  FUSE_OPT_KEY("-h", KEY_HELP),
  FUSE_OPT_KEY("--help", KEY_HELP),
  FUSE_OPT_END};

static void usage(const char* progname) {
    printf("file system options:\n"
           "    -o size=N          max file system size (bytes)\n"
           "    -debug             turn on verbose logging\n");
}

static int
fs_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs) {
    switch (key) {
    case FUSE_OPT_KEY_OPT:
        return 1;
    case FUSE_OPT_KEY_NONOPT:
        return 1;
    case KEY_HELP:
        usage(NULL);
        exit(1);
    default:
        assert(0);
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    struct filesystem_opts opts;

    // option defaults
    opts.size = 512 << 20;
    opts.debug = false;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (fuse_opt_parse(&args, &opts, fs_fuse_opts, fs_opt_proc) == -1) {
        exit(1);
    }

    auto console = spdlog::stdout_color_mt("console");
    if (opts.debug) {
        console->set_level(spdlog::level::debug);
    } else {
        console->set_level(spdlog::level::info);
    }

    assert(opts.size > 0);

    struct fuse_chan* ch;
    int err = -1;

    // Operation registry
    struct fuse_lowlevel_ops ll_oper;
    std::memset(&ll_oper, 0, sizeof(ll_oper));
    ll_oper.destroy = ll_destroy;
    ll_oper.lookup = ll_lookup;
    ll_oper.getattr = ll_getattr;
    ll_oper.opendir = ll_opendir;
    ll_oper.readdir = ll_readdir;
    ll_oper.releasedir = ll_releasedir;
    ll_oper.open = ll_open;
    ll_oper.read = ll_read;
    ll_oper.write_buf = ll_write_buf;
    ll_oper.create = ll_create;
    ll_oper.release = ll_release;
    ll_oper.unlink = ll_unlink;
    ll_oper.mkdir = ll_mkdir;
    ll_oper.rmdir = ll_rmdir;
    ll_oper.rename = ll_rename;
    ll_oper.setattr = ll_setattr;
    ll_oper.symlink = ll_symlink;
    ll_oper.readlink = ll_readlink;
    ll_oper.fsync = ll_fsync;
    ll_oper.fsyncdir = ll_fsyncdir;
    ll_oper.statfs = ll_statfs;
    ll_oper.link = ll_link;
    ll_oper.access = ll_access;
    ll_oper.mknod = ll_mknod;
    ll_oper.forget = ll_forget;
    ll_oper.forget_multi = ll_forget_multi;
    ll_oper.fallocate = ll_fallocate;

    FileSystem fs(opts.size, console);

    char* mountpoint = nullptr;
    if (
      fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1
      && (ch = fuse_mount(mountpoint, &args)) != NULL) {
        struct fuse_session* se;

        se = fuse_lowlevel_new(&args, &ll_oper, sizeof(ll_oper), &fs);
        if (se != NULL) {
            if (fuse_set_signal_handlers(se) != -1) {
                fuse_session_add_chan(se, ch);
                err = fuse_session_loop_mt(se);
                fuse_remove_signal_handlers(se);
                fuse_session_remove_chan(ch);
            }
            fuse_session_destroy(se);
        }
        fuse_unmount(mountpoint, ch);
    }
    fuse_opt_free_args(&args);
    if (mountpoint) {
        free(mountpoint);
    }

    int rv = err ? 1 : 0;

    return rv;
}
