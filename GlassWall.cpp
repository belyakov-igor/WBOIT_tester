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
    f->glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, stride, reinterpret_cast<void *>(sizeof(QVector2D)));
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, stride, reinterpret_cast<void *>(sizeof(QVector2D) + 1));
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



struct GlassWall_DrawNonTransparent_GLProgram {
    QOpenGLShaderProgram p;
    explicit GlassWall_DrawNonTransparent_GLProgram()
    {
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    "#version 430                                                 \n"
                    "layout (location = 0) in vec2 vertex;                        \n"
                    "layout (location = 1) in uint edgeColorComponent;            \n"
                    "layout (location = 2) in uint fillColorComponent;            \n"
                    "                                                             \n"
                    "layout (location = 0) uniform float d;                       \n"
                    "layout (location = 1) uniform mat3 tr;                       \n"
                    "                                                             \n"
                    "out float colorComponent;                                    \n"
                    "                                                             \n"
                    "void main()                                                  \n"
                    "{                                                            \n"
                    "    gl_Position = vec4(tr * vec3(vertex, d), 1);             \n"
                    "    colorComponent = float(edgeColorComponent) / 255;        \n"
                    "}                                                            \n"
                                      )
           ) assert(false);
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Geometry,
                    "#version 430 core                                                          \n"
                    "layout (triangles) in;                                                     \n"
                    "layout (triangle_strip, max_vertices = 3) out;                             \n"
                    "                                                                           \n"
                    "in float colorComponent[];                                                 \n"
                    "out vec3 color;                                                            \n"
                    "                                                                           \n"
                    "void main()                                                                \n"
                    "{                                                                          \n"
                    "    color = vec3(colorComponent[0], colorComponent[1], colorComponent[2]); \n"
                    "                                                                           \n"
                    "    gl_Position = gl_in[0].gl_Position;                                    \n"
                    "    EmitVertex();                                                          \n"
                    "    gl_Position = gl_in[1].gl_Position;                                    \n"
                    "    EmitVertex();                                                          \n"
                    "    gl_Position = gl_in[2].gl_Position;                                    \n"
                    "    EmitVertex(); EndPrimitive();                                          \n"
                    "}                                                                          \n"
                                      )
           ) assert(false);
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    "#version 430                              \n"
                    "                                          \n"
                    "in vec3 color;                            \n"
                    "out vec3 fragColor;                       \n"
                    "                                          \n"
                    "void main() { fragColor = color; }        \n"
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

    f->glUniform1f(0, MyDepth());
    f->glUniformMatrix3fv(1, 1, GL_FALSE, (projMat * m_transformation).data());

    auto [vao, ready] = m_vaoHolder.GetVAO();
    f->glBindVertexArray(vao);
    if (!m_vbo->bind()) assert(false);
    if (!ready) SetupVAO(vao, f);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    f->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));f->glBindVertexArray(0);
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
                    "layout (location = 1) in uint edgeColorComponent;            \n"
                    "layout (location = 2) in uint fillColorComponent;            \n"
                    "                                                             \n"
                    "layout (location = 0) uniform float d;                       \n"
                    "layout (location = 1) uniform mat3 tr;                       \n"
                    "                                                             \n"
                    "out float colorComponent;                                    \n"
                    "                                                             \n"
                    "void main()                                                  \n"
                    "{                                                            \n"
                    "    gl_Position = vec4(tr * vec3(vertex, d), 1);             \n"
                    "    colorComponent = float(fillColorComponent) / 255;        \n"
                    "}                                                            \n"
                                      )
           ) assert(false);
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Geometry,
                    "#version 430 core                                                          \n"
                    "layout (triangles) in;                                                     \n"
                    "layout (triangle_strip, max_vertices = 3) out;                             \n"
                    "                                                                           \n"
                    "in float colorComponent[];                                                 \n"
                    "out vec3 color;                                                            \n"
                    "                                                                           \n"
                    "void main()                                                                \n"
                    "{                                                                          \n"
                    "    color = vec3(colorComponent[0], colorComponent[1], colorComponent[2]); \n"
                    "                                                                           \n"
                    "    gl_Position = gl_in[0].gl_Position;                                    \n"
                    "    EmitVertex();                                                          \n"
                    "    gl_Position = gl_in[1].gl_Position;                                    \n"
                    "    EmitVertex();                                                          \n"
                    "    gl_Position = gl_in[2].gl_Position;                                    \n"
                    "    EmitVertex(); EndPrimitive();                                          \n"
                    "}                                                                          \n"
                                      )
           ) assert(false);
        if (!p.addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    "#version 430                               \n"
                    "                                           \n"
                    "in vec3 color;                             \n"
                    "                                           \n"
                    "layout (location = 0) out vec4 outData;    \n"
                    "layout (location = 1) out float alpha;     \n"
                    "                                           \n"
                    "layout (location = 2) uniform float w;     \n"
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
    f->glUniform1f(2, 0.5f);

    auto [vao, ready] = m_vaoHolder.GetVAO();
    f->glBindVertexArray(vao);
    if (!m_vbo->bind()) assert(false);
    if (!ready) SetupVAO(vao, f);

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glDepthFunc(GL_LEQUAL);
    f->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
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
