add_gtest(test_${PROJECT_NAME} test_executors.cpp test_future.cpp)
target_link_libraries(test_${PROJECT_NAME} ${PROJECT_NAME})

target_include_directories(test_${PROJECT_NAME} PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
