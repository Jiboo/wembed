#include <wasi/api.h>

#include <stdlib.h>
#include <stdint.h>

#include "dep/minunit.h"

#define check_error(error, call, ...) do { int res = call(__VA_ARGS__); mu_assert_int_eq(error, res); } while(0)
#define check_success(call, ...) check_error(__WASI_ERRNO_SUCCESS, call, __VA_ARGS__)

MU_TEST(args_get) {
  size_t argc, argv_bufsize;
  check_success(__wasi_args_sizes_get, &argc, &argv_bufsize);
  uint8_t argv_buf[argv_bufsize];
  uint8_t *argv[argc];
  check_success(__wasi_args_get, argv, argv_buf);

  for (size_t i = 0; i < argc; i++) {
    mu_check(strlen((char*)argv[i]) > 0);
  }

  check_error(__WASI_ERRNO_FAULT, __wasi_args_get, (uint8_t**)0xF0000000, argv_buf);
  check_error(__WASI_ERRNO_FAULT, __wasi_args_get, argv, (uint8_t*)0xF0000000);
}

MU_TEST(args_sizes_get) {
  size_t argc, argv_buf_size;
  check_success(__wasi_args_sizes_get, &argc, &argv_buf_size);
  mu_check(argc > 0);
  mu_check(argv_buf_size > 0);

  size_t argc2, argv_bufsize2;
  check_success(__wasi_args_sizes_get, &argc2, &argv_bufsize2);
  mu_check(argc == argc2);
  mu_check(argv_buf_size == argv_bufsize2);

  check_error(__WASI_ERRNO_FAULT, __wasi_args_sizes_get, (size_t*)0xF0000000, &argv_buf_size);
  check_error(__WASI_ERRNO_FAULT, __wasi_args_sizes_get, &argc, (size_t*)0xF0000000);
}

MU_TEST(clock_res_get) {
  __wasi_timestamp_t resolution;
  check_success(__wasi_clock_res_get, __WASI_CLOCKID_REALTIME, &resolution);
  check_success(__wasi_clock_res_get, __WASI_CLOCKID_MONOTONIC, &resolution);
  check_success(__wasi_clock_res_get, __WASI_CLOCKID_PROCESS_CPUTIME_ID, &resolution);
  check_success(__wasi_clock_res_get, __WASI_CLOCKID_THREAD_CPUTIME_ID, &resolution);

  check_success(__wasi_clock_res_get, __WASI_CLOCKID_REALTIME, NULL);
  check_error(__WASI_ERRNO_FAULT, __wasi_clock_res_get, __WASI_CLOCKID_REALTIME, (__wasi_timestamp_t*)0xF0000000);
  check_error(__WASI_ERRNO_INVAL, __wasi_clock_res_get, 4, &resolution);
}

MU_TEST(clock_time_get) {
  for (int clock = 0; clock < 4; clock++) {
    __wasi_timestamp_t resolution;
    check_success(__wasi_clock_res_get, clock, &resolution);
    __wasi_timestamp_t tp;
    check_success(__wasi_clock_time_get, clock, resolution, &tp);
  }

  __wasi_timestamp_t tp;
  check_error(__WASI_ERRNO_FAULT, __wasi_clock_time_get, __WASI_CLOCKID_REALTIME, 0, (__wasi_timestamp_t*)0xF0000000);
  check_error(__WASI_ERRNO_INVAL, __wasi_clock_time_get, 4, 0, &tp);
}

MU_TEST(environ_get) {
  size_t environ_count, environ_buf_size;
  check_success(__wasi_environ_sizes_get, &environ_count, &environ_buf_size);
  uint8_t environ_buf[environ_buf_size];
  uint8_t *env[environ_count];
  check_success(__wasi_environ_get, env, environ_buf);

  for (size_t i = 0; i < environ_count; i++) {
    mu_check(strlen((char*)env[i]) > 0);
  }
}

MU_TEST(environ_sizes_get) {
  size_t environ_count, environ_buf_size;
  check_success(__wasi_environ_sizes_get, &environ_count, &environ_buf_size);
  mu_check(environ_count > 0);
  mu_check(environ_buf_size > 0);

  size_t environ_count2, environ_buf_size2;
  check_success(__wasi_environ_sizes_get, &environ_count2, &environ_buf_size2);
  mu_check(environ_count == environ_count2);
  mu_check(environ_buf_size == environ_buf_size2);
}

MU_TEST(fd_advise) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
      __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_ADVISE, 0, 0, &fd);

  check_success(__wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_NORMAL);
  check_success(__wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_SEQUENTIAL);
  check_success(__wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_RANDOM);
  check_success(__wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_WILLNEED);
  check_success(__wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_DONTNEED);
  check_success(__wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_NOREUSE);

  check_error(__WASI_ERRNO_INVAL, __wasi_fd_advise, fd, 0, 256, 6);

  check_success(__wasi_fd_close, fd);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_NORMAL);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, 0, 0, 0, &fd);
  check_error(__WASI_ERRNO_ACCES, __wasi_fd_advise, fd, 0, 256, __WASI_ADVICE_NORMAL);
  check_success(__wasi_fd_close, fd);
}

MU_TEST(fd_allocate) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_ALLOCATE | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);

  __wasi_filestat_t filestat;
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(0, filestat.size);

  check_success(__wasi_fd_allocate, fd, 0, 256);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(256, filestat.size);

  check_success(__wasi_fd_allocate, fd, 0, 256);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(256, filestat.size);

  check_success(__wasi_fd_allocate, fd, 256, 256);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(512, filestat.size);

  check_error(__WASI_ERRNO_INVAL, __wasi_fd_allocate, fd, 0, 0);

  check_success(__wasi_fd_close, fd);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_allocate, fd, 0, 256);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, 0, 0, 0, &fd);
  check_error(__WASI_ERRNO_ACCES, __wasi_fd_allocate, fd, 0, 256);
  check_success(__wasi_fd_close, fd);

  // FIXME Test __WASI_ERRNO_FBIG __WASI_ERRNO_NOSPC __WASI_ERRNO_NODEV
}

