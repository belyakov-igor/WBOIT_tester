// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "GlassWall.h"

#include <map>
#include <QOpenGLShaderProgram>

struct GlassWall::Impl
{
    explicit Impl(int depthLevel, float opacity, bool transparent, bool visible)
        : m_depthLevel(depthLevel), m_opacity(opacity)
        , m_transparent(transparent), m_visible(visible)
    {
        if (opacity < 0 || opacity > 1) throw GlassWallException_CantConstruct();
        UpdateDepthsOnConctruction();
    }
    int m_depthLevel; float m_opacity; bool m_transparent; bool m_visible;
    QMatrix3x3 m_transformation;

    static void UpdateDepths();
    void UpdateDepthsOnConctruction();
    GLfloat MyDepth();

    std::vector<QVector2D> m_vertices;
    std::vector<QColor> m_edgeColors;
    std::vector<QColor> m_fillColors;

    bool m_vboNeedsToBeCreated = true;
    bool m_vboNeedsToBeReallocated = true;
    std::optional<QOpenGLBuffer> m_tri_vbo;
    VAO_Holder m_triFaces_vaoHolder, m_triEdges_vaoHolder;

    void CreateVBO();
    void ReallocateVBO();
    void SetupTriFacesVAO(GLuint vao, OpenGLFunctions * f);
    void SetupTriEdgesVAO(GLuint vao, OpenGLFunctions * f);

    int  DepthLevel(       ) const { return m_depthLevel;                }
    void DepthLevel(int lvl)       { m_depthLevel = lvl; UpdateDepths(); }
    float Opacity(             ) const { return m_opacity;    }
    void  Opacity(float opacity)       { m_opacity = opacity; }
    bool Transparent(                ) const { return m_transparent;        }
    void Transparent(bool transparent)       { m_transparent = transparent; }
    bool Visible(            ) const { return m_visible;    }
    void Visible(bool visible)       { m_visible = visible; }
    QMatrix3x3 Transformation(            ) const { return m_transformation;         }
    void       Transformation(QMatrix3x3 t)       { m_transformation = std::move(t); }

    void AddTriangle( QVector2D a, QVector2D b, QVector2D c,
                      QColor edgeColor, QColor fillColor     );

    void DrawNonTransparent        (OpenGLFunctions * f, const QMatrix3x3 & projMat);
    void DrawTransparentForWBOIT   (OpenGLFunctions * f, const QMatrix3x3 & projMat);
    void DrawTransparentForCODB    (OpenGLFunctions * f, const QMatrix3x3 & projMat);
    void DrawTransparentForAdditive(OpenGLFunctions * f, const QMatrix3x3 & projMat);
private:
    enum class TransparentStrategy { WBOIT, CODB, Additive };
    void DrawTransparent( OpenGLFunctions * f, const QMatrix3x3 & projMat,
                          TransparentStrategy strategy                     );
};

GlassWall::GlassWall(int depthLevel, float opacity, bool transparent, bool visible)
    : impl(std::make_unique<Impl>(depthLevel, opacity, transparent, visible)) {}

static std::map<int, std::unique_ptr<GlassWall>> g_gwalls;
// iterate from far to near
static std::map<int, std::unique_ptr<GlassWall>>::reverse_iterator g_gwallsIter;
static float g_gwalls_k, g_gwalls_b; // depth = k * depthLevel + b;

static void CalcCoefsFromMinAndMax(int min, int max)
{
    if (min == max) { g_gwalls_k = 0; g_gwalls_b = 0.5; return; }
    g_gwalls_k = 1.0f / (max - min);
    g_gwalls_b = static_cast<float>(-min) / (max - min);
}

void GlassWall::Impl::UpdateDepths()
{
    assert(!g_gwalls.empty());
    auto it = g_gwalls.begin();
    assert(it != g_gwalls.end());
    int min = it->second->DepthLevel(), max = it->second->DepthLevel();
    for (++it; it != g_gwalls.end(); ++it)
    {
        auto dl = it->second->DepthLevel();
        if (dl > max) max = dl; else if (dl < min) min = dl;
    }
    CalcCoefsFromMinAndMax(min, max);
}

