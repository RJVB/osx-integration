TEMPLATE = lib
CONFIG += plugin

# generate the target under a different name so it can co-exist with the
# stock QPA plugin. When multiple plugins share the same name to load order
# appears to be
# 1 platform theme plugin 
# 2 "external" platform plugin
# 3 built-in platform plugin

TARGET = qaltcocoa

SOURCES += main.mm \
    qcocoaintegration.mm \
    qcocoatheme.mm \
    qcocoabackingstore.mm \
    qcocoawindow.mm \
    qnsview.mm \
    qnsviewaccessibility.mm \
    qnswindowdelegate.mm \
    qcocoanativeinterface.mm \
    qcocoaeventdispatcher.mm \
    qcocoaapplicationdelegate.mm \
    qcocoaapplication.mm \
    qcocoamenu.mm \
    qcocoamenuitem.mm \
    qcocoamenubar.mm \
    qcocoamenuloader.mm \
    qcocoahelpers.mm \
    qmultitouch_mac.mm \
    qcocoaaccessibilityelement.mm \
    qcocoaaccessibility.mm \
    qcocoacolordialoghelper.mm \
    qcocoafiledialoghelper.mm \
    qcocoacursor.mm \
    qcocoaclipboard.mm \
    qcocoadrag.mm \
    qmacclipboard.mm \
    qcocoasystemsettings.mm \
    qcocoainputcontext.mm \
    qcocoaservices.mm \
    qcocoasystemtrayicon.mm \
    qcocoaintrospection.mm \
    qcocoakeymapper.mm \
    qcocoamimetypes.mm \
    messages.cpp

HEADERS += qcocoaintegration.h \
    qcocoatheme.h \
    qcocoabackingstore.h \
    qcocoawindow.h \
    qnsview.h \
    qnswindowdelegate.h \
    qcocoanativeinterface.h \
    qcocoaeventdispatcher.h \
    qcocoaapplicationdelegate.h \
    qcocoaapplication.h \
    qcocoamenu.h \
    qcocoamenuitem.h \
    qcocoamenubar.h \
    qcocoamenuloader.h \
    qcocoahelpers.h \
    qmultitouch_mac_p.h \
    qcocoaaccessibilityelement.h \
    qcocoaaccessibility.h \
    qcocoacolordialoghelper.h \
    qcocoafiledialoghelper.h \
    qcocoacursor.h \
    qcocoaclipboard.h \
    qcocoadrag.h \
    qmacclipboard.h \
    qcocoasystemsettings.h \
    qcocoainputcontext.h \
    qcocoaservices.h \
    qcocoasystemtrayicon.h \
    qcocoaintrospection.h \
    qcocoakeymapper.h \
    messages.h \
    qcocoamimetypes.h

# qtConfig(opengl.*) {
    SOURCES += qcocoaglcontext.mm

    HEADERS += qcocoaglcontext.h
# }

RESOURCES += qcocoaresources.qrc

LIBS += -framework AppKit -framework Carbon -framework IOKit -lcups
# CONFIG += qpa/basicunixfontdatabase

QT += \
    core-private gui-private \
    accessibility_support-private clipboard_support-private theme_support-private \
    fontdatabase_support-private graphics_support-private cgl_support-private

CONFIG += no_app_extension_api_only

# qtHaveModule(widgets) {
    SOURCES += \
        qpaintengine_mac.mm \
        qprintengine_mac.mm \
        qcocoaprintersupport.mm \
        qcocoaprintdevice.mm \

    HEADERS += \
        qpaintengine_mac_p.h \
        qprintengine_mac_p.h \
        qcocoaprintersupport.h \
        qcocoaprintdevice.h \

#     qtConfig(fontdialog) {
        SOURCES += qcocoafontdialoghelper.mm
        HEADERS += qcocoafontdialoghelper.h
#     }

    QT += widgets-private printsupport-private
# }

OTHER_FILES += cocoa.json

# Acccessibility debug support
# DEFINES += QT_COCOA_ENABLE_ACCESSIBILITY_INSPECTOR
# include ($$PWD/../../../../util/accessibilityinspector/accessibilityinspector.pri)


PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QCocoaIntegrationPlugin
!equals(TARGET, $$QT_DEFAULT_QPA_PLUGIN): PLUGIN_EXTENDS = -
# load(qt_plugin)
