// Copyright 2020 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Mayhem port (savantenvs/opencv, 5.x) — SEMANTICS FIX. The mayhemheroes/opencv 4.x copy passed the
// fuzzer bytes to open(..., READ) WITHOUT FileStorage::MEMORY, so OpenCV treated them as a FILE NAME
// (a duplicate of filestorage_read_filename_fuzzer) and never parsed a byte of them. With
// READ | MEMORY the bytes are the document itself (persistence.cpp: mem_mode -> strbufv = source),
// which is what this harness's own comment always claimed to test.
//
// Surface: the in-memory XML / YAML / JSON FileStorage parsers (format sniffed from the first bytes,
// then a full parse of nodes, sequences, maps, strings, numbers and base64 blobs).

#include <cstddef>
#include <cstdint>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

#include "mayhem_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Tests reading from a string (instead of a file) using cv::FileStorage,
  // which attempts to parse JSON, XML, and YAML, using the first few bytes of a
  // string to determine which type to parse it as.
  cv::FileStorage storage;
  try {
    storage.open(std::string(reinterpret_cast<const char*>(data), size),
                 cv::FileStorage::READ | cv::FileStorage::MEMORY);
  } catch (const cv::Exception&) {
    // Do nothing.
  }
  return 0;
}
