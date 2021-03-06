/*  This file is part of the KDE libraries
 *  Copyright 2013 Aleix Pol Gonzalez <aleixpol@blue-systems.com>
 *  Copyright 2014 Martin Klapetek <mklapetek@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License or ( at
 *  your option ) version 3 or, at the discretion of KDE e.V. ( which shall
 *  act as a proxy as in section 14 of the GPLv3 ), any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kdeplatformfiledialoghelper.h"
#include "kdeplatformfiledialogbase_p.h"
#include "kdirselectdialog_p.h"
#include "platformtheme_logging.h"

#include <kfilefiltercombo.h>
#include <kfilewidget.h>
#include <klocalizedstring.h>
#include <kdiroperator.h>
#include <KUrlComboBox>
#include <KSharedConfig>
#include <KWindowConfig>
#include <KProtocolInfo>
#include <kio_version.h>
#include <KIO/StatJob>
#include <KJobWidgets>

#include <QMimeDatabase>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QWindow>
#include <QTextStream>
#include <QFileDialog>

namespace
{

/*
 * Map a Qt filter string into a KDE one.
 */
static QString qt2KdeFilter(const QStringList &f)
{
    QString               filter;
    QTextStream           str(&filter, QIODevice::WriteOnly);
    QStringList           list(f);
    list.replaceInStrings(QStringLiteral("/"), QStringLiteral("\\/"));
    QStringList::const_iterator it(list.constBegin()), end(list.constEnd());
    bool                  first = true;

    for (; it != end; ++it) {
        int ob = it->lastIndexOf(QLatin1Char('(')),
            cb = it->lastIndexOf(QLatin1Char(')'));

        if (-1 != cb && ob < cb) {
            if (first) {
                first = false;
            } else {
                str << '\n';
            }
            str << it->mid(ob + 1, (cb - ob) - 1) << '|' << it->mid(0, ob);
        }
    }

    return filter;
}

/*
 * Map a KDE filter string into a Qt one.
 */
static QString kde2QtFilter(const QStringList &list, const QString &kde)
{
    QStringList::const_iterator it(list.constBegin()), end(list.constEnd());
    int                   pos;

    for (; it != end; ++it) {
        if (-1 != (pos = it->indexOf(kde)) && pos > 0 &&
                (QLatin1Char('(') == (*it)[pos - 1] || QLatin1Char(' ') == (*it)[pos - 1]) &&
                it->length() >= kde.length() + pos &&
                (QLatin1Char(')') == (*it)[pos + kde.length()] || QLatin1Char(' ') == (*it)[pos + kde.length()])) {
            return *it;
        }
    }
    return QString();
}
}

KDEPlatformFileDialog::KDEPlatformFileDialog()
    : KDEPlatformFileDialogBase()
    , m_fileWidget(new KFileWidget(QUrl(), this))
{
    m_fileWidget->setObjectName(QStringLiteral("KDEPlatformFileDialogFileWidget"));
    setLayout(new QVBoxLayout);
    connect(m_fileWidget, SIGNAL(filterChanged(QString)), SIGNAL(filterSelected(QString)));
    layout()->addWidget(m_fileWidget);

    m_buttons = new QDialogButtonBox(this);
    m_buttons->addButton(m_fileWidget->okButton(), QDialogButtonBox::AcceptRole);
    m_buttons->addButton(m_fileWidget->cancelButton(), QDialogButtonBox::RejectRole);
    connect(m_buttons, SIGNAL(rejected()), m_fileWidget, SLOT(slotCancel()));
    // Also call the cancel function when the dialog is closed via the escape key
    // or titlebar close button to make sure we always save the view config
    connect(this, &KDEPlatformFileDialog::rejected,
            m_fileWidget, &KFileWidget::slotCancel);
    connect(m_fileWidget->okButton(), SIGNAL(clicked(bool)), m_fileWidget, SLOT(slotOk()));
    connect(m_fileWidget, SIGNAL(accepted()), m_fileWidget, SLOT(accept()));
    connect(m_fileWidget, SIGNAL(accepted()), SLOT(accept()));
    connect(m_fileWidget->cancelButton(), SIGNAL(clicked(bool)), SLOT(reject()));
    connect(m_fileWidget->dirOperator(), &KDirOperator::urlEntered, this, &KDEPlatformFileDialogBase::directoryEntered);
    layout()->addWidget(m_buttons);
}

