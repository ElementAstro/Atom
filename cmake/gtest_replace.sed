/^# Testing framework/,/^endif()/ {
  /^# Testing framework/r cmake/gtest_insert.txt
  d
}
