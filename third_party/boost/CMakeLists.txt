cmake_minimum_required(VERSION 3.14)
project(BoostMath LANGUAGES CXX)

add_library(boost_math INTERFACE)

target_include_directories(boost_math INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(Boost::math ALIAS boost_math)
add_library(Boost::boost ALIAS boost_math)

if (CDR_ENABLE_INSTALL)
    install(TARGETS boost_math
        EXPORT CdrTargets
    )
    install(DIRECTORY boost DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
