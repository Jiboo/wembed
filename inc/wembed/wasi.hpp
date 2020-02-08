#pragma once

#include <filesystem>

#ifdef WEMBED_TRACE_WASI_CALLS
#include <iostream>
#include <string_view>
#endif

#include "wembed/context.hpp"
#include "wembed/wasi_decls.hpp"

namespace wembed::wasi {

#ifdef WEMBED_TRACE_WASI_CALLS
  std::ostream & operator<<(std::ostream &os, const __wasi_fdstat_t &pStat) {
    return os << "fdstat_t{" << (int)pStat.fs_filetype << ", " << pStat.fs_flags
       << ", " << pStat.fs_rights_base << ", " << pStat.fs_rights_inheriting
       << "}";
  }
  std::ostream & operator<<(std::ostream &os, const __wasi_filestat_t &pStat) {
    return os << "filestat_t{" << pStat.dev << ", " << pStat.ino
       << ", " << (int)pStat.filetype << ", " << pStat.nlink
       << ", " << pStat.size << ", " << pStat.atim
       << ", " << pStat.mtim << ", " << pStat.ctim
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
      case __WASI_ERRNO_SUCCESS: return "SUCCESS"sv;
      case __WASI_ERRNO_2BIG: return "E2BIG"sv;
      case __WASI_ERRNO_ACCES: return "EACCES"sv;
      case __WASI_ERRNO_ADDRINUSE: return "EADDRINUSE"sv;
      case __WASI_ERRNO_ADDRNOTAVAIL: return "EADDRNOTAVAIL"sv;
      case __WASI_ERRNO_AFNOSUPPORT: return "EAFNOSUPPORT"sv;
      case __WASI_ERRNO_AGAIN: return "EAGAIN"sv;
      case __WASI_ERRNO_ALREADY: return "EALREADY"sv;
      case __WASI_ERRNO_BADF: return "EBADF"sv;
      case __WASI_ERRNO_BADMSG: return "EBADMSG"sv;
      case __WASI_ERRNO_BUSY: return "EBUSY"sv;
      case __WASI_ERRNO_CANCELED: return "ECANCELED"sv;
      case __WASI_ERRNO_CHILD: return "ECHILD"sv;
      case __WASI_ERRNO_CONNABORTED: return "ECONNABORTED"sv;
      case __WASI_ERRNO_CONNREFUSED: return "ECONNREFUSED"sv;
      case __WASI_ERRNO_CONNRESET: return "ECONNRESET"sv;
      case __WASI_ERRNO_DEADLK: return "EDEADLK"sv;
      case __WASI_ERRNO_DESTADDRREQ: return "EDESTADDRREQ"sv;
      case __WASI_ERRNO_DOM: return "EDOM"sv;
      case __WASI_ERRNO_DQUOT: return "EDQUOT"sv;
      case __WASI_ERRNO_EXIST: return "EEXIST"sv;
      case __WASI_ERRNO_FAULT: return "EFAULT"sv;
      case __WASI_ERRNO_FBIG: return "EFBIG"sv;
      case __WASI_ERRNO_HOSTUNREACH: return "EHOSTUNREACH"sv;
      case __WASI_ERRNO_IDRM: return "EIDRM"sv;
      case __WASI_ERRNO_ILSEQ: return "EILSEQ"sv;
      case __WASI_ERRNO_INPROGRESS: return "EINPROGRESS"sv;
      case __WASI_ERRNO_INTR: return "EINTR"sv;
      case __WASI_ERRNO_INVAL: return "EINVAL"sv;
      case __WASI_ERRNO_IO: return "EIO"sv;
      case __WASI_ERRNO_ISCONN: return "EISCONN"sv;
      case __WASI_ERRNO_ISDIR: return "EISDIR"sv;
      case __WASI_ERRNO_LOOP: return "ELOOP"sv;
      case __WASI_ERRNO_MFILE: return "EMFILE"sv;
      case __WASI_ERRNO_MLINK: return "EMLINK"sv;
      case __WASI_ERRNO_MSGSIZE: return "EMSGSIZE"sv;
      case __WASI_ERRNO_MULTIHOP: return "EMULTIHOP"sv;
      case __WASI_ERRNO_NAMETOOLONG: return "ENAMETOOLONG"sv;
      case __WASI_ERRNO_NETDOWN: return "ENETDOWN"sv;
      case __WASI_ERRNO_NETRESET: return "ENETRESET"sv;
      case __WASI_ERRNO_NETUNREACH: return "ENETUNREACH"sv;
      case __WASI_ERRNO_NFILE: return "ENFILE"sv;
      case __WASI_ERRNO_NOBUFS: return "ENOBUFS"sv;
      case __WASI_ERRNO_NODEV: return "ENODEV"sv;
      case __WASI_ERRNO_NOENT: return "ENOENT"sv;
      case __WASI_ERRNO_NOEXEC: return "ENOEXEC"sv;
      case __WASI_ERRNO_NOLCK: return "ENOLCK"sv;
      case __WASI_ERRNO_NOLINK: return "ENOLINK"sv;
      case __WASI_ERRNO_NOMEM: return "ENOMEM"sv;
      case __WASI_ERRNO_NOMSG: return "ENOMSG"sv;
      case __WASI_ERRNO_NOPROTOOPT: return "ENOPROTOOPT"sv;
      case __WASI_ERRNO_NOSPC: return "ENOSPC"sv;
      case __WASI_ERRNO_NOSYS: return "ENOSYS"sv;
      case __WASI_ERRNO_NOTCONN: return "ENOTCONN"sv;
      case __WASI_ERRNO_NOTDIR: return "ENOTDIR"sv;
      case __WASI_ERRNO_NOTEMPTY: return "ENOTEMPTY"sv;
      case __WASI_ERRNO_NOTRECOVERABLE: return "ENOTRECOVERABLE"sv;
      case __WASI_ERRNO_NOTSOCK: return "ENOTSOCK"sv;
      case __WASI_ERRNO_NOTSUP: return "ENOTSUP"sv;
      case __WASI_ERRNO_NOTTY: return "ENOTTY"sv;
      case __WASI_ERRNO_NXIO: return "ENXIO"sv;
      case __WASI_ERRNO_OVERFLOW: return "EOVERFLOW"sv;
      case __WASI_ERRNO_OWNERDEAD: return "EOWNERDEAD"sv;
      case __WASI_ERRNO_PERM: return "EPERM"sv;
      case __WASI_ERRNO_PIPE: return "EPIPE"sv;
      case __WASI_ERRNO_PROTO: return "EPROTO"sv;
      case __WASI_ERRNO_PROTONOSUPPORT: return "EPROTONOSUPPORT"sv;
      case __WASI_ERRNO_PROTOTYPE: return "EPROTOTYPE"sv;
      case __WASI_ERRNO_RANGE: return "ERANGE"sv;
      case __WASI_ERRNO_ROFS: return "EROFS"sv;
      case __WASI_ERRNO_SPIPE: return "ESPIPE"sv;
      case __WASI_ERRNO_SRCH: return "ESRCH"sv;
      case __WASI_ERRNO_STALE: return "ESTALE"sv;
      case __WASI_ERRNO_TIMEDOUT: return "ETIMEDOUT"sv;
      case __WASI_ERRNO_TXTBSY: return "ETXTBSY"sv;
      case __WASI_ERRNO_XDEV: return "EXDEV"sv;
      case __WASI_ERRNO_NOTCAPABLE: return "ENOTCAPABLE"sv;
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

  class proc_exit_exception : public vm_runtime_exception {
  public:
    uint32_t mCode;
    explicit proc_exit_exception(uint32_t pCode)
      : vm_runtime_exception(std::string("proc_exit: ") + std::to_string(pCode)), mCode(pCode) {}
  };

  __wasi_errno_t errno_translate(int error = errno);

  __wasi_errno_t args_get(void*, charptrptr_w, charptr_w);
  __wasi_errno_t args_sizes_get(void*, sizeptr_w, sizeptr_w);
  __wasi_errno_t clock_res_get(void*, __wasi_clockid_t, timestampptr_w);
  __wasi_errno_t clock_time_get(void*, __wasi_clockid_t, __wasi_timestamp_t, timestampptr_w);
  __wasi_errno_t environ_get(void*, charptrptr_w, charptr_w);
  __wasi_errno_t environ_sizes_get(void*, sizeptr_w, sizeptr_w);
  __wasi_errno_t fd_advise(void*, __wasi_fd_t, __wasi_filesize_t, __wasi_filesize_t, __wasi_advice_t);
  __wasi_errno_t fd_allocate(void*, __wasi_fd_t, __wasi_filesize_t, __wasi_filesize_t);
  __wasi_errno_t fd_close(void*, __wasi_fd_t);
  __wasi_errno_t fd_datasync(void*, __wasi_fd_t);
  __wasi_errno_t fd_fdstat_get(void*, __wasi_fd_t, fdstatptr_w);
  __wasi_errno_t fd_fdstat_set_flags(void*, __wasi_fd_t, __wasi_fdflags_t);
  __wasi_errno_t fd_fdstat_set_rights(void*, __wasi_fd_t, __wasi_rights_t, __wasi_rights_t);
  __wasi_errno_t fd_filestat_get(void*, __wasi_fd_t, filestatptr_w);
  __wasi_errno_t fd_filestat_set_size(void*, __wasi_fd_t, __wasi_filesize_t);
  __wasi_errno_t fd_filestat_set_times(void*, __wasi_fd_t, __wasi_timestamp_t, __wasi_timestamp_t, __wasi_fstflags_t);
  __wasi_errno_t fd_pread(void*, __wasi_fd_t, iovecptr_w, size_w, __wasi_filesize_t, sizeptr_w);
  __wasi_errno_t fd_prestat_get(void*, __wasi_fd_t, prestatptr_w);
  __wasi_errno_t fd_prestat_dir_name(void*, __wasi_fd_t, charptr_w, size_w);
  __wasi_errno_t fd_pwrite(void*, __wasi_fd_t, ciovecptr_w, size_w, __wasi_filesize_t, sizeptr_w);
  __wasi_errno_t fd_read(void*, __wasi_fd_t, ciovecptr_w, size_w, sizeptr_w);
  __wasi_errno_t fd_readdir(void*, __wasi_fd_t, voiptr_w, size_w, __wasi_dircookie_t, sizeptr_w);
  __wasi_errno_t fd_renumber(void*, __wasi_fd_t, __wasi_fd_t);
  __wasi_errno_t fd_seek(void*, __wasi_fd_t, __wasi_filedelta_t, __wasi_whence_t, filesizeptr_w);
  __wasi_errno_t fd_sync(void*, __wasi_fd_t);
  __wasi_errno_t fd_tell(void*, __wasi_fd_t, filesizeptr_w);
  __wasi_errno_t fd_write(void*, __wasi_fd_t, ciovecptr_w, size_w, sizeptr_w);
  __wasi_errno_t path_create_directory(void*, __wasi_fd_t, charptr_w, size_w);
  __wasi_errno_t path_filestat_get(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, filestatptr_w);
  __wasi_errno_t path_filestat_set_times(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, __wasi_timestamp_t, __wasi_timestamp_t, __wasi_fstflags_t);
  __wasi_errno_t path_link(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, __wasi_fd_t, charptr_w, size_w);
  __wasi_errno_t path_open(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, __wasi_oflags_t, __wasi_rights_t, __wasi_rights_t, __wasi_fdflags_t, fdptr_w);
  __wasi_errno_t path_readlink(void*, __wasi_fd_t, charptr_w, size_w, charptr_w, size_w, sizeptr_w);
  __wasi_errno_t path_remove_directory(void*, __wasi_fd_t, charptr_w, size_w);
  __wasi_errno_t path_rename(void*, __wasi_fd_t, charptr_w, size_w, __wasi_fd_t, charptr_w, size_w);
  __wasi_errno_t path_symlink(void*, charptr_w, size_w, __wasi_fd_t, charptr_w, size_w);
  __wasi_errno_t path_unlink_file(void*, __wasi_fd_t, charptr_w, size_w);
  __wasi_errno_t poll_oneoff(void*, subscriptionptr_w, eventptr_w, size_w, sizeptr_w);
  void           proc_exit(void*, __wasi_exitcode_t);
  __wasi_errno_t proc_raise(void*, __wasi_signal_t);
  __wasi_errno_t random_get(void*, voiptr_w, size_w);
  __wasi_errno_t sched_yield(void*);
  __wasi_errno_t sock_recv(void*, __wasi_fd_t, iovecptr_w, size_w, __wasi_riflags_t, sizeptr_w, roflagsptr_w);
  __wasi_errno_t sock_send(void*, __wasi_fd_t, ciovecptr_w, size_w, __wasi_siflags_t, sizeptr_w);
  __wasi_errno_t sock_shutdown(void*, __wasi_fd_t, __wasi_sdflags_t);

  class wasi_context {
  protected:
    struct file {
      int mHostFD;
      std::filesystem::path mPath;
      __wasi_rights_t mRights = 0, mIRights = 0;
    };

  public:
    explicit wasi_context(std::filesystem::path pRoot);

    void add_arg(std::string_view pArg);
    void add_args(int pArgc, const char **pArgv);

    void add_env(std::string_view pEnv);
    void add_env_host();

    void add_preopen(__wasi_fd_t pFD, file pFile);
    void add_preopen_host();

  protected:
    struct strings_buff {
      std::vector<uint8_t> mBuffer;
      std::vector<size_t> mElements;
    };
    strings_buff mArgs, mEnv;

    std::unordered_map<__wasi_fd_t, file> mFiles;
    std::filesystem::path mRoot;

    std::unordered_set<__wasi_fd_t> mPreopens;

  protected:
    friend __wasi_errno_t args_get(void*, charptrptr_w, charptr_w);
    friend __wasi_errno_t args_sizes_get(void*, sizeptr_w, sizeptr_w);
    friend __wasi_errno_t clock_res_get(void*, __wasi_clockid_t, timestampptr_w);
    friend __wasi_errno_t clock_time_get(void*, __wasi_clockid_t, __wasi_timestamp_t, timestampptr_w);
    friend __wasi_errno_t environ_get(void*, charptrptr_w, charptr_w);
    friend __wasi_errno_t environ_sizes_get(void*, sizeptr_w, sizeptr_w);
    friend __wasi_errno_t fd_advise(void*, __wasi_fd_t, __wasi_filesize_t, __wasi_filesize_t, __wasi_advice_t);
    friend __wasi_errno_t fd_allocate(void*, __wasi_fd_t, __wasi_filesize_t, __wasi_filesize_t);
    friend __wasi_errno_t fd_close(void*, __wasi_fd_t);
    friend __wasi_errno_t fd_datasync(void*, __wasi_fd_t);
    friend __wasi_errno_t fd_fdstat_get(void*, __wasi_fd_t, fdstatptr_w);
    friend __wasi_errno_t fd_fdstat_set_flags(void*, __wasi_fd_t, __wasi_fdflags_t);
    friend __wasi_errno_t fd_fdstat_set_rights(void*, __wasi_fd_t, __wasi_rights_t, __wasi_rights_t);
    friend __wasi_errno_t fd_filestat_get(void*, __wasi_fd_t, filestatptr_w);
    friend __wasi_errno_t fd_filestat_set_size(void*, __wasi_fd_t, __wasi_filesize_t);
    friend __wasi_errno_t fd_filestat_set_times(void*, __wasi_fd_t, __wasi_timestamp_t, __wasi_timestamp_t, __wasi_fstflags_t);
    friend __wasi_errno_t fd_pread(void*, __wasi_fd_t, iovecptr_w, size_w, __wasi_filesize_t, sizeptr_w);
    friend __wasi_errno_t fd_prestat_get(void*, __wasi_fd_t, prestatptr_w);
    friend __wasi_errno_t fd_prestat_dir_name(void*, __wasi_fd_t, charptr_w, size_w);
    friend __wasi_errno_t fd_pwrite(void*, __wasi_fd_t, ciovecptr_w, size_w, __wasi_filesize_t, sizeptr_w);
    friend __wasi_errno_t fd_read(void*, __wasi_fd_t, ciovecptr_w, size_w, sizeptr_w);
    friend __wasi_errno_t fd_readdir(void*, __wasi_fd_t, voiptr_w, size_w, __wasi_dircookie_t, sizeptr_w);
    friend __wasi_errno_t fd_renumber(void*, __wasi_fd_t, __wasi_fd_t);
    friend __wasi_errno_t fd_seek(void*, __wasi_fd_t, __wasi_filedelta_t, __wasi_whence_t, filesizeptr_w);
    friend __wasi_errno_t fd_sync(void*, __wasi_fd_t);
    friend __wasi_errno_t fd_tell(void*, __wasi_fd_t, filesizeptr_w);
    friend __wasi_errno_t fd_write(void*, __wasi_fd_t, ciovecptr_w, size_w, sizeptr_w);
    friend __wasi_errno_t path_create_directory(void*, __wasi_fd_t, charptr_w, size_w);
    friend __wasi_errno_t path_filestat_get(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, filestatptr_w);
    friend __wasi_errno_t path_filestat_set_times(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, __wasi_timestamp_t, __wasi_timestamp_t, __wasi_fstflags_t);
    friend __wasi_errno_t path_link(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, __wasi_fd_t, charptr_w, size_w);
    friend __wasi_errno_t path_open(void*, __wasi_fd_t, __wasi_lookupflags_t, charptr_w, size_w, __wasi_oflags_t, __wasi_rights_t, __wasi_rights_t, __wasi_fdflags_t, fdptr_w);
    friend __wasi_errno_t path_readlink(void*, __wasi_fd_t, charptr_w, size_w, charptr_w, size_w, sizeptr_w);
    friend __wasi_errno_t path_remove_directory(void*, __wasi_fd_t, charptr_w, size_w);
    friend __wasi_errno_t path_rename(void*, __wasi_fd_t, charptr_w, size_w, __wasi_fd_t, charptr_w, size_w);
    friend __wasi_errno_t path_symlink(void*, charptr_w, size_w, __wasi_fd_t, charptr_w, size_w);
    friend __wasi_errno_t path_unlink_file(void*, __wasi_fd_t, charptr_w, size_w);
    friend __wasi_errno_t poll_oneoff(void*, subscriptionptr_w, eventptr_w, size_w, sizeptr_w);
    friend void           proc_exit(void*, __wasi_exitcode_t);
    friend __wasi_errno_t proc_raise(void*, __wasi_signal_t);
    friend __wasi_errno_t random_get(void*, voiptr_w, size_w);
    friend __wasi_errno_t sched_yield(void*);
    friend __wasi_errno_t sock_recv(void*, __wasi_fd_t, iovecptr_w, size_w, __wasi_riflags_t, sizeptr_w, roflagsptr_w);
    friend __wasi_errno_t sock_send(void*, __wasi_fd_t, ciovecptr_w, size_w, __wasi_siflags_t, sizeptr_w);
    friend __wasi_errno_t sock_shutdown(void*, __wasi_fd_t, __wasi_sdflags_t);
  };

  resolver_t make_resolver();

};  // namespace wembed::wasi
