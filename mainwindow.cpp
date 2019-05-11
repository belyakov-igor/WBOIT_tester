// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLayout>

#include "GLWidget.h"
#include "GlassWall.h"

MainWindow::MainWindow(QWidget * parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto glay = new QGridLayout(centralWidget());
    centralWidget()->setLayout(glay);

    auto wgt = new GLWidget(this);
    glay->addWidget(wgt, 0, 0);
}

void MainWindow::InitWalls() const
{
    auto & wall = GlassWall::MakeInstance("#1", 0);
    wall.AddTriangle({-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f}, Qt::white, Qt::green);
}

MainWindow::~MainWindow()
{ delete ui; }
