// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <unordered_set>

#include "GLWidget.h"

#include "GlassWall.h"

static GLsizei numOfSamples = 8;
std::vector<GLWidget *> g_GLWidgets;

GLWidgetSignalEmitter & GLWidgetSignalEmitter::Instance()
{
    static GLWidgetSignalEmitter t;
    return t;
}

struct GLWidget::Impl
{
    std::unordered_set<GLuint> vaos;
};




GLWidget::GLWidget(QWidget * parent)
    : QOpenGLWidget(parent), impl(std::make_unique<Impl>())
{
    QSurfaceFormat format;
    format.setSamples(numOfSamples);
    setFormat(format);
    create();

    g_GLWidgets.push_back(this);

    emit GLWidgetSignalEmitter::Instance().ComingToLife(this);
}

GLWidget::~GLWidget()
{
    makeCurrent();

    auto f = GLFunctions();

    //transparent drawing
    f->glDeleteFramebuffers(1, &m_Tframebuffer1);
    f->glDeleteTextures(1, &m_TColorTexture1);
    f->glDeleteRenderbuffers(1, &m_TDepthRenderbuffer);
    f->glDeleteFramebuffers(1, &m_Tframebuffer2);
    f->glDeleteTextures(1, &m_TColorTexture2);
    f->glDeleteTextures(1, &m_TAlphaTexture2);

    for (auto vao : impl->vaos)
    {
        f->glDeleteVertexArrays(1, &vao);
        assert(f->glGetError() == GL_NO_ERROR);
    }

    doneCurrent();

    auto it_this = std::find(g_GLWidgets.begin(), g_GLWidgets.end(), this);
    assert(it_this != g_GLWidgets.end());
    g_GLWidgets.erase(it_this);

    emit GLWidgetSignalEmitter::Instance().GoingToDie(this);
}

void GLWidget::initializeGL()
{
    auto f = GLFunctions();
    f->glMinSampleShading(1.0f);

    //transparent drawing
    f->glGenFramebuffers(1, &m_Tframebuffer1);
    f->glGenTextures(1, &m_TColorTexture1);
    f->glGenRenderbuffers(1, &m_TDepthRenderbuffer);
    f->glGenFramebuffers(1, &m_Tframebuffer2);
    f->glGenTextures(1, &m_TColorTexture2);
    f->glGenTextures(1, &m_TAlphaTexture2);
    InitializeTFramebuffers();
}

void GLWidget::resizeGL(int width, int height)
{
    auto f = GLFunctions();
	m_width = width; m_height = height;
    f->glViewport(0, 0, width, height);
	
	float aspect = static_cast<float>(width) / height;
	
    float halfHeight = 0.5f, halfWidth = 0.5f;
	if (width > height) halfWidth  *= aspect;
	else                halfHeight /= aspect;
	float nearPlane = 0.0f, farPlane = 1.0f;
    m_projectionMatrix.ortho( -halfWidth , halfWidth ,
                              -halfHeight, halfHeight,
                               nearPlane , farPlane    );
}


OpenGLFunctions *GLWidget::GLFunctions()
{
    return QOpenGLContext::currentContext()->
            versionFunctions<OpenGLFunctions>();
}

void GLWidget::paintGL()
{
    auto f = GLFunctions();

    paintGL_FirstRenderingPass();
    f->glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
    paintGL_SecondRenderingPass();
    f->glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
    paintGL_ThirdRenderingPass();
}

void GLWidget::paintGL_FirstRenderingPass()
{
    auto f = GLFunctions();

    GLfloat clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    static constexpr GLfloat clearDepth = 1.0f;

    f->glBindFramebuffer(GL_FRAMEBUFFER, m_Tframebuffer1);
    f->glClearBufferfv(GL_COLOR, 0,  clearColor);
    f->glClearBufferfv(GL_DEPTH, 0, &clearDepth);

    auto & iter = GlassWallIterator::Instance();
    for (iter.Reset(); !iter.AtEnd(); ++iter) iter->DrawNonTransparent(f);
}

void GLWidget::paintGL_SecondRenderingPass()
{
    auto f = GLFunctions();

    static constexpr GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static constexpr GLfloat clearAlpha = 1.0f;

    f->glBindFramebuffer(GL_FRAMEBUFFER, m_Tframebuffer2);
    f->glClearBufferfv(GL_COLOR, 0,  clearColor);
    f->glClearBufferfv(GL_COLOR, 1, &clearAlpha);
    SetupTTextureProgram();

    auto & iter = GlassWallIterator::Instance();
    for (iter.Reset(); !iter.AtEnd(); ++iter) iter->DrawTransparent(f);

    CleanupAfterTTextureRendering();
}

void GLWidget::paintGL_ThirdRenderingPass()
{
    auto f = GLFunctions();

    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

    ApplyTTextures();
}


void GLWidget::InitializeTFramebuffers()
{
    AllocateTTextures(1, 1);

    auto f = GLFunctions();

    f->glBindFramebuffer(GL_FRAMEBUFFER, m_Tframebuffer1);
    f->glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D_MULTISAMPLE,  m_TColorTexture1, 0
                             );
    f->glFramebufferRenderbuffer(
                GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER, m_TDepthRenderbuffer
                                );

    f->glBindFramebuffer(GL_FRAMEBUFFER, m_Tframebuffer2);
    f->glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D_MULTISAMPLE,  m_TColorTexture2, 0
                             );
    f->glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
            GL_TEXTURE_2D_MULTISAMPLE,  m_TAlphaTexture2, 0
                             );
    GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    f->glDrawBuffers(2, attachments);
    f->glFramebufferRenderbuffer(
                GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER, m_TDepthRenderbuffer
                                );
}