MU_TEST(fd_close) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_READ, 0, 0, &fd);
  check_success(__wasi_fd_close, fd);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_close, fd);
}

MU_TEST(fd_datasync) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_DATASYNC, 0, 0, &fd);
  check_success(__wasi_fd_datasync, fd);
  check_success(__wasi_fd_close, fd);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, 0, 0, 0, &fd);
  check_error(__WASI_ERRNO_ACCES, __wasi_fd_datasync, fd);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_datasync, fd);

  // FIXME Test __WASI_ERRNO_IO __WASI_ERRNO_NOSPC __WASI_ERRNO_INVAL
}

MU_TEST(fd_fdstat_get) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_READ, 0, __WASI_FDFLAGS_NONBLOCK, &fd);
  __wasi_fdstat_t fdstat;
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(__WASI_RIGHTS_FD_READ, fdstat.fs_rights_base);
  mu_assert_int_eq(0, fdstat.fs_rights_inheriting);
  mu_assert_int_eq(__WASI_FILETYPE_REGULAR_FILE, fdstat.fs_filetype);
  mu_assert_int_eq(__WASI_FDFLAGS_NONBLOCK, fdstat.fs_flags);

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_fdstat_get, fd, (__wasi_fdstat_t*)0xF0000000);

  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_fdstat_get, fd, &fdstat);

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test_dir", strlen("test_dir"),
                __WASI_OFLAGS_DIRECTORY, __WASI_RIGHTS_FD_READDIR, __WASI_RIGHTS_FD_READ, 0, &fd);
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(__WASI_RIGHTS_FD_READDIR, fdstat.fs_rights_base);
  mu_assert_int_eq(__WASI_RIGHTS_FD_READ, fdstat.fs_rights_inheriting);
  mu_assert_int_eq(__WASI_FILETYPE_DIRECTORY, fdstat.fs_filetype);
  mu_assert_int_eq(0, fdstat.fs_flags);
  check_success(__wasi_fd_close, fd);
  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));

  // FIXME Test __WASI_ERRNO_IO __WASI_ERRNO_NOSPC __WASI_ERRNO_INVAL
}

MU_TEST(fd_fdstat_set_flags) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS, 0, 0, &fd);
  __wasi_fdstat_t fdstat;
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(0, fdstat.fs_flags);
  check_success(__wasi_fd_fdstat_set_flags, fd, __WASI_FDFLAGS_APPEND);
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(__WASI_FDFLAGS_APPEND, fdstat.fs_flags);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_fdstat_set_flags, fd, 0);
}

MU_TEST(fd_fdstat_set_rights) {
  /*__wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS, 0, 0, &fd);
  __wasi_fdstat_t fdstat;
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(__WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS, fdstat.fs_rights_base);
  check_success(__wasi_fd_fdstat_set_rights, fd, __WASI_RIGHTS_FD_READ, 0);
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(__WASI_RIGHTS_FD_READ, fdstat.fs_rights_base);
  check_success(__wasi_fd_close, fd);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_READ, 0, 0, &fd);
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(__WASI_RIGHTS_FD_READ, fdstat.fs_rights_base);
  check_error(__WASI_ERRNO_NOTCAPABLE, __wasi_fd_fdstat_set_rights, fd, __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS, 0);
  check_success(__wasi_fd_fdstat_get, fd, &fdstat);
  mu_assert_int_eq(__WASI_RIGHTS_FD_READ, fdstat.fs_rights_base);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_fdstat_set_rights, fd, 0, 0);*/
}

MU_TEST(fd_filestat_get) {
  __wasi_fd_t fd;
  int rights = __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_FILESTAT_GET | __WASI_RIGHTS_FD_ALLOCATE;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, rights, 0, 0, &fd);
  __wasi_filestat_t filestat;
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(0, filestat.size);
  mu_assert_int_eq(__WASI_FILETYPE_REGULAR_FILE, filestat.filetype);

  check_success(__wasi_fd_allocate, fd, 0, 256);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(256, filestat.size);

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_filestat_get, fd, (__wasi_filestat_t*)0xF0000000);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_filestat_get, fd, &filestat);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd);
  check_error(__WASI_ERRNO_ACCES, __wasi_fd_filestat_get, fd, &filestat);
  check_success(__wasi_fd_close, fd);

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test_dir", strlen("test_dir"),
                __WASI_OFLAGS_DIRECTORY, __WASI_RIGHTS_FD_READDIR | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(__WASI_FILETYPE_DIRECTORY, filestat.filetype);
  check_success(__wasi_fd_close, fd);
  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));
}

MU_TEST(fd_filestat_set_size) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC,
                __WASI_RIGHTS_FD_FILESTAT_SET_SIZE | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);

  __wasi_filestat_t filestat;
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(0, filestat.size);
  check_success(__wasi_fd_filestat_set_size, fd, 512);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(512, filestat.size);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_filestat_set_size, fd, 0);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd);
  check_error(__WASI_ERRNO_ACCES, __wasi_fd_filestat_set_size, fd, 0);
  check_success(__wasi_fd_close, fd);
}

MU_TEST(fd_filestat_set_times) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC,
                __WASI_RIGHTS_FD_FILESTAT_SET_TIMES | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);

  __wasi_filestat_t previous, filestat;
  check_success(__wasi_fd_filestat_get, fd, &previous);

  check_success(__wasi_fd_filestat_set_times, fd, 0, 0, 0);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(previous.ctim, filestat.ctim);
  mu_assert_int_eq(previous.atim, filestat.atim);
  mu_assert_int_eq(previous.mtim, filestat.mtim);

  check_success(__wasi_fd_filestat_set_times, fd, 0, 0, __WASI_FSTFLAGS_ATIM_NOW | __WASI_FSTFLAGS_MTIM_NOW);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_check(previous.atim < filestat.atim);
  mu_check(previous.mtim < filestat.mtim);

  check_success(__wasi_fd_filestat_set_times, fd, 0, 0, __WASI_FSTFLAGS_ATIM | __WASI_FSTFLAGS_MTIM);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(0, filestat.atim);
  mu_assert_int_eq(0, filestat.mtim);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_filestat_set_times, fd, 0, 0, 0);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd);
  check_error(__WASI_ERRNO_ACCES, __wasi_fd_filestat_set_times, fd, 0, 0, 0);
  check_success(__wasi_fd_close, fd);
}

