// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLayout>
#include <QSlider>

#include "GLWidget.h"
#include "GlassWall.h"

struct MainWindow::Impl
{
    Ui::MainWindow *ui = new Ui::MainWindow;
    ~Impl() { delete ui; }
    GLWidget * wgt_WBOIT = nullptr, * wgt_CODB = nullptr;
};

MainWindow::MainWindow(QWidget * parent) :
    QMainWindow(parent), impl(std::make_unique<Impl>())
{
    impl->ui->setupUi(this);

    auto glay = new QGridLayout(centralWidget());
    centralWidget()->setLayout(glay);

    impl->wgt_WBOIT = new GLWidget(this);
    glay->addWidget(impl->wgt_WBOIT, 0, 0);
    impl->wgt_CODB = new GLWidget(this);
    glay->addWidget(impl->wgt_CODB, 0, 1);
    auto slider = new QSlider(Qt::Horizontal, this);
    glay->addWidget(slider, 1, 0, 1, 2);

    static constexpr int max = 1000;
    slider->setRange(0, max);
    slider->setValue(0);

    connect( slider, &QSlider::valueChanged, this,
             [this, max = max](int val)
             { this->UpdateWalls(static_cast<float>(val) / max); }
           );
}

MainWindow::~MainWindow() = default;

QVector2D Mult(QVector2D vec, QMatrix2x2 mat)
{
    return { mat(0, 0) * vec.x() + mat(0, 1) * vec.y(),
             mat(1, 0) * vec.x() + mat(1, 1) * vec.y()  };
}

void MainWindow::InitWalls() const
{
    {
        static constexpr QVector2D a{-0.1f, -0.6f}, b{0.1f, -0.6f}, c{0.0f, -0.9f};
        static constexpr QVector2D d{-0.1f, -0.9f}, e{0.1f, -0.9f}, f{0.0f, -0.6f};
        QColor clrs [] = { QColor(255, 0, 0), QColor(255, 130, 0), QColor(255, 220, 0),
                           QColor(0, 255, 0), QColor(0, 200, 255), QColor(0, 100, 255),
                           QColor(100, 0, 255), QColor(220, 0, 200) };

        static constexpr uint8_t count = 8;
        static const float angleStep = 2 * g_pi_f / count;
        static const float rotdata [] = {  std::cos(angleStep), std::sin(angleStep),
                                          -std::sin(angleStep), std::cos(angleStep)  };
        static const QMatrix2x2 rot(rotdata);
        QMatrix2x2 rot_t{};

        auto & wall1 = GlassWall::MakeInstance(0, 0.5f, true);
        auto & wall2 = GlassWall::MakeInstance(1, 0.5f, true);
        for (auto i = 0; i < count; (++i), (rot_t = rot * rot_t))
        {
            wall1.AddTriangle( Mult(a, rot_t), Mult(b, rot_t), Mult(c, rot_t),
                               Qt::white, clrs[i]                              );
            wall2.AddTriangle( Mult(d, rot_t), Mult(e, rot_t), Mult(f, rot_t),
                               Qt::white, clrs[i]                              );
        }
    }
    {
        static constexpr QVector2D a{-0.9f, -0.034f}, b{-0.9f, 0.04f}, c{0.0f, 0.0f};
        QColor clrs [] = { QColor(80, 150, 120), QColor(100, 130, 45), QColor(170, 100, 40),
                           QColor(40, 110, 145), QColor(140, 50, 50), QColor(50, 145, 145),
                           QColor(80, 65, 130), QColor(125, 30, 130) };

        static constexpr uint8_t count = 8;
        static const float angleStep = 2 * g_pi_f / count;
        static const float rotdata [] = {  std::cos(angleStep), std::sin(angleStep),
                                          -std::sin(angleStep), std::cos(angleStep)  };
        static const QMatrix2x2 rot(rotdata);
        QMatrix2x2 rot_t{};

        auto & wall1 = GlassWall::MakeInstance(2, 0.5f, true);
        for (auto i = 0; i < count; (++i), (rot_t = rot * rot_t))
        {
            wall1.AddTriangle( Mult(a, rot_t), Mult(b, rot_t), Mult(c, rot_t),
                               Qt::yellow, clrs[i]                             );
        }
    }
    {
        static constexpr QVector2D a{-1.0f, 0.0f}, b{0.0f, 0.0f}, c{0.0f, 1.0f};
        QColor clrs [] = { QColor(255, 255, 255), QColor(0, 255, 255),
                           QColor(127, 127, 127), QColor(35, 35, 100)  };

        static constexpr uint8_t count = 4;
        static const float angleStep = 2 * g_pi_f / count;
        static const float rotdata [] = {  std::cos(angleStep), std::sin(angleStep),
                                          -std::sin(angleStep), std::cos(angleStep)  };
        static const QMatrix2x2 rot(rotdata);
        QMatrix2x2 rot_t{};

        auto & wall1 = GlassWall::MakeInstance(3, 0.5f, true);
        for (auto i = 0; i < count; (++i), (rot_t = rot * rot_t))
        {
            wall1.AddTriangle( Mult(a, rot_t), Mult(b, rot_t), Mult(c, rot_t),
                               clrs[i], clrs[i]                                );
        }
    }
}

void MainWindow::UpdateWalls(float p) const
{
    float angle = 2 * g_pi_f * p;
    auto cos  = std::cos(angle    ), sin  = std::sin(angle    );
    auto cos2 = std::cos(angle / 2), sin2 = std::sin(angle / 2);
    float trData1 [] = {  cos ,  sin , 0,
                         -sin ,  cos , 0,
                            0 ,    0 , 1  };
    float trData2 [] = {  cos , -sin , 0,
                          sin ,  cos , 0,
                            0 ,    0 , 1  };
    float trData3 [] = {  cos2, -sin2, 0,
                          sin2,  cos2, 0,
                            0 ,    0 , 1  };
    auto & wall1 = GlassWall::FindInstance(0);
    wall1.Transformation(QMatrix3x3(trData1));
    auto & wall2 = GlassWall::FindInstance(1);
    wall2.Transformation(QMatrix3x3(trData2));
    auto & wall3 = GlassWall::FindInstance(2);
    wall3.Transformation(QMatrix3x3(trData3));
    impl->wgt_WBOIT->update(); impl->wgt_CODB->update();
}
