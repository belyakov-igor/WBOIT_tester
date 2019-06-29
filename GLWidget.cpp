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
    explicit Impl(RenderStrategyEnum s)
        : trs(    s == RenderStrategyEnum::WBOIT
                  ? std::unique_ptr<RenderStrategy>(
                        std::make_unique<WBOITRenderStrategy>(*this)
                                                   )
                  : s == RenderStrategyEnum::CODB
                    ? std::unique_ptr<RenderStrategy>(
                          std::make_unique<CODBRenderStrategy>(*this)
                                                     )
                    : s == RenderStrategyEnum::Additive
                      ? std::unique_ptr<RenderStrategy>(
                            std::make_unique<AdditiveRenderStrategy>(*this)
                                                       )
                      : ( assert(s == RenderStrategyEnum::AdditiveEP),
                          std::unique_ptr<RenderStrategy>(
                              std::make_unique<AdditiveEPRenderStrategy>(*this)
                                                         )
                        )
             ){}

    std::unordered_set<GLuint> vaos;

    int width = 0, height = 0;
    QMatrix3x3 projMat;

    static OpenGLFunctions * GLFunctions();

    void RenderNonTransparent() const;

    struct RenderStrategy
    {
        explicit RenderStrategy(Impl & impl_) : impl(impl_) {}
        virtual ~RenderStrategy() = default;

        virtual void GenGLResources() = 0;
        virtual void DeleteGLResources() = 0;
        virtual void Render(GLuint defaultFBO) const = 0;

        virtual void ReallocateFramebufferStorages(int w, int h) = 0;
        void ReallocateFramebufferStorages()
        { ReallocateFramebufferStorages(impl.width, impl.height); }
    protected:
        Impl & impl;
    };
    std::unique_ptr<RenderStrategy> trs;

    struct WBOITRenderStrategy : RenderStrategy
    {
        explicit WBOITRenderStrategy(Impl & impl_) : RenderStrategy(impl_) {}

        GLuint framebufferNT = 0, colorTextureNT = 0, depthRenderbuffer = 0;
        GLuint framebuffer = 0, colorTexture = 0, alphaTexture = 0;

        void GenGLResources() override;
        void DeleteGLResources() override;
        void ReallocateFramebufferStorages(int w, int h) override;
        void Render(GLuint defaultFBO) const override;

        void PrepareToTransparentRendering() const;
        void CleanupAfterTransparentRendering() const;
        void ApplyTextures() const;
    };

    struct CODBRenderStrategy : RenderStrategy
    {
        explicit CODBRenderStrategy(Impl & impl_) : RenderStrategy(impl_) {}

        void GenGLResources() override {}
        void DeleteGLResources() override {}
        void ReallocateFramebufferStorages(int, int) override {}
        void Render(GLuint defaultFBO) const override;

        void PrepareToTransparentRendering() const;
        void CleanupAfterTransparentRendering() const;
    };

    struct AdditiveRenderStrategy : RenderStrategy
    {
        explicit AdditiveRenderStrategy(Impl & impl_) : RenderStrategy(impl_) {}

        void GenGLResources() override {}
        void DeleteGLResources() override {}
        void ReallocateFramebufferStorages(int, int) override {}
        void Render(GLuint defaultFBO) const override;

        void PrepareToTransparentRendering() const;
        void CleanupAfterTransparentRendering() const;
    };

    struct AdditiveEPRenderStrategy : RenderStrategy // exposition in posprocessing
    {
        explicit AdditiveEPRenderStrategy(Impl & impl_) : RenderStrategy(impl_) {}

        GLuint framebuffer = 0, colorTexture = 0, depthRenderbuffer = 0;

        void GenGLResources() override;
        void DeleteGLResources() override;
        void ReallocateFramebufferStorages(int w, int h) override;
        void Render(GLuint defaultFBO) const override;

        void PrepareToTransparentRendering() const;
        void CleanupAfterTransparentRendering() const;
        void ApplyTextures() const;
    };
};