KDEPlatformFileDialog::~KDEPlatformFileDialog()
{
    delete m_fileWidget;
    delete m_buttons;
}

QUrl KDEPlatformFileDialog::directory()
{
    return m_fileWidget->baseUrl();
}

QList<QUrl> KDEPlatformFileDialog::selectedFiles()
{
    return m_fileWidget->selectedUrls();
}

void KDEPlatformFileDialog::selectFile(const QUrl &filename)
{
    QUrl dirUrl = filename.adjusted(QUrl::RemoveFilename);
    m_fileWidget->setUrl(dirUrl);
    m_fileWidget->setSelectedUrl(filename);
}

void KDEPlatformFileDialog::setViewMode(QFileDialogOptions::ViewMode view)
{
    switch (view) {
    case QFileDialogOptions::ViewMode::Detail:
        m_fileWidget->setViewMode(KFile::FileView::Detail);
        break;
    case QFileDialogOptions::ViewMode::List:
        m_fileWidget->setViewMode(KFile::FileView::Simple);
        break;
    default:
        m_fileWidget->setViewMode(KFile::FileView::Default);
        break;
    }
}

void KDEPlatformFileDialog::setFileMode(QFileDialogOptions::FileMode mode)
{
    switch (mode) {
    case QFileDialogOptions::FileMode::AnyFile:
        m_fileWidget->setMode(KFile::File);
        break;
    case QFileDialogOptions::FileMode::ExistingFile:
        m_fileWidget->setMode(KFile::Mode::File | KFile::Mode::ExistingOnly);
        break;
    case QFileDialogOptions::FileMode::Directory:
        m_fileWidget->setMode(KFile::Mode::Directory | KFile::Mode::ExistingOnly);
        break;
    case QFileDialogOptions::FileMode::ExistingFiles:
        m_fileWidget->setMode(KFile::Mode::Files | KFile::Mode::ExistingOnly);
        break;
    default:
        m_fileWidget->setMode(KFile::File);
        break;
    }
}

void KDEPlatformFileDialog::setCustomLabel(QFileDialogOptions::DialogLabel label, const QString &text)
{
    if (label == QFileDialogOptions::Accept) { // OK button
        m_fileWidget->okButton()->setText(text);
    } else if (label == QFileDialogOptions::Reject) { // Cancel button
        m_fileWidget->cancelButton()->setText(text);
    } else if (label == QFileDialogOptions::LookIn) { // Location label
        m_fileWidget->setLocationLabel(text);
    }
}

QString KDEPlatformFileDialog::selectedMimeTypeFilter()
{
    if (m_fileWidget->filterWidget()->isMimeFilter()) {
        const auto mimeTypeFromFilter = QMimeDatabase().mimeTypeForName(m_fileWidget->filterWidget()->currentFilter());
        // If one does not call selectMimeTypeFilter(), KFileFilterCombo::currentFilter() returns invalid mimeTypes,
        // such as "application/json application/zip".
        if (mimeTypeFromFilter.isValid()) {
            return mimeTypeFromFilter.name();
        }
    }

    if (selectedFiles().isEmpty()) {
        return QString();
    }

    // Works for both KFile::File and KFile::Files modes.
    return QMimeDatabase().mimeTypeForUrl(selectedFiles().at(0)).name();
}

QString KDEPlatformFileDialog::selectedNameFilter()
{
    return m_fileWidget->filterWidget()->currentFilter();
}

void KDEPlatformFileDialog::selectMimeTypeFilter(const QString &filter)
{
    m_fileWidget->filterWidget()->setCurrentFilter(filter);
}

void KDEPlatformFileDialog::selectNameFilter(const QString &filter)
{
    m_fileWidget->filterWidget()->setCurrentFilter(filter);
}

