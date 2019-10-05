#pragma once

#include <filesystem>

#include "wembed/context.hpp"
#include "wembed/wasi_decls.hpp"

namespace wembed::wasi {

  class proc_exit_exception : public vm_runtime_exception {
  public:
    uint32_t mCode;
    proc_exit_exception(uint32_t pCode)
      : vm_runtime_exception(std::string("proc_exit: ") + std::to_string(pCode)), mCode(pCode) {}
  };

  using errno_w = __wasi_errno_t;
  using exitcode_w = __wasi_exitcode_t;

  using fd_w = __wasi_fd_t;
  using fdstat_w = __wasi_fdstat_t;
  using fdflags_w = __wasi_fdflags_t;
  using filesize_w = __wasi_filesize_t;
  using filedelta_w = __wasi_filedelta_t;
  using whence_w = __wasi_whence_t;
  using ciovec_w = __wasi_ciovec_t;

  struct file {
    fd_w mFD;
    std::filesystem::path mPath;
    fdflags_w mFlags = 0;

    file(fd_w pFD, std::filesystem::path pPath) : mFD(pFD), mPath(std::move(pPath)) {}
    virtual ~file() = default;

    virtual fdstat_w status() const;
    virtual bool close() { return false; }
    virtual bool seek(filedelta_w delta, whence_w whence, filesize_w *out_size) { return false; }
    virtual bool tell(filesize_w *out_size) { return false; }
    virtual bool write(const std::vector<std::string_view> &iovecs, size_w *out_written) { return false; }
  };

  struct host_file : file {
    FILE *mStream;

    host_file(fd_w pFD, std::filesystem::path pPath, FILE *pStream) : file(pFD, std::move(pPath)), mStream(pStream) {}

    bool close() override;
    bool seek(filedelta_w delta, whence_w whence, filesize_w *out_size) override;
    bool tell(filesize_w *out_size) override;
    bool write(const std::vector<std::string_view> &iovecs, size_w *out_written) override;
  };

  class wasi_context {
  public:
    explicit wasi_context(uint64_t pRights = std::numeric_limits<uint64_t>::max());

    void add_arg(std::string_view pArg);
    void add_args(int pArgc, const char **pArgv);

    void add_env(std::string_view pEnv);
    void add_env_host();

    void add_preopen(std::unique_ptr<file> && pFile);
    void add_preopen_host();

  protected:
    uint64_t mRights;

    struct strings_buff {
      std::vector<uint8_t> mBuffer;
      std::vector<size_t> mElements;
    };
    strings_buff mArgs, mEnv;

    std::unordered_map<fd_w, std::unique_ptr<file>> mFiles;

    friend errno_w args_sizes_get(void*, sizeptr_w, sizeptr_w);
    friend errno_w args_get(void*, charptrptr_w, charptr_w);
    friend errno_w environ_sizes_get(void*, sizeptr_w, sizeptr_w);
    friend errno_w environ_get(void*, charptrptr_w, charptr_w);

    friend errno_w fd_prestat_get(void*, fd_w, prestatptr_w);
    friend errno_w fd_prestat_dir_name(void*, fd_w, charptr_w, size_w);
    friend errno_w fd_fdstat_get(void*, fd_w, fdstatptr_w);

    friend errno_w fd_close(void*, fd_w);
    friend errno_w fd_seek(void*, fd_w, filedelta_w, whence_w, filesizeptr_w);
    friend errno_w fd_write(void*, fd_w, ciovecptr_w, size_w, sizeptr_w);
    friend errno_w fd_tell(void*, fd_w, filesizeptr_w);
  };

  resolver_t make_unstable_resolver();

};  // namespace wembed::wasi
