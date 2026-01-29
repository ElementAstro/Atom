# CMakeLists.txt for Atom-Secret Manager Tests

set(MANAGER_TEST_SOURCES test_session.cpp test_audit_log.cpp
                         test_password_manager.cpp)

foreach(test_source ${MANAGER_TEST_SOURCES})
  get_filename_component(test_name ${test_source} NAME_WE)
  set(test_target "secret_${test_name}")

  add_executable(${test_target} ${test_source})
  target_link_libraries(${test_target} PRIVATE GTest::GTest GTest::Main
                                               atom::secret Threads::Threads)
  target_compile_features(${test_target} PRIVATE cxx_std_20)

  add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()
