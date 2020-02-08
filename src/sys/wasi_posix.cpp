#include "wembed/context.hpp"
#include "wembed/wasi.hpp"

#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <random>

namespace wembed::wasi {

__wasi_errno_t errno_translate(int error) {
  switch(error) {
    case E2BIG: return __WASI_ERRNO_2BIG;
    case EACCES: return __WASI_ERRNO_ACCES;
    case EADDRINUSE: return __WASI_ERRNO_ADDRINUSE;
    case EADDRNOTAVAIL: return __WASI_ERRNO_ADDRNOTAVAIL;
    case EAFNOSUPPORT: return __WASI_ERRNO_AFNOSUPPORT;
    case EAGAIN: return __WASI_ERRNO_AGAIN;
    case EALREADY: return __WASI_ERRNO_ALREADY;
    case EBADF: return __WASI_ERRNO_BADF;
    case EBADMSG: return __WASI_ERRNO_BADMSG;
    case EBUSY: return __WASI_ERRNO_BUSY;
    case ECANCELED: return __WASI_ERRNO_CANCELED;
    case ECHILD: return __WASI_ERRNO_CHILD;
    case ECONNABORTED: return __WASI_ERRNO_CONNABORTED;
    case ECONNREFUSED: return __WASI_ERRNO_CONNREFUSED;
    case ECONNRESET: return __WASI_ERRNO_CONNRESET;
    case EDEADLK: return __WASI_ERRNO_DEADLK;
    case EDESTADDRREQ: return __WASI_ERRNO_DESTADDRREQ;
    case EDOM: return __WASI_ERRNO_DOM;
    case EDQUOT: return __WASI_ERRNO_DQUOT;
    case EEXIST: return __WASI_ERRNO_EXIST;
    case EFAULT: return __WASI_ERRNO_FAULT;
    case EFBIG: return __WASI_ERRNO_FBIG;
    case EHOSTUNREACH: return __WASI_ERRNO_HOSTUNREACH;
    case EIDRM: return __WASI_ERRNO_IDRM;
    case EILSEQ: return __WASI_ERRNO_ILSEQ;
    case EINPROGRESS: return __WASI_ERRNO_INPROGRESS;
    case EINTR: return __WASI_ERRNO_INTR;
    case EINVAL: return __WASI_ERRNO_INVAL;
    case EIO: return __WASI_ERRNO_IO;
    case EISCONN: return __WASI_ERRNO_ISCONN;
    case EISDIR: return __WASI_ERRNO_ISDIR;
    case ELOOP: return __WASI_ERRNO_LOOP;
    case EMFILE: return __WASI_ERRNO_MFILE;
    case EMLINK: return __WASI_ERRNO_MLINK;
    case EMSGSIZE: return __WASI_ERRNO_MSGSIZE;
    case EMULTIHOP: return __WASI_ERRNO_MULTIHOP;
    case ENAMETOOLONG: return __WASI_ERRNO_NAMETOOLONG;
    case ENETDOWN: return __WASI_ERRNO_NETDOWN;
    case ENETRESET: return __WASI_ERRNO_NETRESET;
    case ENETUNREACH: return __WASI_ERRNO_NETUNREACH;
    case ENFILE: return __WASI_ERRNO_NFILE;
    case ENOBUFS: return __WASI_ERRNO_NOBUFS;
    case ENODEV: return __WASI_ERRNO_NODEV;
    case ENOENT: return __WASI_ERRNO_NOENT;
    case ENOEXEC: return __WASI_ERRNO_NOEXEC;
    case ENOLCK: return __WASI_ERRNO_NOLCK;
    case ENOLINK: return __WASI_ERRNO_NOLINK;
    case ENOMEM: return __WASI_ERRNO_NOMEM;
    case ENOMSG: return __WASI_ERRNO_NOMSG;
    case ENOPROTOOPT: return __WASI_ERRNO_NOPROTOOPT;
    case ENOSPC: return __WASI_ERRNO_NOSPC;
    case ENOSYS: return __WASI_ERRNO_NOSYS;
    case ENOTCONN: return __WASI_ERRNO_NOTCONN;
    case ENOTDIR: return __WASI_ERRNO_NOTDIR;
    case ENOTEMPTY: return __WASI_ERRNO_NOTEMPTY;
    case ENOTRECOVERABLE: return __WASI_ERRNO_NOTRECOVERABLE;
    case ENOTSOCK: return __WASI_ERRNO_NOTSOCK;
    case ENOTSUP: return __WASI_ERRNO_NOTSUP;
    case ENOTTY: return __WASI_ERRNO_NOTTY;
    case ENXIO: return __WASI_ERRNO_NXIO;
    case EOVERFLOW: return __WASI_ERRNO_OVERFLOW;
    case EOWNERDEAD: return __WASI_ERRNO_OWNERDEAD;
    case EPERM: return __WASI_ERRNO_PERM;
    case EPIPE: return __WASI_ERRNO_PIPE;
    case EPROTO: return __WASI_ERRNO_PROTO;
    case EPROTONOSUPPORT: return __WASI_ERRNO_PROTONOSUPPORT;
    case EPROTOTYPE: return __WASI_ERRNO_PROTOTYPE;
    case ERANGE: return __WASI_ERRNO_RANGE;
    case EROFS: return __WASI_ERRNO_ROFS;
    case ESPIPE: return __WASI_ERRNO_SPIPE;
    case ESRCH: return __WASI_ERRNO_SRCH;
    case ESTALE: return __WASI_ERRNO_STALE;
    case ETIMEDOUT: return __WASI_ERRNO_TIMEDOUT;
    case ETXTBSY: return __WASI_ERRNO_TXTBSY;
    case EXDEV: return __WASI_ERRNO_XDEV;
    default: return __WASI_ERRNO_NOTCAPABLE;
  }
}

wasi_context::wasi_context(std::filesystem::path pRoot) : mRoot(std::move(pRoot)) {
  // Operations that apply to regular files.
  #define REGULAR_FILE_RIGHTS                                                                        \
    (__WASI_RIGHTS_FD_DATASYNC | __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK                       \
     | __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS | __WASI_RIGHTS_FD_SYNC | __WASI_RIGHTS_FD_TELL             \
     | __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_ADVISE | __WASI_RIGHTS_FD_ALLOCATE                  \
     | __WASI_RIGHTS_FD_FILESTAT_GET | __WASI_RIGHTS_FD_FILESTAT_SET_SIZE                            \
     | __WASI_RIGHTS_FD_FILESTAT_SET_TIMES | __WASI_RIGHTS_POLL_FD_READWRITE)

  // Only allow directory operations on directories.
  #define DIRECTORY_RIGHTS                                                                           \
    (__WASI_RIGHTS_FD_FDSTAT_SET_FLAGS | __WASI_RIGHTS_FD_SYNC | __WASI_RIGHTS_FD_ADVISE             \
     | __WASI_RIGHTS_PATH_CREATE_DIRECTORY | __WASI_RIGHTS_PATH_CREATE_FILE                          \
     | __WASI_RIGHTS_PATH_LINK_SOURCE | __WASI_RIGHTS_PATH_LINK_TARGET | __WASI_RIGHTS_PATH_OPEN     \
     | __WASI_RIGHTS_FD_READDIR | __WASI_RIGHTS_PATH_READLINK | __WASI_RIGHTS_PATH_RENAME_SOURCE     \
     | __WASI_RIGHTS_PATH_RENAME_TARGET | __WASI_RIGHTS_PATH_FILESTAT_GET                            \
     | __WASI_RIGHTS_PATH_FILESTAT_SET_SIZE | __WASI_RIGHTS_PATH_FILESTAT_SET_TIMES                  \
     | __WASI_RIGHTS_FD_FILESTAT_GET | __WASI_RIGHTS_FD_FILESTAT_SET_TIMES                           \
     | __WASI_RIGHTS_PATH_SYMLINK | __WASI_RIGHTS_PATH_UNLINK_FILE                                   \
     | __WASI_RIGHTS_PATH_REMOVE_DIRECTORY | __WASI_RIGHTS_POLL_FD_READWRITE)

  // Only allow directory or file operations to be derived from directories.
  #define INHERITING_DIRECTORY_RIGHTS (DIRECTORY_RIGHTS | REGULAR_FILE_RIGHTS)

  int lHostRootFD = open(mRoot.c_str(), O_PATH);
  add_preopen(3, {lHostRootFD, ".", DIRECTORY_RIGHTS, INHERITING_DIRECTORY_RIGHTS});
}

void wasi_context::add_env_host() {
  for (char **env = environ; *env != nullptr; env++) {
    add_env(*env);
  }
}

void wasi_context::add_preopen_host() {
  __wasi_rights_t lStdioRights = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS
                                 | __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_FILESTAT_GET
                                 | __WASI_RIGHTS_POLL_FD_READWRITE;

  mFiles.emplace(0, file{STDIN_FILENO,  "__wembed_stdin", lStdioRights, lStdioRights});
  mFiles.emplace(1, file{STDOUT_FILENO,  "__wembed_stdout", lStdioRights, lStdioRights});
  mFiles.emplace(2, file{STDERR_FILENO,  "__wembed_stderr", lStdioRights, lStdioRights});
}

__wasi_errno_t clock_res_get(void *ctx, __wasi_clockid_t clock_id, timestampptr_w resolution) {
  TRACE_WASI_CALLS(clock_id, resolution);

  auto *lWasmCtx = static_cast<context*>(ctx);

  if ((sizeof(__wasi_timestamp_t) + resolution) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lOut = (__wasi_timestamp_t*)(lWasmCtx->mem()->data() + resolution);

  clockid_t lHostID;
  switch (clock_id) {
    case __WASI_CLOCKID_REALTIME: lHostID = CLOCK_REALTIME; break;
    case __WASI_CLOCKID_MONOTONIC: lHostID = CLOCK_MONOTONIC; break;
    case __WASI_CLOCKID_PROCESS_CPUTIME_ID: lHostID = CLOCK_PROCESS_CPUTIME_ID; break;
    case __WASI_CLOCKID_THREAD_CPUTIME_ID: lHostID = CLOCK_THREAD_CPUTIME_ID; break;
    default:
      TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  }

  timespec lHostOut;
  if (clock_getres(lHostID, &lHostOut))
    TRACE_WASI_RETURN(errno_translate());
  *lOut = lHostOut.tv_nsec + lHostOut.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(resolution, *lOut);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t clock_time_get(void *ctx, __wasi_clockid_t clock_id, __wasi_timestamp_t precision, timestampptr_w time) {
  TRACE_WASI_CALLS(clock_id, precision, time);

  auto *lWasmCtx = static_cast<context*>(ctx);

  if ((sizeof(__wasi_timestamp_t) + time) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lOut = (__wasi_timestamp_t*)(lWasmCtx->mem()->data() + time);

  clockid_t lHostID;
  switch (clock_id) {
    case __WASI_CLOCKID_REALTIME: lHostID = CLOCK_REALTIME; break;
    case __WASI_CLOCKID_MONOTONIC: lHostID = CLOCK_MONOTONIC; break;
    case __WASI_CLOCKID_PROCESS_CPUTIME_ID: lHostID = CLOCK_PROCESS_CPUTIME_ID; break;
    case __WASI_CLOCKID_THREAD_CPUTIME_ID: lHostID = CLOCK_THREAD_CPUTIME_ID; break;
    default:
      TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  }

  timespec lHostOut;
  if (clock_gettime(lHostID, &lHostOut))
    TRACE_WASI_RETURN(errno_translate());
  *lOut = lHostOut.tv_nsec + lHostOut.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(time, *lOut);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_advise(void *ctx, __wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len, __wasi_advice_t advice) {
  TRACE_WASI_CALLS(fd, offset, len, advice);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_ADVISE) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  int lHostAdvice;
  switch (advice) {
    case __WASI_ADVICE_NORMAL: lHostAdvice = POSIX_FADV_NORMAL; break;
    case __WASI_ADVICE_SEQUENTIAL: lHostAdvice = POSIX_FADV_SEQUENTIAL; break;
    case __WASI_ADVICE_RANDOM: lHostAdvice = POSIX_FADV_RANDOM; break;
    case __WASI_ADVICE_WILLNEED: lHostAdvice = POSIX_FADV_WILLNEED; break;
    case __WASI_ADVICE_DONTNEED: lHostAdvice = POSIX_FADV_DONTNEED; break;
    case __WASI_ADVICE_NOREUSE: lHostAdvice = POSIX_FADV_NOREUSE; break;
    default:
      TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  }

  int lResult = posix_fadvise(lIt->second.mHostFD, offset, len, lHostAdvice);
  if (lResult)
    TRACE_WASI_RETURN(errno_translate(lResult));

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_allocate(void *ctx, __wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len) {
  TRACE_WASI_CALLS(fd, offset, len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_ALLOCATE) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  int lResult = posix_fallocate(lIt->second.mHostFD, offset, len);
  if (lResult)
    TRACE_WASI_RETURN(errno_translate(lResult));

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_close(void *ctx, __wasi_fd_t fd) {
  TRACE_WASI_CALLS(fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if(close(lIt->second.mHostFD))
    TRACE_WASI_RETURN(errno_translate());

  lWasiCtx->mFiles.erase(lIt);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_datasync(void *ctx, __wasi_fd_t fd) {
  TRACE_WASI_CALLS(fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_DATASYNC) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  if(fdatasync(lIt->second.mHostFD))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_fdstat_get(void *ctx, __wasi_fd_t fd, fdstatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_fdstat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lDst = (__wasi_fdstat_t*)(lWasmCtx->mem()->data() + buf);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  int result = fcntl(lIt->second.mHostFD, F_GETFL);
  if (result < 0)
    TRACE_WASI_RETURN(errno_translate());

  __wasi_fdflags_t lOutFlags = 0;
  if (result & O_APPEND)
    lOutFlags |= __WASI_FDFLAGS_APPEND;
  if (result & O_DSYNC)
    lOutFlags |= __WASI_FDFLAGS_DSYNC;
  if (result & O_NONBLOCK)
    lOutFlags |= __WASI_FDFLAGS_NONBLOCK;
  if (result & O_RSYNC)
    lOutFlags |= __WASI_FDFLAGS_RSYNC;
  if (result & O_SYNC)
    lOutFlags |= __WASI_FDFLAGS_SYNC;

  struct stat lStat;
  if (fstat(lIt->second.mHostFD, &lStat))
    TRACE_WASI_RETURN(errno_translate());

  __wasi_filetype_t lType = __WASI_FILETYPE_UNKNOWN;
  if (S_ISREG(lStat.st_mode)) lType = __WASI_FILETYPE_REGULAR_FILE;
  else if (S_ISDIR(lStat.st_mode)) lType = __WASI_FILETYPE_DIRECTORY;
  else if (S_ISLNK(lStat.st_mode)) lType = __WASI_FILETYPE_SYMBOLIC_LINK;
  else if (S_ISSOCK(lStat.st_mode)) lType = __WASI_FILETYPE_SOCKET_STREAM; // FIXME: Socket type
  else if (S_ISCHR(lStat.st_mode)) lType = __WASI_FILETYPE_CHARACTER_DEVICE;
  else if (S_ISBLK(lStat.st_mode)) lType = __WASI_FILETYPE_BLOCK_DEVICE;

  lDst->fs_filetype = lType;
  lDst->fs_flags = lOutFlags;
  lDst->fs_rights_base = lIt->second.mRights;
  lDst->fs_rights_inheriting = lIt->second.mIRights;
  TRACE_WASI_OUT(buf, *lDst);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_fdstat_set_flags(void *ctx, __wasi_fd_t fd, __wasi_fdflags_t flags) {
  TRACE_WASI_CALLS(fd, flags);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  __wasi_fdflags_t lOutFlags = 0;
  if (flags & __WASI_FDFLAGS_APPEND)
    lOutFlags |= O_APPEND;
  if (flags & __WASI_FDFLAGS_DSYNC)
    lOutFlags |= O_DSYNC;
  if (flags & __WASI_FDFLAGS_NONBLOCK)
    lOutFlags |= O_NONBLOCK;
  if (flags & __WASI_FDFLAGS_RSYNC)
    lOutFlags |= O_RSYNC;
  if (flags & __WASI_FDFLAGS_SYNC)
    lOutFlags |= O_SYNC;

  if (fcntl(lIt->second.mHostFD, F_SETFL, lOutFlags))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_fdstat_set_rights(void *ctx, __wasi_fd_t fd, __wasi_rights_t fs_rights_base, __wasi_rights_t fs_rights_inheriting) {
  TRACE_WASI_CALLS(fd, fs_rights_base, fs_rights_inheriting);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  __wasi_fdflags_t lNewFlags = lIt->second.mRights & fs_rights_base;
  __wasi_fdflags_t lNewIFlags = lIt->second.mIRights & fs_rights_inheriting;

  if (lNewFlags != fs_rights_base || lNewIFlags != fs_rights_inheriting)
    TRACE_WASI_RETURN(__WASI_ERRNO_NOTCAPABLE);

  lIt->second.mRights = lNewFlags;
  lIt->second.mIRights = lNewIFlags;

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_filestat_get(void *ctx, __wasi_fd_t fd, filestatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_filestat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lDst = (__wasi_filestat_t*)(lWasmCtx->mem()->data() + buf);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_FILESTAT_GET) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  struct stat lStat;
  if (fstat(lIt->second.mHostFD, &lStat))
    TRACE_WASI_RETURN(errno_translate());

  if (S_ISREG(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_REGULAR_FILE;
  else if (S_ISDIR(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_DIRECTORY;
  else if (S_ISLNK(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_SYMBOLIC_LINK;
  else if (S_ISSOCK(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_SOCKET_STREAM; // FIXME: Socket type
  else if (S_ISCHR(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_CHARACTER_DEVICE;
  else if (S_ISBLK(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_BLOCK_DEVICE;

  lDst->dev = lStat.st_dev;
  lDst->ino = lStat.st_ino;
  lDst->size = lStat.st_size;
  lDst->nlink = lStat.st_nlink;
  lDst->atim = lStat.st_atim.tv_nsec + lStat.st_atim.tv_sec * 1'000'000'000;
  lDst->ctim = lStat.st_ctim.tv_nsec + lStat.st_ctim.tv_sec * 1'000'000'000;
  lDst->mtim = lStat.st_mtim.tv_nsec + lStat.st_mtim.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(buf, *lDst);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_filestat_set_size(void *ctx, __wasi_fd_t fd, __wasi_filesize_t st_size) {
  TRACE_WASI_CALLS(fd, st_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_FILESTAT_SET_SIZE) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  if (ftruncate(lIt->second.mHostFD, st_size))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_filestat_set_times(void *ctx, __wasi_fd_t fd, __wasi_timestamp_t st_atim, __wasi_timestamp_t st_mtim, __wasi_fstflags_t fstflags) {
  TRACE_WASI_CALLS(fd, st_atim, st_mtim, fstflags);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_FILESTAT_SET_TIMES) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  struct stat lStat;
  if (fstat(lIt->second.mHostFD, &lStat))
    TRACE_WASI_RETURN(errno_translate());

  timespec lNow;
  if (fstflags & __WASI_FSTFLAGS_ATIM_NOW || fstflags & __WASI_FSTFLAGS_MTIM_NOW) {
    if (clock_gettime(CLOCK_REALTIME, &lNow))
      TRACE_WASI_RETURN(errno_translate());
  }

  timespec lTimes[2];
  lTimes[0] = lStat.st_atim;
  lTimes[1] = lStat.st_mtim;

  if (fstflags & __WASI_FSTFLAGS_ATIM_NOW)
    lTimes[0] = lNow;
  else if (fstflags & __WASI_FSTFLAGS_ATIM) {
    lTimes[0].tv_sec = st_atim / 1'000'000'000;
    lTimes[0].tv_nsec = st_atim - (lTimes[0].tv_sec * 1'000'000'000);
  }
  if (fstflags & __WASI_FSTFLAGS_MTIM_NOW)
    lTimes[1] = lNow;
  else if (fstflags & __WASI_FSTFLAGS_MTIM) {
    lTimes[1].tv_sec = st_mtim / 1'000'000'000;
    lTimes[1].tv_nsec = st_mtim - (lTimes[1].tv_sec * 1'000'000'000);
  }

  if (futimens(lIt->second.mHostFD, lTimes))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_pread(void *ctx, __wasi_fd_t fd, const iovecptr_w iovs, size_w iovs_len, __wasi_filesize_t offset, sizeptr_w nread) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, offset, nread);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_iovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + nread) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNRead = (size_w*)(lWasmCtx->mem()->data() + nread);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_READ) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  }

  int read = preadv(lIt->second.mHostFD, lIOVecs, iovs_len, offset);
  if (read < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNRead = read;
  TRACE_WASI_OUT(nread, *lNRead);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_prestat_get(void *ctx, __wasi_fd_t fd, prestatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_prestat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lDst = (__wasi_prestat_t*)(lWasmCtx->mem()->data() + buf);

  if (lWasiCtx->mPreopens.find(fd) == lWasiCtx->mPreopens.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  auto &lPath = lIt->second.mPath.native();
  if (lPath.size() > size_t(std::numeric_limits<int32_t>::max()))
    TRACE_WASI_RETURN(__WASI_ERRNO_OVERFLOW);

  lDst->pr_type = __WASI_PREOPENTYPE_DIR;
  lDst->u.dir.pr_name_len = lPath.size();
  TRACE_WASI_OUT(buf, *lDst);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_prestat_dir_name(void *ctx, __wasi_fd_t fd, charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if (lWasiCtx->mPreopens.find(fd) == lWasiCtx->mPreopens.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  auto *lDst = (char*)(lWasmCtx->mem()->data() + path);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  auto &lPath = lIt->second.mPath.native();
  if (lPath.size() != size_t(path_len))
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);

  memcpy(lDst, lPath.data(), path_len);
  TRACE_WASI_OUT(path, (std::string_view{lDst, path_len}));
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_pwrite(void *ctx, __wasi_fd_t fd, const ciovecptr_w iovs, size_w iovs_len, __wasi_filesize_t offset, sizeptr_w nwritten) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, offset, nwritten);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_ciovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + nwritten) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNWritten = (size_w*)(lWasmCtx->mem()->data() + nwritten);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_WRITE) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  }

  int written = pwritev(lIt->second.mHostFD, lIOVecs, iovs_len, offset);
  if (written < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNWritten = written;
  TRACE_WASI_OUT(nwritten, *lNWritten);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_read(void *ctx, __wasi_fd_t fd, const ciovecptr_w iovs, size_w iovs_len, sizeptr_w nread) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, nread);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_ciovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + nread) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNRead = (size_w*)(lWasmCtx->mem()->data() + nread);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_READ) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  }

  int read = readv(lIt->second.mHostFD, lIOVecs, iovs_len);
  if (read < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNRead = read;
  TRACE_WASI_OUT(nread, *lNRead);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_readdir(void *ctx, __wasi_fd_t fd, voiptr_w buf, size_w buf_len, __wasi_dircookie_t cookie, sizeptr_w bufused) {
  TRACE_WASI_CALLS(fd, buf, buf_len, cookie, bufused);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (buf_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((buf + buf_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + bufused) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lBuf = (char*)(lWasmCtx->mem()->data() + buf);
  auto *lBufUsed = (size_w*)(lWasmCtx->mem()->data() + bufused);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_READDIR) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  char lHostBuf[buf_len + cookie];
  long lRead = syscall(SYS_getdents64, lIt->second.mHostFD, lHostBuf, sizeof(lHostBuf));
  if (lRead < 0)
    TRACE_WASI_RETURN(errno_translate());

  struct linux_dirent64 {
    ino64_t        d_ino;    /* 64-bit inode number */
    off64_t        d_off;    /* 64-bit offset to next structure */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[]; /* Filename (null-terminated) */
  };

  uint lHostRead = 0;
  uint lCopied = 0;
  while (lHostRead < lRead) {
    auto *lHostCurrent = reinterpret_cast<linux_dirent64*> (lHostBuf + lHostRead);
    auto *lTempCurrent = reinterpret_cast<__wasi_dirent_t*>(lBuf + lCopied);
    auto lNameLength = strlen(lHostCurrent->d_name);
    lHostRead += lHostCurrent->d_reclen;
    if (lHostRead < cookie)
      continue;
    lCopied += sizeof(__wasi_dirent_t) + lNameLength;
    lTempCurrent->d_namlen = lNameLength;
    lTempCurrent->d_next = lHostRead;
    lTempCurrent->d_ino = lHostCurrent->d_ino;
    memcpy((char*)lTempCurrent + sizeof(__wasi_dirent_t), lHostCurrent->d_name, lTempCurrent->d_namlen);
    switch (lHostCurrent->d_type) {
      case DT_REG: lTempCurrent->d_type = __WASI_FILETYPE_REGULAR_FILE; break;
      case DT_DIR: lTempCurrent->d_type = __WASI_FILETYPE_DIRECTORY; break;
      case DT_SOCK: lTempCurrent->d_type = __WASI_FILETYPE_SOCKET_STREAM; break;
      case DT_LNK: lTempCurrent->d_type = __WASI_FILETYPE_SYMBOLIC_LINK; break;
      case DT_BLK: lTempCurrent->d_type = __WASI_FILETYPE_BLOCK_DEVICE; break;
      case DT_CHR: lTempCurrent->d_type = __WASI_FILETYPE_CHARACTER_DEVICE; break;
      default: lTempCurrent->d_type = __WASI_FILETYPE_UNKNOWN; break;
    }
  }
  *lBufUsed = lCopied;
  TRACE_WASI_OUT(bufused, *lBufUsed);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_renumber(void *ctx, __wasi_fd_t from, __wasi_fd_t to) {
  TRACE_WASI_CALLS(from, to);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lItFrom = lWasiCtx->mFiles.find(from);
  if (lItFrom == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);
  auto lItTo = lWasiCtx->mFiles.find(to);
  if (lItTo == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  close(lItTo->second.mHostFD);
  lWasiCtx->mFiles[to] = lItFrom->second;
  lWasiCtx->mFiles.erase(lItFrom);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_seek(void *ctx, __wasi_fd_t fd, __wasi_filedelta_t offset, __wasi_whence_t whence, filesizeptr_w newoffset) {
  TRACE_WASI_CALLS(fd, offset, whence, newoffset);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + newoffset) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lOffset = (__wasi_filesize_t*)(lWasmCtx->mem()->data() + newoffset);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_SEEK) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  int lWhence;
  switch (whence) {
    case __WASI_WHENCE_CUR: lWhence = SEEK_CUR; break;
    case __WASI_WHENCE_SET: lWhence = SEEK_SET; break;
    case __WASI_WHENCE_END: lWhence = SEEK_END; break;
    default: TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  }

  off_t lResult = lseek(lIt->second.mHostFD, offset, lWhence);
  if (lResult < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lOffset = lResult;
  TRACE_WASI_OUT(newoffset, *lOffset);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_sync(void *ctx, __wasi_fd_t fd) {
  TRACE_WASI_CALLS(fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_SYNC) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  if(fsync(lIt->second.mHostFD))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_tell(void *ctx, __wasi_fd_t fd, filesizeptr_w newoffset) {
  TRACE_WASI_CALLS(fd, newoffset);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + newoffset) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lOffset = (__wasi_filesize_t*)(lWasmCtx->mem()->data() + newoffset);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_TELL) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  off_t lResult = lseek(lIt->second.mHostFD, 0, SEEK_CUR);
  if (lResult < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lOffset = lResult;
  TRACE_WASI_OUT(newoffset, *lOffset);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t fd_write(void *ctx, __wasi_fd_t fd, const ciovecptr_w iovs, size_w iovs_len, sizeptr_w nwritten) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, nwritten);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_ciovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + nwritten) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNWritten = (size_w*)(lWasmCtx->mem()->data() + nwritten);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_WRITE) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  }

  int written = writev(lIt->second.mHostFD, lIOVecs, iovs_len);
  if (written < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNWritten = written;
  TRACE_WASI_OUT(nwritten, *lNWritten);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_create_directory(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if (mkdirat(lIt->second.mHostFD, lPath, 0755))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_filestat_get(void *ctx, __wasi_fd_t fd, __wasi_lookupflags_t flags, const charptr_w path, size_w path_len, filestatptr_w buf) {
  TRACE_WASI_CALLS(fd, flags, path, path_len, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(__wasi_filestat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);
  auto *lDst = (__wasi_filestat_t*)(lWasmCtx->mem()->data() + buf);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_READDIR) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  int lSearchFlags = 0;
  if ((flags & __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW) == 0)
    lSearchFlags |= AT_SYMLINK_NOFOLLOW;

  struct stat lStat;
  if (fstatat(lIt->second.mHostFD, lPath, &lStat, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());

  if (S_ISREG(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_REGULAR_FILE;
  else if (S_ISDIR(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_DIRECTORY;
  else if (S_ISLNK(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_SYMBOLIC_LINK;
  else if (S_ISSOCK(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_SOCKET_STREAM; // FIXME: Socket type
  else if (S_ISCHR(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_CHARACTER_DEVICE;
  else if (S_ISBLK(lStat.st_mode)) lDst->filetype = __WASI_FILETYPE_BLOCK_DEVICE;

  lDst->dev = lStat.st_dev;
  lDst->ino = lStat.st_ino;
  lDst->size = lStat.st_size;
  lDst->nlink = lStat.st_nlink;
  lDst->atim = lStat.st_atim.tv_nsec + lStat.st_atim.tv_sec * 1'000'000'000;
  lDst->ctim = lStat.st_ctim.tv_nsec + lStat.st_ctim.tv_sec * 1'000'000'000;
  lDst->mtim = lStat.st_mtim.tv_nsec + lStat.st_mtim.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(buf, *lDst);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_filestat_set_times(void *ctx, __wasi_fd_t fd, __wasi_lookupflags_t flags, const charptr_w path, size_w path_len, __wasi_timestamp_t st_atim, __wasi_timestamp_t st_mtim, __wasi_fstflags_t fstflags) {
  TRACE_WASI_CALLS(fd, flags, path, path_len, st_atim, st_mtim, fstflags);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if ((lIt->second.mRights & __WASI_RIGHTS_FD_FILESTAT_SET_TIMES) == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_ACCES);

  int lSearchFlags = 0;
  if ((flags & __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW) == 0)
    lSearchFlags |= AT_SYMLINK_NOFOLLOW;

  struct stat lStat;
  if (fstatat(lIt->second.mHostFD, lPath, &lStat, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());

  timespec lNow;
  if (fstflags & __WASI_FSTFLAGS_ATIM_NOW || fstflags & __WASI_FSTFLAGS_MTIM_NOW) {
    if (clock_gettime(CLOCK_REALTIME, &lNow))
      TRACE_WASI_RETURN(errno_translate());
  }

  timespec lTimes[2];
  lTimes[0] = lStat.st_atim;
  lTimes[1] = lStat.st_mtim;

  if (fstflags & __WASI_FSTFLAGS_ATIM_NOW)
    lTimes[0] = lNow;
  else if (fstflags & __WASI_FSTFLAGS_ATIM) {
    lTimes[0].tv_sec = st_atim / 1'000'000'000;
    lTimes[0].tv_nsec = st_atim - (lTimes[0].tv_sec * 1'000'000'000);
  }
  if (fstflags & __WASI_FSTFLAGS_MTIM_NOW)
    lTimes[1] = lNow;
  else if (fstflags & __WASI_FSTFLAGS_MTIM) {
    lTimes[1].tv_sec = st_mtim / 1'000'000'000;
    lTimes[1].tv_nsec = st_mtim - (lTimes[1].tv_sec * 1'000'000'000);
  }

  if (utimensat(lIt->second.mHostFD, lPath, lTimes, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_link(void *ctx, __wasi_fd_t old_fd, __wasi_lookupflags_t old_flags, const charptr_w old_path, size_w old_path_len, __wasi_fd_t new_fd, const charptr_w new_path, size_w new_path_len) {
  TRACE_WASI_CALLS(old_fd, old_flags, old_path, old_path_len, new_fd, new_path, new_path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (old_path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if (new_path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((old_path + old_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((new_path + new_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lOldPath = (char*)(lWasmCtx->mem()->data() + old_path);
  auto *lNewPath = (char*)(lWasmCtx->mem()->data() + new_path);

  TRACE_WASI_IN_STR(lOldPath, old_path_len);
  TRACE_WASI_IN_STR(lOldPath, new_path_len);

  auto lOld = lWasiCtx->mFiles.find(old_fd);
  if (lOld == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);
  auto lNew = lWasiCtx->mFiles.find(new_fd);
  if (lNew == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  int lSearchFlags = 0;
  if (old_flags & __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW)
    lSearchFlags |= AT_SYMLINK_FOLLOW;

  if (linkat(lOld->second.mHostFD, lOldPath, lNew->second.mHostFD, lNewPath, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_open(void *ctx, __wasi_fd_t dirfd, __wasi_lookupflags_t dirflags, const charptr_w path, size_w path_len, __wasi_oflags_t oflags, __wasi_rights_t fs_rights_base, __wasi_rights_t fs_rights_inheriting, __wasi_fdflags_t fs_flags, fdptr_w fd) {
  TRACE_WASI_CALLS(dirfd, dirflags, path, path_len, oflags, fs_rights_base, fs_rights_inheriting, fs_flags, fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(__wasi_fd_t) + fd) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);
  auto *lFD = (__wasi_fd_t *)(lWasmCtx->mem()->data() + fd);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lDir = lWasiCtx->mFiles.find(dirfd);
  if (lDir == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  int lOpenFlags = 0;
  if (oflags & __WASI_OFLAGS_CREAT)
    lOpenFlags |= O_CREAT;
  if (oflags & __WASI_OFLAGS_DIRECTORY)
    lOpenFlags |= O_DIRECTORY;
  if (oflags & __WASI_OFLAGS_EXCL)
    lOpenFlags |= O_EXCL;
  if (oflags & __WASI_OFLAGS_TRUNC)
    lOpenFlags |= O_TRUNC;
  if ((dirflags & __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW) == 0)
    lOpenFlags |= O_NOFOLLOW;
  if (fs_flags & __WASI_FDFLAGS_APPEND)
    lOpenFlags |= O_APPEND;
  if (fs_flags & __WASI_FDFLAGS_DSYNC)
    lOpenFlags |= O_DSYNC;
  if (fs_flags & __WASI_FDFLAGS_NONBLOCK)
    lOpenFlags |= O_NONBLOCK;
  if (fs_flags & __WASI_FDFLAGS_RSYNC)
    lOpenFlags |= O_RSYNC;
  if (fs_flags & __WASI_FDFLAGS_SYNC)
    lOpenFlags |= O_SYNC;

  constexpr __wasi_rights_t lReadMask = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_READDIR;
  constexpr __wasi_rights_t lWriteMask = __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_ALLOCATE | __WASI_RIGHTS_FD_FILESTAT_SET_SIZE;
  const bool lRead = fs_rights_base & lReadMask;
  const bool lWrite = fs_rights_base & lWriteMask;
  if (lRead && !lWrite)
    lOpenFlags |= O_RDONLY;
  else if (lWrite && !lRead)
    lOpenFlags |= O_WRONLY;
  else if (lWrite && lRead)
    lOpenFlags |= O_RDWR;

  int lResult = openat(lDir->second.mHostFD, lPath, lOpenFlags, 0664);
  if (lResult < 0)
    TRACE_WASI_RETURN(errno_translate());

  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<__wasi_fd_t> dis(1000, 1U<<30U);

  __wasi_fd_t lNewFD;
  do {
    lNewFD = dis(gen);
  } while (lWasiCtx->mFiles.find(lNewFD) != lWasiCtx->mFiles.end());

  *lFD = lNewFD;
  TRACE_WASI_OUT(fd, *lFD);

  lWasiCtx->mFiles.emplace(lNewFD, wasi_context::file{lResult, lDir->second.mPath / lPath, fs_rights_base, fs_rights_inheriting});

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_readlink(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len, charptr_w buf, size_w buf_len, sizeptr_w bufused) {
  TRACE_WASI_CALLS(fd, path, path_len, buf, buf_len, bufused);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if (buf_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((buf + buf_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + bufused) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);
  auto *lBuf = (char*)(lWasmCtx->mem()->data() + buf);
  auto *lBufUsed = (size_w*)(lWasmCtx->mem()->data() + bufused);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  int lResult = readlinkat(lIt->second.mHostFD, lPath, lBuf, buf_len);
  if (lResult < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lBufUsed = lResult;
  TRACE_WASI_OUT(bufused, *lBufUsed);
  TRACE_WASI_OUT(buf, lBuf);
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_remove_directory(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if (unlinkat(lIt->second.mHostFD, lPath, AT_REMOVEDIR))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_rename(void *ctx, __wasi_fd_t old_fd, const charptr_w old_path, size_w old_path_len, __wasi_fd_t new_fd, const charptr_w new_path, size_w new_path_len) {
  TRACE_WASI_CALLS(old_fd, old_path, old_path_len, new_fd, new_path, new_path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (old_path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if (new_path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((old_path + old_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((new_path + new_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lOldPath = (char*)(lWasmCtx->mem()->data() + old_path);
  auto *lNewPath = (char*)(lWasmCtx->mem()->data() + new_path);

  TRACE_WASI_IN_STR(lOldPath, old_path_len);
  TRACE_WASI_IN_STR(lNewPath, new_path_len);

  auto lOld = lWasiCtx->mFiles.find(old_fd);
  if (lOld == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);
  auto lNew = lWasiCtx->mFiles.find(new_fd);
  if (lNew == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if (renameat(lOld->second.mHostFD, lOldPath, lNew->second.mHostFD, lNewPath))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_symlink(void *ctx, const charptr_w old_path, size_w old_path_len, __wasi_fd_t fd, const charptr_w new_path, size_w new_path_len) {
  TRACE_WASI_CALLS(old_path, old_path_len, fd, new_path, new_path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (new_path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if (old_path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((old_path + old_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((new_path + new_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lOldPath = (char*)(lWasmCtx->mem()->data() + old_path);
  auto *lNewPath = (char*)(lWasmCtx->mem()->data() + new_path);

  TRACE_WASI_IN_STR(lOldPath, old_path_len);
  TRACE_WASI_IN_STR(lNewPath, new_path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if (symlinkat(lOldPath, lIt->second.mHostFD, lNewPath))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t path_unlink_file(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_ERRNO_BADF);

  if (unlinkat(lIt->second.mHostFD, lPath, 0))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t proc_raise(void *ctx, __wasi_signal_t sig) {
  TRACE_WASI_CALLS(sig);

  int lCode;
  switch (sig) {
    case __WASI_SIGNAL_HUP : lCode = SIGHUP; break;
    case __WASI_SIGNAL_INT : lCode = SIGINT; break;
    case __WASI_SIGNAL_QUIT : lCode = SIGQUIT; break;
    case __WASI_SIGNAL_ILL : lCode = SIGILL; break;
    case __WASI_SIGNAL_TRAP : lCode = SIGTRAP; break;
    case __WASI_SIGNAL_ABRT : lCode = SIGABRT; break;
    case __WASI_SIGNAL_BUS : lCode = SIGBUS; break;
    case __WASI_SIGNAL_FPE : lCode = SIGFPE; break;
    case __WASI_SIGNAL_KILL : lCode = SIGKILL; break;
    case __WASI_SIGNAL_USR1 : lCode = SIGUSR1; break;
    case __WASI_SIGNAL_SEGV : lCode = SIGSEGV; break;
    case __WASI_SIGNAL_USR2 : lCode = SIGUSR2; break;
    case __WASI_SIGNAL_PIPE : lCode = SIGPIPE; break;
    case __WASI_SIGNAL_ALRM : lCode = SIGALRM; break;
    case __WASI_SIGNAL_TERM : lCode = SIGTERM; break;
    case __WASI_SIGNAL_CHLD : lCode = SIGCHLD; break;
    case __WASI_SIGNAL_CONT : lCode = SIGCONT; break;
    case __WASI_SIGNAL_STOP : lCode = SIGSTOP; break;
    case __WASI_SIGNAL_TSTP : lCode = SIGTSTP; break;
    case __WASI_SIGNAL_TTIN : lCode = SIGTTIN; break;
    case __WASI_SIGNAL_TTOU : lCode = SIGTTOU; break;
    case __WASI_SIGNAL_URG : lCode = SIGURG; break;
    case __WASI_SIGNAL_XCPU : lCode = SIGXCPU; break;
    case __WASI_SIGNAL_XFSZ : lCode = SIGXFSZ; break;
    case __WASI_SIGNAL_VTALRM : lCode = SIGVTALRM; break;
    case __WASI_SIGNAL_PROF : lCode = SIGPROF; break;
    case __WASI_SIGNAL_WINCH : lCode = SIGWINCH; break;
    case __WASI_SIGNAL_POLL : lCode = SIGPOLL; break;
    case __WASI_SIGNAL_PWR : lCode = SIGPWR; break;
    case __WASI_SIGNAL_SYS : lCode = SIGSYS; break;
    default: TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  }

  if (!raise(lCode))
    TRACE_WASI_RETURN(__WASI_ERRNO_NOTCAPABLE);
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t random_get(void *ctx, voiptr_w buf, size_w buf_len) {
  TRACE_WASI_CALLS(buf, buf_len);

  auto *lWasmCtx = static_cast<context*>(ctx);

  if (buf_len == 0)
    TRACE_WASI_RETURN(__WASI_ERRNO_INVAL);
  if ((buf + buf_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lBuf = (char*)(lWasmCtx->mem()->data() + buf);

  auto lWritten = getrandom(lBuf, buf_len, 0);
  if (lWritten < 0)
    TRACE_WASI_RETURN(errno_translate());
  if (lWritten != buf_len)
    TRACE_WASI_RETURN(__WASI_ERRNO_AGAIN);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t sched_yield(void *ctx) {
  TRACE_WASI_CALLS("no inputs");
  if (::sched_yield())
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

}  // namespace wembed::wasi
