cmake_minimum_required(VERSION 2.7)
project(KayShell C)

set(CMAKE_C_STANDARD 99)

add_executable(KayShell tsh.c tsh_helper.c csapp.c wrapper.c)