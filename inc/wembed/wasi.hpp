#pragma once

#include <filesystem>

#include "wembed/context.hpp"
#include "wembed/wasi_decls.hpp"

namespace wembed::wasi {

  class proc_exit_exception : public vm_runtime_exception {
  public:
    uint32_t mCode;
    explicit proc_exit_exception(uint32_t pCode)
      : vm_runtime_exception(std::string("proc_exit: ") + std::to_string(pCode)), mCode(pCode) {}
  };

  struct file {
    int mHostFD;
    std::filesystem::path mPath;
    __wasi_rights_t mRights = 0, mIRights = 0;
  };

  class wasi_context {
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
    friend __wasi_errno_t fd_write_(void*, __wasi_fd_t, ciovecptr_w, size_w, sizeptr_w);
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
