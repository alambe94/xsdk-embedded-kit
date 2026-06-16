xsdk_add_test(xtrace_test      SOURCES test_xtrace.c      LIBS xsdk::xtrace)
xsdk_add_test(xtrace_cobs_test SOURCES test_xtrace_cobs.c LIBS xsdk::xtrace)

# Benchmark - built but not registered as a CTest test.
# Run manually: build\host\src\components\xutil\xtrace\tests\bench_xtrace.exe
add_executable(xtrace_bench bench_xtrace.c)
target_link_libraries(xtrace_bench PRIVATE xsdk::xtrace)
xsdk_apply_target_warnings(xtrace_bench)