void GLWidget::AllocateTTextures(int w, int h)
{
    auto f = GLFunctions();

    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_TColorTexture1);
    f->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, numOfSamples,
                                GL_RGB16F, w, h, GL_TRUE                 );
    f->glBindRenderbuffer(GL_RENDERBUFFER, m_TDepthRenderbuffer);
    f->glRenderbufferStorageMultisample( GL_RENDERBUFFER, numOfSamples,
                                         GL_DEPTH_COMPONENT, w, h        );
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_TColorTexture2);
    f->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, numOfSamples,
                                GL_RGBA16F, w, h, GL_TRUE                );
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_TAlphaTexture2);
    f->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, numOfSamples,
                                GL_R16F, w, h, GL_TRUE                   );
}

void GLWidget::AllocateTTextures()
{ AllocateTTextures(m_width, m_height); }



void GLWidget::SetupTTextureProgram()
{
    auto f = GLFunctions();
    f->glEnable(GL_DEPTH_TEST); f->glDepthMask(GL_FALSE);
    f->glDepthFunc(GL_LEQUAL);
    f->glEnable(GL_SAMPLE_SHADING);
    f->glDisable(GL_CULL_FACE);

    f->glEnable(GL_BLEND);

    f->glBlendFunci(0, GL_ONE, GL_ONE);
    f->glBlendEquationi(0, GL_FUNC_ADD);

    f->glBlendFunci(1, GL_DST_COLOR, GL_ZERO);
    f->glBlendEquationi(1, GL_FUNC_ADD);

    f->glEnable(GL_MULTISAMPLE);
}

void GLWidget::CleanupAfterTTextureRendering()
{
    auto f = GLFunctions();
    f->glDepthMask(GL_TRUE);
    f->glDisable(GL_BLEND);
    f->glDisable(GL_SAMPLE_SHADING);
}



struct ApplyTTexturesGLResources {
    QOpenGLShaderProgram program;

    explicit ApplyTTexturesGLResources()
    {
        if (!program.addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    "#version 430                                  \n"
                    "const vec2 p[4] = vec2[4]                     \n"
                    "    (   vec2(-1, -1), vec2( 1, -1),           \n"
                    "        vec2( 1,  1), vec2(-1,  1)    );      \n"
                    "void main()                                   \n"
                    "{ gl_Position = vec4(p[gl_VertexID], 0, 1); } \n"
                                            )
           ) assert(false);
        if (!program.addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    "#version 430                                       \n"
                    "out vec4 outColor;                                 \n"
                    "                                                   \n"
                    "layout (location = 0) uniform  sampler2DMS         \n"
                    "                     colorTexture;                 \n"
                    "layout (location = 1) uniform  sampler2DMS         \n"
                    "                    tcolorTexture;                 \n"
                    "layout (location = 2) uniform  sampler2DMS         \n"
                    "                     alphaTexture;                 \n"
                    "                                                   \n"
                    "void main() {                                      \n"
                    "    ivec2 upos = ivec2(gl_FragCoord.xy);           \n"
                    "    vec4  cc    = texelFetch(                      \n"
                    "                  tcolorTexture, upos, gl_SampleID \n"
                    "                            );                     \n"
                    "    vec3 sumOfTColors = cc.rgb;                    \n"
                    "    float sumOfWeights = cc.a;                     \n"
                    "    vec3  color = texelFetch(                      \n"
                    "                   colorTexture, upos, gl_SampleID \n"
                    "                            ).rgb;                 \n"
                    "    if (sumOfWeights == 0)                         \n"
                    "    { outColor = vec4(color, 1.0); return; }       \n"
                    "    float alpha = 1 - texelFetch(                  \n"
                    "                   alphaTexture, upos, gl_SampleID \n"
                    "                                ).r;               \n"
                    "    color = sumOfTColors / sumOfWeights * alpha    \n"
                    "                + color * (1 - alpha);             \n"
                    "    outColor = vec4(color, 1.0);                   \n"
                    "}                                                  \n"
                                            )
           ) assert(false);
        if (!program.link()) assert(false);
    }
};

void GLWidget::ApplyTTextures()
{
    auto f = GLFunctions();
    static ApplyTTexturesGLResources res;

    if (!res.program.bind()) assert(false);

    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_TColorTexture1);
    f->glUniform1i(0, 0);
    f->glActiveTexture(GL_TEXTURE1);
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_TColorTexture2);
    f->glUniform1i(1, 1);
    f->glActiveTexture(GL_TEXTURE2);
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_TAlphaTexture2);
    f->glUniform1i(2, 2);

    f->glEnable(GL_MULTISAMPLE); f->glDisable(GL_DEPTH_TEST);
    f->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}



[[nodiscard]] GLuint GLWidget::GenVAO()
{
    if (!context()->makeCurrent(context()->surface())) throw "wtf???";
    makeCurrent();
    auto f = GLFunctions();

    GLuint ret;
    f->glGenVertexArrays(1, &ret);
    assert(impl->vaos.find(ret) == impl->vaos.end());
    impl->vaos.insert(ret);

    doneCurrent();

    return ret;
}

[[nodiscard]] bool GLWidget::DeleteVAO(GLuint vao)
{
    if (!context()->makeCurrent(context()->surface())) throw "wtf???";
    makeCurrent();
    auto f = GLFunctions();

    auto it = impl->vaos.find(vao);
    bool ret = it != impl->vaos.end();
    if (ret)
    {
        impl->vaos.erase(it);
        f->glDeleteVertexArrays(1, &vao);
        assert(f->glGetError() == GL_NO_ERROR);
    }

    doneCurrent();
    return ret;
}