void KDEPlatformFileDialog::setDirectory(const QUrl &directory)
{
    if (!directory.isLocalFile())  {
        // Qt can not determine if the remote URL points to a file or a
        // directory, that is why options()->initialDirectory() always returns
        // the full URL.
        KIO::StatJob *job = KIO::stat(directory);
        KJobWidgets::setWindow(job, this);
        if (job->exec()) {
            KIO::UDSEntry entry = job->statResult();
            if (!entry.isDir()) {
                // this is probably a file remove the file part
                m_fileWidget->setUrl(directory.adjusted(QUrl::RemoveFilename));
                m_fileWidget->setSelectedUrl(directory);
            }
            else {
                m_fileWidget->setUrl(directory);
            }
        }
    }
    else {
        m_fileWidget->setUrl(directory);
    }
}

bool KDEPlatformFileDialogHelper::isSupportedUrl(const QUrl& url) const
{
    return KProtocolInfo::protocols().contains(url.scheme());
}

////////////////////////////////////////////////

KDEPlatformFileDialogHelper::KDEPlatformFileDialogHelper()
    : QPlatformFileDialogHelper()
    , m_dialog(new KDEPlatformFileDialog)
{
    connect(m_dialog, SIGNAL(closed()), SLOT(saveSize()));
    connect(m_dialog, SIGNAL(finished(int)), SLOT(saveSize()));
    connect(m_dialog, SIGNAL(currentChanged(QUrl)), SIGNAL(currentChanged(QUrl)));
    connect(m_dialog, SIGNAL(directoryEntered(QUrl)), SIGNAL(directoryEntered(QUrl)));
    connect(m_dialog, SIGNAL(fileSelected(QUrl)), SIGNAL(fileSelected(QUrl)));
    connect(m_dialog, SIGNAL(filesSelected(QList<QUrl>)), SIGNAL(filesSelected(QList<QUrl>)));
    connect(m_dialog, SIGNAL(filterSelected(QString)), SIGNAL(filterSelected(QString)));
    connect(m_dialog, SIGNAL(accepted()), SIGNAL(accept()));
    connect(m_dialog, SIGNAL(rejected()), SIGNAL(reject()));
}

KDEPlatformFileDialogHelper::~KDEPlatformFileDialogHelper()
{
    if (!m_dialogReparented) {
        saveSize();
        if (m_replaced) {
            if (auto layout = m_parentWidget->layout()) {
                if (auto old = layout->replaceWidget(m_dialog, m_replaced)) {
                    qWarning() << Q_FUNC_INFO << old;
                }
            } else {
                qWarning() << Q_FUNC_INFO << "delete" << m_replaced;
                m_replaced->deleteLater();
                m_replaced = nullptr;
            }
        }
        delete m_dialog;
    }
}

