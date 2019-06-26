// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLayout>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>

#include "GLWidget.h"
#include "GlassWall.h"

struct MainWindow::Impl
{
    ~Impl() { delete ui; }

    Ui::MainWindow *ui = new Ui::MainWindow;
    GLWidget * wgt_WBOIT = nullptr, * wgt_CODB = nullptr;

    QWidget * settingsBoard = nullptr;
    QHBoxLayout * settingsBoardLayout = nullptr;

    void ArrangeWallSettings();
    void UpdateWidgets();
};

void MainWindow::Impl::ArrangeWallSettings()
{
    assert(settingsBoard && settingsBoardLayout);
    std::vector<QObject *> objs;
    for (auto obj : settingsBoard->children()) objs.push_back(obj);
    for (auto obj : objs)
        if (!obj->metaObject()->inherits(&QLayout::staticMetaObject))
            delete obj;

    std::vector<QWidget *> settingsWidgets;
    settingsWidgets.reserve(GlassWall::CountOfInstances());
    auto & iter = GlassWallIterator::Instance();
    for (iter.Reset(); !iter.AtEnd(); ++iter)
    {
        auto wgt = settingsWidgets.emplace_back(new QWidget(settingsBoard));
        auto glay = new QGridLayout(wgt); wgt->setLayout(glay);

        auto blackWall = new QLabel(wgt);
        glay->addWidget(blackWall, 0, 0, 5, 1);
        auto pmap = QPixmap(2, 110); pmap.fill(Qt::black);
        blackWall->setPixmap(std::move(pmap));

        auto lbl = new QLabel( QString(QStringLiteral("Depth level %1"))
                               .arg(iter->DepthLevel())                 , wgt );
        glay->addWidget(lbl, 0, 1, Qt::AlignHCenter);

        auto vcbx = new QCheckBox(QStringLiteral("Visible"), wgt);
        glay->addWidget(vcbx, 1, 1);
        vcbx->setChecked(iter->Visible());
        connect( vcbx, &QCheckBox::toggled, wgt,
                 [&wall = *iter, this](bool checked)//clazy:exclude=lambda-in-connect
                 { wall.Visible(checked); UpdateWidgets(); } );

        auto tcbx = new QCheckBox(QStringLiteral("Transparent"), wgt);
        glay->addWidget(tcbx, 2, 1);
        tcbx->setChecked(iter->Transparent());
        connect( tcbx, &QCheckBox::toggled, wgt,
                 [&wall = *iter, this](bool checked)//clazy:exclude=lambda-in-connect
                 { wall.Transparent(checked); UpdateWidgets(); } );

        auto olbl = new QLabel(QStringLiteral("Opacity:"), wgt);
        glay->addWidget(olbl, 3, 1);

        auto oslider = new QSlider(Qt::Orientation::Horizontal, wgt);
        glay->addWidget(oslider, 4, 1);
        static constexpr int smax = 1000;
        oslider->setRange(0, smax);
        oslider->setValue(static_cast<int>(iter->Opacity() * smax));
        connect( oslider, &QSlider::valueChanged, wgt,
                 [&wall = *iter, this](int value)//clazy:exclude=lambda-in-connect
                 { wall.Opacity(static_cast<float>(value) / smax); UpdateWidgets(); } );
    }

    for (auto it = settingsWidgets.rbegin(); it != settingsWidgets.rend(); ++it)
        settingsBoardLayout->addWidget(*it);
}

void MainWindow::Impl::UpdateWidgets() { wgt_WBOIT->update(); wgt_CODB->update(); }

