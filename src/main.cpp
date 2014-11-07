/*
    Copyright (C) 2014 by Project Tox <https://tox.im>

    This file is part of qTox, a Qt-based graphical interface for Tox.

    This program is libre software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the COPYING file for more details.
*/

#include "widget/widget.h"
#include "misc/settings.h"
#include "src/ipc.h"
#include "src/widget/toxuri.h"
#include <QApplication>
#include <QFontDatabase>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QMutexLocker>

#ifdef LOG_TO_FILE
static QtMessageHandler dflt;
static QTextStream* logFile {nullptr};
static QMutex mutex;

void myMessageHandler(QtMsgType type, const QMessageLogContext& ctxt, const QString& msg)
{
    if (!logFile)
        return;

    // Silence qWarning spam due to bug in QTextBrowser (trying to open a file for base64 images)
    if (ctxt.function == QString("virtual bool QFSFileEngine::open(QIODevice::OpenMode)")
            && msg == QString("QFSFileEngine::open: No file name specified"))
        return;

    dflt(type, ctxt, msg); // this must be thread safe, otherwise qDebug() would never ever work
    QMutexLocker locker(&mutex);
    *logFile << QTime::currentTime().toString("HH:mm:ss' '") << msg << '\n';
    logFile->flush();
}
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("qTox");
    a.setOrganizationName("Tox");

#ifdef LOG_TO_FILE
    logFile = new QTextStream;
    dflt = qInstallMessageHandler(nullptr);
    QFile logfile(QDir(Settings::getSettingsDirPath()).filePath("qtox.log"));
    if (logfile.open(QIODevice::Append))
    {
        logFile->setDevice(&logfile);
        *logFile << QDateTime::currentDateTime().toString("\nyyyy-dd-MM HH:mm:ss' file logger starting\n'");
        qInstallMessageHandler(myMessageHandler);
    }
    else
    {
        fprintf(stderr, "Couldn't open log file!!!\n");
        delete logFile;
        logFile = nullptr;
    }
#endif

    // Windows platform plugins DLL hell fix
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
    a.addLibraryPath("platforms");
    
    qDebug() << "built on: " << __TIME__ << __DATE__;
    qDebug() << "commit: " << GIT_VERSION << "\n";

    // Install Unicode 6.1 supporting font
    QFontDatabase::addApplicationFont("://DejaVuSans.ttf");

    // Inter-process communication
    IPC ipc;
    ipc.registerEventHandler(&toxURIEventHandler);

    // Process arguments
    if (argc >= 2)
    {
        QString firstParam(argv[1]);
        // Tox URIs. If there's already another qTox instance running, we ask it to handle the URI and we exit
        // Otherwise we start a new qTox instance and process it ourselves
        if (firstParam.startsWith("tox:"))
        {
            if (ipc.isCurrentOwner()) // Don't bother sending an event if we're going to process it ourselves
            {
                handleToxURI(firstParam.toUtf8());
            }
            else
            {
                time_t event = ipc.postEvent(firstParam.toUtf8());
                ipc.waitUntilProcessed(event);
                // If someone else processed it, we're done here, no need to actually start qTox
                if (!ipc.isCurrentOwner())
                    return EXIT_SUCCESS;
            }
        }
    }

    // Run
    Widget* w = Widget::getInstance();
    int errorcode = a.exec();

    delete w;
#ifdef LOG_TO_FILE
    delete logFile;
    logFile = nullptr;
#endif

    return errorcode;
}
