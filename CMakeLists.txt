cmake_minimum_required(VERSION 2.8.4)
project(tour)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -std=gnu89 -DDEBUG -DCOLOR")

set(SOURCE_FILES_TEST
    cache.c
    cache.h
    debug.h
    minunit.h
    minunit_cache.c)

set(SOURCE_FILES_TOUR
    api.c
    api.h
    ping.c
    ping.h
    common.h
    debug.h
    tour.c
    tour.h)

set(SOURCE_FILES_ARP
    api.h
    common.h
    debug.h
    get_hw_addrs.c
    get_hw_addrs.h
    cache.c
    cache.h
    arp.c
    arp.h)

add_executable(test ${SOURCE_FILES_TEST})
add_executable(tour ${SOURCE_FILES_TOUR})
add_executable(arp ${SOURCE_FILES_ARP})
