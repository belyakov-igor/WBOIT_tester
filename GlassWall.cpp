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
    std::optional<QOpenGLBuffer> m_vbo;
    VAO_Holder m_vaoHolder;

    void CreateVBO();
    void ReallocateVBO();
    void SetupVAO(GLuint vao, OpenGLFunctions * f);

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

    void DrawNonTransparent     (OpenGLFunctions * f, const QMatrix3x3 & projMat);
    void DrawTransparentForWBOIT(OpenGLFunctions * f, const QMatrix3x3 & projMat);
    void DrawTransparentForCODB (OpenGLFunctions * f, const QMatrix3x3 & projMat);
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
    m_vbo.emplace(QOpenGLBuffer::VertexBuffer);
    if (!m_vbo->create()) assert(false);
    if (!m_vbo->bind()) assert(false);
    m_vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vboNeedsToBeCreated = false;
}

void GlassWall::Impl::ReallocateVBO()
{
    size_t size = m_vertices.size() * (sizeof(QVector2D) + 2);
    std::vector<uint8_t> data; data.resize(size);
    assert(m_vertices.size() == m_edgeColors.size() * 3);
    assert(m_fillColors.size() == m_edgeColors.size());

    auto it_ec = m_edgeColors.cbegin(); auto it_fc = m_fillColors.cbegin();
    uint8_t * ptr = &data[0]; uint8_t vit = 0;
    for (auto it_v = m_vertices.cbegin(); it_v != m_vertices.cend(); ++it_v)
    {
        reinterpret_cast<QVector2D &>(*ptr) = *it_v; ptr += sizeof(QVector2D);

        if (vit == 0)
        {
            *ptr++ = static_cast<uint8_t>(it_ec->red  ());
            *ptr++ = static_cast<uint8_t>(it_fc->red  ());
            ++vit;
        }
        else if (vit == 1)
        {
            *ptr++ = static_cast<uint8_t>(it_ec->green());
            *ptr++ = static_cast<uint8_t>(it_fc->green());
            ++vit;
        }
        else
        {
            assert(vit == 2);
            *ptr++ = static_cast<uint8_t>(it_ec->blue ());
            *ptr++ = static_cast<uint8_t>(it_fc->blue ());
            vit = 0; ++it_ec; ++it_fc;
        }
    }
    m_vbo->allocate(data.data(), static_cast<int>(size));
    m_vboNeedsToBeReallocated = false;
}

void GlassWall::Impl::SetupVAO(GLuint vao, OpenGLFunctions * f)
{
    f->glBindVertexArray(vao);

    GLsizei stride = sizeof(QVector2D) + 2;
    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribIPointer( 1, 1, GL_UNSIGNED_BYTE, stride,
                               reinterpret_cast<void *>(sizeof(QVector2D)    ) );
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribIPointer( 2, 1, GL_UNSIGNED_BYTE, stride,
                               reinterpret_cast<void *>(sizeof(QVector2D) + 1) );
    f->glEnableVertexAttribArray(2);

    m_vaoHolder.VAO_SetReady();
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
    enum class Mode { NT, WBOIT, COIDB };
    static constexpr auto vs_source =
            "#version 430                                                 \n"
            "layout (location = 0) in vec2 vertex;                        \n"
            "layout (location = 1) in uint edgeColorComponent;            \n"
            "layout (location = 2) in uint fillColorComponent;            \n"
            "                                                             \n"
            "layout (location = 0) uniform float d;                       \n"
            "layout (location = 1) uniform mat3 tr;                       \n"
            "layout (location = 2) uniform bool drawEdges;                \n"
            "                                                             \n"
            "out float colorComponent;                                    \n"
            "                                                             \n"
            "void main()                                                  \n"
            "{                                                            \n"
            "    gl_Position = vec4(tr * vec3(vertex, d), 1);             \n"
            "    colorComponent = float( drawEdges ? edgeColorComponent   \n"
            "                                      : fillColorComponent   \n"
            "                          ) / 255;                           \n"
            "}                                                            \n";
    static constexpr auto gs_source =
            "#version 430 core                                                     \n"
            "layout (triangles) in;                                                \n"
            "layout (triangle_strip, max_vertices = 3) out;                        \n"
            "                                                                      \n"
            "in float colorComponent[];                                            \n"
            "out vec3 color;                                                       \n"
            "                                                                      \n"
            "void main()                                                           \n"
            "{                                                                     \n"
            "    color = vec3(                                                     \n"
            "        colorComponent[0], colorComponent[1], colorComponent[2]       \n"
            "                );                                                    \n"
            "                                                                      \n"
            "    gl_Position = gl_in[0].gl_Position; EmitVertex();                 \n"
            "    gl_Position = gl_in[1].gl_Position; EmitVertex();                 \n"
            "    gl_Position = gl_in[2].gl_Position; EmitVertex(); EndPrimitive(); \n"
            "}                                                                     \n";
    static constexpr auto fs_source_NT =
            "#version 430                              \n"
            "                                          \n"
            "in vec3 color;                            \n"
            "out vec3 fragColor;                       \n"
            "                                          \n"
            "void main() { fragColor = color; }        \n";
    static constexpr auto fs_source_WBOIT =
            "#version 430                                                 \n"
            "                                                             \n"
            "in vec3 color;                                               \n"
            "                                                             \n"
            "layout (location = 0) out vec4 outData;                      \n"
            "layout (location = 1) out float alpha;                       \n"
            "                                                             \n"
            "layout (location = 3) uniform float w;                       \n"
            "void main() { outData = vec4(w * color, w); alpha = 1 - w; } \n";
    static constexpr auto fs_source_CODB =
            "#version 430                                    \n"
            "                                                \n"
            "in vec3 color;                                  \n"
            "out vec4 colorAndAlpha;                         \n"
            "layout (location = 3) uniform float w;          \n"
            "                                                \n"
            "void main() { colorAndAlpha = vec4(color, w); } \n";
    explicit GlassWall_GLProgram(Mode mode)
    {
        if (!p.addShaderFromSourceCode(QOpenGLShader::Vertex  , vs_source)) assert(false);
        if (!p.addShaderFromSourceCode(QOpenGLShader::Geometry, gs_source)) assert(false);
        switch (mode)
        {
        case Mode::NT:
            if (!p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs_source_NT   ))
                assert(false);
            break;
        case Mode::WBOIT:
            if (!p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs_source_WBOIT))
                assert(false);
            break;
        case Mode::COIDB:
            if (!p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs_source_CODB ))
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
    f->glUniform1i(2, GL_TRUE);

    auto [vao, ready] = m_vaoHolder.GetVAO();
    f->glBindVertexArray(vao);
    if (!m_vbo->bind()) assert(false);
    if (!ready) SetupVAO(vao, f);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    f->glDrawArrays( GL_TRIANGLES, 0,
                     static_cast<GLsizei>(m_vertices.size()) );
    f->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (!m_transparent)
    {
        f->glUniform1i(2, GL_FALSE);
        f->glDrawArrays( GL_TRIANGLES, 0,
                         static_cast<GLsizei>(m_vertices.size()) );
    }
    f->glBindVertexArray(0);
}

