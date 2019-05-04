// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "GLDrawingFacilities.h"

#include <unordered_map>

#include "GLWidget.h"

const double g_pi = 3.1415926535897932384626433832795;
const float & g_pi_f = static_cast<const float &>(g_pi);

struct VAO_Holder::Impl
{
    std::unordered_map<
            QOpenGLContext *,    // Context where the VAO is generated
            std::pair< GLuint,   // VAO name
                       bool    > // true: VAO's settings are fresh; just bind
                                 //       it and draw without tweaking anything
                                 // false: need to call some glVertexAttribPointer or whatever
                     > wgt_vao_map;
};

VAO_Holder::VAO_Holder()
{
    for (auto wgt : g_GLWidgets)
        impl->wgt_vao_map.emplace(wgt->context(), std::pair{ wgt->GenVAO(), false });

    connect(&GLWidgetSignalEmitter::Instance(), &GLWidgetSignalEmitter::GoingToDie,
            this, [this](GLWidget * sender)
    {
        auto it = impl->wgt_vao_map.find(sender->context());
        assert(it != impl->wgt_vao_map.end());
    });
    connect(&GLWidgetSignalEmitter::Instance(), &GLWidgetSignalEmitter::ComingToLife,
            this, [this](GLWidget * sender)
    {
        assert(impl->wgt_vao_map.find(sender->context()) == impl->wgt_vao_map.end());
        impl->wgt_vao_map.emplace(sender->context(), std::pair{ sender->GenVAO(), false });
    });
}

VAO_Holder::~VAO_Holder()
{
    for (auto wgt : g_GLWidgets)
    {
        auto it = impl->wgt_vao_map.find(wgt->context());
        assert(it != impl->wgt_vao_map.end());
        if (!wgt->DeleteVAO(it->second.first)) assert(false);
    }
}

std::pair<GLuint, bool> VAO_Holder::GetVAO() const
{
    auto current = QOpenGLContext::currentContext();
    auto it = impl->wgt_vao_map.find(current);
    assert(it != impl->wgt_vao_map.end());
    return { it->second.first, it->second.second };
}

void VAO_Holder::VAO_SetReady()
{
    auto current = QOpenGLContext::currentContext();
    auto it = impl->wgt_vao_map.find(current);
    assert(it != impl->wgt_vao_map.end());
    it->second.second = true;
}
