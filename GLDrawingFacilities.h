// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#ifndef GLDRAWINGFACILITIES_H
#define GLDRAWINGFACILITIES_H

#include <QOpenGLFunctions_4_5_Core>
#include <QObject>

using OpenGLFunctions = QOpenGLFunctions_4_5_Core;

extern const double g_pi;
extern const float g_pi_f;

class VAO_Holder // gen VAO in all GL contexts in ctor; delete VAO in all GL contexts in dtor
        : public QObject
{
    Q_OBJECT
public:
    explicit VAO_Holder();
    ~VAO_Holder();
    VAO_Holder(const VAO_Holder & ) = delete;
    VAO_Holder(      VAO_Holder &&) = delete;
    VAO_Holder & operator=(const VAO_Holder & ) = delete;
    VAO_Holder & operator=(      VAO_Holder &&) = delete;

    std::pair<GLuint, bool> GetVAO() const; // true if VAO is ready (VAO_SetReady() was called)
    void VAO_SetReady();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // GLDRAWINGFACILITIES_H
