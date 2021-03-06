include(ECMMarkAsTest)
include(ECMMarkNonGuiExecutable)

find_package(Qt5Test ${REQUIRED_QT_VERSION} CONFIG QUIET)

if(NOT Qt5Test_FOUND)
    message(STATUS "Qt5Test not found, autotests will not be built.")
    return()
endif()

include_directories( ${Qt5Gui_PRIVATE_INCLUDE_DIRS} )

set(CONFIGFILE "${CMAKE_CURRENT_SOURCE_DIR}/kdeplatformtheme_kdeglobals")
configure_file(kdeplatformtheme_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/kdeplatformtheme_config.h)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

macro(OSXINTEGRATION_TESTS _testname)
    add_executable(${_testname} ${_testname}.cpp ${ARGN})
    set_target_properties(${_testname} PROPERTIES COMPILE_FLAGS "-DUNIT_TEST")
    add_test(osxintegration-${_testname} ${_testname})
    ecm_mark_as_test(${_testname})
    ecm_mark_nongui_executable(${_testname})
    target_link_libraries(${_testname} Qt5::Test KF5::ConfigWidgets KF5::ConfigCore KF5::IconThemes KF5::Style KF5::Notifications)
endmacro()


