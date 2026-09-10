# PNG decoder: unbounded allocation from an untrusted chunk length (OOM / DoS)

Found by `imencode_fuzzer` in fork mode within ~30 s of the first smoke fuzz (savantenvs port of the
mayhemheroes/opencv layer, OpenCV 5.x @ 816f9517); it is a decoder bug, reachable from every image
target (`imdecode_fuzzer`, `imread_fuzzer`, `core_fuzzer`, `imencode_fuzzer`). Reproducers are kept
OUT of the testsuites: under libFuzzer's default `-rss_limit_mb=2048` this input is an out-of-memory
abort on every run, which would kill Mayhem's sanity probe.

## Reproduce

    /mayhem/imdecode_fuzzer -runs=1 -malloc_limit_mb=2048 mayhem/imdecode_fuzzer/known-findings/png-chunk-length-unbounded-alloc/repro-minimal.png
    /mayhem/imread_fuzzer   -runs=1 -malloc_limit_mb=2048 mayhem/imdecode_fuzzer/known-findings/png-chunk-length-unbounded-alloc/repro-minimal.png

    ==N== ERROR: libFuzzer: out-of-memory (malloc(3372220412))
        #10 cv::PngDecoder::read_chunk(cv::Chunk&) modules/imgcodecs/src/grfmt_png.cpp:839:13
        #11 cv::PngDecoder::readHeader()           modules/imgcodecs/src/grfmt_png.cpp:304:14
        #12 cv::imdecode_(...)                     modules/imgcodecs/src/loadsave.cpp:1359:22

* `repro-minimal.png` (41 bytes): PNG signature + a valid 8x8 IHDR + a `tEXt` chunk header DECLARING
  0xC8FFFFF0 (3.37 GB) of data with no payload at all.
* `repro-fuzzer-found.png` (4192 bytes): the fuzzer's artifact (IHDR 45056 x 1677721600, a `tEXt`
  chunk declaring 3.4 GB). Note the absurd dimensions are irrelevant — the allocation happens before
  `validateInputImageSize()` (and before the harness's OPENCV_IO_MAX_IMAGE_* caps) is ever consulted.

## Cause

`modules/imgcodecs/src/grfmt_png.cpp`, `PngDecoder::read_chunk()` (called from `readHeader()` while
scanning the chunk list for APNG/IHDR/PLTE/... metadata):

    const size_t size = static_cast<size_t>(png_get_uint_32(size_id)) + 12;   // untrusted 32-bit length
    ...
    } else if (id != id_fdAT && id != id_IDAT && id != id_IEND && id != id_PLTE && id != id_tEXt && id != id_tRNS) {
        if (size > PNG_USER_CHUNK_MALLOC_MAX)   // 8 MB cap ... but only for the OTHER chunk types
            return 0;
    }
    chunk.p.resize(size);                       // allocates the declared size BEFORE reading a byte of it
    memcpy(chunk.p.data(), size_id, 8);
    if (readFromStreamOrBuffer(&chunk.p[8], chunk.p.size() - 8))   // then fails: the data is not there

The 8 MB sanity cap is skipped for `tEXt`, `PLTE`, `tRNS`, `IDAT`, `fdAT` and `IEND`, so any of those
chunk headers can request up to 4 GB, and the buffer is sized from the header alone — the decoder
never compares the declared length with the bytes actually available in the source buffer/file.

## Impact

Denial of service: a 41-byte PNG makes `cv::imdecode` / `cv::imread` (and anything built on them)
attempt a multi-GB allocation; under a memory limit the process is killed, otherwise it commits
gigabytes of address space and time for a rejected image. No memory corruption.

## One-line fix

Apply the cap to every chunk type, or (better) bound the declared size by what is left in the
source before resizing — for the in-memory path `size <= m_buf.total() - m_buf_pos`:

    if (size > PNG_USER_CHUNK_MALLOC_MAX && id != id_IDAT && id != id_fdAT) return 0;   // metadata chunks
    // and for IDAT/fdAT: if (m_buf.data && size > m_buf.total()*m_buf.elemSize() - m_buf_pos) return 0;
