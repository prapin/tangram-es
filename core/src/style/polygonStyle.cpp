#include "polygonStyle.h"

#include "tangram.h"
#include "platform.h"
#include "material.h"
#include "util/builders.h"
#include "util/extrude.h"
#include "gl/shaderProgram.h"
#include "gl/typedMesh.h"
#include "tile/tile.h"
#include "scene/drawRule.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/gtc/type_precision.hpp"

#include <cmath>

constexpr float position_scale = 8192.0f;
constexpr float texture_scale = 65535.0f;
constexpr float normal_scale = 127.0f;

namespace Tangram {

struct PolygonVertex {

    PolygonVertex(glm::vec3 position, uint32_t order,
                  glm::vec3 normal, glm::vec2 uv, GLuint abgr)
        : pos(glm::i16vec4{ glm::round(position * position_scale), order }),
          norm(normal * normal_scale),
          texcoord(uv * texture_scale),
          abgr(abgr) {}

    glm::i16vec4 pos; // pos.w contains layer (params.order)
    glm::i8vec3 norm;
    uint8_t padding = 0;
    glm::u16vec2 texcoord;
    GLuint abgr;
};


PolygonStyle::PolygonStyle(std::string _name, Blending _blendMode, GLenum _drawMode)
    : Style(_name, _blendMode, _drawMode) {}

void PolygonStyle::constructVertexLayout() {

    m_vertexLayout = std::shared_ptr<VertexLayout>(new VertexLayout({
        {"a_position", 4, GL_SHORT, false, 0},
        {"a_normal", 4, GL_BYTE, true, 0}, // The 4th byte is for padding
        {"a_texcoord", 2, GL_UNSIGNED_SHORT, true, 0},
        {"a_color", 4, GL_UNSIGNED_BYTE, true, 0},
    }));

}

void PolygonStyle::constructShaderProgram() {

    std::string vertShaderSrcStr = stringFromFile("shaders/polygon.vs", PathType::internal);
    std::string fragShaderSrcStr = stringFromFile("shaders/polygon.fs", PathType::internal);

    m_shaderProgram->setSourceStrings(fragShaderSrcStr, vertShaderSrcStr);
}

using Mesh = TypedMesh<PolygonVertex>;

struct PolygonStyleBuilder : public StyleBuilder {

    const PolygonStyle& m_style;

    MeshData<PolygonVertex> m_meshData;

    std::unique_ptr<Mesh> m_mesh;
    float m_tileUnitsPerMeter;
    int m_zoom;

    struct {
        uint32_t order = 0;
        uint32_t color = 0xff00ffff;
        glm::vec2 extrude;
        float height;
        float minHeight;
    } m_params;

    void begin(const Tile& _tile) override {
        m_tileUnitsPerMeter = _tile.getInverseScale();
        m_zoom = _tile.getID().z;
        m_mesh = std::make_unique<Mesh>(m_style.vertexLayout(), m_style.drawMode());
        m_meshData.clear();
    }

    void addPolygon(const Polygon& _polygon, const Properties& _props, const DrawRule& _rule) override;

    const Style& style() const override { return m_style; }

    std::unique_ptr<VboMesh> build() override;

    PolygonStyleBuilder(const PolygonStyle& _style) : StyleBuilder(_style), m_style(_style) {}

    void parseRule(const DrawRule& _rule, const Properties& _props);

    PolygonBuilder m_builder;
};

std::unique_ptr<VboMesh> PolygonStyleBuilder::build() {
    auto mesh = std::make_unique<Mesh>(m_style.vertexLayout(), m_style.drawMode());

    mesh->compile(m_meshData);
    m_meshData.clear();

    return std::move(mesh);
}


void PolygonStyleBuilder::parseRule(const DrawRule& _rule, const Properties& _props) {
    _rule.get(StyleParamKey::color, m_params.color);
    _rule.get(StyleParamKey::extrude, m_params.extrude);
    _rule.get(StyleParamKey::order, m_params.order);

    if (Tangram::getDebugFlag(Tangram::DebugFlags::proxy_colors)) {
        m_params.color <<= (m_zoom % 6);
    }

    auto& extrude = m_params.extrude;
    m_params.minHeight = getLowerExtrudeMeters(extrude, _props) * m_tileUnitsPerMeter;
    m_params.height = getUpperExtrudeMeters(extrude, _props) * m_tileUnitsPerMeter;

}

void PolygonStyleBuilder::addPolygon(const Polygon& _polygon, const Properties& _props, const DrawRule& _rule) {

    parseRule(_rule, _props);

    m_builder.addVertex = [this](const glm::vec3& coord,
                                 const glm::vec3& normal,
                                 const glm::vec2& uv) {
        m_meshData.vertices.push_back({ coord, m_params.order,
                                        normal, uv, m_params.color });
    };

    if (m_params.minHeight != m_params.height) {
        Builders::buildPolygonExtrusion(_polygon, m_params.minHeight,
                                        m_params.height, m_builder);
    }

    Builders::buildPolygon(_polygon, m_params.height, m_builder);

    auto& m = m_meshData;
    m.indices.insert(m.indices.end(),
                     m_builder.indices.begin(),
                     m_builder.indices.end());

    m.offsets.emplace_back(m_builder.indices.size(),
                           m_builder.numVertices);
    m_builder.clear();
}

std::unique_ptr<StyleBuilder> PolygonStyle::createBuilder() const {
    return std::make_unique<PolygonStyleBuilder>(*this);
}

}
