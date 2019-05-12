// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#ifndef GLASSWALL_H
#define GLASSWALL_H

#include <QMatrix4x4>
#include <QVector2D>
#include <QColor>
#include <QOpenGLBuffer>
#include <optional>

#include "GLDrawingFacilities.h"

class GLResourceAllocator;

class GlassWall
{
    explicit GlassWall(int depthLevel);
public:
    struct GlassWallException_CantInsert : std::exception
    { const char * what() const noexcept override
      { return "There is already a glass wall with this name"; }
    };
    struct GlassWallException_CantFind : std::exception
    { const char * what() const noexcept override
      { return "There is no glass wall with this name"; }
    };

    static GlassWall & MakeInstance(std::string name, int depthLevel);
    static GlassWall & FindInstance(std::string name);

    ~GlassWall() = default;
    GlassWall(const GlassWall & ) = delete;
    GlassWall & operator=(const GlassWall & ) = delete;
    GlassWall(      GlassWall &&) = delete;
    GlassWall & operator=(      GlassWall &&) = delete;

    int  DepthLevel(       ) const;
    void DepthLevel(int lvl);
    QMatrix3x3 Transformation(            ) const;
    void       Transformation(QMatrix3x3 t);

    void AddTriangle(QVector2D a, QVector2D b, QVector2D c, QColor edgeColor, QColor fillColor);

    void DrawNonTransparent(OpenGLFunctions * f, QMatrix3x3 projMat);
    void DrawTransparent   (OpenGLFunctions * f, QMatrix3x3 projMat);

private:
    struct Impl; std::unique_ptr<Impl> impl;
};

struct GlassWallIterator
{
    static GlassWallIterator & Instance();
    void Reset();
    GlassWallIterator & operator++();
    GlassWall & operator*();
    GlassWall * operator->() { return &**this; }
    bool AtEnd();
private:
    explicit GlassWallIterator() = default;
};

#endif // GLASSWALL_H