MU_TEST(fd_pread) {
  __wasi_fd_t fd;
  {
    check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                  __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd);
    __wasi_ciovec_t vecs[256];
    int data[256];
    for (int i = 0; i < 256; i++) {
      data[i] = i;
      vecs[i].buf = (uint8_t*)(data + i);
      vecs[i].buf_len = sizeof(int);
    }
    size_t nwritten;
    check_success(__wasi_fd_write, fd, vecs, 256, &nwritten);
    mu_assert_int_eq(256 * sizeof(int), nwritten);
    check_success(__wasi_fd_close, fd);
  }
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                0, __WASI_RIGHTS_FD_READ, 0, 0, &fd);
  __wasi_iovec_t vecs[256];
  int data[256];
  for (int i = 0; i < 256; i++) {
    vecs[i].buf = (uint8_t*)(data + i);
    vecs[i].buf_len = sizeof(int);
  }
  size_t nread;
  check_success(__wasi_fd_pread, fd, vecs, 256, 0, &nread);
  mu_assert_int_eq(256 * sizeof(int), nread);
  for (int i = 0; i < 256; i++) {
    mu_assert_int_eq(i, data[i]);
  }
  check_success(__wasi_fd_pread, fd, vecs, 10, 128 * sizeof(int), &nread);
  mu_assert_int_eq(10 * sizeof(int), nread);
  for (int i = 0; i < 10; i++) {
    mu_assert_int_eq(128 + i, data[i]);
  }

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_pread, fd, vecs, 10, 0, (size_t*)0xF0000000);

  __wasi_iovec_t badvec[2];
  badvec[0].buf_len = 0xFFFFFFFF;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)data;
  check_error(__WASI_ERRNO_INVAL, __wasi_fd_pread, fd, badvec, 2, 0, &nread);

  badvec[0].buf_len = 0x1;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)0xF0000000;
  check_error(__WASI_ERRNO_FAULT, __wasi_fd_pread, fd, badvec, 2, 0, &nread);

  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_pread, fd, vecs, 10, 0, &nread);

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test_dir", strlen("test_dir"),
                __WASI_OFLAGS_DIRECTORY, __WASI_RIGHTS_FD_READ, 0, 0, &fd);
  check_error(__WASI_ERRNO_ISDIR, __wasi_fd_pread, fd, vecs, 10, 0, &nread);
  check_success(__wasi_fd_close, fd);
  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));
}

MU_TEST(fd_prestat_get) {
  __wasi_prestat_t prestat;
  check_success(__wasi_fd_prestat_get, 3, &prestat);
  mu_assert_int_eq(__WASI_PREOPENTYPE_DIR, prestat.pr_type);
  mu_assert_int_eq(1, prestat.u.dir.pr_name_len);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_prestat_get, 0, &prestat);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_prestat_get, 10, &prestat);
}

MU_TEST(fd_prestat_dir_name) {
  __wasi_prestat_t prestat;
  check_success(__wasi_fd_prestat_get, 3, &prestat);
  uint8_t name[prestat.u.dir.pr_name_len+1];
  check_success(__wasi_fd_prestat_dir_name, 3, name, prestat.u.dir.pr_name_len);
  name[prestat.u.dir.pr_name_len] = 0;
  mu_assert_string_eq(".", (char*)name);

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_prestat_dir_name, 3, (uint8_t*)0xF0000000, prestat.u.dir.pr_name_len);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_prestat_dir_name, 0, name, prestat.u.dir.pr_name_len);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_prestat_dir_name, 10, name, prestat.u.dir.pr_name_len);
}

MU_TEST(fd_pwrite) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC,
                __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_READ, 0, 0, &fd);
  __wasi_ciovec_t cvecs[256];
  int data[256];
  for (int i = 0; i < 256; i++) {
    data[i] = i;
    cvecs[i].buf = (uint8_t*)(data + i);
    cvecs[i].buf_len = sizeof(int);
  }
  size_t nwritten;
  check_success(__wasi_fd_pwrite, fd, cvecs, 256, 0, &nwritten);
  mu_assert_int_eq(256 * sizeof(int), nwritten);

  __wasi_iovec_t vecs[256];
  for (int i = 0; i < 256; i++) {
    vecs[i].buf = (uint8_t*)(data + i);
    vecs[i].buf_len = sizeof(int);
  }
  size_t nread;
  check_success(__wasi_fd_pread, fd, vecs, 256, 0, &nread);
  mu_assert_int_eq(256 * sizeof(int), nread);
  for (int i = 0; i < 256; i++) {
    mu_assert_int_eq(i, data[i]);
  }

  check_success(__wasi_fd_pwrite, fd, cvecs, 10, 128 * sizeof(int), &nwritten);
  mu_assert_int_eq(10 * sizeof(int), nwritten);
  check_success(__wasi_fd_pread, fd, vecs, 10, 128 * sizeof(int), &nread);
  mu_assert_int_eq(10 * sizeof(int), nread);
  for (int i = 0; i < 10; i++) {
    mu_assert_int_eq(i, data[i]);
  }

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_pwrite, fd, cvecs, 10, 0, (size_t*)0xF0000000);

  __wasi_ciovec_t badvec[2];
  badvec[0].buf_len = 0xFFFFFFFF;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)data;
  check_error(__WASI_ERRNO_INVAL, __wasi_fd_pwrite, fd, badvec, 2, 0, &nwritten);

  badvec[0].buf_len = 0x1;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)0xF0000000;
  check_error(__WASI_ERRNO_FAULT, __wasi_fd_pwrite, fd, badvec, 2, 0, &nwritten);

  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_pwrite, fd, cvecs, 10, 0, &nwritten);
}

