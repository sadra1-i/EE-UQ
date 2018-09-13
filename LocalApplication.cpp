/* *****************************************************************************
Copyright (c) 2016-2017, The Regents of the University of California (Regents).
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.

REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED HEREUNDER IS
PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT,
UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

*************************************************************************** */


// Written: fmckenna

// Purpose: a widget for managing submiited jobs by uqFEM tool
//  - allow for refresh of status, deletion of submitted jobs, and download of results from finished job

#include "LocalApplication.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QJsonObject>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QProcess>

//#include <AgaveInterface.h>
#include <QDebug>
#include <QDir>


LocalApplication::LocalApplication(QWidget *parent)
: Application(parent)
{
    QVBoxLayout *layout = new QVBoxLayout();
    QGridLayout *runLayout = new QGridLayout();

    QLabel *workingDirLabel = new QLabel();
    workingDirLabel->setText(QString("Working Dir Location:"));
    runLayout->addWidget(workingDirLabel,1,0);

    workingDirName = new QLineEdit();
    workingDirName->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    runLayout->addWidget(workingDirName,1,1);

    QLabel *appDirLabel = new QLabel();
    appDirLabel->setText(QString("Application Dir Location:"));
    runLayout->addWidget(appDirLabel,2,0);

    appDirName = new QLineEdit();
    appDirName->setText(QCoreApplication::applicationDirPath());
    runLayout->addWidget(appDirName,2,1);

    QPushButton *pushButton = new QPushButton();
    pushButton->setText("Submit");
    runLayout->addWidget(pushButton,3,1);

   // layout->addWidget(theUQ);
    layout->addLayout(runLayout);

    this->setLayout(layout);

    //
    // set up connections
    //

    connect(pushButton,SIGNAL(clicked()), this, SLOT(onRunButtonPressed()));
}

bool
LocalApplication::outputToJSON(QJsonObject &jsonObject)
{
    jsonObject["localAppDir"]=appDirName->text();
    jsonObject["remoteAppDir"]=appDirName->text();
    jsonObject["workingDir"]=workingDirName->text();

    return true;
}

bool
LocalApplication::inputFromJSON(QJsonObject &dataObject) {

    if (dataObject.contains("localAppDir")) {
        QJsonValue theName = dataObject["localAppDir"];
        appDirName->setText(theName.toString());
    } else
        return false;

    if (dataObject.contains("workingDir")) {
        QJsonValue theName = dataObject["workingDir"];
        workingDirName->setText(theName.toString());
    } else
        return false;

    return true;
}




void
LocalApplication::onRunButtonPressed(void)
{
    int ok = 0;
    QString workingDir = workingDirName->text();
    QDir dirWork(workingDir);
    if (!dirWork.exists())
        if (!dirWork.mkdir(workingDir)) {
            emit sendErrorMessage(QString("Could not create Working Dir: ") + workingDir);
            return;
        }

   QString appDir = appDirName->text();
   QDir dirApp(appDir);
   if (!dirApp.exists()) {
       emit sendErrorMessage(QString("The application directory, ") + appDir +QString(" specified does not exist!"));
       return;
   }

    QString templateDir("templatedir");

    emit setupForRun(workingDir, templateDir);
}


//
// now use the applications Workflow Application EE-UQ.py  to run dakota and produce output files:
//    dakota.in dakota.out dakotaTab.out dakota.err
//

bool
LocalApplication::setupDoneRunApplication(QString &tmpDirectory,QString &inputFile) {

    QString appDir = appDirName->text();

    QString pySCRIPT = appDir +  QDir::separator() + "applications" + QDir::separator() + "Workflow" + QDir::separator() +
            QString("EE-UQ.py");

    QString registryFile = appDir +  QDir::separator() + "applications" + QDir::separator() + "Workflow" + QDir::separator() +
            QString("WorkflowApplications.json");
    qDebug() << pySCRIPT;


    QStringList files;
    files << "dakota.in" << "dakota.out" << "dakotaTab.out" << "dakota.err";

    /************************************************************************
for (int i = 0; i < files.size(); i++) {
    QString copy = files.at(i);
    QFile file(destinationDir + copy);
    file.remove();
}
***********************************************************************/

    //
    // now invoke dakota, done via a python script in tool app dircetory
    //

    QProcess *proc = new QProcess();

#ifdef Q_OS_WIN
    QString command = QString("python ") + pySCRIPT + QString(" ") + tDirectory + QString(" ") + tmpDirectory  + QString(" runningLocal");
    qDebug() << command;
    proc->execute("cmd", QStringList() << "/C" << command);
    //   proc->start("cmd", QStringList(), QIODevice::ReadWrite);

#else
    QString command = QString("source $HOME/.bash_profile; python ") + pySCRIPT + QString(" run ") + inputFile + QString(" ") +
            registryFile;

    proc->execute("bash", QStringList() << "-c" <<  command);

    qInfo() << command;

#endif
    proc->waitForStarted();

    //
    // process the results
    //

    QString filenameOUT = tmpDirectory + QDir::separator() +  QString("dakota.out");
    QString filenameTAB = tmpDirectory + QDir::separator() +  QString("dakotaTab.out");

    emit processResults(filenameOUT, filenameTAB);
    qDebug() << "PROCESSED RESULTS";
}