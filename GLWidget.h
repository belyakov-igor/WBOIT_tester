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
    explicit GLWidget(QWidget * parent);
    ~GLWidget() override;
protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

private:
    static OpenGLFunctions * GLFunctions();

    QMatrix4x4 m_projectionMatrix;
	int m_width = 0, m_height = 0;

    // Transparent drawing ===============================================
    GLuint m_Tframebuffer1      = 0; // Render
    GLuint m_TColorTexture1     = 0; // non-transparent
    GLuint m_TDepthRenderbuffer = 0; // objects here

    GLuint m_Tframebuffer2  = 0; // Render
    GLuint m_TColorTexture2 = 0; // transparent
    GLuint m_TAlphaTexture2 = 0; // objects here

    void InitializeTFramebuffers();
    void AllocateTTextures(int w, int h);
    void AllocateTTextures();
    void SetupTTextureProgram();
    void CleanupAfterTTextureRendering();
    void ApplyTTextures();

    void paintGL_FirstRenderingPass();
    void paintGL_SecondRenderingPass();
    void paintGL_ThirdRenderingPass();
    // /Transparent drawing ==============================================

public:
    [[nodiscard]] GLuint GenVAO();
    [[nodiscard]] bool DeleteVAO(GLuint vao);
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