GLWidget::GLWidget(RenderStrategyEnum strategy, QWidget * parent)
    : QOpenGLWidget(parent), impl(std::make_unique<Impl>(strategy))
{
    QSurfaceFormat format;
    format.setMajorVersion(4); format.setMinorVersion(5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(numOfSamples);
    format.setColorSpace(QSurfaceFormat::sRGBColorSpace);
    setFormat(format);
    setTextureFormat(GL_SRGB8_ALPHA8);
    create();

    g_GLWidgets.push_back(this);

    emit GLWidgetSignalEmitter::Instance().ComingToLife(this);
}

GLWidget::~GLWidget()
{
    makeCurrent();

    impl->trs->DeleteGLResources();

    auto f = Impl::GLFunctions();
    for (auto vao : impl->vaos) { f->glDeleteVertexArrays(1, &vao);
                                  assert(f->glGetError() == GL_NO_ERROR); }

    doneCurrent();

    auto it_this = std::find(g_GLWidgets.begin(), g_GLWidgets.end(), this);
    assert(it_this != g_GLWidgets.end());
    g_GLWidgets.erase(it_this);

    emit GLWidgetSignalEmitter::Instance().GoingToDie(this);
}

void GLWidget::initializeGL()
{
    impl->trs->GenGLResources();

    Impl::GLFunctions()->glDisable(GL_FRAMEBUFFER_SRGB);
}

void GLWidget::resizeGL(int width, int height)
{
    auto f = Impl::GLFunctions();
    impl->width = width; impl->height = height;
    f->glViewport(0, 0, width, height);
	
	float aspect = static_cast<float>(width) / height;
	
    float halfHeight = 1.0f, halfWidth = 1.0f;
	if (width > height) halfWidth  *= aspect;
    else                halfHeight /= aspect;

    impl->projMat.data()[0] = 1 / halfWidth ;
    impl->projMat.data()[4] = 1 / halfHeight;

    impl->trs->ReallocateFramebufferStorages();
}


OpenGLFunctions * GLWidget::Impl::GLFunctions()
{ return QOpenGLContext::currentContext()->versionFunctions<OpenGLFunctions>(); }

void GLWidget::paintGL() { impl->trs->Render(defaultFramebufferObject()); }

void GLWidget::Impl::RenderNonTransparent() const
{
    auto f = GLFunctions();

    static constexpr GLfloat clearColor[3] = { 0.0f, 0.0f, 0.0f };
    static constexpr GLfloat clearDepth = 1.0f;

    f->glClearBufferfv(GL_COLOR, 0,  clearColor);
    f->glClearBufferfv(GL_DEPTH, 0, &clearDepth);

    auto & iter = GlassWallIterator::Instance();
    for (iter.Reset(); !iter.AtEnd(); ++iter) iter->DrawNonTransparent(f, projMat);
}




void GLWidget::Impl::WBOITRenderStrategy::GenGLResources()
{
    auto f = GLFunctions();

    f->glGenFramebuffers (1, &framebufferNT    );
    f->glGenTextures     (1, &colorTextureNT   );
    f->glGenRenderbuffers(1, &depthRenderbuffer);

    f->glGenFramebuffers(1, &framebuffer );
    f->glGenTextures    (1, &colorTexture);
    f->glGenTextures    (1, &alphaTexture);

    ReallocateFramebufferStorages(1, 1);

    f->glBindFramebuffer(GL_FRAMEBUFFER, framebufferNT);
    f->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE, colorTextureNT, 0 );
    f->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depthRenderbuffer   );

    f->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    f->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE, colorTexture, 0 );
    f->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D_MULTISAMPLE, alphaTexture, 0 );
    GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    f->glDrawBuffers(2, attachments);
    f->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depthRenderbuffer   );
}

void GLWidget::Impl::WBOITRenderStrategy::DeleteGLResources()
{
    auto f = GLFunctions();

    f->glDeleteFramebuffers (1, &framebufferNT);
    f->glDeleteTextures     (1, &colorTextureNT);
    f->glDeleteRenderbuffers(1, &depthRenderbuffer);

    f->glDeleteFramebuffers (1, &framebuffer);
    f->glDeleteTextures     (1, &colorTexture);
    f->glDeleteTextures     (1, &alphaTexture);
}