void KDEPlatformFileDialogHelper::initializeDialog()
{
    m_dialogInitialized = true;
    if (options()->testOption(QFileDialogOptions::ShowDirsOnly)) {
        m_dialog->deleteLater();
        m_dialog = new KDirSelectDialog(options()->initialDirectory());
        connect(m_dialog, SIGNAL(accepted()), SIGNAL(accept()));
        connect(m_dialog, SIGNAL(rejected()), SIGNAL(reject()));
        if (!options()->windowTitle().isEmpty())
            m_dialog->setWindowTitle(options()->windowTitle());
    } else {
        // needed for accessing m_fileWidget
        KDEPlatformFileDialog *dialog = qobject_cast<KDEPlatformFileDialog*>(m_dialog);
        dialog->m_fileWidget->setOperationMode(options()->acceptMode() == QFileDialogOptions::AcceptOpen ? KFileWidget::Opening : KFileWidget::Saving);
        if (options()->windowTitle().isEmpty()) {
            dialog->setWindowTitle(options()->acceptMode() == QFileDialogOptions::AcceptOpen ? i18nc("@title:window", "Open File") : i18nc("@title:window", "Save File"));
        } else {
            dialog->setWindowTitle(options()->windowTitle());
        }
        if (!m_directorySet) {
            setDirectory(options()->initialDirectory());
        }
        //dialog->setViewMode(options()->viewMode()); // don't override our options, fixes remembering the chosen view mode and sizes!
        dialog->setFileMode(options()->fileMode());

        // custom labels
        if (options()->isLabelExplicitlySet(QFileDialogOptions::Accept)) { // OK button
            dialog->setCustomLabel(QFileDialogOptions::Accept, options()->labelText(QFileDialogOptions::Accept));
        } else if (options()->isLabelExplicitlySet(QFileDialogOptions::Reject)) { // Cancel button
            dialog->setCustomLabel(QFileDialogOptions::Reject, options()->labelText(QFileDialogOptions::Reject));
        } else if (options()->isLabelExplicitlySet(QFileDialogOptions::LookIn)) { // Location label
            dialog->setCustomLabel(QFileDialogOptions::LookIn, options()->labelText(QFileDialogOptions::LookIn));
        }

        const QStringList mimeFilters = options()->mimeTypeFilters();
        const QStringList nameFilters = options()->nameFilters();
        if (!mimeFilters.isEmpty()) {
            QString defaultMimeFilter;
            if (options()->acceptMode() == QFileDialogOptions::AcceptSave) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
                defaultMimeFilter = options()->initiallySelectedMimeTypeFilter();
#endif
                if (defaultMimeFilter.isEmpty()) {
                    defaultMimeFilter = mimeFilters.at(0);
                }
            }
            dialog->m_fileWidget->setMimeFilter(mimeFilters, defaultMimeFilter);

            if ( mimeFilters.contains( QStringLiteral("inode/directory") ) )
                dialog->m_fileWidget->setMode( dialog->m_fileWidget->mode() | KFile::Directory );
        } else if (!nameFilters.isEmpty()) {
            dialog->m_fileWidget->setFilter(qt2KdeFilter(nameFilters));
        }

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        if (!options()->initiallySelectedMimeTypeFilter().isEmpty()) {
            selectMimeTypeFilter(options()->initiallySelectedMimeTypeFilter());
        } else if (!options()->initiallySelectedNameFilter().isEmpty()) {
            selectNameFilter(options()->initiallySelectedNameFilter());
        }
#else
        if (!options()->initiallySelectedNameFilter().isEmpty()) {
            selectNameFilter(options()->initiallySelectedNameFilter());
        }
#endif

        // overwrite option
        // TODO: figure out how to avoid a native "OK to overwrite" request followed by one from KDE (mod in KIO??)
        if (options()->testOption(QFileDialogOptions::FileDialogOption::DontConfirmOverwrite)) {
            dialog->m_fileWidget->setConfirmOverwrite(false);
         } else if (options()->acceptMode() == QFileDialogOptions::AcceptSave) {
             dialog->m_fileWidget->setConfirmOverwrite(true);
        }

#if KIO_VERSION >= QT_VERSION_CHECK(5, 43, 0)
        dialog->m_fileWidget->setSupportedSchemes(options()->supportedSchemes());
#endif
    }
}

void KDEPlatformFileDialogHelper::exec()
{
    restoreSize();
    // KDEPlatformFileDialog::show() will always be called during QFileDialog::exec()
    // discard the delayed show() it and use exec() and it will call show() for us.
    // We can't hide and show it here because of https://bugreports.qt.io/browse/QTBUG-48248
    m_dialog->discardDelayedShow();
    m_dialog->exec();
}

void KDEPlatformFileDialogHelper::hide()
{
    m_dialog->discardDelayedShow();
    m_dialog->hide();
}

void KDEPlatformFileDialogHelper::saveSize()
{
    KSharedConfig::Ptr conf = KSharedConfig::openConfig();
    KConfigGroup group = conf->group("FileDialogSize");
    KWindowConfig::saveWindowSize(m_dialog->windowHandle(), group);
}

void KDEPlatformFileDialogHelper::restoreSize()
{
    m_dialog->winId(); // ensure there's a window created
    KSharedConfig::Ptr conf = KSharedConfig::openConfig();
    KWindowConfig::restoreWindowSize(m_dialog->windowHandle(), conf->group("FileDialogSize"));
    // NOTICE: QWindow::setGeometry() does NOT impact the backing QWidget geometry even if the platform
    // window was created -> QTBUG-40584. We therefore copy the size here.
    // TODO: remove once this was resolved in QWidget QPA
    m_dialog->resize(m_dialog->windowHandle()->size());
}

