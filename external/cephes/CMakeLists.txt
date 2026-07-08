add_library(cephes STATIC
  src/atanll.c
  src/constll.c
  src/exp2ll.c
  src/floorll.c
  src/log2ll.c
  src/mtherr.c
  src/polevll.c
  src/sinll.c
  src/tanll.c
)

# 128-bit library
target_link_libraries(cephes berkeley-softfloat)
target_include_directories(cephes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(cephes PRIVATE -fno-builtin)
