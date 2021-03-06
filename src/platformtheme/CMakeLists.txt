if(DEFINE_ICONTHEME_SETTINGS)
    message(STATUS "The icon theme search path will be set")
    add_definitions(-DKDEMACTHEME_ADD_ICONTHEMESETTINGS)
endif()
if(PREFER_NATIVE_DIALOGS)
    message(STATUS "Native dialogs will be preferred")
    add_definitions(-DKDEMACTHEME_PREFER_NATIVE_DIALOGS)
else()
    if(NEVER_NATIVE_DIALOGS)
        message(STATUS "Native dialogs will be avoided")
        add_definitions(-DKDEMACTHEME_NEVER_NATIVE_DIALOGS)
    endif()
endif()
if(EMULATE_MENU_KEY)
    add_definitions(-DADD_MENU_KEY)
endif()

if(USE_PLCRASHREPORTER)
    find_library(CRASHREP CrashReporter
        DOC "The Plausible CrashReporter framework from https://www.plcrashreporter.org"
        PATHS ${CMAKE_INSTALL_PREFIX}/Library/Frameworks)
    if(NOT CRASHREP)
        message(WARNING "CrashReporter framework not found, building without!")
    else()
        add_definitions(-DUSE_PLCRASHREPORTER)
    endif()
endif()

add_subdirectory(presets)

if(OVERRIDE_NATIVE_THEME)
    set(PLATFORM_PLUGIN_THEME_NAME "cocoa")
    set(PLUGIN_FILENAME "CocoaPlatformTheme")
else()
    set(PLATFORM_PLUGIN_THEME_NAME "kde")
    set(PLUGIN_FILENAME "KDEPlatformTheme")
endif()
configure_file(config-platformtheme.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-platformtheme.h )

set(platformtheme_SRCS
    kdeplatformtheme.cpp
    kfontsettingsdata.cpp
    khintssettings.cpp
    kdeplatformfiledialoghelper.cpp
    kdeplatformfiledialogbase.cpp
    kdeplatformsystemtrayicon.cpp
    kfiletreeview.cpp
    kdirselectdialog.cpp
    kdemactheme.mm
    kfontsettingsdatamac.mm
    khintssettingsmac.mm
)
if(OVERRIDE_NATIVE_THEME)
    set(platformtheme_SRCS ${platformtheme_SRCS} main_cocoa.cpp)
else()
    set(platformtheme_SRCS ${platformtheme_SRCS} main_kde.cpp)
endif()

ecm_qt_declare_logging_category(platformtheme_SRCS
    HEADER platformtheme_logging.h
    IDENTIFIER PLATFORMTHEME
    CATEGORY_NAME ${PLUGIN_FILENAME}
)

if (NOT Qt5::GuiPrivate)
    include_directories(
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    )
endif()

# Use the same name for the platform theme module. This makes it impossible
# to install both the plasma and the OS X integration modules, but that's
# intentional.
add_library(${PLUGIN_FILENAME} MODULE ${platformtheme_SRCS})

if (Qt5::GuiPrivate)
    target_link_libraries(${PLUGIN_FILENAME}
        PRIVATE
            Qt5::GuiPrivate
    )
endif()
if (NOT DISABLE_DBUS_SUPPORT)
    target_link_libraries(${PLUGIN_FILENAME}
        PRIVATE
            Qt5::DBus
    )
endif()
# add KF5:WindowSystem?
target_link_libraries(${PLUGIN_FILENAME}
    PRIVATE
        KF5::ConfigWidgets
        KF5::ConfigCore
        KF5::IconThemes
        KF5::KIOFileWidgets # KFileFilterCombo, KDirSortFilterProxyModel, KRecentDirs
        KF5::KIOWidgets
        KF5::XmlGui
        KF5::I18n
        KF5::Notifications
        "${CRASHREP}"
)

target_link_libraries(${PLUGIN_FILENAME} PRIVATE "-framework AppKit")

set_target_properties(${PLUGIN_FILENAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/platformthemes/")
install(TARGETS ${PLUGIN_FILENAME} DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/platformthemes)