MU_TEST(fd_read) {
  __wasi_fd_t fd;
  {
    check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                  __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd);
    __wasi_ciovec_t vecs[256];
    int data[256];
    for (int i = 0; i < 256; i++) {
      data[i] = i;
      vecs[i].buf = (uint8_t*)(data + i);
      vecs[i].buf_len = sizeof(int);
    }
    size_t nwritten;
    check_success(__wasi_fd_write, fd, vecs, 256, &nwritten);
    mu_assert_int_eq(256 * sizeof(int), nwritten);
    check_success(__wasi_fd_close, fd);
  }
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                0, __WASI_RIGHTS_FD_READ, 0, 0, &fd);
  __wasi_iovec_t vecs[256];
  int data[256];
  for (int i = 0; i < 256; i++) {
    vecs[i].buf = (uint8_t*)(data + i);
    vecs[i].buf_len = sizeof(int);
  }
  size_t nread;
  check_success(__wasi_fd_read, fd, vecs, 10, &nread);
  mu_assert_int_eq(10 * sizeof(int), nread);
  for (int i = 0; i < 10; i++) {
    mu_assert_int_eq(i, data[i]);
  }
  check_success(__wasi_fd_read, fd, vecs, 10, &nread);
  mu_assert_int_eq(10 * sizeof(int), nread);
  for (int i = 0; i < 10; i++) {
    mu_assert_int_eq(10 + i, data[i]);
  }

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_read, fd, vecs, 10, (size_t*)0xF0000000);

  __wasi_iovec_t badvec[2];
  badvec[0].buf_len = 0xFFFFFFFF;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)data;
  check_error(__WASI_ERRNO_INVAL, __wasi_fd_read, fd, badvec, 2, &nread);

  badvec[0].buf_len = 0x1;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)0xF0000000;
  check_error(__WASI_ERRNO_FAULT, __wasi_fd_read, fd, badvec, 2, &nread);

  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_read, fd, vecs, 10, &nread);

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test_dir", strlen("test_dir"),
                __WASI_OFLAGS_DIRECTORY, __WASI_RIGHTS_FD_READ, 0, 0, &fd);
  check_error(__WASI_ERRNO_ISDIR, __wasi_fd_pread, fd, vecs, 10, 0, &nread);
  check_success(__wasi_fd_close, fd);
  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));
}

MU_TEST(fd_readdir) {
  __wasi_fd_t dirfd;

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test_dir", strlen("test_dir"),
                __WASI_OFLAGS_DIRECTORY,
                __WASI_RIGHTS_FD_READDIR | __WASI_RIGHTS_FD_TELL | __WASI_RIGHTS_FD_SEEK, 0, 0, &dirfd);

  uint8_t buf[1024];
  size_t buf_used;
  check_success(__wasi_fd_readdir, dirfd, buf, sizeof(buf), 0, &buf_used);

  mu_assert_int_eq(sizeof(__wasi_dirent_t) * 2 + strlen(".") + strlen(".."), buf_used);
  __wasi_dirent_t *dirent = (__wasi_dirent_t*)buf;
  mu_assert_int_eq(__WASI_FILETYPE_DIRECTORY, dirent->d_type);
  dirent = (__wasi_dirent_t *)(buf + sizeof(__wasi_dirent_t) + dirent->d_namlen);
  mu_assert_int_eq(__WASI_FILETYPE_DIRECTORY, dirent->d_type);

  check_success(__wasi_fd_seek, dirfd, 0, __WASI_WHENCE_SET, NULL);

  uint8_t underbuf[sizeof(__wasi_dirent_t) + strlen("..") + 1];
  dirent = (__wasi_dirent_t*)underbuf;
  check_success(__wasi_fd_readdir, dirfd, underbuf, sizeof(underbuf), 0, &buf_used);
  mu_assert_int_eq(sizeof(__wasi_dirent_t) + strlen("."), buf_used);
  mu_assert_int_eq(1, dirent->d_namlen);
  underbuf[sizeof(__wasi_dirent_t) + dirent->d_namlen] = 0;
  mu_assert_string_eq(".", (char*)underbuf + sizeof(__wasi_dirent_t));

  check_success(__wasi_fd_readdir, dirfd, underbuf, sizeof(underbuf), dirent->d_next, &buf_used);
  mu_assert_int_eq(sizeof(__wasi_dirent_t) + strlen(".."), buf_used);
  mu_assert_int_eq(2, dirent->d_namlen);
  underbuf[ sizeof(__wasi_dirent_t) + dirent->d_namlen] = 0;
  mu_assert_string_eq("..", (char*)underbuf + sizeof(__wasi_dirent_t));

  check_success(__wasi_fd_close, dirfd);
  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));
}

MU_TEST(fd_renumber) {
  __wasi_fd_t oldfd, newfd;

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_READ, 0, 0, &oldfd);
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                0, __WASI_RIGHTS_FD_READ, 0, 0, &newfd);
  mu_check(oldfd != newfd);

  check_success(__wasi_fd_renumber, oldfd, newfd);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_renumber, oldfd, newfd);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_renumber, newfd, oldfd);
  check_error(__WASI_ERRNO_BADF, __wasi_fd_close, oldfd);

  check_success(__wasi_fd_close, newfd);
}

MU_TEST(fd_seek) {
  __wasi_fd_t fd;
  {
    check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                  __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd);
    __wasi_ciovec_t vecs[256];
    int data[256];
    for (int i = 0; i < 256; i++) {
      data[i] = i;
      vecs[i].buf = (uint8_t*)(data + i);
      vecs[i].buf_len = sizeof(int);
    }
    size_t nwritten;
    check_success(__wasi_fd_write, fd, vecs, 256, &nwritten);
    mu_assert_int_eq(256 * sizeof(int), nwritten);
    check_success(__wasi_fd_close, fd);
  }

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                0, __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK, 0, 0, &fd);

  __wasi_filesize_t curpos;
  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(0, curpos);

  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_SET, &curpos);
  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(0, curpos);

  check_success(__wasi_fd_seek, fd, 10, __WASI_WHENCE_SET, &curpos);
  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(10, curpos);

  check_success(__wasi_fd_seek, fd, 20, __WASI_WHENCE_SET, NULL);
  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(20, curpos);

  check_success(__wasi_fd_seek, fd, 10, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(30, curpos);

  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_END, &curpos);
  mu_assert_int_eq(sizeof(int) * 256, curpos);

  check_success(__wasi_fd_seek, fd, sizeof(int) * -128, __WASI_WHENCE_END, &curpos);
  mu_assert_int_eq(sizeof(int) * 128, curpos);

  check_error(__WASI_ERRNO_INVAL, __wasi_fd_seek, fd, 0, 10, &curpos);
  check_error(__WASI_ERRNO_INVAL, __wasi_fd_seek, fd, -1, __WASI_WHENCE_SET, &curpos);
  check_error(__WASI_ERRNO_FAULT, __wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, (__wasi_filesize_t*)0xF0000000);

  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_seek, fd, 0, __WASI_WHENCE_END, &curpos);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                0, __WASI_RIGHTS_FD_READ, 0, 0, &fd);

  check_error(__WASI_ERRNO_ACCES, __wasi_fd_seek, fd, 0, __WASI_WHENCE_END, &curpos);

  check_success(__wasi_fd_close, fd);
}