bool KDEPlatformFileDialogHelper::show(Qt::WindowFlags windowFlags, Qt::WindowModality windowModality, QWindow *parent)
{
    initializeDialog();
    m_parentWidget = parent ? QWidget::find(parent->winId()) : nullptr;
    if (m_parentWidget) {
        const auto fDialogs = m_parentWidget->findChildren<QFileDialog*>();
        if (qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME_VERBOSE")) {
            qCWarning(PLATFORMTHEME) << Q_FUNC_INFO << this << windowFlags << m_dialog << parent << m_parentWidget;
            qCWarning(PLATFORMTHEME) << "\tQFileDialogInstance property:" << m_parentWidget->property("QFileDialogInstance");
            qCWarning(PLATFORMTHEME) << "\tQFileDialog children:" << fDialogs;
        }
        if (!fDialogs.isEmpty()) {
            if (auto fDialog = fDialogs.at(0)) {
                if (auto layout = m_parentWidget->layout()) {
                    restoreSize();
                    if (m_dialog->windowHandle()) {
                        fDialog->resize(m_dialog->windowHandle()->size());
                    }
                    if (auto old = layout->replaceWidget(fDialog, m_dialog)) {
                        m_replaced = fDialog;
                        if (qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME_VERBOSE") || fDialogs.size() > 1) {
                            qCWarning(PLATFORMTHEME) << "\treplaced" << old->widget() << "with" << m_dialog;
                        }
                    }
                }
//                 m_dialogReparented = true;
//                 m_dialog->setParent(m_parentWidget);
                windowFlags = fDialog->windowFlags();
                windowModality = fDialog->windowModality();
            }
        }
    }
    m_dialog->setWindowFlags(windowFlags);
    m_dialog->setWindowModality(windowModality);
    if (!m_replaced) {
        restoreSize();
        m_dialog->windowHandle()->setTransientParent(parent);
    }
    // Use a delayed show here to delay show() after the internal Qt invisible QDialog.
    // The delayed call shouldn't matter, because for other "real" native QPlatformDialog
    // implementation like Mac and Windows, the native dialog is not necessarily
    // show up immediately.
    m_dialog->delayedShow();
    return true;
}

QList<QUrl> KDEPlatformFileDialogHelper::selectedFiles() const
{
    return m_dialog->selectedFiles();
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
QString KDEPlatformFileDialogHelper::selectedMimeTypeFilter() const
{
    return m_dialog->selectedMimeTypeFilter();
}

void KDEPlatformFileDialogHelper::selectMimeTypeFilter(const QString &filter)
{
    m_dialog->selectMimeTypeFilter(filter);
}
#endif

QString KDEPlatformFileDialogHelper::selectedNameFilter() const
{
    return kde2QtFilter(options()->nameFilters(), m_dialog->selectedNameFilter());
}

QUrl KDEPlatformFileDialogHelper::directory() const
{
    return m_dialog->directory();
}

void KDEPlatformFileDialogHelper::selectFile(const QUrl &filename)
{
    // This is called once by QFileDialogPrivate::init -> QFileDialog::selectUrl -> QFileDialogPrivate::selectFile_sys
    // and then again by selectFile in the QFileDialog constructor, with a wrong value for remote URLs, until the Qt 5.11.2 fix.
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    if (m_fileSelected && !m_dialogInitialized)
        return;
#endif
    m_dialog->selectFile(filename);

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 1)
    // Qt 5 at least <= 5.8.0 does not derive the directory from the passed url
    // and set the initialDirectory option accordingly, also not for known schemes
    // like file://, so we have to do it ourselves
    options()->setInitialDirectory(m_dialog->directory());
#endif
    m_fileSelected = true;
}

void KDEPlatformFileDialogHelper::setDirectory(const QUrl &directory)
{
    if (!directory.isEmpty()) {
        m_dialog->setDirectory(directory);
        m_directorySet = true;
    }
}

void KDEPlatformFileDialogHelper::selectNameFilter(const QString &filter)
{
    m_dialog->selectNameFilter(qt2KdeFilter(QStringList(filter)));
}

void KDEPlatformFileDialogHelper::setFilter()
{
}

bool KDEPlatformFileDialogHelper::defaultNameFilterDisables() const
{
    return false;
}