void GLWidget::Impl::WBOITRenderStrategy::ReallocateFramebufferStorages(int w, int h)
{
    auto f = GLFunctions();

    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTextureNT);
    f->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, numOfSamples,
                                GL_R11F_G11F_B10F, w, h, GL_TRUE         );
    f->glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    f->glRenderbufferStorageMultisample( GL_RENDERBUFFER, numOfSamples,
                                         GL_DEPTH_COMPONENT, w, h        );

    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTexture);
    f->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, numOfSamples,
                                GL_RGBA16F, w, h, GL_TRUE                );
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, alphaTexture);
    f->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, numOfSamples,
                                GL_R16F, w, h, GL_TRUE                   );
}

void GLWidget::Impl::WBOITRenderStrategy::Render(GLuint defaultFBO) const
{
    auto f = GLFunctions();

    f->glBindFramebuffer(GL_FRAMEBUFFER, framebufferNT);
    impl.RenderNonTransparent();

    static constexpr GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static constexpr GLfloat clearAlpha = 1.0f;

    f->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    f->glClearBufferfv(GL_COLOR, 0,  clearColor);
    f->glClearBufferfv(GL_COLOR, 1, &clearAlpha);

    f->glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    PrepareToTransparentRendering();
    {
        auto & iter = GlassWallIterator::Instance();
        for (iter.Reset(); !iter.AtEnd(); ++iter)
            iter->DrawTransparentForWBOIT(f, impl.projMat);
    }
    CleanupAfterTransparentRendering();

    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
    f->glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    ApplyTextures();
}



void GLWidget::Impl::WBOITRenderStrategy::PrepareToTransparentRendering() const
{
    auto f = GLFunctions();
    f->glEnable(GL_DEPTH_TEST); f->glDepthMask(GL_FALSE); f->glDepthFunc(GL_LEQUAL);
    f->glDisable(GL_CULL_FACE); f->glEnable(GL_MULTISAMPLE);

    f->glEnable(GL_BLEND);

    f->glBlendFunci(0, GL_ONE, GL_ONE);
    f->glBlendEquationi(0, GL_FUNC_ADD);

    f->glBlendFunci(1, GL_DST_COLOR, GL_ZERO);
    f->glBlendEquationi(1, GL_FUNC_ADD);
}

void GLWidget::Impl::WBOITRenderStrategy::CleanupAfterTransparentRendering() const
{ auto f = GLFunctions(); f->glDepthMask(GL_TRUE); f->glDisable(GL_BLEND); }



struct ApplyTTexturesGLResources {
    QOpenGLShaderProgram program;

    explicit ApplyTTexturesGLResources()
    {
        if (!program.addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    "#version 450 core                                            \n"
                    "const vec2 p[4] = vec2[4](                                   \n"
                    "     vec2(-1, -1), vec2( 1, -1), vec2( 1,  1), vec2(-1,  1)  \n"
                    "                         );                                  \n"
                    "void main() { gl_Position = vec4(p[gl_VertexID], 0, 1); }    \n"
                                            )
           ) assert(false);
        if (!program.addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    "#version 450 core                                                     \n"
                    "out vec4 outColor;                                                    \n"
                    "                                                                      \n"
                    "layout (location = 0) uniform  sampler2DMS colorTextureNT;            \n"
                    "layout (location = 1) uniform  sampler2DMS colorTexture;              \n"
                    "layout (location = 2) uniform  sampler2DMS alphaTexture;              \n"
                    "                                                                      \n"
                    "void main() {                                                         \n"
                    "    ivec2 upos = ivec2(gl_FragCoord.xy);                              \n"
                    "                                                                      \n"
                    "    vec4 cc = texelFetch(colorTexture, upos, gl_SampleID);            \n"
                    "    vec3 sumOfColors = cc.rgb;                                        \n"
                    "    float sumOfWeights = cc.a;                                        \n"
                    "                                                                      \n"
                    "    vec3  colorNT = texelFetch(colorTextureNT, upos, gl_SampleID).rgb;\n"
                    "                                                                      \n"
                    "    if (sumOfWeights == 0)                                            \n"
                    "    { outColor = vec4(colorNT, 1.0); return; }                        \n"
                    "                                                                      \n"
                    "    float alpha = 1 - texelFetch(alphaTexture, upos, gl_SampleID).r;  \n"
                    "    colorNT = sumOfColors / sumOfWeights * alpha +                    \n"
                    "              colorNT * (1 - alpha);                                  \n"
                    "    outColor = vec4(colorNT, 1.0);                                    \n"
                    "}                                                                     \n"
                                            )
           ) assert(false);
        if (!program.link()) assert(false);
    }
};

