# CMakeLists.txt for Atom-Secret Storage Tests

set(STORAGE_TEST_SOURCES test_storage.cpp test_file_storage.cpp test_backup.cpp)

foreach(test_source ${STORAGE_TEST_SOURCES})
  get_filename_component(test_name ${test_source} NAME_WE)
  set(test_target "secret_${test_name}")

  add_executable(${test_target} ${test_source})
  target_link_libraries(${test_target} PRIVATE GTest::GTest GTest::Main
                                               atom::secret Threads::Threads)
  target_compile_features(${test_target} PRIVATE cxx_std_20)

  add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()