MU_TEST(fd_sync) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, __WASI_RIGHTS_FD_SYNC, 0, 0, &fd);
  check_success(__wasi_fd_sync, fd);
  check_success(__wasi_fd_close, fd);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, 0, 0, 0, &fd);
  check_error(__WASI_ERRNO_ACCES, __wasi_fd_sync, fd);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_sync, fd);
}

MU_TEST(fd_tell) {
  __wasi_fd_t fd;
  {
    check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                  __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd);
    __wasi_ciovec_t vecs[256];
    int data[256];
    for (int i = 0; i < 256; i++) {
      data[i] = i;
      vecs[i].buf = (uint8_t*)(data + i);
      vecs[i].buf_len = sizeof(int);
    }
    size_t nwritten;
    check_success(__wasi_fd_write, fd, vecs, 256, &nwritten);
    mu_assert_int_eq(256 * sizeof(int), nwritten);
    check_success(__wasi_fd_close, fd);
  }

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                0, __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK | __WASI_RIGHTS_FD_TELL, 0, 0, &fd);

  __wasi_filesize_t curpos;
  check_success(__wasi_fd_tell, fd, &curpos);
  mu_assert_int_eq(0, curpos);

  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_SET, NULL);
  check_success(__wasi_fd_tell, fd, &curpos);
  mu_assert_int_eq(0, curpos);

  check_success(__wasi_fd_seek, fd, 10, __WASI_WHENCE_SET, NULL);
  check_success(__wasi_fd_tell, fd, &curpos);
  mu_assert_int_eq(10, curpos);

  check_success(__wasi_fd_seek, fd, 20, __WASI_WHENCE_SET, NULL);
  check_success(__wasi_fd_tell, fd, &curpos);
  mu_assert_int_eq(20, curpos);

  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_END, NULL);
  check_success(__wasi_fd_tell, fd, &curpos);
  mu_assert_int_eq(sizeof(int) * 256, curpos);

  check_success(__wasi_fd_seek, fd, sizeof(int) * -128, __WASI_WHENCE_END, NULL);
  check_success(__wasi_fd_tell, fd, &curpos);
  mu_assert_int_eq(sizeof(int) * 128, curpos);

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_tell, fd, (__wasi_filesize_t*)0xF0000000);

  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_tell, fd, &curpos);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                0, __WASI_RIGHTS_FD_READ, 0, 0, &fd);

  check_error(__WASI_ERRNO_ACCES, __wasi_fd_tell, fd, &curpos);

  check_success(__wasi_fd_close, fd);
}

MU_TEST(fd_write) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC,
                __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK, 0, 0, &fd);
  __wasi_ciovec_t cvecs[256];
  int data[256];
  for (int i = 0; i < 256; i++) {
    data[i] = i;
    cvecs[i].buf = (uint8_t*)(data + i);
    cvecs[i].buf_len = sizeof(int);
  }
  size_t nwritten;
  check_success(__wasi_fd_write, fd, cvecs, 256, &nwritten);
  mu_assert_int_eq(256 * sizeof(int), nwritten);

  __wasi_filesize_t curpos;
  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(256 * sizeof(int), curpos);

  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_SET, NULL);

  __wasi_iovec_t vecs[256];
  for (int i = 0; i < 256; i++) {
    vecs[i].buf = (uint8_t*)(data + i);
    vecs[i].buf_len = sizeof(int);
  }
  size_t nread;
  check_success(__wasi_fd_read, fd, vecs, 256, &nread);
  mu_assert_int_eq(256 * sizeof(int), nread);
  for (int i = 0; i < 256; i++) {
    mu_assert_int_eq(i, data[i]);
  }

  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_SET, NULL);
  check_success(__wasi_fd_write, fd, cvecs + 128, 10, &nwritten);
  mu_assert_int_eq(10 * sizeof(int), nwritten);
  check_success(__wasi_fd_pread, fd, vecs, 10, 0, &nread);
  mu_assert_int_eq(10 * sizeof(int), nread);
  for (int i = 0; i < 10; i++) {
    mu_assert_int_eq(i + 128, data[i]);
  }

  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_SET, NULL);
  check_success(__wasi_fd_write, fd, cvecs, 128, &nwritten);
  mu_assert_int_eq(128 * sizeof(int), nwritten);
  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(128 * sizeof(int), curpos);

  check_success(__wasi_fd_write, fd, cvecs + 128, 128, &nwritten);
  mu_assert_int_eq(128 * sizeof(int), nwritten);
  check_success(__wasi_fd_seek, fd, 0, __WASI_WHENCE_CUR, &curpos);
  mu_assert_int_eq(256 * sizeof(int), curpos);

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_write, fd, cvecs, 10, (size_t*)0xF0000000);

  __wasi_ciovec_t badvec[2];
  badvec[0].buf_len = 0xFFFFFFFF;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)data;
  check_error(__WASI_ERRNO_INVAL, __wasi_fd_write, fd, badvec, 2, &nwritten);

  badvec[0].buf_len = 0x1;
  badvec[0].buf = (uint8_t*)data;
  badvec[1].buf_len = 0x1;
  badvec[1].buf = (uint8_t*)0xF0000000;
  check_error(__WASI_ERRNO_FAULT, __wasi_fd_write, fd, badvec, 2, &nwritten);

  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_fd_write, fd, cvecs, 10, &nwritten);

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT,
                 __WASI_RIGHTS_FD_READ, 0, 0, &fd);

  check_error(__WASI_ERRNO_ACCES, __wasi_fd_write, fd, cvecs, 10, &nwritten);

  check_success(__wasi_fd_close, fd);
}

