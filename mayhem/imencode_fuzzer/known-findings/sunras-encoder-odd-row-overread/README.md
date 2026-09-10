# Sun Raster encoder: 1-byte heap over-read on every odd-width row

Found while smoke-testing the ported `imencode_fuzzer` on its own seed corpus (savantenvs port of the
mayhemheroes/opencv layer, OpenCV 5.x @ 816f9517). Reproducers in this directory are kept OUT of
`mayhem/imencode_fuzzer/testsuite/` on purpose: a crashing seed would abort Mayhem's `-runs=5`
sanity probe on every run.

## Harness guard (2026-09-14)

Mayhem's server-side corpus for `imencode_fuzzer` accumulated inputs that reach this over-read
deterministically, and Mayhem's `-runs=5` sanity probe aborted run #6 on them before any fuzzing
happened. The harness therefore skips `cv::imencode(".ras", img)` when `img.cols * img.channels()` is
odd (`SkipKnownFinding()` in `mayhem/harnesses/imencode_fuzzer.cc`); every other codec and every
even-width `.ras` encode is still fuzzed. To reproduce the finding with the harness, rebuild it with
`-DIMENCODE_FUZZER_KEEP_KNOWN_FINDINGS` (the reproducers below then crash again), or call the library
directly: `cv::imencode(".ras", cv::Mat::zeros(1, 1, CV_8UC1), buf)` under ASan.

## Reproduce

    /mayhem/imencode_fuzzer-standalone mayhem/imencode_fuzzer/known-findings/sunras-encoder-odd-row-overread/repro-minimal.bin
    /mayhem/imencode_fuzzer-standalone mayhem/imencode_fuzzer/known-findings/sunras-encoder-odd-row-overread/repro-templ_png_to_ras.bin

* `repro-minimal.bin` (2 bytes, `A\x0b`): harness selector byte 0x0b = ".ras"; the remaining single
  byte becomes a 1x1 CV_8UC1 row -> `AddressSanitizer: heap-buffer-overflow ... READ of size 2`
  from a 1-byte heap region (`bitstrm.cpp:462 WLByteStream::putBytes` <- `grfmt_sunras.cpp:418
  SunRasterEncoder::write`).
* `repro-templ_png_to_ras.bin` (1636 bytes): the original seed (samples/data/templ.png + 0x0b) —
  the raw 1x1635 row path reads 1636 bytes from the 1635-byte buffer.

Any caller of `cv::imencode(".ras", img)` / `cv::imwrite("x.ras", img)` with `img.cols * img.channels()`
odd and a tightly allocated last row hits it (e.g. a 5x5 CV_8UC3 image: 15 bytes per row, fileStep 16).

## Cause

`modules/imgcodecs/src/grfmt_sunras.cpp`, `SunRasterEncoder::write()`:

    int fileStep = (width*channels + 1) & -2;          // rows padded to an even length in the file
    ...
    for( y = 0; y < height; y++ )
        CHECK_WRITE(strm.putBytes( img.ptr(y), fileStep ));   // reads fileStep bytes from the SOURCE row

The Sun Raster format pads each scanline to 16 bits, and the encoder correctly declares `fileStep`
in the header, but it then copies `fileStep` bytes out of the source `Mat` row — one byte past the
end of the pixel data when `width*channels` is odd. For interior rows that byte is the next row's
first pixel (wrong data written into the file); for the last row it is past the end of the
allocation (out-of-bounds read).

## Impact

Out-of-bounds heap read of 1 byte per image; the byte is written into the encoded output (a
1-byte heap information leak), and a read at a page boundary can crash the process. Reachable from
`cv::imwrite`/`cv::imencode` with any odd-width single-channel or odd-`width*channels` 3-channel
image. Not exploitable for code execution on its own; it is a correctness + memory-safety defect in
the encoder.

## One-line fix

Write the pixel bytes and an explicit zero pad instead of over-reading the row:

    for( y = 0; y < height; y++ )
    {
        CHECK_WRITE(strm.putBytes( img.ptr(y), width*channels ));
        if( fileStep > width*channels ) CHECK_WRITE(strm.putByte( 0 ));
    }
