add_benchmark(bench_${PROJECT_NAME} main.cpp)
target_link_libraries(bench_${PROJECT_NAME} ${PROJECT_NAME})

target_include_directories(bench_${PROJECT_NAME} PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