MU_TEST(path_create_directory) {
  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));

  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT,
                __WASI_RIGHTS_FD_READ, 0, 0, &fd);

  check_error(__WASI_ERRNO_EXIST, __wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_error(__WASI_ERRNO_EXIST, __wasi_path_create_directory, 3, "test.txt", strlen("test.txt"));
  check_error(__WASI_ERRNO_FAULT, __wasi_path_create_directory, 3, (const char*)0xF0000000, strlen("test.txt"));
  check_error(__WASI_ERRNO_NOENT, __wasi_path_create_directory, 3, "test/invalid", strlen("test/invalid"));
  check_error(__WASI_ERRNO_NOTDIR, __wasi_path_create_directory, 3, "test.txt/invalid", strlen("test.txt/invalid"));
  check_error(__WASI_ERRNO_NOTDIR, __wasi_path_create_directory, 0, "test_dir2", strlen("test_dir2"));
  check_error(__WASI_ERRNO_BADF, __wasi_path_create_directory, 10, "test_dir2", strlen("test_dir2"));

  __wasi_fd_t dirfd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test_dir", strlen("test_dir"),
                __WASI_OFLAGS_DIRECTORY,
                __WASI_RIGHTS_FD_READDIR, 0, 0, &dirfd);

  check_success(__wasi_path_create_directory, dirfd, "test_subdir", strlen("test_subdir"));

  check_success(__wasi_path_remove_directory, dirfd, "test_subdir", strlen("test_subdir"));

  check_success(__wasi_fd_close, dirfd);

  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));
}

MU_TEST(path_filestat_get) {
  __wasi_fd_t fd;
  int rights = __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_FILESTAT_GET | __WASI_RIGHTS_FD_ALLOCATE;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC, rights, 0, 0, &fd);
  __wasi_filestat_t fd_filestat, path_filestat;
  check_success(__wasi_fd_filestat_get, fd, &fd_filestat);
  mu_assert_int_eq(0, fd_filestat.size);
  mu_assert_int_eq(__WASI_FILETYPE_REGULAR_FILE, fd_filestat.filetype);
  check_success(__wasi_path_filestat_get, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"), &path_filestat);
  mu_assert_int_eq(fd_filestat.filetype, path_filestat.filetype);
  mu_assert_int_eq(fd_filestat.size, path_filestat.size);
  mu_assert_int_eq(fd_filestat.atim, path_filestat.atim);
  mu_assert_int_eq(fd_filestat.nlink, path_filestat.nlink);
  mu_assert_int_eq(fd_filestat.ctim, path_filestat.ctim);
  mu_assert_int_eq(fd_filestat.ino, path_filestat.ino);
  mu_assert_int_eq(fd_filestat.dev, path_filestat.dev);
  mu_assert_int_eq(fd_filestat.mtim, path_filestat.mtim);

  check_success(__wasi_fd_allocate, fd, 0, 256);
  check_success(__wasi_fd_filestat_get, fd, &fd_filestat);
  mu_assert_int_eq(256, fd_filestat.size);
  check_success(__wasi_path_filestat_get, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"), &path_filestat);
  mu_assert_int_eq(fd_filestat.filetype, path_filestat.filetype);
  mu_assert_int_eq(fd_filestat.size, path_filestat.size);
  mu_assert_int_eq(fd_filestat.atim, path_filestat.atim);
  mu_assert_int_eq(fd_filestat.nlink, path_filestat.nlink);
  mu_assert_int_eq(fd_filestat.ctim, path_filestat.ctim);
  mu_assert_int_eq(fd_filestat.ino, path_filestat.ino);
  mu_assert_int_eq(fd_filestat.dev, path_filestat.dev);
  mu_assert_int_eq(fd_filestat.mtim, path_filestat.mtim);

  check_error(__WASI_ERRNO_FAULT, __wasi_fd_filestat_get, fd, (__wasi_filestat_t*)0xF0000000);
  check_error(__WASI_ERRNO_FAULT, __wasi_path_filestat_get, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"), (__wasi_filestat_t*)0xF0000000);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_path_filestat_get, 10, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"), &path_filestat);
}

MU_TEST(path_filestat_set_times) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT | __WASI_OFLAGS_TRUNC,
                __WASI_RIGHTS_FD_FILESTAT_SET_TIMES | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);

  __wasi_filestat_t previous, filestat;
  check_success(__wasi_fd_filestat_get, fd, &previous);

  check_success(__wasi_path_filestat_set_times, 3, 0, "test.txt", strlen("test.txt"), 0, 0, 0);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(previous.ctim, filestat.ctim);
  mu_assert_int_eq(previous.atim, filestat.atim);
  mu_assert_int_eq(previous.mtim, filestat.mtim);

  check_success(__wasi_path_filestat_set_times, 3, 0, "test.txt", strlen("test.txt"), 0, 0, __WASI_FSTFLAGS_ATIM_NOW | __WASI_FSTFLAGS_MTIM_NOW);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_check(previous.atim < filestat.atim);
  mu_check(previous.mtim < filestat.mtim);

  check_success(__wasi_path_filestat_set_times, 3, 0, "test.txt", strlen("test.txt"), 0, 0, __WASI_FSTFLAGS_ATIM | __WASI_FSTFLAGS_MTIM);
  check_success(__wasi_fd_filestat_get, fd, &filestat);
  mu_assert_int_eq(0, filestat.atim);
  mu_assert_int_eq(0, filestat.mtim);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_BADF, __wasi_path_filestat_set_times, 10, 0, "test.txt", strlen("test.txt"), 0, 0, 0);
}

