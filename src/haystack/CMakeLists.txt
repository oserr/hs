find_package(PkgConfig REQUIRED)
pkg_check_modules(REDIS REQUIRED hiredis)
set(REDISLIB "${CMAKE_PREFIX_PATH}/lib/lib${REDIS_LIBRARIES}.a")

add_library(haystack
    asyncmap.hh
    cache.cc
    cache.hh
    directory.cc
    directory.hh
    haystack.cc
    haystack.hh
    needle.cc
    needle.hh
    store.cc
    store.hh
)
target_compile_options(haystack PUBLIC ${REDIS_CFLAGS_OTHER})

target_include_directories(haystack PUBLIC
    ${LIBMONGOCXX_INCLUDE_DIRS}
    ${REDIS_INCLUDE_DIRS}
)

target_link_libraries(haystack
    ${Boost_LIBRARIES}
    ${LIBMONGOCXX_LIBRARIES}
    ${REDISLIB}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_executable(store_app store_app.cc)
target_link_libraries(store_app haystack)

add_executable(cache_app cache_app.cc)
target_link_libraries(cache_app haystack)

add_executable(dir_app dir_app.cc)
target_link_libraries(dir_app haystack)

install(
    TARGETS store_app cache_app dir_app
    DESTINATION bin
)
