try_compile(
  supports_fuzzing
  SOURCE_FROM_CONTENT
    entry.c
      "
      #include <stddef.h>
      #include <stdint.h>

      int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size) {}
      "
  LINK_OPTIONS -fsanitize=fuzzer
)

if(supports_fuzzing)
  add_executable(fuzz entry.c)

  target_link_libraries(
    fuzz
    PRIVATE
      bitarray_static
  )

  target_compile_options(
    fuzz
    PRIVATE
      -fsanitize=fuzzer
  )

  target_link_options(
    fuzz
    PRIVATE
      -fsanitize=fuzzer
  )
endif()
