@echo off
echo Running async tests...
cd /d %~dp0
build\tests\async\atom_async_tests.exe --gtest_filter=GeneratorTest.EmptyGenerator --gtest_break_on_failure
pause