void GlassWall::Impl::UpdateDepthsOnConctruction()
{
    int min = m_depthLevel, max = m_depthLevel;
    for (auto it = g_gwalls.begin(); it != g_gwalls.end(); ++it)
    {
        auto dl = it->second->DepthLevel();
        if (dl > max) max = dl; else if (dl < min) min = dl;
    }
    CalcCoefsFromMinAndMax(min, max);
}

GLfloat GlassWall::Impl::MyDepth()
{
    return g_gwalls_k * m_depthLevel + g_gwalls_b;
}

void GlassWall::Impl::CreateVBO()
{
    m_tri_vbo.emplace(QOpenGLBuffer::VertexBuffer);
    if (!m_tri_vbo->create()) assert(false);
    if (!m_tri_vbo->bind()) assert(false);
    m_tri_vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);

    m_vboNeedsToBeCreated = false;
}

void GlassWall::Impl::ReallocateVBO()
{
    assert(m_vertices.size() == m_edgeColors.size() * 3);
    assert(m_fillColors.size() == m_edgeColors.size());

    size_t posOffset = 0,
           fcOffset =            m_vertices  .size() * sizeof(float) * 2,
           ecOffset = fcOffset + m_fillColors.size() * sizeof(RGB16),
           size     = ecOffset + m_edgeColors.size() * sizeof(RGB16);
    std::vector<uint8_t> data(size);

    float *  p_ptr = reinterpret_cast<float *>(&data[posOffset]);
    RGB16 * fc_ptr = reinterpret_cast<RGB16 *>(&data[ fcOffset]);
    RGB16 * ec_ptr = reinterpret_cast<RGB16 *>(&data[ ecOffset]);

    auto it_ec = m_edgeColors.cbegin(); auto it_fc = m_fillColors.cbegin();
    for (auto it_v = m_vertices.cbegin(); it_v != m_vertices.cend();)
    {
        for (uint8_t i = 0; i != 3; ++i)
        { *p_ptr++ = it_v->x(); *p_ptr++ = it_v->y(); ++it_v; }

        *fc_ptr++ = SRGB_to_Linear(*it_fc++);
        *ec_ptr++ = SRGB_to_Linear(*it_ec++);
    }
    m_tri_vbo->allocate(data.data(), static_cast<int>(data.size()));

    m_vboNeedsToBeReallocated = false;
}

void GlassWall::Impl::SetupTriFacesVAO(GLuint vao, OpenGLFunctions * f)
{
    f->glBindVertexArray(vao);
    assert(m_tri_vbo->isCreated());
    if (!m_tri_vbo->bind()) assert(false);

    static constexpr GLsizei stride = sizeof(float) * 6;
    static const GLvoid * offset0 = reinterpret_cast<const void *>(0                );
    static const GLvoid * offset1 = reinterpret_cast<const void *>(2 * sizeof(float));
    static const GLvoid * offset2 = reinterpret_cast<const void *>(4 * sizeof(float));
    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, offset0);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, offset1);
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, offset2);
    f->glEnableVertexAttribArray(2);

    auto fcOffset = reinterpret_cast<void *>(m_vertices.size() * sizeof(float) * 2);

    f->glVertexAttribPointer(3, 3, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(RGB16), fcOffset);
    f->glEnableVertexAttribArray(3);

    m_triFaces_vaoHolder.VAO_SetReady();
}

void GlassWall::Impl::SetupTriEdgesVAO(GLuint vao, OpenGLFunctions * f)
{
    f->glBindVertexArray(vao);
    assert(m_tri_vbo->isCreated());
    if (!m_tri_vbo->bind()) assert(false);

    static constexpr GLsizei stride = sizeof(float) * 6;
    static const GLvoid * offset0 = reinterpret_cast<const void *>(0                );
    static const GLvoid * offset1 = reinterpret_cast<const void *>(2 * sizeof(float));
    static const GLvoid * offset2 = reinterpret_cast<const void *>(4 * sizeof(float));
    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, offset0);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, offset1);
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, offset2);
    f->glEnableVertexAttribArray(2);

    auto ecOffset = reinterpret_cast<void *>( m_vertices.size() * sizeof(float) * 2 +
                                              m_fillColors.size() * sizeof(RGB16)     );

    f->glVertexAttribPointer(3, 3, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(RGB16), ecOffset);
    f->glEnableVertexAttribArray(3);

    m_triEdges_vaoHolder.VAO_SetReady();
}

