// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "GlassWall.h"

#include <unordered_map>
#include <QOpenGLShaderProgram>

struct GlassWall::Impl
{
    explicit Impl(int depthLevel) : m_depthLevel(depthLevel) { UpdateDepthsOnConctruction(); }
    int m_depthLevel;
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

    int  DepthLevel(       ) const { return m_depthLevel; }
    void DepthLevel(int lvl)       { m_depthLevel = lvl; UpdateDepths(); }
    QMatrix3x3 Transformation(            ) const { return m_transformation;            }
    void       Transformation(QMatrix3x3 t)       { m_transformation = std::move(t);    }

    void AddTriangle( QVector2D a, QVector2D b, QVector2D c,
                      QColor edgeColor, QColor fillColor     );

    void DrawNonTransparent(OpenGLFunctions * f, const QMatrix3x3 & projMat);
    void DrawTransparent   (OpenGLFunctions * f, const QMatrix3x3 & projMat);
};

GlassWall::GlassWall(int depthLevel) : impl(std::make_unique<Impl>(depthLevel)) {}

static std::unordered_map<std::string, std::unique_ptr<GlassWall>> g_gwalls;
static std::unordered_map<std::string, std::unique_ptr<GlassWall>>::iterator g_gwallsIter;
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
    m_vbo->allocate(
                m_vertices.data(),
                static_cast<int>(m_vertices.size() * sizeof(QVector2D))
                   );
    m_vboNeedsToBeReallocated = false;
}

void GlassWall::Impl::SetupVAO(GLuint vao, OpenGLFunctions * f)
{
    f->glBindVertexArray(vao);

    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glEnableVertexAttribArray(0);

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



void QColorToGLfloat3(QColor c, GLfloat (&out)[3])
{
    out[0] = static_cast<float>(c.redF  ());
    out[1] = static_cast<float>(c.greenF());
    out[2] = static_cast<float>(c.blueF ());
}

struct GlassWall_DrawNonTransparent_GLProgram {
    QOpenGLShaderProgram p;
    explicit GlassWall_DrawNonTransparent_GLProgram()
    {
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    "#version 430                                                 \n"
                    "layout (location = 0) in vec2 vertex;                        \n"
                    "layout (location = 0) uniform float d;                       \n"
                    "layout (location = 1) uniform mat3 tr;                       \n"
                    "void main() { gl_Position = vec4(tr * vec3(vertex, d), 1); } \n"
                                      )
           ) assert(false);
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    "#version 430                              \n"
                    "                                          \n"
                    "layout (location = 2) uniform vec4 color; \n"
                    "out vec3 fragColor;                       \n"
                    "                                          \n"
                    "void main() { fragColor = color.rgb; }    \n"
                                      )
           ) assert(false);
        if (!p.link()) assert(false);
    }
};

void GlassWall::Impl::DrawNonTransparent(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{
    if (m_vertices.empty()) return;
    if (m_vboNeedsToBeCreated) CreateVBO();
    if (m_vboNeedsToBeReallocated) ReallocateVBO();

    static GlassWall_DrawNonTransparent_GLProgram program;
    assert (program.p.isLinked());
    if (!program.p.bind()) assert(false);

    program.p.setUniformValue(0, MyDepth());
    program.p.setUniformValue(1, (projMat * m_transformation));
    program.p.setUniformValue(2, m_edgeColors[0]);

    auto [vao, ready] = m_vaoHolder.GetVAO();
    if (!ready) SetupVAO(vao, f);
    if (!m_vbo->bind()) assert(false);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    f->glDrawArrays(GL_TRIANGLES, 0, int(m_vertices.size()));
    f->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

struct GlassWall_DrawTransparent_GLProgram {
    QOpenGLShaderProgram p;
    explicit GlassWall_DrawTransparent_GLProgram()
    {
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    "#version 430                                                 \n"
                    "layout (location = 0) in vec2 vertex;                        \n"
                    "layout (location = 0) uniform float d;                       \n"
                    "layout (location = 1) uniform mat3 tr;                       \n"
                    "void main() { gl_Position = vec4(tr * vec3(vertex, d), 1); } \n"
                                      )
           ) assert(false);
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    "#version 430                               \n"
                    "layout (location = 0) out vec4 outData;    \n"
                    "layout (location = 1) out float alpha;     \n"
                    "                                           \n"
                    "layout (location = 2) uniform vec3 color;  \n"
                    "layout (location = 3) uniform float w;     \n"
                    "void main()                                \n"
                    "{                                          \n"
                    "    outData = vec4(w * color, w);          \n"
                    "    alpha = 1 - w;                         \n"
                    "}                                          \n"
                                      )
           ) assert(false);
        if (!p.link()) assert(false);
    }
};

void GlassWall::Impl::DrawTransparent(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{
    if (m_vertices.empty()) return;
    if (m_vboNeedsToBeCreated) CreateVBO();
    if (m_vboNeedsToBeReallocated) ReallocateVBO();

    static GlassWall_DrawTransparent_GLProgram program;
    assert (program.p.isLinked());
    if (!program.p.bind()) assert(false);

    f->glUniform1f(0, MyDepth());
    f->glUniformMatrix3fv(1, 1, GL_FALSE, (projMat * m_transformation).data());
    float color[3]; QColorToGLfloat3(m_edgeColors[0], color);
    f->glUniform3fv(2, 1, color);
    f->glUniform1f(3, 0.5f);

    auto [vao, ready] = m_vaoHolder.GetVAO();
    if (!ready) SetupVAO(vao, f);
    if (!m_vbo->bind()) assert(false);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glDrawArrays(GL_TRIANGLES, 0, int(m_vertices.size()));
}





GlassWall & GlassWall::MakeInstance(std::string name, int depthLevel)
{
    auto hint = g_gwalls.find(name);
    if (hint != g_gwalls.end()) throw GlassWallException_CantInsert();
    auto iter = g_gwalls.insert(hint, std::pair{ name, std::unique_ptr<GlassWall>(
                                                       new GlassWall(depthLevel) )
                                               }
                               );
    return *iter->second;
}

GlassWall & GlassWall::FindInstance(const std::string & name)
{
    auto it = g_gwalls.find(name);
    if (it == g_gwalls.end()) throw GlassWallException_CantFind();
    return *it->second;
}

int  GlassWall::DepthLevel(       ) const { return impl->DepthLevel(); }
void GlassWall::DepthLevel(int lvl)       { impl->DepthLevel(lvl);     }

QMatrix3x3 GlassWall::Transformation(            ) const
{ return impl->Transformation(); }
void       GlassWall::Transformation(QMatrix3x3 t)
{ impl->Transformation(std::move(t)); }


void GlassWall::AddTriangle( QVector2D a, QVector2D b, QVector2D c,
                             QColor edgeColor, QColor fillColor     )
{ impl->AddTriangle(a, b, c, edgeColor, fillColor); }

void GlassWall::DrawNonTransparent(OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawNonTransparent(f, projMat); }

void GlassWall::DrawTransparent   (OpenGLFunctions * f, const QMatrix3x3 & projMat)
{ impl->DrawTransparent   (f, projMat); }

GlassWallIterator & GlassWallIterator::Instance() { static GlassWallIterator ins; return ins; }

void GlassWallIterator::Reset() { g_gwallsIter = g_gwalls.begin(); }

GlassWallIterator & GlassWallIterator::operator++() { ++g_gwallsIter; return *this; }

GlassWall & GlassWallIterator::operator*() { return *g_gwallsIter->second; }

bool GlassWallIterator::AtEnd() { return g_gwallsIter == g_gwalls.end(); }
