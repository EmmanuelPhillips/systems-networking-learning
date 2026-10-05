cmake_minimum_required(VERSION 3.10)
project(beej_sockets C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra)

add_executable(server 01_networking_funamentals/server.c)