void GlassWall::Impl::AddTriangle( QVector2D a, QVector2D b, QVector2D c,
                                   QColor edgeColor, QColor fillColor     )
{
    m_vertices.push_back(a); m_vertices.push_back(b); m_vertices.push_back(c);
    m_edgeColors.push_back(edgeColor);
    m_fillColors.push_back(fillColor);
    m_vboNeedsToBeReallocated = true;
}



struct GlassWall_GLProgram {
    QOpenGLShaderProgram p;
    static constexpr auto vs_source =
            "#version 450 core                                                     \n"
            "layout (location = 0) in vec2 vertex0;                                \n"
            "layout (location = 1) in vec2 vertex1;                                \n"
            "layout (location = 2) in vec2 vertex2;                                \n"
            "                                                                      \n"
            "layout (location = 3) in vec3 color;                                  \n"
            "                                                                      \n"
            "out vec2 gs_vertex0;                                                  \n"
            "out vec2 gs_vertex1;                                                  \n"
            "out vec2 gs_vertex2;                                                  \n"
            "                                                                      \n"
            "out vec3 gs_color;                                                    \n"
            "                                                                      \n"
            "void main()                                                           \n"
            "{                                                                     \n"
            "    gs_vertex0 = vertex0; gs_vertex1 = vertex1; gs_vertex2 = vertex2; \n"
            "    gs_color = color;                                                 \n"
            "}                                                                     \n";
    static constexpr auto gs_source =
            "#version 450 core                                              \n"
            "layout (points) in;                                            \n"
            "layout (triangle_strip, max_vertices = 3) out;                 \n"
            "                                                               \n"
            "layout (location = 0) uniform float d;                         \n"
            "layout (location = 1) uniform mat3 tr;                         \n"
            "                                                               \n"
            "in vec2 gs_vertex0[];                                          \n"
            "in vec2 gs_vertex1[];                                          \n"
            "in vec2 gs_vertex2[];                                          \n"
            "                                                               \n"
            "in vec3 gs_color[];                                            \n"
            "                                                               \n"
            "out flat vec3 fs_color;                                        \n"
            "                                                               \n"
            "void main()                                                    \n"
            "{                                                              \n"
            "    // Provoking vertex:                                       \n"
            "    gl_Position = vec4(  ( tr * vec3(gs_vertex0[0], 1) ).xy,   \n"
            "                         d, 1                                  \n"
            "                      ); fs_color = gs_color[0]; EmitVertex(); \n"
            "    gl_Position = vec4(  ( tr * vec3(gs_vertex1[0], 1) ).xy,   \n"
            "                         d, 1                                  \n"
            "                      ); EmitVertex();                         \n"
            "    gl_Position = vec4(  ( tr * vec3(gs_vertex2[0], 1) ).xy,   \n"
            "                         d, 1                                  \n"
            "                      ); EmitVertex();                         \n"
            "    EmitVertex(); EndPrimitive();                              \n"
            "}                                                              \n";
    static constexpr auto fs_source_NT =
            "#version 450 core                 \n"
            "                                  \n"
            "in vec3 fs_color;                 \n"
            "out vec3 color;                   \n"
            "                                  \n"
            "void main() { color = fs_color; } \n";
    static constexpr auto fs_source_WBOIT =
            "#version 450 core                                               \n"
            "                                                                \n"
            "in vec3 fs_color;                                               \n"
            "                                                                \n"
            "layout (location = 0) out vec4 outData;                         \n"
            "layout (location = 1) out float alpha;                          \n"
            "                                                                \n"
            "layout (location = 2) uniform float w;                          \n"
            "void main() { outData = vec4(w * fs_color, w); alpha = 1 - w; } \n";
    static constexpr auto fs_source_CODB =
            "#version 450 core                          \n"
            "                                           \n"
            "in vec3 fs_color;                          \n"
            "out vec4 color;                            \n"
            "layout (location = 2) uniform float w;     \n"
            "                                           \n"
            "void main() { color = vec4(fs_color, w); } \n";
    static constexpr auto fs_source_Additive =
            "#version 450 core                           \n"
            "                                            \n"
            "in vec3 fs_color;                           \n"
            "out vec3 color;                             \n"
            "layout (location = 2) uniform float w;      \n"
            "                                            \n"
            "void main() { color = vec3(fs_color * w); } \n";

