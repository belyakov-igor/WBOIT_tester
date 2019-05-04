// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLayout>

#include "GLWidget.h"

MainWindow::MainWindow(QWidget * parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto glay = new QGridLayout(this);
    centralWidget()->setLayout(glay);

    auto wgt = new GLWidget(this);
    glay->addWidget(wgt, 0, 0);
}

MainWindow::~MainWindow()
{ delete ui; }