MU_TEST(path_link) {
  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));

  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT,
                __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);

  __wasi_filestat_t previous, after;
  check_success(__wasi_fd_filestat_get, fd, &previous);

  check_success(__wasi_path_link, 3, 0, "test.txt", strlen("test.txt"), 3, "link1", strlen("link1"));

  check_success(__wasi_fd_filestat_get, fd, &after);
  mu_assert_int_eq(previous.nlink + 1, after.nlink);

  check_error(__WASI_ERRNO_EXIST, __wasi_path_link, 3, 0, "test.txt", strlen("test.txt"), 3, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_BADF, __wasi_path_link, 10, 0, "test.txt", strlen("test.txt"), 3, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_BADF, __wasi_path_link, 3, 0, "test.txt", strlen("test.txt"), 10, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_FAULT, __wasi_path_link, 3, 0, (const char*)0xF0000000, strlen("test.txt"), 3, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_FAULT, __wasi_path_link, 3, 0, "test.txt", strlen("test.txt"), 3, (const char*)0xF0000000, strlen("link1"));

  check_success(__wasi_fd_close, fd);
}

MU_TEST(path_open) {
  __wasi_fd_t fd1;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT,
                __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd1);

  __wasi_fd_t fd2;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT,
                __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd2);

  mu_check(fd2 != (fd1 + 1));

  check_error(__WASI_ERRNO_FAULT, __wasi_path_open, 3, 0, (const char*)0xF0000000, strlen("test.txt"), 0, 0, 0, 0, &fd2);

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_error(__WASI_ERRNO_ISDIR, __wasi_path_open, 3, 0, "test_dir", strlen("test_dir"), 0, __WASI_RIGHTS_FD_WRITE, 0, 0, &fd2);
  check_error(__WASI_ERRNO_NOENT, __wasi_path_open, 3, 0, "test2.txt", strlen("test2.txt"), 0, 0, 0, 0, &fd2);
  check_error(__WASI_ERRNO_NOENT, __wasi_path_open, 3, 0, "test_dir2/test.txt", strlen("test_dir2/test.txt"), 0, 0, 0, 0, &fd2);
  check_error(__WASI_ERRNO_NOTDIR, __wasi_path_open, 3, 0, "test.txt", strlen("test.txt"), __WASI_OFLAGS_DIRECTORY, 0, 0, 0, &fd2);
  check_error(__WASI_ERRNO_BADF, __wasi_path_open, 10, 0, "test.txt", strlen("test.txt"), 0, 0, 0, 0, &fd2);

  check_success(__wasi_fd_close, fd2);
  check_success(__wasi_fd_close, fd1);
}

MU_TEST(path_readlink) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT,
                __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);
  check_success(__wasi_fd_close, fd);

  check_success(__wasi_path_symlink, "test.txt", strlen("test.txt"), 3, "link1", strlen("link1"));

  uint8_t buf[128];
  size_t buf_used;
  check_success(__wasi_path_readlink, 3, "link1", strlen("link1"), buf, sizeof(buf), &buf_used);
  mu_assert_int_eq(strlen("test.txt"), buf_used);
  mu_assert_string_eq("test.txt", (char*)buf);

  check_error(__WASI_ERRNO_FAULT, __wasi_path_readlink, 3, (const char*)0xF0000000, strlen("link1"), buf, sizeof(buf), &buf_used);
  check_error(__WASI_ERRNO_FAULT, __wasi_path_readlink, 3, "link1", strlen("link1"), buf, sizeof(buf), (size_t*)0xF0000000);

  check_error(__WASI_ERRNO_INVAL, __wasi_path_readlink, 3, "test.txt", strlen("test.txt"), buf, sizeof(buf), &buf_used);
  check_error(__WASI_ERRNO_BADF, __wasi_path_readlink, 10, "link1", strlen("link1"), buf, sizeof(buf), &buf_used);
}

MU_TEST(path_remove_directory) {
  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));

  __wasi_fd_t dirfd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test_dir", strlen("test_dir"),
                __WASI_OFLAGS_DIRECTORY,
                __WASI_RIGHTS_FD_READDIR, 0, 0, &dirfd);

  check_success(__wasi_path_create_directory, dirfd, "test_subdir", strlen("test_subdir"));

  check_error(__WASI_ERRNO_NOTEMPTY, __wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));

  check_success(__wasi_path_remove_directory, dirfd, "test_subdir", strlen("test_subdir"));
  check_success(__wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));

  check_error(__WASI_ERRNO_NOENT, __wasi_path_remove_directory, 3, "test_dir", strlen("test_dir"));
  check_error(__WASI_ERRNO_FAULT, __wasi_path_remove_directory, 3, (const char*)0xF0000000, strlen("test_dir"));

  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, 0, 0, 0, &fd);
  check_success(__wasi_fd_close, fd);

  check_error(__WASI_ERRNO_NOTDIR, __wasi_path_remove_directory, 3, "test.txt", strlen("test.txt"));
}

MU_TEST(path_rename) {
  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, 0, 0, 0, &fd);
  check_success(__wasi_fd_close, fd);
  check_success(__wasi_path_rename, 3, "test.txt", strlen("test.txt"), 3, "link1", strlen("link1"));
  check_success(__wasi_path_unlink_file, 3, "link1", strlen("link1"));

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));
  check_success(__wasi_path_rename, 3, "test_dir", strlen("test_dir"), 3, "link1", strlen("link1"));
  check_success(__wasi_path_remove_directory, 3, "link1", strlen("link1"));

  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT, 0, 0, 0, &fd);
  check_success(__wasi_fd_close, fd);

  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));

  check_error(__WASI_ERRNO_ISDIR, __wasi_path_rename, 3, "test.txt", strlen("test.txt"), 3, "test_dir", strlen("test_dir"));
  check_error(__WASI_ERRNO_NOENT, __wasi_path_rename, 3, "test_dir2", strlen("test_dir2"), 3, "test_dir3", strlen("test_dir3"));

  check_error(__WASI_ERRNO_FAULT, __wasi_path_rename, 3, (const char*)0xF0000000, strlen("test_dir"), 3, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_FAULT, __wasi_path_rename, 3, "test_dir", strlen("test_dir"), 3, (const char*)0xF0000000, strlen("link1"));
}