MainWindow::MainWindow(QWidget * parent) :
    QMainWindow(parent), impl(std::make_unique<Impl>())
{
    impl->ui->setupUi(this);

    auto glay = new QGridLayout(centralWidget());
    centralWidget()->setLayout(glay);

    auto sbparent = new QWidget(this);
    glay->addWidget(sbparent, 0, 0, 1, 2);
    {
        auto hlay = new QHBoxLayout(sbparent);
        sbparent->setLayout(hlay);
        auto lbl = new QLabel(sbparent);
        hlay->addWidget(lbl);
        lbl->setPixmap(QPixmap(":/Res/Eye.png"));
        impl->settingsBoard = new QWidget(sbparent);
        hlay->addWidget(impl->settingsBoard);
        impl->settingsBoardLayout = new QHBoxLayout(impl->settingsBoard);
        impl->settingsBoard->setLayout(impl->settingsBoardLayout);
    }

    auto lbl = new QLabel(QStringLiteral(
                              "Weighted blended order-independent transparency"
                                        ), this);
    lbl->setMinimumWidth(300); lbl->setAlignment(Qt::AlignHCenter);
    glay->addWidget(lbl, 1, 0);
    impl->wgt_WBOIT = new GLWidget(GLWidget::RenderStrategyEnum::WBOIT, this);
    glay->addWidget(impl->wgt_WBOIT, 2, 0);

    lbl = new QLabel(QStringLiteral("Classic order-dependent blending"), this);
    lbl->setMinimumWidth(300); lbl->setAlignment(Qt::AlignHCenter);
    glay->addWidget(lbl, 1, 1);
    impl->wgt_CODB  = new GLWidget(GLWidget::RenderStrategyEnum::CODB , this);
    glay->addWidget(impl->wgt_CODB, 2, 1);

    glay->setRowStretch(2, 1);
    auto slider = new QSlider(Qt::Horizontal, this);
    glay->addWidget(slider, 3, 0, 1, 2);

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
        QColor clrs [] = { QColor(111, 0, 0), QColor(111, 25, 0), QColor(111, 80, 0),
                           QColor(0, 111, 0), QColor(0, 65, 111), QColor(0, 14, 111),
                           QColor(14, 0, 111), QColor(80, 0, 65) };

        static constexpr uint8_t count = 8;
        static const float angleStep = 2 * g_pi_f / count;
        static const float rotdata [] = {  std::cos(angleStep), std::sin(angleStep),
                                          -std::sin(angleStep), std::cos(angleStep)  };
        static const QMatrix2x2 rot(rotdata);
        QMatrix2x2 rot_t{};

        auto & wall1 = GlassWall::MakeInstance(0, 0.5f, true, true);
        auto & wall2 = GlassWall::MakeInstance(1, 0.5f, true, true);
        for (auto i = 0; i < count; (++i), (rot_t = rot * rot_t))
        {
            wall1.AddTriangle( Mult(a, rot_t), Mult(b, rot_t), Mult(c, rot_t),
                               QColor(111, 111, 111), clrs[i]                  );
            wall2.AddTriangle( Mult(d, rot_t), Mult(e, rot_t), Mult(f, rot_t),
                               QColor(111, 111, 111), clrs[i]                  );
        }
    }
    {
        static constexpr QVector2D a{-0.9f, -0.04f}, b{-0.9f, 0.04f}, c{0.0f, 0.0f};
        QColor clrs [] = { QColor(9, 35, 21), QColor(14, 25, 45), QColor(45, 14, 2),
                           QColor(2, 17, 32), QColor(30, 3, 3), QColor(3, 32, 32),
                           QColor(9, 5, 1), QColor(125, 1, 1) };

        static constexpr uint8_t count = 8;
        static const float angleStep = 2 * g_pi_f / count;
        static const float rotdata [] = {  std::cos(angleStep), std::sin(angleStep),
                                          -std::sin(angleStep), std::cos(angleStep)  };
        static const QMatrix2x2 rot(rotdata);
        QMatrix2x2 rot_t{};

        auto & wall1 = GlassWall::MakeInstance(2, 0.5f, true, true);
        for (auto i = 0; i < count; (++i), (rot_t = rot * rot_t))
        {
            wall1.AddTriangle( Mult(a, rot_t), Mult(b, rot_t), Mult(c, rot_t),
                               QColor(111, 111, 0), clrs[i]                    );
        }
    }
    {
        static constexpr QVector2D a{-1.0f, 0.0f}, b{0.0f, 0.0f}, c{0.0f, 1.0f};
        QColor clrs [] = { QColor(111, 111, 111), QColor(0, 111, 111),
                           QColor(24, 24, 24), QColor(2, 2, 14)  };

        static constexpr uint8_t count = 4;
        static const float angleStep = 2 * g_pi_f / count;
        static const float rotdata [] = {  std::cos(angleStep), std::sin(angleStep),
                                          -std::sin(angleStep), std::cos(angleStep)  };
        static const QMatrix2x2 rot(rotdata);
        QMatrix2x2 rot_t{};

        auto & wall1 = GlassWall::MakeInstance(3, 0.5f, true, true);
        for (auto i = 0; i < count; (++i), (rot_t = rot * rot_t))
        {
            wall1.AddTriangle( Mult(a, rot_t), Mult(b, rot_t), Mult(c, rot_t),
                               clrs[i], clrs[i]                                );
        }
    }
    impl->ArrangeWallSettings();
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
    impl->UpdateWidgets();
}
