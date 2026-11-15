/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcocoabackingstore.h"

#include "qcocoawindow.h"
#include "qcocoahelpers.h"

#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE

QCocoaBackingStore::QCocoaBackingStore(QWindow *window)
    : QRasterBackingStore(window)
{
    // RJVB: backport from Qt 5.12.12, replacing the hardcoded limit set in QCocoaWindow::createNSWindow().
    // Choose an appropriate window depth based on the requested surface format.
    // On deep colour displays the default bit depth is 16-bit, so unless we need
    // that level of precision we opt out of it (and the expensive RGB32 -> RGB64
    // conversions that come with it if our backingstore depth does not match).

    NSWindow *nsWindow = static_cast<QCocoaWindow *>(window->handle())->view().window;
    auto colorSpaceName = NSColorSpaceFromDepth(nsWindow.depthLimit);

    static const int kDefaultBitDepth = 8;
    auto surfaceFormat = window->requestedFormat();
    auto bitsPerSample = qMax(kDefaultBitDepth, qMax(surfaceFormat.redBufferSize(),
        qMax(surfaceFormat.greenBufferSize(), surfaceFormat.blueBufferSize())));

    // NSBestDepth does not seem to guarantee a window depth deep enough for the
    // given bits per sample, even if documented as such. For example, requesting
    // 10 bits per sample will not give us a 16-bit format, even if that's what's
    // available. Work around this by manually bumping the bit depth.
    bitsPerSample = !(bitsPerSample & (bitsPerSample - 1))
        ? bitsPerSample : qNextPowerOfTwo(bitsPerSample);

    auto bestDepth = NSBestDepth(colorSpaceName, bitsPerSample, 0, NO, nullptr);

    // Disable dynamic depth limit, otherwise our depth limit will be overwritten
    // by AppKit if the window moves to a screen with a different depth. We call
    // this before setting the depth limit, as the call will reset the depth to 0.
    [nsWindow setDynamicDepthLimit:NO];

    qDebug() << "Using" << NSBitsPerSampleFromDepth(bestDepth)
        << "bit window depth for" << nsWindow << "with surface format" << surfaceFormat
        << "in colour space" << colorSpaceName;

    nsWindow.depthLimit = bestDepth;
}

QCocoaBackingStore::~QCocoaBackingStore()
{
    if (QCocoaWindow *cocoaWindow = static_cast<QCocoaWindow *>(window()->handle()))
        [qnsview_cast(cocoaWindow->view()) clearBackingStore:this];
}

QImage::Format QCocoaBackingStore::format() const
{
    QCocoaWindow *cocoaWindow = static_cast<QCocoaWindow *>(window()->handle());
    if (cocoaWindow && cocoaWindow->m_drawContentBorderGradient)
        return QImage::Format_ARGB32_Premultiplied;

    return QRasterBackingStore::format();
}

void QCocoaBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    if (m_image.isNull())
        return;

    if (QCocoaWindow *cocoaWindow = static_cast<QCocoaWindow *>(window->handle()))
        [qnsview_cast(cocoaWindow->view()) flushBackingStore:this region:region offset:offset];
}

QT_END_NAMESPACE
