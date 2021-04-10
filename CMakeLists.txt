cmake_minimum_required(VERSION 2.7)
project(KayShell C)

set(CMAKE_C_STANDARD 99)

add_executable(KayShell kshell.c destructor.c parser.c builtin.c)