    enum class Mode { NT, WBOIT, CODB, Additive };
    explicit GlassWall_GLProgram(Mode mode)
    {
        if (!p.addShaderFromSourceCode(QOpenGLShader::Vertex  , vs_source)) assert(false);
        if (!p.addShaderFromSourceCode(QOpenGLShader::Geometry, gs_source)) assert(false);
        switch (mode)
        {
        case Mode::NT:
            if (!p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs_source_NT       ))
                assert(false);
            break;
        case Mode::WBOIT:
            if (!p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs_source_WBOIT    ))
                assert(false);
            break;
        case Mode::CODB:
            if (!p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs_source_CODB     ))
                assert(false);
            break;
        case Mode::Additive:
            if (!p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs_source_Additive ))
                assert(false);
            break;
        }
        if (!p.link()) assert(false);
    }
};

void GlassWall::Impl::DrawNonTransparent(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{
    if (!m_visible || m_vertices.empty()) return;
    if (m_vboNeedsToBeCreated) CreateVBO();
    if (m_vboNeedsToBeReallocated) ReallocateVBO();

    static GlassWall_GLProgram program(GlassWall_GLProgram::Mode::NT);
    assert (program.p.isLinked());
    if (!program.p.bind()) assert(false);

    f->glUniform1f(0, MyDepth());
    f->glUniformMatrix3fv(1, 1, GL_FALSE, (projMat * m_transformation).data());

    auto [vao, ready] = m_triEdges_vaoHolder.GetVAO();
    f->glBindVertexArray(vao);
    if (!ready) SetupTriEdgesVAO(vao, f);
    if (!m_tri_vbo->bind()) assert(false);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    f->glDrawArrays( GL_POINTS, 0,
                     static_cast<GLsizei>(m_edgeColors.size()) );
    f->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (!m_transparent)
    {
        auto [vao, ready] = m_triFaces_vaoHolder.GetVAO();
        f->glBindVertexArray(vao);
        if (!ready) SetupTriFacesVAO(vao, f);
        if (!m_tri_vbo->bind()) assert(false);

        f->glDrawArrays( GL_POINTS, 0,
                         static_cast<GLsizei>(m_fillColors.size()) );
    }
}


void GlassWall::Impl::DrawTransparent( OpenGLFunctions * f, const QMatrix3x3 & projMat,
                                       TransparentStrategy strategy                     )
{
    if (!m_visible || m_vertices.empty() || !m_transparent) return;
    if (m_vboNeedsToBeCreated) CreateVBO();
    if (m_vboNeedsToBeReallocated) ReallocateVBO();

    QOpenGLShaderProgram * p;
    switch (strategy)
    {
        case TransparentStrategy::WBOIT:
        {
            static GlassWall_GLProgram program(GlassWall_GLProgram::Mode::WBOIT);
            p = &program.p;
            break;
        }
        case TransparentStrategy::CODB:
        {
            static GlassWall_GLProgram program(GlassWall_GLProgram::Mode::CODB);
           p = &program.p;
            break;
        }
        case TransparentStrategy::Additive:
        {
            static GlassWall_GLProgram program(GlassWall_GLProgram::Mode::Additive);
            p = &program.p;
            break;
        }
    }
    assert (p->isLinked());
    if (!p->bind()) assert(false);

    f->glUniform1f(0, MyDepth());
    f->glUniformMatrix3fv(1, 1, GL_FALSE, (projMat * m_transformation).data());
    f->glUniform1f(2, m_opacity);

    auto [vao, ready] = m_triFaces_vaoHolder.GetVAO();
    f->glBindVertexArray(vao);
    if (!ready) SetupTriFacesVAO(vao, f);
    if (!m_tri_vbo->bind()) assert(false);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_fillColors.size()));
}

