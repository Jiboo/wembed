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

#ifdef WEMBED_TRACE_WASI_CALLS
#include <iostream>
#include <string_view>
#endif

namespace wembed::wasi {

#ifdef WEMBED_TRACE_WASI_CALLS

std::ostream & operator<<(std::ostream &os, const __wasi_fdstat_t &pStat) {
  return os << "fdstat_t{" << (int)pStat.fs_filetype << ", " << pStat.fs_flags
     << ", " << pStat.fs_rights_base << ", " << pStat.fs_rights_inheriting
     << "}";
}
std::ostream & operator<<(std::ostream &os, const __wasi_filestat_t &pStat) {
  return os << "filestat_t{" << pStat.st_dev << ", " << pStat.st_ino
     << ", " << (int)pStat.st_filetype << ", " << pStat.st_nlink
     << ", " << pStat.st_size << ", " << pStat.st_atim
     << ", " << pStat.st_mtim << ", " << pStat.st_ctim
     << "}";
}
std::ostream & operator<<(std::ostream &os, const __wasi_prestat_t &pStat) {
  os << "prestat_t{" << (int)pStat.pr_type << ", ";
  switch (pStat.pr_type) {
    case __WASI_PREOPENTYPE_DIR: {
      os << pStat.u.dir.pr_name_len;
    }
  }
  return os << "}";
}

template <typename T>
void wasi_trace_args(std::string_view pName, const T &pValue)
{
  std::cout << pName << " = " << pValue;
}
template <typename T, typename... TRest>
void wasi_trace_args(std::string_view pNames, const T &pArg1, const TRest &...pArgs)
{
  auto lComma = pNames.find(',', 1);
  auto lArg1Name = pNames.substr(0, lComma);
  std::cout << lArg1Name << " = " << pArg1 << ", ";
  auto lRestNames = pNames.substr(lComma + 2);
  wasi_trace_args(lRestNames, pArgs...);
}
#define TRACE_WASI_CALLS(...) { std::cout << "[wasi] " << __FUNCTION__ << " ("; wasi_trace_args(#__VA_ARGS__, __VA_ARGS__); std::cout << ")" << std::endl; }

template<typename T>
void wasi_trace_write(const char *pName, const T &pValue) {
  std::cout << "[wasi] \tout: " << pName << " => " << pValue << std::endl;
}
#define TRACE_WASI_OUT(out, val) { wasi_trace_write(#out, val); }

void wasi_trace_read(const char *pName, std::string_view pStr) {
  std::cout << "[wasi] \tin: " << pName << " => \"" << pStr << "\"" << std::endl;
}
#define TRACE_WASI_IN_STR(in, len) { wasi_trace_read(#in, std::string_view{in, (uint)len}); }

std::string_view wasi_errno_str(wembed::wasi::__wasi_errno_t pCode) {
  using namespace std::literals;
  switch(pCode) {
    case __WASI_ESUCCESS: return "SUCCESS"sv;
    case __WASI_E2BIG: return "E2BIG"sv;
    case __WASI_EACCES: return "EACCES"sv;
    case __WASI_EADDRINUSE: return "EADDRINUSE"sv;
    case __WASI_EADDRNOTAVAIL: return "EADDRNOTAVAIL"sv;
    case __WASI_EAFNOSUPPORT: return "EAFNOSUPPORT"sv;
    case __WASI_EAGAIN: return "EAGAIN"sv;
    case __WASI_EALREADY: return "EALREADY"sv;
    case __WASI_EBADF: return "EBADF"sv;
    case __WASI_EBADMSG: return "EBADMSG"sv;
    case __WASI_EBUSY: return "EBUSY"sv;
    case __WASI_ECANCELED: return "ECANCELED"sv;
    case __WASI_ECHILD: return "ECHILD"sv;
    case __WASI_ECONNABORTED: return "ECONNABORTED"sv;
    case __WASI_ECONNREFUSED: return "ECONNREFUSED"sv;
    case __WASI_ECONNRESET: return "ECONNRESET"sv;
    case __WASI_EDEADLK: return "EDEADLK"sv;
    case __WASI_EDESTADDRREQ: return "EDESTADDRREQ"sv;
    case __WASI_EDOM: return "EDOM"sv;
    case __WASI_EDQUOT: return "EDQUOT"sv;
    case __WASI_EEXIST: return "EEXIST"sv;
    case __WASI_EFAULT: return "EFAULT"sv;
    case __WASI_EFBIG: return "EFBIG"sv;
    case __WASI_EHOSTUNREACH: return "EHOSTUNREACH"sv;
    case __WASI_EIDRM: return "EIDRM"sv;
    case __WASI_EILSEQ: return "EILSEQ"sv;
    case __WASI_EINPROGRESS: return "EINPROGRESS"sv;
    case __WASI_EINTR: return "EINTR"sv;
    case __WASI_EINVAL: return "EINVAL"sv;
    case __WASI_EIO: return "EIO"sv;
    case __WASI_EISCONN: return "EISCONN"sv;
    case __WASI_EISDIR: return "EISDIR"sv;
    case __WASI_ELOOP: return "ELOOP"sv;
    case __WASI_EMFILE: return "EMFILE"sv;
    case __WASI_EMLINK: return "EMLINK"sv;
    case __WASI_EMSGSIZE: return "EMSGSIZE"sv;
    case __WASI_EMULTIHOP: return "EMULTIHOP"sv;
    case __WASI_ENAMETOOLONG: return "ENAMETOOLONG"sv;
    case __WASI_ENETDOWN: return "ENETDOWN"sv;
    case __WASI_ENETRESET: return "ENETRESET"sv;
    case __WASI_ENETUNREACH: return "ENETUNREACH"sv;
    case __WASI_ENFILE: return "ENFILE"sv;
    case __WASI_ENOBUFS: return "ENOBUFS"sv;
    case __WASI_ENODEV: return "ENODEV"sv;
    case __WASI_ENOENT: return "ENOENT"sv;
    case __WASI_ENOEXEC: return "ENOEXEC"sv;
    case __WASI_ENOLCK: return "ENOLCK"sv;
    case __WASI_ENOLINK: return "ENOLINK"sv;
    case __WASI_ENOMEM: return "ENOMEM"sv;
    case __WASI_ENOMSG: return "ENOMSG"sv;
    case __WASI_ENOPROTOOPT: return "ENOPROTOOPT"sv;
    case __WASI_ENOSPC: return "ENOSPC"sv;
    case __WASI_ENOSYS: return "ENOSYS"sv;
    case __WASI_ENOTCONN: return "ENOTCONN"sv;
    case __WASI_ENOTDIR: return "ENOTDIR"sv;
    case __WASI_ENOTEMPTY: return "ENOTEMPTY"sv;
    case __WASI_ENOTRECOVERABLE: return "ENOTRECOVERABLE"sv;
    case __WASI_ENOTSOCK: return "ENOTSOCK"sv;
    case __WASI_ENOTSUP: return "ENOTSUP"sv;
    case __WASI_ENOTTY: return "ENOTTY"sv;
    case __WASI_ENXIO: return "ENXIO"sv;
    case __WASI_EOVERFLOW: return "EOVERFLOW"sv;
    case __WASI_EOWNERDEAD: return "EOWNERDEAD"sv;
    case __WASI_EPERM: return "EPERM"sv;
    case __WASI_EPIPE: return "EPIPE"sv;
    case __WASI_EPROTO: return "EPROTO"sv;
    case __WASI_EPROTONOSUPPORT: return "EPROTONOSUPPORT"sv;
    case __WASI_EPROTOTYPE: return "EPROTOTYPE"sv;
    case __WASI_ERANGE: return "ERANGE"sv;
    case __WASI_EROFS: return "EROFS"sv;
    case __WASI_ESPIPE: return "ESPIPE"sv;
    case __WASI_ESRCH: return "ESRCH"sv;
    case __WASI_ESTALE: return "ESTALE"sv;
    case __WASI_ETIMEDOUT: return "ETIMEDOUT"sv;
    case __WASI_ETXTBSY: return "ETXTBSY"sv;
    case __WASI_EXDEV: return "EXDEV"sv;
    case __WASI_ENOTCAPABLE: return "ENOTCAPABLE"sv;
    default: return "<invalid>"sv;
  }
}
#define TRACE_WASI_RETURN(expr) { __wasi_errno_t value = expr; if (value) { std::cout << "[wasi] \tres: " << wasi_errno_str(value) << std::endl; } return value; }
#else
#define TRACE_WASI_CALLS(...)
#define TRACE_WASI_IN_STR(in, len)
#define TRACE_WASI_OUT(out, val)
#define TRACE_WASI_RETURN(value) return value
#endif

wasi_context::wasi_context(std::filesystem::path pRoot) : mRoot(std::move(pRoot)) {
  // Operations that apply to regular files.
  #define REGULAR_FILE_RIGHTS                                                                        \
    (__WASI_RIGHT_FD_DATASYNC | __WASI_RIGHT_FD_READ | __WASI_RIGHT_FD_SEEK                        \
     | __WASI_RIGHT_FD_FDSTAT_SET_FLAGS | __WASI_RIGHT_FD_SYNC | __WASI_RIGHT_FD_TELL              \
     | __WASI_RIGHT_FD_WRITE | __WASI_RIGHT_FD_ADVISE | __WASI_RIGHT_FD_ALLOCATE                   \
     | __WASI_RIGHT_FD_FILESTAT_GET | __WASI_RIGHT_FD_FILESTAT_SET_SIZE                            \
     | __WASI_RIGHT_FD_FILESTAT_SET_TIMES | __WASI_RIGHT_POLL_FD_READWRITE)

  // Only allow directory operations on directories.
  #define DIRECTORY_RIGHTS                                                                           \
    (__WASI_RIGHT_FD_FDSTAT_SET_FLAGS | __WASI_RIGHT_FD_SYNC | __WASI_RIGHT_FD_ADVISE              \
     | __WASI_RIGHT_PATH_CREATE_DIRECTORY | __WASI_RIGHT_PATH_CREATE_FILE                          \
     | __WASI_RIGHT_PATH_LINK_SOURCE | __WASI_RIGHT_PATH_LINK_TARGET | __WASI_RIGHT_PATH_OPEN      \
     | __WASI_RIGHT_FD_READDIR | __WASI_RIGHT_PATH_READLINK | __WASI_RIGHT_PATH_RENAME_SOURCE      \
     | __WASI_RIGHT_PATH_RENAME_TARGET | __WASI_RIGHT_PATH_FILESTAT_GET                            \
     | __WASI_RIGHT_PATH_FILESTAT_SET_SIZE | __WASI_RIGHT_PATH_FILESTAT_SET_TIMES                  \
     | __WASI_RIGHT_FD_FILESTAT_GET | __WASI_RIGHT_FD_FILESTAT_SET_TIMES                           \
     | __WASI_RIGHT_PATH_SYMLINK | __WASI_RIGHT_PATH_UNLINK_FILE                                   \
     | __WASI_RIGHT_PATH_REMOVE_DIRECTORY | __WASI_RIGHT_POLL_FD_READWRITE)
  // Only allow directory or file operations to be derived from directories.
  #define INHERITING_DIRECTORY_RIGHTS (DIRECTORY_RIGHTS | REGULAR_FILE_RIGHTS)

  int lHostRootFD = open(mRoot.c_str(), O_PATH);
  add_preopen(3, {lHostRootFD, ".", DIRECTORY_RIGHTS, INHERITING_DIRECTORY_RIGHTS});
}

void wasi_context::add_arg(std::string_view pArg) {
  auto lOffset = mArgs.mBuffer.size();
  auto lArgLen = pArg.size();
  mArgs.mElements.emplace_back(lOffset);
  mArgs.mBuffer.resize(lOffset + lArgLen + 1);
  memcpy(mArgs.mBuffer.data() + lOffset, pArg.data(), lArgLen);
}

void wasi_context::add_args(int pArgc, const char **pArgv) {
  for (int lArg = 0; lArg < pArgc; lArg++) {
    add_arg(pArgv[lArg]);
  }
}

void wasi_context::add_env(std::string_view pEnv) {
  auto lOffset = mEnv.mBuffer.size();
  auto lEnvLen = pEnv.size();
  mEnv.mElements.emplace_back(lOffset);
  mEnv.mBuffer.resize(lOffset + lEnvLen + 1);
  memcpy(mEnv.mBuffer.data() + lOffset, pEnv.data(), lEnvLen);
}

void wasi_context::add_env_host() {
  for (char **env = environ; *env != nullptr; env++) {
    add_env(*env);
  }
}

void wasi_context::add_preopen(__wasi_fd_t pFD, file pFile) {
  mFiles.emplace(pFD, std::move(pFile));
  mPreopens.emplace(pFD);
}

void wasi_context::add_preopen_host() {
  __wasi_rights_t lStdioRights = __WASI_RIGHT_FD_READ | __WASI_RIGHT_FD_FDSTAT_SET_FLAGS
                               | __WASI_RIGHT_FD_WRITE | __WASI_RIGHT_FD_FILESTAT_GET
                               | __WASI_RIGHT_POLL_FD_READWRITE;

  mFiles.emplace(0, file{STDIN_FILENO,  "__wembed_stdin", lStdioRights, lStdioRights});
  mFiles.emplace(1, file{STDOUT_FILENO,  "__wembed_stdout", lStdioRights, lStdioRights});
  mFiles.emplace(2, file{STDERR_FILENO,  "__wembed_stderr", lStdioRights, lStdioRights});
}

__wasi_errno_t errno_translate(int error = errno) {
  switch(error) {
    case E2BIG: TRACE_WASI_RETURN(__WASI_E2BIG);
    case EACCES: TRACE_WASI_RETURN(__WASI_EACCES);
    case EADDRINUSE: TRACE_WASI_RETURN(__WASI_EADDRINUSE);
    case EADDRNOTAVAIL: TRACE_WASI_RETURN(__WASI_EADDRNOTAVAIL);
    case EAFNOSUPPORT: TRACE_WASI_RETURN(__WASI_EAFNOSUPPORT);
    case EAGAIN: TRACE_WASI_RETURN(__WASI_EAGAIN);
    case EALREADY: TRACE_WASI_RETURN(__WASI_EALREADY);
    case EBADF: TRACE_WASI_RETURN(__WASI_EBADF);
    case EBADMSG: TRACE_WASI_RETURN(__WASI_EBADMSG);
    case EBUSY: TRACE_WASI_RETURN(__WASI_EBUSY);
    case ECANCELED: TRACE_WASI_RETURN(__WASI_ECANCELED);
    case ECHILD: TRACE_WASI_RETURN(__WASI_ECHILD);
    case ECONNABORTED: TRACE_WASI_RETURN(__WASI_ECONNABORTED);
    case ECONNREFUSED: TRACE_WASI_RETURN(__WASI_ECONNREFUSED);
    case ECONNRESET: TRACE_WASI_RETURN(__WASI_ECONNRESET);
    case EDEADLK: TRACE_WASI_RETURN(__WASI_EDEADLK);
    case EDESTADDRREQ: TRACE_WASI_RETURN(__WASI_EDESTADDRREQ);
    case EDOM: TRACE_WASI_RETURN(__WASI_EDOM);
    case EDQUOT: TRACE_WASI_RETURN(__WASI_EDQUOT);
    case EEXIST: TRACE_WASI_RETURN(__WASI_EEXIST);
    case EFAULT: TRACE_WASI_RETURN(__WASI_EFAULT);
    case EFBIG: TRACE_WASI_RETURN(__WASI_EFBIG);
    case EHOSTUNREACH: TRACE_WASI_RETURN(__WASI_EHOSTUNREACH);
    case EIDRM: TRACE_WASI_RETURN(__WASI_EIDRM);
    case EILSEQ: TRACE_WASI_RETURN(__WASI_EILSEQ);
    case EINPROGRESS: TRACE_WASI_RETURN(__WASI_EINPROGRESS);
    case EINTR: TRACE_WASI_RETURN(__WASI_EINTR);
    case EINVAL: TRACE_WASI_RETURN(__WASI_EINVAL);
    case EIO: TRACE_WASI_RETURN(__WASI_EIO);
    case EISCONN: TRACE_WASI_RETURN(__WASI_EISCONN);
    case EISDIR: TRACE_WASI_RETURN(__WASI_EISDIR);
    case ELOOP: TRACE_WASI_RETURN(__WASI_ELOOP);
    case EMFILE: TRACE_WASI_RETURN(__WASI_EMFILE);
    case EMLINK: TRACE_WASI_RETURN(__WASI_EMLINK);
    case EMSGSIZE: TRACE_WASI_RETURN(__WASI_EMSGSIZE);
    case EMULTIHOP: TRACE_WASI_RETURN(__WASI_EMULTIHOP);
    case ENAMETOOLONG: TRACE_WASI_RETURN(__WASI_ENAMETOOLONG);
    case ENETDOWN: TRACE_WASI_RETURN(__WASI_ENETDOWN);
    case ENETRESET: TRACE_WASI_RETURN(__WASI_ENETRESET);
    case ENETUNREACH: TRACE_WASI_RETURN(__WASI_ENETUNREACH);
    case ENFILE: TRACE_WASI_RETURN(__WASI_ENFILE);
    case ENOBUFS: TRACE_WASI_RETURN(__WASI_ENOBUFS);
    case ENODEV: TRACE_WASI_RETURN(__WASI_ENODEV);
    case ENOENT: TRACE_WASI_RETURN(__WASI_ENOENT);
    case ENOEXEC: TRACE_WASI_RETURN(__WASI_ENOEXEC);
    case ENOLCK: TRACE_WASI_RETURN(__WASI_ENOLCK);
    case ENOLINK: TRACE_WASI_RETURN(__WASI_ENOLINK);
    case ENOMEM: TRACE_WASI_RETURN(__WASI_ENOMEM);
    case ENOMSG: TRACE_WASI_RETURN(__WASI_ENOMSG);
    case ENOPROTOOPT: TRACE_WASI_RETURN(__WASI_ENOPROTOOPT);
    case ENOSPC: TRACE_WASI_RETURN(__WASI_ENOSPC);
    case ENOSYS: TRACE_WASI_RETURN(__WASI_ENOSYS);
    case ENOTCONN: TRACE_WASI_RETURN(__WASI_ENOTCONN);
    case ENOTDIR: TRACE_WASI_RETURN(__WASI_ENOTDIR);
    case ENOTEMPTY: TRACE_WASI_RETURN(__WASI_ENOTEMPTY);
    case ENOTRECOVERABLE: TRACE_WASI_RETURN(__WASI_ENOTRECOVERABLE);
    case ENOTSOCK: TRACE_WASI_RETURN(__WASI_ENOTSOCK);
    case ENOTSUP: TRACE_WASI_RETURN(__WASI_ENOTSUP);
    case ENOTTY: TRACE_WASI_RETURN(__WASI_ENOTTY);
    case ENXIO: TRACE_WASI_RETURN(__WASI_ENXIO);
    case EOVERFLOW: TRACE_WASI_RETURN(__WASI_EOVERFLOW);
    case EOWNERDEAD: TRACE_WASI_RETURN(__WASI_EOWNERDEAD);
    case EPERM: TRACE_WASI_RETURN(__WASI_EPERM);
    case EPIPE: TRACE_WASI_RETURN(__WASI_EPIPE);
    case EPROTO: TRACE_WASI_RETURN(__WASI_EPROTO);
    case EPROTONOSUPPORT: TRACE_WASI_RETURN(__WASI_EPROTONOSUPPORT);
    case EPROTOTYPE: TRACE_WASI_RETURN(__WASI_EPROTOTYPE);
    case ERANGE: TRACE_WASI_RETURN(__WASI_ERANGE);
    case EROFS: TRACE_WASI_RETURN(__WASI_EROFS);
    case ESPIPE: TRACE_WASI_RETURN(__WASI_ESPIPE);
    case ESRCH: TRACE_WASI_RETURN(__WASI_ESRCH);
    case ESTALE: TRACE_WASI_RETURN(__WASI_ESTALE);
    case ETIMEDOUT: TRACE_WASI_RETURN(__WASI_ETIMEDOUT);
    case ETXTBSY: TRACE_WASI_RETURN(__WASI_ETXTBSY);
    case EXDEV: TRACE_WASI_RETURN(__WASI_EXDEV);
    default: TRACE_WASI_RETURN(__WASI_ENOTCAPABLE);
  }
}

__wasi_errno_t args_get(void *ctx, charptrptr_w argv, charptr_w argv_buf) {
  TRACE_WASI_CALLS(argv, argv_buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((lWasiCtx->mArgs.mElements.size() * sizeof(charptr_w) + argv) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((lWasiCtx->mArgs.mBuffer.size() + argv_buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lArgv = (charptr_w*)(lWasmCtx->mem()->data() + argv);
  auto *lBuff = (char*)(lWasmCtx->mem()->data() + argv_buf);

  memcpy(lBuff, lWasiCtx->mArgs.mBuffer.data(), lWasiCtx->mArgs.mBuffer.size());

  auto lCount = lWasiCtx->mArgs.mElements.size();
  for (size_t i = 0; i < lCount; i++) {
    lArgv[i] = argv_buf + lWasiCtx->mArgs.mElements[i];
  }

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t args_sizes_get(void *ctx, sizeptr_w argc, sizeptr_w argv_buf_size) {
  TRACE_WASI_CALLS(argc, argv_buf_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + argc) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + argv_buf_size) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lCount = (size_w*)(lWasmCtx->mem()->data() + argc);
  auto *lBufSize = (size_w*)(lWasmCtx->mem()->data() + argv_buf_size);

  *lCount = lWasiCtx->mArgs.mElements.size();
  TRACE_WASI_OUT(argc, *lCount);
  *lBufSize = lWasiCtx->mArgs.mBuffer.size();
  TRACE_WASI_OUT(argv_buf_size, *lBufSize);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t clock_res_get(void *ctx, __wasi_clockid_t clock_id, timestampptr_w resolution) {
  TRACE_WASI_CALLS(clock_id, resolution);

  auto *lWasmCtx = static_cast<context*>(ctx);

  if ((sizeof(__wasi_timestamp_t) + resolution) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lOut = (__wasi_timestamp_t*)(lWasmCtx->mem()->data() + resolution);

  clockid_t lHostID;
  switch (clock_id) {
    case __WASI_CLOCK_REALTIME: lHostID = CLOCK_REALTIME; break;
    case __WASI_CLOCK_MONOTONIC: lHostID = CLOCK_MONOTONIC; break;
    case __WASI_CLOCK_PROCESS_CPUTIME_ID: lHostID = CLOCK_PROCESS_CPUTIME_ID; break;
    case __WASI_CLOCK_THREAD_CPUTIME_ID: lHostID = CLOCK_THREAD_CPUTIME_ID; break;
    default:
      TRACE_WASI_RETURN(__WASI_EINVAL);
  }

  timespec lHostOut;
  if (clock_getres(lHostID, &lHostOut))
    TRACE_WASI_RETURN(errno_translate());
  *lOut = lHostOut.tv_nsec + lHostOut.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(resolution, *lOut);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t clock_time_get(void *ctx, __wasi_clockid_t clock_id, __wasi_timestamp_t precision, timestampptr_w time) {
  TRACE_WASI_CALLS(clock_id, precision, time);

  auto *lWasmCtx = static_cast<context*>(ctx);

  if ((sizeof(__wasi_timestamp_t) + time) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lOut = (__wasi_timestamp_t*)(lWasmCtx->mem()->data() + time);

  clockid_t lHostID;
  switch (clock_id) {
    case __WASI_CLOCK_REALTIME: lHostID = CLOCK_REALTIME; break;
    case __WASI_CLOCK_MONOTONIC: lHostID = CLOCK_MONOTONIC; break;
    case __WASI_CLOCK_PROCESS_CPUTIME_ID: lHostID = CLOCK_PROCESS_CPUTIME_ID; break;
    case __WASI_CLOCK_THREAD_CPUTIME_ID: lHostID = CLOCK_THREAD_CPUTIME_ID; break;
    default:
      TRACE_WASI_RETURN(__WASI_EINVAL);
  }

  timespec lHostOut;
  if (clock_gettime(lHostID, &lHostOut))
    TRACE_WASI_RETURN(errno_translate());
  *lOut = lHostOut.tv_nsec + lHostOut.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(time, *lOut);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t environ_get(void *ctx, charptrptr_w environ, charptr_w environ_buf) {
  TRACE_WASI_CALLS(environ, environ_buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((lWasiCtx->mEnv.mElements.size() * sizeof(charptr_w) + environ) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((lWasiCtx->mEnv.mBuffer.size() + environ_buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lEnv = (charptr_w*)(lWasmCtx->mem()->data() + environ);
  auto *lEnvBuf = (char*)(lWasmCtx->mem()->data() + environ_buf);

  memcpy(lEnvBuf, lWasiCtx->mEnv.mBuffer.data(), lWasiCtx->mEnv.mBuffer.size());

  auto lCount = lWasiCtx->mEnv.mElements.size();
  for (size_t i = 0; i < lCount; i++) {
    lEnv[i] = environ_buf + lWasiCtx->mEnv.mElements[i];
  }
  lEnv[lCount] = 0;

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t environ_sizes_get(void *ctx, sizeptr_w environ_count, sizeptr_w environ_buf_size) {
  TRACE_WASI_CALLS(environ_count, environ_buf_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + environ_count) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + environ_buf_size) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lCount = (size_w*)(lWasmCtx->mem()->data() + environ_count);
  auto *lBufSize = (size_w*)(lWasmCtx->mem()->data() + environ_buf_size);

  *lCount = lWasiCtx->mEnv.mElements.size();
  TRACE_WASI_OUT(environ_count, *lCount);
  *lBufSize = lWasiCtx->mEnv.mBuffer.size();
  TRACE_WASI_OUT(environ_buf_size, *lBufSize);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_advise(void *ctx, __wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len, __wasi_advice_t advice) {
  TRACE_WASI_CALLS(fd, offset, len, advice);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_ADVISE) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  int lHostAdvice;
  switch (advice) {
    case __WASI_ADVICE_NORMAL: lHostAdvice = POSIX_FADV_NORMAL; break;
    case __WASI_ADVICE_SEQUENTIAL: lHostAdvice = POSIX_FADV_SEQUENTIAL; break;
    case __WASI_ADVICE_RANDOM: lHostAdvice = POSIX_FADV_RANDOM; break;
    case __WASI_ADVICE_WILLNEED: lHostAdvice = POSIX_FADV_WILLNEED; break;
    case __WASI_ADVICE_DONTNEED: lHostAdvice = POSIX_FADV_DONTNEED; break;
    case __WASI_ADVICE_NOREUSE: lHostAdvice = POSIX_FADV_NOREUSE; break;
    default:
      TRACE_WASI_RETURN(__WASI_EINVAL);
  }

  int lResult = posix_fadvise(lIt->second.mHostFD, offset, len, lHostAdvice);
  if (lResult)
    TRACE_WASI_RETURN(errno_translate(lResult));

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_allocate(void *ctx, __wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len) {
  TRACE_WASI_CALLS(fd, offset, len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_ALLOCATE) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  int lResult = posix_fallocate(lIt->second.mHostFD, offset, len);
  if (lResult)
    TRACE_WASI_RETURN(errno_translate(lResult));

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_close(void *ctx, __wasi_fd_t fd) {
  TRACE_WASI_CALLS(fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if(close(lIt->second.mHostFD))
    TRACE_WASI_RETURN(errno_translate());

  lWasiCtx->mFiles.erase(lIt);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_datasync(void *ctx, __wasi_fd_t fd) {
  TRACE_WASI_CALLS(fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_DATASYNC) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  if(fdatasync(lIt->second.mHostFD))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_fdstat_get(void *ctx, __wasi_fd_t fd, fdstatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_fdstat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lDst = (__wasi_fdstat_t*)(lWasmCtx->mem()->data() + buf);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  int result = fcntl(lIt->second.mHostFD, F_GETFL);
  if (result < 0)
    TRACE_WASI_RETURN(errno_translate());

  __wasi_fdflags_t lOutFlags = 0;
  if (result & O_APPEND)
    lOutFlags |= __WASI_FDFLAG_APPEND;
  if (result & O_DSYNC)
    lOutFlags |= __WASI_FDFLAG_DSYNC;
  if (result & O_NONBLOCK)
    lOutFlags |= __WASI_FDFLAG_NONBLOCK;
  if (result & O_RSYNC)
    lOutFlags |= __WASI_FDFLAG_RSYNC;
  if (result & O_SYNC)
    lOutFlags |= __WASI_FDFLAG_SYNC;

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

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_fdstat_set_flags(void *ctx, __wasi_fd_t fd, __wasi_fdflags_t flags) {
  TRACE_WASI_CALLS(fd, flags);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_FDSTAT_SET_FLAGS) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  __wasi_fdflags_t lOutFlags = 0;
  if (flags & __WASI_FDFLAG_APPEND)
    lOutFlags |= O_APPEND;
  if (flags & __WASI_FDFLAG_DSYNC)
    lOutFlags |= O_DSYNC;
  if (flags & __WASI_FDFLAG_NONBLOCK)
    lOutFlags |= O_NONBLOCK;
  if (flags & __WASI_FDFLAG_RSYNC)
    lOutFlags |= O_RSYNC;
  if (flags & __WASI_FDFLAG_SYNC)
    lOutFlags |= O_SYNC;

  if (fcntl(lIt->second.mHostFD, F_SETFL, lOutFlags))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_fdstat_set_rights(void *ctx, __wasi_fd_t fd, __wasi_rights_t fs_rights_base, __wasi_rights_t fs_rights_inheriting) {
  TRACE_WASI_CALLS(fd, fs_rights_base, fs_rights_inheriting);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  __wasi_fdflags_t lNewFlags = lIt->second.mRights & fs_rights_base;
  __wasi_fdflags_t lNewIFlags = lIt->second.mIRights & fs_rights_inheriting;

  if (lNewFlags != fs_rights_base || lNewIFlags != fs_rights_inheriting)
    TRACE_WASI_RETURN(__WASI_ENOTCAPABLE);

  lIt->second.mRights = lNewFlags;
  lIt->second.mIRights = lNewIFlags;

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_filestat_get(void *ctx, __wasi_fd_t fd, filestatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_filestat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lDst = (__wasi_filestat_t*)(lWasmCtx->mem()->data() + buf);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_FILESTAT_GET) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  struct stat lStat;
  if (fstat(lIt->second.mHostFD, &lStat))
    TRACE_WASI_RETURN(errno_translate());

  if (S_ISREG(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_REGULAR_FILE;
  else if (S_ISDIR(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_DIRECTORY;
  else if (S_ISLNK(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_SYMBOLIC_LINK;
  else if (S_ISSOCK(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_SOCKET_STREAM; // FIXME: Socket type
  else if (S_ISCHR(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_CHARACTER_DEVICE;
  else if (S_ISBLK(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_BLOCK_DEVICE;

  lDst->st_dev = lStat.st_dev;
  lDst->st_ino = lStat.st_ino;
  lDst->st_size = lStat.st_size;
  lDst->st_nlink = lStat.st_nlink;
  lDst->st_atim = lStat.st_atim.tv_nsec + lStat.st_atim.tv_sec * 1'000'000'000;
  lDst->st_ctim = lStat.st_ctim.tv_nsec + lStat.st_ctim.tv_sec * 1'000'000'000;
  lDst->st_mtim = lStat.st_mtim.tv_nsec + lStat.st_mtim.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(buf, *lDst);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_filestat_set_size(void *ctx, __wasi_fd_t fd, __wasi_filesize_t st_size) {
  TRACE_WASI_CALLS(fd, st_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_FILESTAT_SET_SIZE) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  if (ftruncate(lIt->second.mHostFD, st_size))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_filestat_set_times(void *ctx, __wasi_fd_t fd, __wasi_timestamp_t st_atim, __wasi_timestamp_t st_mtim, __wasi_fstflags_t fstflags) {
  TRACE_WASI_CALLS(fd, st_atim, st_mtim, fstflags);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_FILESTAT_SET_TIMES) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  struct stat lStat;
  if (fstat(lIt->second.mHostFD, &lStat))
    TRACE_WASI_RETURN(errno_translate());

  timespec lNow;
  if (fstflags & __WASI_FILESTAT_SET_ATIM_NOW || fstflags & __WASI_FILESTAT_SET_MTIM_NOW) {
    if (clock_gettime(CLOCK_REALTIME, &lNow))
      TRACE_WASI_RETURN(errno_translate());
  }

  timespec lTimes[2];
  lTimes[0] = lStat.st_atim;
  lTimes[1] = lStat.st_mtim;

  if (fstflags & __WASI_FILESTAT_SET_ATIM_NOW)
    lTimes[0] = lNow;
  else if (fstflags & __WASI_FILESTAT_SET_ATIM) {
    lTimes[0].tv_sec = st_atim / 1'000'000'000;
    lTimes[0].tv_nsec = st_atim - (lTimes[0].tv_sec * 1'000'000'000);
  }
  if (fstflags & __WASI_FILESTAT_SET_MTIM_NOW)
    lTimes[1] = lNow;
  else if (fstflags & __WASI_FILESTAT_SET_MTIM) {
    lTimes[1].tv_sec = st_mtim / 1'000'000'000;
    lTimes[1].tv_nsec = st_mtim - (lTimes[1].tv_sec * 1'000'000'000);
  }

  if (futimens(lIt->second.mHostFD, lTimes))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_pread(void *ctx, __wasi_fd_t fd, const iovecptr_w iovs, size_w iovs_len, __wasi_filesize_t offset, sizeptr_w nread) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, offset, nread);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_iovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + nread) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNRead = (size_w*)(lWasmCtx->mem()->data() + nread);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_READ) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_EINVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_EFAULT);
  }

  int read = preadv(lIt->second.mHostFD, lIOVecs, iovs_len, offset);
  if (read < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNRead = read;
  TRACE_WASI_OUT(nread, *lNRead);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_prestat_get(void *ctx, __wasi_fd_t fd, prestatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_prestat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lDst = (__wasi_prestat_t*)(lWasmCtx->mem()->data() + buf);

  if (lWasiCtx->mPreopens.find(fd) == lWasiCtx->mPreopens.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  auto &lPath = lIt->second.mPath.native();
  if (lPath.size() > size_t(std::numeric_limits<int32_t>::max()))
    TRACE_WASI_RETURN(__WASI_EOVERFLOW);

  lDst->pr_type = __WASI_PREOPENTYPE_DIR;
  lDst->u.dir.pr_name_len = lPath.size();
  TRACE_WASI_OUT(buf, *lDst);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_prestat_dir_name(void *ctx, __wasi_fd_t fd, charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if (lWasiCtx->mPreopens.find(fd) == lWasiCtx->mPreopens.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  auto *lDst = (char*)(lWasmCtx->mem()->data() + path);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  auto &lPath = lIt->second.mPath.native();
  if (lPath.size() != size_t(path_len))
    TRACE_WASI_RETURN(__WASI_EINVAL);

  memcpy(lDst, lPath.data(), path_len);
  TRACE_WASI_OUT(path, (std::string_view{lDst, path_len}));
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_pwrite(void *ctx, __wasi_fd_t fd, const ciovecptr_w iovs, size_w iovs_len, __wasi_filesize_t offset, sizeptr_w nwritten) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, offset, nwritten);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_ciovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + nwritten) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNWritten = (size_w*)(lWasmCtx->mem()->data() + nwritten);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_WRITE) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_EINVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_EFAULT);
  }

  int written = pwritev(lIt->second.mHostFD, lIOVecs, iovs_len, offset);
  if (written < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNWritten = written;
  TRACE_WASI_OUT(nwritten, *lNWritten);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_read(void *ctx, __wasi_fd_t fd, const ciovecptr_w iovs, size_w iovs_len, sizeptr_w nread) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, nread);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_ciovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + nread) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNRead = (size_w*)(lWasmCtx->mem()->data() + nread);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_READ) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_EINVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_EFAULT);
  }

  int read = readv(lIt->second.mHostFD, lIOVecs, iovs_len);
  if (read < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNRead = read;
  TRACE_WASI_OUT(nread, *lNRead);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_readdir(void *ctx, __wasi_fd_t fd, voiptr_w buf, size_w buf_len, __wasi_dircookie_t cookie, sizeptr_w bufused) {
  TRACE_WASI_CALLS(fd, buf, buf_len, cookie, bufused);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (buf_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((buf + buf_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + bufused) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lBuf = (char*)(lWasmCtx->mem()->data() + buf);
  auto *lBufUsed = (size_w*)(lWasmCtx->mem()->data() + bufused);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_READDIR) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

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

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_renumber(void *ctx, __wasi_fd_t from, __wasi_fd_t to) {
  TRACE_WASI_CALLS(from, to);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lItFrom = lWasiCtx->mFiles.find(from);
  if (lItFrom == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);
  auto lItTo = lWasiCtx->mFiles.find(to);
  if (lItTo == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  close(lItTo->second.mHostFD);
  lWasiCtx->mFiles[to] = lItFrom->second;
  lWasiCtx->mFiles.erase(lItFrom);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_seek(void *ctx, __wasi_fd_t fd, __wasi_filedelta_t offset, __wasi_whence_t whence, filesizeptr_w newoffset) {
  TRACE_WASI_CALLS(fd, offset, whence, newoffset);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + newoffset) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lOffset = (__wasi_filesize_t*)(lWasmCtx->mem()->data() + newoffset);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_SEEK) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  int lWhence;
  switch (whence) {
    case __WASI_WHENCE_CUR: lWhence = SEEK_CUR; break;
    case __WASI_WHENCE_SET: lWhence = SEEK_SET; break;
    case __WASI_WHENCE_END: lWhence = SEEK_END; break;
    default: TRACE_WASI_RETURN(__WASI_EINVAL);
  }

  off_t lResult = lseek(lIt->second.mHostFD, offset, lWhence);
  if (lResult < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lOffset = lResult;
  TRACE_WASI_OUT(newoffset, *lOffset);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_sync(void *ctx, __wasi_fd_t fd) {
  TRACE_WASI_CALLS(fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_SYNC) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  if(fsync(lIt->second.mHostFD))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_tell(void *ctx, __wasi_fd_t fd, filesizeptr_w newoffset) {
  TRACE_WASI_CALLS(fd, newoffset);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + newoffset) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lOffset = (__wasi_filesize_t*)(lWasmCtx->mem()->data() + newoffset);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_TELL) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  off_t lResult = lseek(lIt->second.mHostFD, 0, SEEK_CUR);
  if (lResult < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lOffset = lResult;
  TRACE_WASI_OUT(newoffset, *lOffset);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t fd_write(void *ctx, __wasi_fd_t fd, const ciovecptr_w iovs, size_w iovs_len, sizeptr_w nwritten) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, nwritten);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(__wasi_ciovec_t) * iovs_len + iovs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + nwritten) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lIOVecsIn = (__wasi_iovec_t*)(lWasmCtx->mem()->data() + iovs);
  auto *lNWritten = (size_w*)(lWasmCtx->mem()->data() + nwritten);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_WRITE) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  size_t lTotalSize = 0;
  iovec lIOVecs[iovs_len];
  for (size_w i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = {lDst, (size_t)lIOVecsIn[i].buf_len};
    lTotalSize += lIOVecsIn[i].buf_len;
    if (lTotalSize > std::numeric_limits<size_w>::max())
      TRACE_WASI_RETURN(__WASI_EINVAL);
    if ((lIOVecsIn[i].buf + lIOVecsIn[i].buf_len) > lWasmCtx->mem()->byte_size())
      TRACE_WASI_RETURN(__WASI_EFAULT);
  }

  int written = writev(lIt->second.mHostFD, lIOVecs, iovs_len);
  if (written < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lNWritten = written;
  TRACE_WASI_OUT(nwritten, *lNWritten);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_create_directory(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if (mkdirat(lIt->second.mHostFD, lPath, 0755))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_filestat_get(void *ctx, __wasi_fd_t fd, __wasi_lookupflags_t flags, const charptr_w path, size_w path_len, filestatptr_w buf) {
  TRACE_WASI_CALLS(fd, flags, path, path_len, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(__wasi_filestat_t) + buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);
  auto *lDst = (__wasi_filestat_t*)(lWasmCtx->mem()->data() + buf);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_READDIR) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  int lSearchFlags = 0;
  if ((flags & __WASI_LOOKUP_SYMLINK_FOLLOW) == 0)
    lSearchFlags |= AT_SYMLINK_NOFOLLOW;

  struct stat lStat;
  if (fstatat(lIt->second.mHostFD, lPath, &lStat, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());

  if (S_ISREG(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_REGULAR_FILE;
  else if (S_ISDIR(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_DIRECTORY;
  else if (S_ISLNK(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_SYMBOLIC_LINK;
  else if (S_ISSOCK(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_SOCKET_STREAM; // FIXME: Socket type
  else if (S_ISCHR(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_CHARACTER_DEVICE;
  else if (S_ISBLK(lStat.st_mode)) lDst->st_filetype = __WASI_FILETYPE_BLOCK_DEVICE;

  lDst->st_dev = lStat.st_dev;
  lDst->st_ino = lStat.st_ino;
  lDst->st_size = lStat.st_size;
  lDst->st_nlink = lStat.st_nlink;
  lDst->st_atim = lStat.st_atim.tv_nsec + lStat.st_atim.tv_sec * 1'000'000'000;
  lDst->st_ctim = lStat.st_ctim.tv_nsec + lStat.st_ctim.tv_sec * 1'000'000'000;
  lDst->st_mtim = lStat.st_mtim.tv_nsec + lStat.st_mtim.tv_sec * 1'000'000'000;
  TRACE_WASI_OUT(buf, *lDst);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_filestat_set_times(void *ctx, __wasi_fd_t fd, __wasi_lookupflags_t flags, const charptr_w path, size_w path_len, __wasi_timestamp_t st_atim, __wasi_timestamp_t st_mtim, __wasi_fstflags_t fstflags) {
  TRACE_WASI_CALLS(fd, flags, path, path_len, st_atim, st_mtim, fstflags);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if ((lIt->second.mRights & __WASI_RIGHT_FD_FILESTAT_SET_TIMES) == 0)
    TRACE_WASI_RETURN(__WASI_EACCES);

  int lSearchFlags = 0;
  if ((flags & __WASI_LOOKUP_SYMLINK_FOLLOW) == 0)
    lSearchFlags |= AT_SYMLINK_NOFOLLOW;

  struct stat lStat;
  if (fstatat(lIt->second.mHostFD, lPath, &lStat, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());

  timespec lNow;
  if (fstflags & __WASI_FILESTAT_SET_ATIM_NOW || fstflags & __WASI_FILESTAT_SET_MTIM_NOW) {
    if (clock_gettime(CLOCK_REALTIME, &lNow))
      TRACE_WASI_RETURN(errno_translate());
  }

  timespec lTimes[2];
  lTimes[0] = lStat.st_atim;
  lTimes[1] = lStat.st_mtim;

  if (fstflags & __WASI_FILESTAT_SET_ATIM_NOW)
    lTimes[0] = lNow;
  else if (fstflags & __WASI_FILESTAT_SET_ATIM) {
    lTimes[0].tv_sec = st_atim / 1'000'000'000;
    lTimes[0].tv_nsec = st_atim - (lTimes[0].tv_sec * 1'000'000'000);
  }
  if (fstflags & __WASI_FILESTAT_SET_MTIM_NOW)
    lTimes[1] = lNow;
  else if (fstflags & __WASI_FILESTAT_SET_MTIM) {
    lTimes[1].tv_sec = st_mtim / 1'000'000'000;
    lTimes[1].tv_nsec = st_mtim - (lTimes[1].tv_sec * 1'000'000'000);
  }

  if (utimensat(lIt->second.mHostFD, lPath, lTimes, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_link(void *ctx, __wasi_fd_t old_fd, __wasi_lookupflags_t old_flags, const charptr_w old_path, size_w old_path_len, __wasi_fd_t new_fd, const charptr_w new_path, size_w new_path_len) {
  TRACE_WASI_CALLS(old_fd, old_flags, old_path, old_path_len, new_fd, new_path, new_path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (old_path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if (new_path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((old_path + old_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((new_path + new_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lOldPath = (char*)(lWasmCtx->mem()->data() + old_path);
  auto *lNewPath = (char*)(lWasmCtx->mem()->data() + new_path);

  TRACE_WASI_IN_STR(lOldPath, old_path_len);
  TRACE_WASI_IN_STR(lOldPath, new_path_len);

  auto lOld = lWasiCtx->mFiles.find(old_fd);
  if (lOld == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);
  auto lNew = lWasiCtx->mFiles.find(new_fd);
  if (lNew == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  int lSearchFlags = 0;
  if (old_flags & __WASI_LOOKUP_SYMLINK_FOLLOW)
    lSearchFlags |= AT_SYMLINK_FOLLOW;

  if (linkat(lOld->second.mHostFD, lOldPath, lNew->second.mHostFD, lNewPath, lSearchFlags))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_open(void *ctx, __wasi_fd_t dirfd, __wasi_lookupflags_t dirflags, const charptr_w path, size_w path_len, __wasi_oflags_t oflags, __wasi_rights_t fs_rights_base, __wasi_rights_t fs_rights_inheriting, __wasi_fdflags_t fs_flags, fdptr_w fd) {
  TRACE_WASI_CALLS(dirfd, dirflags, path, path_len, oflags, fs_rights_base, fs_rights_inheriting, fs_flags, fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(__wasi_fd_t) + fd) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);
  auto *lFD = (__wasi_fd_t *)(lWasmCtx->mem()->data() + fd);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lDir = lWasiCtx->mFiles.find(dirfd);
  if (lDir == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  int lOpenFlags = 0;
  if (oflags & __WASI_O_CREAT)
    lOpenFlags |= O_CREAT;
  if (oflags & __WASI_O_DIRECTORY)
    lOpenFlags |= O_DIRECTORY;
  if (oflags & __WASI_O_EXCL)
    lOpenFlags |= O_EXCL;
  if (oflags & __WASI_O_TRUNC)
    lOpenFlags |= O_TRUNC;
  if ((dirflags & __WASI_LOOKUP_SYMLINK_FOLLOW) == 0)
    lOpenFlags |= O_NOFOLLOW;
  if (fs_flags & __WASI_FDFLAG_APPEND)
    lOpenFlags |= O_APPEND;
  if (fs_flags & __WASI_FDFLAG_DSYNC)
    lOpenFlags |= O_DSYNC;
  if (fs_flags & __WASI_FDFLAG_NONBLOCK)
    lOpenFlags |= O_NONBLOCK;
  if (fs_flags & __WASI_FDFLAG_RSYNC)
    lOpenFlags |= O_RSYNC;
  if (fs_flags & __WASI_FDFLAG_SYNC)
    lOpenFlags |= O_SYNC;

  constexpr __wasi_rights_t lReadMask = __WASI_RIGHT_FD_READ | __WASI_RIGHT_FD_READDIR;
  constexpr __wasi_rights_t lWriteMask = __WASI_RIGHT_FD_WRITE | __WASI_RIGHT_FD_ALLOCATE | __WASI_RIGHT_FD_FILESTAT_SET_SIZE;
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

  lWasiCtx->mFiles.emplace(lNewFD, file{lResult, lDir->second.mPath / lPath, fs_rights_base, fs_rights_inheriting});

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_readlink(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len, charptr_w buf, size_w buf_len, sizeptr_w bufused) {
  TRACE_WASI_CALLS(fd, path, path_len, buf, buf_len, bufused);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if (buf_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((buf + buf_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((sizeof(size_w) + bufused) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);
  auto *lBuf = (char*)(lWasmCtx->mem()->data() + buf);
  auto *lBufUsed = (size_w*)(lWasmCtx->mem()->data() + bufused);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  int lResult = readlinkat(lIt->second.mHostFD, lPath, lBuf, buf_len);
  if (lResult < 0)
    TRACE_WASI_RETURN(errno_translate());
  *lBufUsed = lResult;
  TRACE_WASI_OUT(bufused, *lBufUsed);
  TRACE_WASI_OUT(buf, lBuf);
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_remove_directory(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if (unlinkat(lIt->second.mHostFD, lPath, AT_REMOVEDIR))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_rename(void *ctx, __wasi_fd_t old_fd, const charptr_w old_path, size_w old_path_len, __wasi_fd_t new_fd, const charptr_w new_path, size_w new_path_len) {
  TRACE_WASI_CALLS(old_fd, old_path, old_path_len, new_fd, new_path, new_path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (old_path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if (new_path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((old_path + old_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((new_path + new_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lOldPath = (char*)(lWasmCtx->mem()->data() + old_path);
  auto *lNewPath = (char*)(lWasmCtx->mem()->data() + new_path);

  TRACE_WASI_IN_STR(lOldPath, old_path_len);
  TRACE_WASI_IN_STR(lNewPath, new_path_len);

  auto lOld = lWasiCtx->mFiles.find(old_fd);
  if (lOld == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);
  auto lNew = lWasiCtx->mFiles.find(new_fd);
  if (lNew == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if (renameat(lOld->second.mHostFD, lOldPath, lNew->second.mHostFD, lNewPath))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_symlink(void *ctx, const charptr_w old_path, size_w old_path_len, __wasi_fd_t fd, const charptr_w new_path, size_w new_path_len) {
  TRACE_WASI_CALLS(old_path, old_path_len, fd, new_path, new_path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (new_path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if (old_path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((old_path + old_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);
  if ((new_path + new_path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lOldPath = (char*)(lWasmCtx->mem()->data() + old_path);
  auto *lNewPath = (char*)(lWasmCtx->mem()->data() + new_path);

  TRACE_WASI_IN_STR(lOldPath, old_path_len);
  TRACE_WASI_IN_STR(lNewPath, new_path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if (symlinkat(lOldPath, lIt->second.mHostFD, lNewPath))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t path_unlink_file(void *ctx, __wasi_fd_t fd, const charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if (path_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((path + path_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lPath = (char*)(lWasmCtx->mem()->data() + path);

  TRACE_WASI_IN_STR(lPath, path_len);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if (unlinkat(lIt->second.mHostFD, lPath, 0))
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t poll_oneoff(void *ctx, const subscriptionptr_w in, eventptr_w out, size_w nsubscriptions, sizeptr_w nevents) {
  TRACE_WASI_CALLS(in, out, nsubscriptions, nevents);
  TRACE_WASI_RETURN(__WASI_ENOTCAPABLE);
}

void proc_exit(void *ctx, __wasi_exitcode_t rval) {
  TRACE_WASI_CALLS(rval);
  throw proc_exit_exception(rval);
}

__wasi_errno_t proc_raise(void *ctx, __wasi_signal_t sig) {
  TRACE_WASI_CALLS(sig);

  int lCode;
  switch (sig) {
    case __WASI_SIGHUP : lCode = SIGHUP; break;
    case __WASI_SIGINT : lCode = SIGINT; break;
    case __WASI_SIGQUIT : lCode = SIGQUIT; break;
    case __WASI_SIGILL : lCode = SIGILL; break;
    case __WASI_SIGTRAP : lCode = SIGTRAP; break;
    case __WASI_SIGABRT : lCode = SIGABRT; break;
    case __WASI_SIGBUS : lCode = SIGBUS; break;
    case __WASI_SIGFPE : lCode = SIGFPE; break;
    case __WASI_SIGKILL : lCode = SIGKILL; break;
    case __WASI_SIGUSR1 : lCode = SIGUSR1; break;
    case __WASI_SIGSEGV : lCode = SIGSEGV; break;
    case __WASI_SIGUSR2 : lCode = SIGUSR2; break;
    case __WASI_SIGPIPE : lCode = SIGPIPE; break;
    case __WASI_SIGALRM : lCode = SIGALRM; break;
    case __WASI_SIGTERM : lCode = SIGTERM; break;
    case __WASI_SIGCHLD : lCode = SIGCHLD; break;
    case __WASI_SIGCONT : lCode = SIGCONT; break;
    case __WASI_SIGSTOP : lCode = SIGSTOP; break;
    case __WASI_SIGTSTP : lCode = SIGTSTP; break;
    case __WASI_SIGTTIN : lCode = SIGTTIN; break;
    case __WASI_SIGTTOU : lCode = SIGTTOU; break;
    case __WASI_SIGURG : lCode = SIGURG; break;
    case __WASI_SIGXCPU : lCode = SIGXCPU; break;
    case __WASI_SIGXFSZ : lCode = SIGXFSZ; break;
    case __WASI_SIGVTALRM : lCode = SIGVTALRM; break;
    case __WASI_SIGPROF : lCode = SIGPROF; break;
    case __WASI_SIGWINCH : lCode = SIGWINCH; break;
    case __WASI_SIGPOLL : lCode = SIGPOLL; break;
    case __WASI_SIGPWR : lCode = SIGPWR; break;
    case __WASI_SIGSYS : lCode = SIGSYS; break;
    default: TRACE_WASI_RETURN(__WASI_EINVAL);
  }

  if (!raise(lCode))
    TRACE_WASI_RETURN(__WASI_ENOTCAPABLE);
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t random_get(void *ctx, voiptr_w buf, size_w buf_len) {
  TRACE_WASI_CALLS(buf, buf_len);

  auto *lWasmCtx = static_cast<context*>(ctx);

  if (buf_len == 0)
    TRACE_WASI_RETURN(__WASI_EINVAL);
  if ((buf + buf_len) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_EFAULT);

  auto *lBuf = (char*)(lWasmCtx->mem()->data() + buf);

  auto lWritten = getrandom(lBuf, buf_len, 0);
  if (lWritten < 0)
    TRACE_WASI_RETURN(errno_translate());
  if (lWritten != buf_len)
    TRACE_WASI_RETURN(__WASI_EAGAIN);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t sched_yield(void *ctx) {
  TRACE_WASI_CALLS("no inputs");
  if (::sched_yield())
    TRACE_WASI_RETURN(errno_translate());
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

__wasi_errno_t sock_recv(void *ctx, __wasi_fd_t sock, const iovecptr_w ri_data, size_w ri_data_len, __wasi_riflags_t ri_flags, sizeptr_w ro_datalen, roflagsptr_w ro_flags) {
  TRACE_WASI_CALLS(sock, ri_data, ri_data_len, ri_flags, ro_datalen, ro_flags);
  TRACE_WASI_RETURN(__WASI_ENOTCAPABLE);
}

__wasi_errno_t sock_send(void *ctx, __wasi_fd_t sock, const ciovecptr_w si_data, size_w si_data_len, __wasi_siflags_t si_flags, sizeptr_w so_datalen) {
  TRACE_WASI_CALLS(sock, si_data, si_data_len, si_flags, so_datalen);
  TRACE_WASI_RETURN(__WASI_ENOTCAPABLE);
}

__wasi_errno_t sock_shutdown(void *ctx, __wasi_fd_t sock, __wasi_sdflags_t how) {
  TRACE_WASI_CALLS(sock, how);
  TRACE_WASI_RETURN(__WASI_ENOTCAPABLE);
}

resolver_t make_unstable_resolver() {
  return [](context &pContext, std::string_view pFieldName) {
    const static std::unordered_map<std::string_view, resolve_result_t> sEnvMappings = {
      {"args_get", expose_func(&args_get)},
      {"args_sizes_get", expose_func(&args_sizes_get)},
      {"clock_res_get", expose_func(&clock_res_get)},
      {"clock_time_get", expose_func(&clock_time_get)},
      {"environ_get", expose_func(&environ_get)},
      {"environ_sizes_get", expose_func(&environ_sizes_get)},
      {"fd_advise", expose_func(&fd_advise)},
      {"fd_allocate", expose_func(&fd_allocate)},
      {"fd_close", expose_func(&fd_close)},
      {"fd_datasync", expose_func(&fd_datasync)},
      {"fd_fdstat_get", expose_func(&fd_fdstat_get)},
      {"fd_fdstat_set_flags", expose_func(&fd_fdstat_set_flags)},
      {"fd_fdstat_set_rights", expose_func(&fd_fdstat_set_rights)},
      {"fd_filestat_get", expose_func(&fd_filestat_get)},
      {"fd_filestat_set_size", expose_func(&fd_filestat_set_size)},
      {"fd_filestat_set_times", expose_func(&fd_filestat_set_times)},
      {"fd_pread", expose_func(&fd_pread)},
      {"fd_prestat_get", expose_func(&fd_prestat_get)},
      {"fd_prestat_dir_name", expose_func(&fd_prestat_dir_name)},
      {"fd_pwrite", expose_func(&fd_pwrite)},
      {"fd_read", expose_func(&fd_read)},
      {"fd_readdir", expose_func(&fd_readdir)},
      {"fd_renumber", expose_func(&fd_renumber)},
      {"fd_seek", expose_func(&fd_seek)},
      {"fd_sync", expose_func(&fd_sync)},
      {"fd_tell", expose_func(&fd_tell)},
      {"fd_write", expose_func(&fd_write)},
      {"path_create_directory", expose_func(&path_create_directory)},
      {"path_filestat_get", expose_func(&path_filestat_get)},
      {"path_filestat_set_times", expose_func(&path_filestat_set_times)},
      {"path_link", expose_func(&path_link)},
      {"path_open", expose_func(&path_open)},
      {"path_readlink", expose_func(&path_readlink)},
      {"path_remove_directory", expose_func(&path_remove_directory)},
      {"path_rename", expose_func(&path_rename)},
      {"path_symlink", expose_func(&path_symlink)},
      {"path_unlink_file", expose_func(&path_unlink_file)},
      {"poll_oneoff", expose_func(&poll_oneoff)},
      {"proc_exit", expose_func(&proc_exit)},
      {"proc_raise", expose_func(&proc_raise)},
      {"random_get", expose_func(&random_get)},
      {"sched_yield", expose_func(&sched_yield)},
      {"sock_recv", expose_func(&sock_recv)},
      {"sock_send", expose_func(&sock_send)},
      {"sock_shutdown", expose_func(&sock_shutdown)},
    };

    auto lFound = sEnvMappings.find(pFieldName);
    if (lFound == sEnvMappings.end())
      throw std::runtime_error(std::string("unknown wasi unstable import: ") + std::string(pFieldName));
    return lFound->second;
  };
}

}  // namespace wembed::wasi
