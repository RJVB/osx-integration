# this one (probably) best be avoided:
string(REPLACE "-fapplication-extension" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE "-fapplication-extension" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

if(BUILD_KDE_THEME_PLUGIN)
    add_subdirectory(platformtheme)
endif()
if(BUILD_QT_PLUGINS)
    string(REPLACE "-Wzero-as-null-pointer-constant" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_subdirectory(qmacstyle)
    add_subdirectory(qcocoa-qpa)
endif()
