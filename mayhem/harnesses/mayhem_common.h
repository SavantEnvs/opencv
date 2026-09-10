// Shared helpers for the OpenCV Mayhem harnesses (mayhem/harnesses/*.cc).
//
// Ported from the mayhemheroes/opencv (4.x) layer, which was derived from OpenCV's OSS-Fuzz harnesses
// (Copyright 2018-2020 Google Inc., Apache License 2.0). The temp-file adapter below replaces the
// legacy fuzzer_temp_file.h (which hard-coded /tmp) with the $TMPDIR + mkstemp() form.
#ifndef MAYHEM_OPENCV_COMMON_H_
#define MAYHEM_OPENCV_COMMON_H_

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Process-wide OpenCV runtime limits, applied BEFORE any C++ static initializer runs.
//
// modules/imgcodecs/src/loadsave.cpp reads OPENCV_IO_MAX_IMAGE_{WIDTH,HEIGHT,PIXELS} into
// namespace-scope `static const` variables, i.e. during C++ static initialization — so setting them
// from LLVMFuzzerInitialize() (called from main()) would be too late. A priority-101 constructor
// lands in .init_array.00101, which the linker orders before the default-priority C++ static
// initializers of every translation unit linked into this executable, so loadsave.cpp sees the
// values when it initializes.
//
// Why: OpenCV's defaults (2^20 x 2^20, 2^30 pixels) let a ~30-byte crafted image header request a
// multi-GB decode buffer, which libFuzzer reports as an out-of-memory on every such input — noise,
// not a memory-safety finding. 16 Mpx (4096x4096) x 4 ch x 16-bit = 128 MB keeps one decode well
// under libFuzzer's default -rss_limit_mb=2048 while leaving every codec path reachable.
// setenv(..., 0) never overrides a value the environment already carries, so a Mayhemfile `env:`
// or a manual run can still tune them.
__attribute__((constructor(101))) static void mayhem_opencv_env_init(void) {
  setenv("OPENCV_IO_MAX_IMAGE_PIXELS", "16777216", 0);
  setenv("OPENCV_IO_MAX_IMAGE_WIDTH", "16384", 0);
  setenv("OPENCV_IO_MAX_IMAGE_HEIGHT", "16384", 0);
  // CV_LOG_ERROR chatter (e.g. "Can't open file" on every FileStorage miss) only slows fuzzing.
  setenv("OPENCV_LOG_LEVEL", "SILENT", 0);
}

// RAII adapter from fuzzer bytes to a harness-owned scratch file, for OpenCV APIs that take a file
// name (cv::imread, cv::FileStorage::open). Scratch goes to $TMPDIR (fallback /tmp) via mkstemp();
// the file is removed in the destructor. Any failure here is a harness/environment fault, so abort().
class FuzzerTemporaryFile {
 public:
  FuzzerTemporaryFile(const uint8_t* data, size_t size) {
    const char* dir = getenv("TMPDIR");
    if (dir == NULL || *dir == '\0') dir = "/tmp";
    int n = snprintf(path_, sizeof(path_), "%s/opencv_fuzz.XXXXXX", dir);
    if (n < 0 || (size_t)n >= sizeof(path_)) {
      fprintf(stderr, "FuzzerTemporaryFile: TMPDIR too long\n");
      abort();
    }
    int fd = mkstemp(path_);
    if (fd < 0) {
      perror("FuzzerTemporaryFile: mkstemp");
      abort();
    }
    size_t off = 0;
    while (off < size) {
      ssize_t w = write(fd, data + off, size - off);
      if (w < 0) {
        if (errno == EINTR) continue;
        perror("FuzzerTemporaryFile: write");
        close(fd);
        abort();
      }
      off += (size_t)w;
    }
    close(fd);
  }
  ~FuzzerTemporaryFile() { unlink(path_); }
  const char* filename() const { return path_; }

 private:
  FuzzerTemporaryFile(const FuzzerTemporaryFile&);
  FuzzerTemporaryFile& operator=(const FuzzerTemporaryFile&);
  char path_[PATH_MAX];
};

#endif  // MAYHEM_OPENCV_COMMON_H_
