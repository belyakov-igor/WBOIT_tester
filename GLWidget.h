#ifndef GLWidget_H
#define GLWidget_H

#include <QOpenGLWidget>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>

#include "GLDrawingFacilities.h"

class GLWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    enum class TransparentRenderStrategyEnum { WBOIT, CODB };
    explicit GLWidget(TransparentRenderStrategyEnum strategy, QWidget * parent);
    ~GLWidget() override;

    [[nodiscard]] GLuint GenVAO();
    [[nodiscard]] bool DeleteVAO(GLuint vao);
protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

extern std::vector<GLWidget *> g_GLWidgets;

class GLWidgetSignalEmitter : public QObject
{
    Q_OBJECT
    explicit GLWidgetSignalEmitter() = default;
public:
    static GLWidgetSignalEmitter & Instance();
signals:
    void GoingToDie(GLWidget *);
    void ComingToLife(GLWidget *);
};

#endif // GLWidget_H
