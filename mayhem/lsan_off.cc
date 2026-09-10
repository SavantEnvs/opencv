// Build-time LeakSanitizer off-switch (fleet policy, PORTING.md): compiled with $SANITIZER_FLAGS and
// linked into EVERY fuzz + -standalone binary by mayhem/build.sh. Leaks are not the bug class this
// fleet fuzzes for; ASan's memory-corruption checks and UBSan stay fully active. This weak-interface
// hook is the only sanctioned mechanism (no runtime enable/disable wraps, no compiled-in option
// overrides, no Mayhemfile ASAN option lines — Mayhem alone owns the runtime option set).
extern "C" int __lsan_is_turned_off(void) { return 1; }
