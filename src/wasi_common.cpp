#include "wembed/context.hpp"
#include "wembed/wasi.hpp"

namespace wembed::wasi {

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

void wasi_context::add_preopen(__wasi_fd_t pFD, file pFile) {
  mFiles.emplace(pFD, std::move(pFile));
  mPreopens.emplace(pFD);
}

__wasi_errno_t args_get(void *ctx, charptrptr_w argv, charptr_w argv_buf) {
  TRACE_WASI_CALLS(argv, argv_buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((lWasiCtx->mArgs.mElements.size() * sizeof(charptr_w) + argv) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((lWasiCtx->mArgs.mBuffer.size() + argv_buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lArgv = (charptr_w*)(lWasmCtx->mem()->data() + argv);
  auto *lBuff = (char*)(lWasmCtx->mem()->data() + argv_buf);

  memcpy(lBuff, lWasiCtx->mArgs.mBuffer.data(), lWasiCtx->mArgs.mBuffer.size());

  auto lCount = lWasiCtx->mArgs.mElements.size();
  for (size_t i = 0; i < lCount; i++) {
    lArgv[i] = argv_buf + lWasiCtx->mArgs.mElements[i];
  }

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t args_sizes_get(void *ctx, sizeptr_w argc, sizeptr_w argv_buf_size) {
  TRACE_WASI_CALLS(argc, argv_buf_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + argc) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + argv_buf_size) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lCount = (size_w*)(lWasmCtx->mem()->data() + argc);
  auto *lBufSize = (size_w*)(lWasmCtx->mem()->data() + argv_buf_size);

  *lCount = lWasiCtx->mArgs.mElements.size();
  TRACE_WASI_OUT(argc, *lCount);
  *lBufSize = lWasiCtx->mArgs.mBuffer.size();
  TRACE_WASI_OUT(argv_buf_size, *lBufSize);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t environ_get(void *ctx, charptrptr_w environ_ptrs, charptr_w environ_buf) {
  TRACE_WASI_CALLS(environ, environ_buf);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((lWasiCtx->mEnv.mElements.size() * sizeof(charptr_w) + environ_ptrs) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((lWasiCtx->mEnv.mBuffer.size() + environ_buf) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lEnv = (charptr_w*)(lWasmCtx->mem()->data() + environ_ptrs);
  auto *lEnvBuf = (char*)(lWasmCtx->mem()->data() + environ_buf);

  memcpy(lEnvBuf, lWasiCtx->mEnv.mBuffer.data(), lWasiCtx->mEnv.mBuffer.size());

  auto lCount = lWasiCtx->mEnv.mElements.size();
  for (size_t i = 0; i < lCount; i++) {
    lEnv[i] = environ_buf + lWasiCtx->mEnv.mElements[i];
  }
  lEnv[lCount] = 0;

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t environ_sizes_get(void *ctx, sizeptr_w environ_count, sizeptr_w environ_buf_size) {
  TRACE_WASI_CALLS(environ_count, environ_buf_size);

  auto *lWasmCtx = static_cast<context*>(ctx);
  auto *lWasiCtx = static_cast<wasi_context*>(lWasmCtx->user());

  if ((sizeof(size_w) + environ_count) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);
  if ((sizeof(size_w) + environ_buf_size) > lWasmCtx->mem()->byte_size())
    TRACE_WASI_RETURN(__WASI_ERRNO_FAULT);

  auto *lCount = (size_w*)(lWasmCtx->mem()->data() + environ_count);
  auto *lBufSize = (size_w*)(lWasmCtx->mem()->data() + environ_buf_size);

  *lCount = lWasiCtx->mEnv.mElements.size();
  TRACE_WASI_OUT(environ_count, *lCount);
  *lBufSize = lWasiCtx->mEnv.mBuffer.size();
  TRACE_WASI_OUT(environ_buf_size, *lBufSize);

  TRACE_WASI_RETURN(__WASI_ERRNO_SUCCESS);
}

__wasi_errno_t poll_oneoff(void *ctx, const subscriptionptr_w in, eventptr_w out, size_w nsubscriptions, sizeptr_w nevents) {
  TRACE_WASI_CALLS(in, out, nsubscriptions, nevents);
  TRACE_WASI_RETURN(__WASI_ERRNO_NOTCAPABLE);
}

void proc_exit(void *ctx, __wasi_exitcode_t rval) {
  TRACE_WASI_CALLS(rval);
  throw proc_exit_exception(rval);
}

__wasi_errno_t sock_recv(void *ctx, __wasi_fd_t sock, const iovecptr_w ri_data, size_w ri_data_len, __wasi_riflags_t ri_flags, sizeptr_w ro_datalen, roflagsptr_w ro_flags) {
  TRACE_WASI_CALLS(sock, ri_data, ri_data_len, ri_flags, ro_datalen, ro_flags);
  TRACE_WASI_RETURN(__WASI_ERRNO_NOTCAPABLE);
}

__wasi_errno_t sock_send(void *ctx, __wasi_fd_t sock, const ciovecptr_w si_data, size_w si_data_len, __wasi_siflags_t si_flags, sizeptr_w so_datalen) {
  TRACE_WASI_CALLS(sock, si_data, si_data_len, si_flags, so_datalen);
  TRACE_WASI_RETURN(__WASI_ERRNO_NOTCAPABLE);
}

__wasi_errno_t sock_shutdown(void *ctx, __wasi_fd_t sock, __wasi_sdflags_t how) {
  TRACE_WASI_CALLS(sock, how);
  TRACE_WASI_RETURN(__WASI_ERRNO_NOTCAPABLE);
}

resolver_t make_resolver() {
  //decltype(&wembed_environ_get) t = nullptr;

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
      throw std::runtime_error(std::string("unknown wasi import: ") + std::string(pFieldName));
    return lFound->second;
  };
}

}  // namespace wembed::wasi
