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

QVector2D Mult(const QVector2D & vec, const QMatrix2x2 & mat)
{
    return { mat(0, 0) * vec.x() + mat(0, 1) * vec.y(),
             mat(1, 0) * vec.x() + mat(1, 1) * vec.y()  };
}

void MainWindow::InitWalls() const
{
    static constexpr uint8_t count = 8;

    static constexpr QVector2D a{-0.1f, -0.8f}, b{0.1f, -0.8f}, c{0.0f, -0.5f};
    static const float angleStep = 2 * g_pi_f / count;
    static const float rotdata [] = {  std::cos(angleStep), std::sin(angleStep),
                                      -std::sin(angleStep), std::cos(angleStep)  };
    static const QMatrix2x2 rot(rotdata);
    QMatrix2x2 rot_t{};

    auto & wall = GlassWall::MakeInstance("#1", 0);
    for (auto i = 0; i < count; (++i), (rot_t = rot * rot_t))
        wall.AddTriangle( Mult(a, rot_t), Mult(b, rot_t), Mult(c, rot_t),
                          Qt::white, Qt::green                            );
}

MainWindow::~MainWindow()
{ delete ui; }