void GLWidget::Impl::WBOITRenderStrategy::ApplyTextures() const
{
    auto f = GLFunctions();
    static ApplyTTexturesGLResources res;

    if (!res.program.bind()) assert(false);

    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTextureNT);
    f->glUniform1i(0, 0);
    f->glActiveTexture(GL_TEXTURE1);
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTexture);
    f->glUniform1i(1, 1);
    f->glActiveTexture(GL_TEXTURE2);
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, alphaTexture);
    f->glUniform1i(2, 2);

    f->glEnable(GL_MULTISAMPLE); f->glDisable(GL_DEPTH_TEST);
    f->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}





void GLWidget::Impl::CODBRenderStrategy::Render(GLuint defaultFBO) const
{
    auto f = GLFunctions();

    f->glBindFramebuffer(GL_FRAMEBUFFER, 0); /* Anti-aliasing doesn't work
        if you don't switch framebuffers here, hell if I know why.         */
    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);

    impl.RenderNonTransparent();

    f->glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    PrepareToTransparentRendering();
    {
        auto & iter = GlassWallIterator::Instance();
        for (iter.Reset(); !iter.AtEnd(); ++iter)
            iter->DrawTransparentForCODB(f, impl.projMat);
    }
    CleanupAfterTransparentRendering();
}



void GLWidget::Impl::CODBRenderStrategy::PrepareToTransparentRendering() const
{
    auto f = GLFunctions();
    f->glEnable(GL_DEPTH_TEST); f->glDepthMask(GL_FALSE); f->glDepthFunc(GL_LEQUAL);
    f->glDisable(GL_CULL_FACE); f->glEnable(GL_MULTISAMPLE);

    f->glEnable(GL_BLEND);

    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glBlendEquation(GL_FUNC_ADD);
}

void GLWidget::Impl::CODBRenderStrategy::CleanupAfterTransparentRendering() const
{ auto f = GLFunctions(); f->glDepthMask(GL_TRUE); f->glDisable(GL_BLEND); }



void GLWidget::Impl::AdditiveRenderStrategy::Render(GLuint defaultFBO) const
{
    auto f = GLFunctions();

    f->glBindFramebuffer(GL_FRAMEBUFFER, 0); /* Anti-aliasing doesn't work
        if you don't switch framebuffers here, hell if I know why.         */
    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);

    impl.RenderNonTransparent();

    f->glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    PrepareToTransparentRendering();
    {
        auto & iter = GlassWallIterator::Instance();
        for (iter.Reset(); !iter.AtEnd(); ++iter)
            iter->DrawTransparentForAdditive(f, impl.projMat);
    }
    CleanupAfterTransparentRendering();
}



void GLWidget::Impl::AdditiveRenderStrategy::PrepareToTransparentRendering() const
{
    auto f = GLFunctions();
    f->glEnable(GL_DEPTH_TEST); f->glDepthMask(GL_FALSE); f->glDepthFunc(GL_LEQUAL);
    f->glDisable(GL_CULL_FACE); f->glEnable(GL_MULTISAMPLE);

    f->glEnable(GL_BLEND);

    f->glBlendFunc(GL_ONE, GL_ONE);
    f->glBlendEquation(GL_FUNC_ADD);
}

void GLWidget::Impl::AdditiveRenderStrategy::CleanupAfterTransparentRendering() const
{ auto f = GLFunctions(); f->glDepthMask(GL_TRUE); f->glDisable(GL_BLEND); }



void GLWidget::Impl::AdditiveEPRenderStrategy::GenGLResources()
{
    auto f = GLFunctions();

    f->glGenFramebuffers(1, &framebuffer );
    f->glGenTextures    (1, &colorTexture);
    f->glGenRenderbuffers(1, &depthRenderbuffer);

    ReallocateFramebufferStorages(1, 1);

    f->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    f->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE, colorTexture, 0 );
    f->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depthRenderbuffer   );
}