void GlassWall::Impl::DrawTransparentForWBOIT   ( OpenGLFunctions * f,
                                                  const QMatrix3x3 & projMat )
{ DrawTransparent(f, projMat, TransparentStrategy::WBOIT); }

void GlassWall::Impl::DrawTransparentForCODB    ( OpenGLFunctions * f,
                                                  const QMatrix3x3 & projMat)
{ DrawTransparent(f, projMat, TransparentStrategy::CODB); }

void GlassWall::Impl::DrawTransparentForAdditive( OpenGLFunctions *f,
                                                  const QMatrix3x3 &projMat )
{ DrawTransparent(f, projMat, TransparentStrategy::Additive); }





GlassWall & GlassWall::MakeInstance( int depthLevel, float opacity,
                                     bool transparent, bool visible )
{
    auto hint = g_gwalls.find(depthLevel);
    if (hint != g_gwalls.end()) throw GlassWallException_CantInsert();
    auto iter = g_gwalls.insert(hint, std::pair{ depthLevel,
                                                 std::unique_ptr<GlassWall>(
                                         new GlassWall( depthLevel, opacity,
                                                        transparent, visible )
                                                                           )
                                               }
                               );
    return *iter->second;
}

GlassWall & GlassWall::FindInstance(int depthLevel)
{
    auto it = g_gwalls.find(depthLevel);
    if (it == g_gwalls.end()) throw GlassWallException_CantFind();
    return *it->second;
}

size_t GlassWall::CountOfInstances() { return g_gwalls.size(); }

int  GlassWall::DepthLevel(       ) const { return impl->DepthLevel(); }
void GlassWall::DepthLevel(int lvl)       { impl->DepthLevel(lvl);     }

float GlassWall::Opacity(             ) const { return impl->Opacity(); }
void  GlassWall::Opacity(float opacity)       { impl->Opacity(opacity); }
bool GlassWall::Transparent(                ) const { return impl->Transparent();     }
void GlassWall::Transparent(bool transparent)       { impl->Transparent(transparent); }
bool GlassWall::Visible(            ) const { return impl->Visible(); }
void GlassWall::Visible(bool visible)       { impl->Visible(visible); }

QMatrix3x3 GlassWall::Transformation(            ) const
{ return impl->Transformation(); }
void       GlassWall::Transformation(QMatrix3x3 t)
{ impl->Transformation(std::move(t)); }


void GlassWall::AddTriangle( QVector2D a, QVector2D b, QVector2D c,
                             QColor edgeColor, QColor fillColor     )
{ impl->AddTriangle(a, b, c, edgeColor, fillColor); }

void GlassWall::DrawNonTransparent        (OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawNonTransparent        (f, projMat); }

void GlassWall::DrawTransparentForWBOIT   (OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawTransparentForWBOIT   (f, projMat); }

void GlassWall::DrawTransparentForCODB    (OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawTransparentForCODB    (f, projMat); }

void GlassWall::DrawTransparentForAdditive(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawTransparentForAdditive(f, projMat); }

GlassWallIterator & GlassWallIterator::Instance() { static GlassWallIterator ins; return ins; }

void GlassWallIterator::Reset() { g_gwallsIter = g_gwalls.rbegin(); }

GlassWallIterator & GlassWallIterator::operator++() { ++g_gwallsIter; return *this; }

GlassWall & GlassWallIterator::operator*() { return *g_gwallsIter->second; }

bool GlassWallIterator::AtEnd() { return g_gwallsIter == g_gwalls.rend(); }
