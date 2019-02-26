/* This file is part of the KDE libraries
   Copyright (C) 2000, 2006 David Faure <faure@kde.org>
   Copyright 2008 Friedrich W. H. Kossebau <kossebau@kde.org>
   Copyright 2013 Aleix Pol Gonzalez <aleixpol@blue-systems.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#undef QT_NO_CAST_FROM_ASCII

#include "kfontsettingsdata.h"
#include "platformtheme_logging.h"

#include <QCoreApplication>
#include <QString>
#include <QVariant>
#include <QApplication>
#ifdef DBUS_SUPPORT_ENABLED
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>
#endif
#include <qpa/qwindowsysteminterface.h>

#include <ksharedconfig.h>
#include <kconfiggroup.h>

static inline bool checkUsePortalSupport()
{
#ifdef Q_OS_MACOS
    return false;
#else
    return !QStandardPaths::locate(QStandardPaths::RuntimeLocation, QStringLiteral("flatpak-info")).isEmpty() || qEnvironmentVariableIsSet("SNAP");
#endif
}

KFontSettingsData::KFontSettingsData()
    : QObject(nullptr)
    , mUsePortal(checkUsePortalSupport())
{
#ifdef DBUS_SUPPORT_ENABLED
    QMetaObject::invokeMethod(this, "delayedDBusConnects", Qt::QueuedConnection);
#endif

    for (int i = 0; i < FontTypesCount; ++i) {
        mFonts[i] = nullptr;
    }
}

KFontSettingsData::~KFontSettingsData()
{
    for (int i = 0; i < FontTypesCount; ++i) {
        delete mFonts[i];
    }
}

// NOTE: keep in sync with plasma-desktop/kcms/fonts/fonts.cpp
static const char GeneralId[] =      "General";
static const char DefaultFont[] =    "Lucida Grande";

static const KFontData DefaultFontData[KFontSettingsData::FontTypesCount] = {
    { GeneralId, "font",                 DefaultFont,  10, -1, QFont::SansSerif, "Regular" },
    { GeneralId, "fixed",                "Monaco",  9, -1, QFont::Monospace, "Regular" },
    { GeneralId, "toolBarFont",          DefaultFont,  9, -1, QFont::SansSerif, "Regular" },
    { GeneralId, "menuFont",             DefaultFont,  10, -1, QFont::SansSerif, "Regular" },
    { "WM",      "activeFont",           DefaultFont,  10, -1, QFont::SansSerif, "Regular" },
    { GeneralId, "taskbarFont",          DefaultFont,  10, -1, QFont::SansSerif, "Regular" },
    { GeneralId, "smallestReadableFont", DefaultFont,  8, -1, QFont::SansSerif, "Regular" }
};

KSharedConfigPtr &KFontSettingsData::kdeGlobals()
{
    if (!mKdeGlobals) {
        if (qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME_CONFIG_FILE")) {
            const auto fname(qgetenv("QT_QPA_PLATFORMTHEME_CONFIG_FILE"));
            mKdeGlobals = KSharedConfig::openConfig(fname, KConfig::NoGlobals);
            const auto foundFile = QStandardPaths::locate(mKdeGlobals->locationType(), mKdeGlobals->name());
            if (foundFile.isEmpty()) {
                qCWarning(PLATFORMTHEME) << "WARNING: could not open config file" << fname
                    << "in" << QStandardPaths::standardLocations(mKdeGlobals->locationType());
            }
        } else {
            mKdeGlobals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::NoGlobals);
        }
    }
    return mKdeGlobals;
}

QFont *KFontSettingsData::font(FontTypes fontType)
{
    QFont *cachedFont = mFonts[fontType];

    if (!cachedFont) {
        const KFontData &fontData = DefaultFontData[fontType];
        cachedFont = new QFont(QLatin1String(fontData.FontName), fontData.Size, fontData.Weight);
        cachedFont->setStyleHint(fontData.StyleHint);

        const KConfigGroup configGroup(kdeGlobals(), fontData.ConfigGroupKey);
        QString fontInfo = configGroup.readEntry(fontData.ConfigKey, QString());

        //If we have serialized information for this font, restore it
        //NOTE: We are not using KConfig directly because we can't call QFont::QFont from here
        if (!fontInfo.isEmpty()) {
            cachedFont->fromString(fontInfo);
        } else {
            // set the canonical stylename here, where it cannot override
            // user-specific font attributes if those do not include a stylename.
            cachedFont->setStyleName(QLatin1String(fontData.StyleName));
        }

        mFonts[fontType] = cachedFont;
    }

    return cachedFont;
}

void KFontSettingsData::dropFontSettingsCache()
{
    if (mKdeGlobals) {
        mKdeGlobals->reparseConfiguration();
    }
    for (int i = 0; i < FontTypesCount; ++i) {
        delete mFonts[i];
        mFonts[i] = nullptr;
    }

    QWindowSystemInterface::handleThemeChange(nullptr);

    if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
        QApplication::setFont(*font(KFontSettingsData::GeneralFont));
    } else {
        QGuiApplication::setFont(*font(KFontSettingsData::GeneralFont));
    }
}

void KFontSettingsData::delayedDBusConnects()
{
#ifdef DBUS_SUPPORT_ENABLED
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KDEPlatformTheme"), QStringLiteral("org.kde.KDEPlatformTheme"),
                                          QStringLiteral("refreshFonts"), this, SLOT(dropFontSettingsCache()));

    if (mUsePortal) {
        QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.portal.Settings"),
                                              QStringLiteral("SettingChanged"), this, SLOT(slotPortalSettingChanged(QString,QString,QDBusVariant)));
    }
#endif
}

void KFontSettingsData::slotPortalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value)
{
    Q_UNUSED(value);
#ifdef DBUS_SUPPORT_ENABLED

    if (group == QLatin1String("org.kde.kdeglobals.General") && key == QLatin1String("font")) {
        dropFontSettingsCache();
    }
#else
    Q_UNUSED(group);
    Q_UNUSED(key);
#endif
}

QString KFontSettingsData::readConfigValue(const QString &group, const QString &key, const QString &defaultValue) const
{
#ifdef DBUS_SUPPORT_ENABLED
    if (mUsePortal) {
        const QString settingName = QStringLiteral("org.kde.kdeglobals.%1").arg(group);
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                              QStringLiteral("/org/freedesktop/portal/desktop"),
                                                              QStringLiteral("org.freedesktop.portal.Settings"),
                                                              QStringLiteral("Read"));
        message << settingName << key;

        // FIXME: async?
        QDBusReply<QVariant> reply = QDBusConnection::sessionBus().call(message);
        if (reply.isValid()) {
            QDBusVariant result = qvariant_cast<QDBusVariant>(reply.value());
            const QString resultStr = result.variant().toString();

            if (!resultStr.isEmpty()) {
                return resultStr;
            }
        }
    }
#endif

    const KConfigGroup configGroup(mKdeGlobals, group);
    return configGroup.readEntry(key, defaultValue);
}