void GLWidget::Impl::AdditiveEPRenderStrategy::DeleteGLResources()
{
    auto f = GLFunctions();

    f->glDeleteFramebuffers (1, &framebuffer);
    f->glDeleteTextures     (1, &colorTexture);
    f->glDeleteRenderbuffers(1, &depthRenderbuffer);
}

void GLWidget::Impl::AdditiveEPRenderStrategy::ReallocateFramebufferStorages(int w, int h)
{
    auto f = GLFunctions();

    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTexture);
    f->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, numOfSamples,
                                GL_RGB16F, w, h, GL_TRUE                 );
    f->glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    f->glRenderbufferStorageMultisample( GL_RENDERBUFFER, numOfSamples,
                                         GL_DEPTH_COMPONENT, w, h        );
}

void GLWidget::Impl::AdditiveEPRenderStrategy::Render(GLuint defaultFBO) const
{
    auto f = GLFunctions();

    f->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    impl.RenderNonTransparent();

    f->glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);

    PrepareToTransparentRendering();
    {
        auto & iter = GlassWallIterator::Instance();
        for (iter.Reset(); !iter.AtEnd(); ++iter)
            iter->DrawTransparentForAdditive(f, impl.projMat);
    }
    CleanupAfterTransparentRendering();

    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
    f->glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    ApplyTextures();
}



void GLWidget::Impl::AdditiveEPRenderStrategy::PrepareToTransparentRendering() const
{
    auto f = GLFunctions();
    f->glEnable(GL_DEPTH_TEST); f->glDepthMask(GL_FALSE); f->glDepthFunc(GL_LEQUAL);
    f->glDisable(GL_CULL_FACE); f->glEnable(GL_MULTISAMPLE);

    f->glEnable(GL_BLEND);

    f->glBlendFunc(GL_ONE, GL_ONE);
    f->glBlendEquation(GL_FUNC_ADD);
}

void GLWidget::Impl::AdditiveEPRenderStrategy::CleanupAfterTransparentRendering() const
{
    auto f = GLFunctions();
    f->glDepthMask(GL_TRUE);
    f->glDisable(GL_BLEND);
}



struct ApplyTTexturesGLResources_AdditiveEP {
    QOpenGLShaderProgram program;

    explicit ApplyTTexturesGLResources_AdditiveEP()
    {
        if (!program.addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    "#version 450 core                                            \n"
                    "const vec2 p[4] = vec2[4](                                   \n"
                    "     vec2(-1, -1), vec2( 1, -1), vec2( 1,  1), vec2(-1,  1)  \n"
                    "                         );                                  \n"
                    "void main() { gl_Position = vec4(p[gl_VertexID], 0, 1); }    \n"
                                            )
           ) assert(false);
        if (!program.addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    "#version 450 core                                                     \n"
                    "out vec4 outColor;                                                    \n"
                    "                                                                      \n"
                    "layout (location = 0) uniform  sampler2DMS colorTexture;              \n"
                    "                                                                      \n"
                    "void main() {                                                         \n"
                    "    ivec2 upos = ivec2(gl_FragCoord.xy);                              \n"
                    "    vec3 color = texelFetch(colorTexture, upos, gl_SampleID).rgb;     \n"
                    "                                                                      \n"
                    "    outColor = vec4(vec3(1) - exp(- 0.8 * color), 1.0);               \n"
                    "}                                                                     \n"
                                            )
           ) assert(false);
        if (!program.link()) assert(false);
    }
};

void GLWidget::Impl::AdditiveEPRenderStrategy::ApplyTextures() const
{
    auto f = GLFunctions();
    static ApplyTTexturesGLResources_AdditiveEP res;

    if (!res.program.bind()) assert(false);

    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTexture);
    f->glUniform1i(0, 0);

    f->glEnable(GL_MULTISAMPLE); f->glDisable(GL_DEPTH_TEST);
    f->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}





[[nodiscard]] GLuint GLWidget::GenVAO()
{
    if (!context()->makeCurrent(context()->surface())) throw "wtf???";
    makeCurrent();
    auto f = Impl::GLFunctions();
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
    auto f = Impl::GLFunctions();

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
