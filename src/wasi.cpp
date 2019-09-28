#include "wembed/context.hpp"
#include "wembed/wasi.hpp"

#ifdef WEMBED_TRACE_WASI_CALLS
template<typename ...Args>
void wasi_trace(const char *pHeader, Args && ...pArgs) {
  std::cout << "[wasi-trace] " << pHeader << " " << std::hex;
  ((std::cout << pArgs << ", "), ...);
  std::cout << std::dec;
}
#define TRACE_WASI_CALLS(...) wasi_trace(__FUNCTION__, __VA_ARGS__)
#define TRACE_WASI_RETURN(value) { std::cout << "returned " << value << std::endl; return value; }
#else
#define TRACE_WASI_CALLS(...)
#define TRACE_WASI_RETURN(value) return value;
#endif

extern char **environ;

namespace wembed::wasi {

wasi_context::wasi_context(uint64_t pRights) : mRights(pRights) {
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

void wasi_context::add_preopen(std::unique_ptr<file> && pFile) {
  mFiles.emplace(pFile->mFD, std::move(pFile));
}

void wasi_context::add_preopen_host() {
  add_preopen(std::make_unique<host_file>(0, "/dev/stdin", stdin));
  add_preopen(std::make_unique<host_file>(1, "/dev/stdout", stdout));
  add_preopen(std::make_unique<host_file>(2, "/dev/stderr", stderr));
  add_preopen(std::make_unique<file>(3, "/"));
  // TODO JBL: Apparently there is a default FD 4? Can't find references
}

fdstat_w file::status() const {
  auto status = std::filesystem::status(mPath);

  fdstat_w result;
  result.fs_flags = mFlags;
  switch (status.type()) {
    default: result.fs_filetype = __WASI_FILETYPE_UNKNOWN; break;
    case std::filesystem::file_type::regular: result.fs_filetype = __WASI_FILETYPE_REGULAR_FILE; break;
    case std::filesystem::file_type::directory: result.fs_filetype = __WASI_FILETYPE_DIRECTORY; break;
    case std::filesystem::file_type::symlink: result.fs_filetype = __WASI_FILETYPE_SYMBOLIC_LINK; break;
    case std::filesystem::file_type::block: result.fs_filetype = __WASI_FILETYPE_BLOCK_DEVICE; break;
    case std::filesystem::file_type::character: result.fs_filetype = __WASI_FILETYPE_CHARACTER_DEVICE; break;
    case std::filesystem::file_type::socket:
      result.fs_filetype = __WASI_FILETYPE_SOCKET_DGRAM;
      result.fs_filetype = __WASI_FILETYPE_SOCKET_STREAM; // FIXME
      break;
  }
  return result;
}

bool host_file::close() {
  return fclose(mStream) == 0;
}

bool host_file::seek(filedelta_w delta, whence_w whence, filesize_w *out_size) {
  int lWhence;
  switch (whence) {
    case __WASI_WHENCE_CUR: lWhence = SEEK_CUR; break;
    case __WASI_WHENCE_END: lWhence = SEEK_END; break;
    case __WASI_WHENCE_SET: lWhence = SEEK_SET; break;
    default:
      throw std::runtime_error("invalid whence passed to seek");
  }
  bool lSeekOk = fseek(mStream, delta, lWhence) == 0;
  if (out_size != nullptr)
    *out_size = ftell(mStream);
  return lSeekOk;
}

bool host_file::write(const std::vector<std::string_view> &iovecs, size_w *out_written) {
  size_w lTotalWritten = 0;
  for (auto lView : iovecs) {
    lTotalWritten += fwrite(lView.data(), 1, lView.size(), mStream);
  }
  if (out_written != nullptr)
    *out_written = lTotalWritten;
  return lTotalWritten > 0;
}

errno_w environ_sizes_get(void* ctx, sizeptr_w environ_count, sizeptr_w environ_buf_size) {
  TRACE_WASI_CALLS(environ_count, environ_buf_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lCount = (size_w*)(lWasmCtx->mem()->data() + environ_count);
  auto *lBufSize = (size_w*)(lWasmCtx->mem()->data() + environ_buf_size);

  *lCount = lWasiCtx->mEnv.mElements.size() + 1;
  *lBufSize = lWasiCtx->mEnv.mBuffer.size();

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

errno_w environ_get(void* ctx, charptrptr_w environ, charptr_w environ_buf) {
  TRACE_WASI_CALLS(environ, environ_buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
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

errno_w args_sizes_get(void* ctx, sizeptr_w argc, sizeptr_w argv_buf_size) {
  TRACE_WASI_CALLS(argc, argv_buf_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lCount = (size_w*)(lWasmCtx->mem()->data() + argc);
  auto *lBufSize = (size_w*)(lWasmCtx->mem()->data() + argv_buf_size);

  *lCount = lWasiCtx->mArgs.mElements.size();
  *lBufSize = lWasiCtx->mArgs.mBuffer.size();

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

errno_w args_get(void* ctx, charptrptr_w argv, charptr_w argv_buf) {
  TRACE_WASI_CALLS(argv, argv_buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lArgv = (charptr_w*)(lWasmCtx->mem()->data() + argv);
  auto *lBuff = (char*)(lWasmCtx->mem()->data() + argv_buf);

  memcpy(lBuff, lWasiCtx->mArgs.mBuffer.data(), lWasiCtx->mArgs.mBuffer.size());

  auto lCount = lWasiCtx->mArgs.mElements.size();
  for (size_t i = 0; i < lCount; i++) {
    lArgv[i] = argv_buf + lWasiCtx->mArgs.mElements[i];
  }

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

void proc_exit(void* ctx, exitcode_w rval) {
  TRACE_WASI_CALLS(rval);
  throw proc_exit_exception(rval);
}

errno_w fd_prestat_get(void* ctx, fd_w fd, prestatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lDst = (__wasi_prestat_t*)(lWasmCtx->mem()->data() + buf);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  auto &lPath = lIt->second->mPath.native();
  if (lPath.size() > std::numeric_limits<int32_t>::max())
    TRACE_WASI_RETURN(__WASI_EOVERFLOW);

  lDst->pr_type = __WASI_PREOPENTYPE_DIR;
  lDst->u.dir.pr_name_len = lPath.size();

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

errno_w fd_prestat_dir_name(void* ctx, fd_w fd, charptr_w path, size_w path_len) {
  TRACE_WASI_CALLS(fd, path, path_len);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lDst = (char*)(lWasmCtx->mem()->data() + path);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  auto &lPath = lIt->second->mPath.native();
  if (lPath.size() != path_len)
    TRACE_WASI_RETURN(__WASI_EINVAL);

  memcpy(lDst, lPath.data(), path_len);
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

errno_w fd_fdstat_get(void* ctx, fd_w fd, fdstatptr_w buf) {
  TRACE_WASI_CALLS(fd, buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lDst = (__wasi_fdstat_t*)(lWasmCtx->mem()->data() + buf);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  *lDst = lIt->second->status();
  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

errno_w fd_close(void *ctx, fd_w fd) {
  TRACE_WASI_CALLS(fd);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  lIt->second->close();
  lWasiCtx->mFiles.erase(lIt);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

errno_w fd_seek(void *ctx, fd_w fd, filedelta_w offset, whence_w whence, filesizeptr_w newoffset) {
  TRACE_WASI_CALLS(fd, offset, whence, newoffset);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lOffset = (filesize_w*)(lWasmCtx->mem()->data() + newoffset);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  if (!lIt->second->seek(offset, whence, lOffset))
    TRACE_WASI_RETURN(__WASI_EIO);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

errno_w fd_write(void *ctx, fd_w fd, const ciovecptr_w iovs, size_w iovs_len, sizeptr_w nwritten) {
  TRACE_WASI_CALLS(fd, iovs, iovs_len, nwritten);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());
  auto *lIOVecsIn = (ciovec_w*)(lWasmCtx->mem()->data() + iovs);
  auto *lWritten = (size_w*)(lWasmCtx->mem()->data() + nwritten);

  auto lIt = lWasiCtx->mFiles.find(fd);
  if (lIt == lWasiCtx->mFiles.end())
    TRACE_WASI_RETURN(__WASI_EBADF);

  std::vector<std::string_view> lIOVecs(iovs_len);
  for (int i = 0; i < iovs_len; i++) {
    auto *lDst = (char*)(lWasmCtx->mem()->data() + lIOVecsIn[i].buf);
    lIOVecs[i] = std::string_view{lDst, (size_t)lIOVecsIn[i].buf_len};
  }

  if (!lIt->second->write(lIOVecs, lWritten))
    TRACE_WASI_RETURN(__WASI_EIO);

  TRACE_WASI_RETURN(__WASI_ESUCCESS);
}

resolver_t make_unstable_resolver() {
  return [](context &pContext, std::string_view pFieldName) {
    const static std::unordered_map<std::string_view, resolve_result_t> sEnvMappings = {
      {"environ_sizes_get",   expose_func(&environ_sizes_get)},
      {"environ_get",         expose_func(&environ_get)},
      {"args_sizes_get",      expose_func(&args_sizes_get)},
      {"args_get",            expose_func(&args_get)},
      {"proc_exit",           expose_func(&proc_exit)},

      {"fd_prestat_get",      expose_func(&fd_prestat_get)},
      {"fd_prestat_dir_name", expose_func(&fd_prestat_dir_name)},
      {"fd_fdstat_get",       expose_func(&fd_fdstat_get)},

      {"fd_close",            expose_func(&fd_close)},
      {"fd_seek",             expose_func(&fd_seek)},
      {"fd_write",            expose_func(&fd_write)},
    };

    auto lFound = sEnvMappings.find(pFieldName);
    if (lFound == sEnvMappings.end())
      throw std::runtime_error(std::string("unknown wasi unstable import: ") + std::string(pFieldName));
    return lFound->second;
  };
}

}  // namespace wembed::wasi
