@echo off
gcc test/test.c -Wall -Werror -o test_runner.exe && test_runner.exe