void GlassWall::Impl::DrawTransparentForWBOIT(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{
    if (!m_visible || m_vertices.empty() || !m_transparent) return;
    if (m_vboNeedsToBeCreated) CreateVBO();
    if (m_vboNeedsToBeReallocated) ReallocateVBO();

    static GlassWall_GLProgram program(GlassWall_GLProgram::Mode::WBOIT);
    assert (program.p.isLinked());
    if (!program.p.bind()) assert(false);

    f->glUniform1f(0, MyDepth());
    f->glUniformMatrix3fv(1, 1, GL_FALSE, (projMat * m_transformation).data());
    f->glUniform1i(2, GL_FALSE);
    f->glUniform1f(3, m_opacity);

    auto [vao, ready] = m_vaoHolder.GetVAO();
    f->glBindVertexArray(vao);
    if (!m_vbo->bind()) assert(false);
    if (!ready) SetupVAO(vao, f);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
}

void GlassWall::Impl::DrawTransparentForCODB(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{
    if (!m_visible || m_vertices.empty() || !m_transparent) return;
    if (m_vboNeedsToBeCreated) CreateVBO();
    if (m_vboNeedsToBeReallocated) ReallocateVBO();

    static GlassWall_GLProgram program(GlassWall_GLProgram::Mode::COIDB);
    assert (program.p.isLinked());
    if (!program.p.bind()) assert(false);

    f->glUniform1f(0, MyDepth());
    f->glUniformMatrix3fv(1, 1, GL_FALSE, (projMat * m_transformation).data());
    f->glUniform1i(2, GL_FALSE);
    f->glUniform1f(3, m_opacity);

    auto [vao, ready] = m_vaoHolder.GetVAO();
    f->glBindVertexArray(vao);
    if (!m_vbo->bind()) assert(false);
    if (!ready) SetupVAO(vao, f);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
}





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

void GlassWall::DrawNonTransparent     (OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawNonTransparent     (f, projMat); }

void GlassWall::DrawTransparentForWBOIT(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawTransparentForWBOIT(f, projMat); }

void GlassWall::DrawTransparentForCODB (OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawTransparentForCODB (f, projMat); }

GlassWallIterator & GlassWallIterator::Instance() { static GlassWallIterator ins; return ins; }

void GlassWallIterator::Reset() { g_gwallsIter = g_gwalls.rbegin(); }

GlassWallIterator & GlassWallIterator::operator++() { ++g_gwallsIter; return *this; }

GlassWall & GlassWallIterator::operator*() { return *g_gwallsIter->second; }

bool GlassWallIterator::AtEnd() { return g_gwallsIter == g_gwalls.rend(); }