MU_TEST(path_symlink) {
  check_success(__wasi_path_create_directory, 3, "test_dir", strlen("test_dir"));

  __wasi_fd_t fd;
  check_success(__wasi_path_open, 3, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, "test.txt", strlen("test.txt"),
                __WASI_OFLAGS_CREAT,
                __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_FILESTAT_GET, 0, 0, &fd);

  check_success(__wasi_path_symlink, "test.txt", strlen("test.txt"), 3, "link1", strlen("link1"));

  check_error(__WASI_ERRNO_EXIST, __wasi_path_symlink, "test.txt", strlen("test.txt"), 3, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_BADF, __wasi_path_symlink, "test.txt", strlen("test.txt"), 10, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_FAULT, __wasi_path_symlink, (const char*)0xF0000000, strlen("test.txt"), 3, "link1", strlen("link1"));
  check_error(__WASI_ERRNO_FAULT, __wasi_path_symlink, "test.txt", strlen("test.txt"), 3, (const char*)0xF0000000, strlen("link1"));

  check_success(__wasi_fd_close, fd);
}

MU_TEST(path_unlink_file) {

}

MU_TEST(poll_oneoff) {
  // TODO Not impl in wembed
}

MU_TEST(proc_exit) {
  // TODO Needs another system to test
}

MU_TEST(proc_raise) {
  // TODO Needs another system to test
}

MU_TEST(random_get) {
  int result1 = 0, result2 = 0;
  check_success(__wasi_random_get, (uint8_t*)&result1, sizeof(result1));
  check_success(__wasi_random_get, (uint8_t*)&result2, sizeof(result2));
  mu_check(result1 != result2);

  uint8_t buf[4096];
  check_success(__wasi_random_get, buf, sizeof(buf));

  uint8_t *hugebuf = malloc(0xFFFFFF);
  check_success(__wasi_random_get, hugebuf, 0xFFFFFF);
  free(hugebuf);

  check_error(__WASI_ERRNO_FAULT, __wasi_random_get, (void*)0xF0000000, 1024);
  check_error(__WASI_ERRNO_INVAL, __wasi_random_get, buf, 0);
}

MU_TEST(sched_yield) {
  __wasi_timestamp_t precision, before, without, after;
  check_success(__wasi_clock_res_get, __WASI_CLOCKID_REALTIME, &precision);
  check_success(__wasi_clock_time_get, __WASI_CLOCKID_REALTIME, precision, &before);
  check_success(__wasi_clock_time_get, __WASI_CLOCKID_REALTIME, precision, &without);
  check_success(__wasi_sched_yield);
  check_success(__wasi_clock_time_get, __WASI_CLOCKID_REALTIME, precision, &after);

  __wasi_timestamp_t dwithout = without - before;
  __wasi_timestamp_t dwith = after - without;
  mu_check(dwith > dwithout * 5);
}

MU_TEST(sock_recv) {
  // TODO Not impl in wembed
}

MU_TEST(sock_send) {
  // TODO Not impl in wembed
}

MU_TEST(sock_shutdown) {
  // TODO Not impl in wembed
}

void clean_fs(const char *_name) {
  int result;
  result = __wasi_path_remove_directory(3, _name, strlen(_name));
  result = __wasi_path_unlink_file(3, _name, strlen(_name));

}

void test_suite_setup() {
  clean_fs("test_dir");
  clean_fs("test_dir/test_subdir");
  clean_fs("link1");
  clean_fs("link2");
  clean_fs("test.txt");
}

void test_suite_teardown() {
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(test_suite_setup, test_suite_teardown);
  MU_RUN_TEST(args_get);
  MU_RUN_TEST(args_sizes_get);
  MU_RUN_TEST(clock_res_get);
  MU_RUN_TEST(clock_time_get);
  MU_RUN_TEST(environ_get);
  MU_RUN_TEST(environ_sizes_get);
  MU_RUN_TEST(fd_advise);
  MU_RUN_TEST(fd_allocate);
  MU_RUN_TEST(fd_close);
  MU_RUN_TEST(fd_datasync);
  MU_RUN_TEST(fd_fdstat_get);
  MU_RUN_TEST(fd_fdstat_set_flags);
  MU_RUN_TEST(fd_fdstat_set_rights);
  MU_RUN_TEST(fd_filestat_get);
  MU_RUN_TEST(fd_filestat_set_size);
  MU_RUN_TEST(fd_filestat_set_times);
  MU_RUN_TEST(fd_pread);
  MU_RUN_TEST(fd_prestat_get);
  MU_RUN_TEST(fd_prestat_dir_name);
  MU_RUN_TEST(fd_pwrite);
  MU_RUN_TEST(fd_read);
  MU_RUN_TEST(fd_readdir);
  MU_RUN_TEST(fd_renumber);
  MU_RUN_TEST(fd_seek);
  MU_RUN_TEST(fd_sync);
  MU_RUN_TEST(fd_tell);
  MU_RUN_TEST(fd_write);
  MU_RUN_TEST(path_create_directory);
  MU_RUN_TEST(path_filestat_get);
  MU_RUN_TEST(path_filestat_set_times);
  MU_RUN_TEST(path_link);
  MU_RUN_TEST(path_open);
  MU_RUN_TEST(path_readlink);
  MU_RUN_TEST(path_remove_directory);
  MU_RUN_TEST(path_rename);
  MU_RUN_TEST(path_symlink);
  MU_RUN_TEST(path_unlink_file);
  MU_RUN_TEST(poll_oneoff);
  MU_RUN_TEST(proc_exit);
  MU_RUN_TEST(proc_raise);
  MU_RUN_TEST(random_get);
  MU_RUN_TEST(sched_yield);
  MU_RUN_TEST(sock_recv);
  MU_RUN_TEST(sock_send);
  MU_RUN_TEST(sock_shutdown);
}

int main(int argc, char *argv[]) {
  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  return MU_EXIT_CODE;
}
