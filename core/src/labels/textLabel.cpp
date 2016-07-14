#include "textLabel.h"
#include "textLabels.h"
#include "style/textStyle.h"
#include "text/fontContext.h"
#include "gl/dynamicQuadMesh.h"
#include "util/geom.h"
#include "util/lineSampler.h"

namespace Tangram {

using namespace LabelProperty;

const float TextVertex::position_scale = 4.0f;
const float TextVertex::alpha_scale = 65535.0f;

TextLabel::TextLabel(Label::Transform _transform, Type _type, Label::Options _options,
                     Anchor _anchor, TextLabel::FontVertexAttributes _attrib,
                     glm::vec2 _dim,  TextLabels& _labels, Range _vertexRange,
                     size_t _anchorPoint, const std::vector<glm::vec2>& _line)
    : Label(_transform, _dim, _type, _options, _anchor),
      m_textLabels(_labels),
      m_vertexRange(_vertexRange),
      m_fontAttrib(_attrib),
      m_anchorPoint(_anchorPoint),
      m_line(_line) {

    applyAnchor(_dim, glm::vec2(0.0), _anchor);
}

void TextLabel::applyAnchor(const glm::vec2& _dimension, const glm::vec2& _origin, Anchor _anchor) {
    m_anchor = _origin + LabelProperty::anchorDirection(_anchor) * _dimension * 0.5f;
}

bool TextLabel::updateScreenTransform(const glm::mat4& _mvp, const glm::vec2& _screenSize,
                                      bool _testVisibility, ScreenTransform& _transform) {

    bool clipped = false;

    if (m_type == Type::point || m_type == Type::debug) {
        glm::vec2 p0 = m_transform.modelPosition1;

        glm::vec2 screenPosition = worldToScreenSpace(_mvp, glm::vec4(p0, 0.0, 1.0),
                                                      _screenSize, clipped);

        if (_testVisibility && clipped) {
            return false;
        }

        screenPosition += m_anchor;
        screenPosition.x += m_options.offset.x;
        screenPosition.y -= m_options.offset.y;

        m_transform.state.screenPos = screenPosition;

        return true;
    }

    if (m_type == Type::line) {

        glm::vec2 rotation = {1, 0};

        // project label position from mercator world space to screen
        // coordinates
        glm::vec2 p0 = m_transform.modelPosition1;
        glm::vec2 p2 = m_transform.modelPosition2;

        glm::vec2 ap0 = worldToScreenSpace(_mvp, glm::vec4(p0, 0.0, 1.0),
                                           _screenSize, clipped);
        glm::vec2 ap2 = worldToScreenSpace(_mvp, glm::vec4(p2, 0.0, 1.0),
                                           _screenSize, clipped);

        // check whether the label is behind the camera using the
        // perspective division factor
        if (_testVisibility && clipped) {
            return false;
        }

        float length = glm::length(ap2 - ap0);

        // default heuristic : allow label to be 30% wider than segment
        float minLength = m_dim.x * 0.7;

        if (_testVisibility && length < minLength) {
            return false;
        }

        glm::vec2 p1 = glm::vec2(p2 + p0) * 0.5f;

        // Keep screen position center at world center (less sliding in tilted view)
        glm::vec2 screenPosition = worldToScreenSpace(_mvp, glm::vec4(p1, 0.0, 1.0),
                                                      _screenSize, clipped);


        rotation = (ap0.x <= ap2.x ? ap2 - ap0 : ap0 - ap2) / length;

        glm::vec2 offset = rotateBy(m_options.offset, rotation);
        m_transform.state.screenPos = screenPosition + glm::vec2(offset.x, -offset.y);
        m_transform.state.rotation = rotation;

        return true;
    }

    /* Label::Type::curved */

    m_sampler.clearPoints();

    bool inside = false;

    for (auto& p : m_line) {
        glm::vec2 sp = worldToScreenSpace(_mvp, glm::vec4(p, 0.0, 1.0),
                                          _screenSize, clipped);

        if (clipped) { return false; }

        m_sampler.add(sp);

        if (!inside){
            if ((sp.x >= 0 && sp.x <= _screenSize.x) ||
                (sp.y >= 0 && sp.y <= _screenSize.y)) {
                inside = true;
            }
        }
    }

    float length = m_sampler.sumLength();

    if (!inside || length < m_dim.x) {
        return false;
    }

    float center = m_sampler.point(m_anchorPoint).length;

    if (center - m_dim.x * 0.5f < 0 || center + m_dim.x * 0.5f > length) {
        return false;
    }

    return true;
}

Range TextLabel::obbs(const ScreenTransform& _transform, std::vector<OBB>& _obbs) {

    auto first = int(_obbs.size());

    glm::vec2 dim = m_dim - m_options.buffer;

    if (m_occludedLastFrame) { dim += Label::activation_distance_threshold; }

    if (m_type == Label::Type::curved) {

        float width = dim.x;

        auto center = m_sampler.point(m_anchorPoint).length;
        auto start = center - width * 0.5f;

        glm::vec2 p1, p2, rotation;
        m_sampler.sample(start, p1, rotation);

        float prevLength = start;

        int cnt = 0;
        for (size_t i = m_sampler.curSegment()+1; i < m_sampler.m_points.size(); i++) {

            float currLength = m_sampler.point(i).length;
            float segmentLength = currLength - prevLength;

            cnt++;

            if (start + width > currLength) {
                p2 = m_sampler.point(i).coord;

                rotation = m_sampler.segmentDirection(i-1);
                _obbs.push_back({(p1 + p2) * 0.5f, rotation, segmentLength, dim.y});

                prevLength = currLength;
                p1 = p2;
            } else {

                segmentLength = (start + width) - prevLength;
                m_sampler.sample(start + width, p2, rotation);
                _obbs.push_back({(p1 + p2) * 0.5f, rotation, segmentLength, dim.y});
                break;
            }
        }

        return { first, cnt };

    } else {

        // FIXME: Only for testing
        if (state() == State::dead) { dim -= 4; }

        auto obb = OBB(m_transform.state.screenPos,
                       m_transform.state.rotation,
                       dim.x, dim.y);

        _obbs.push_back(obb);

        return { first, 1 };
    }
}

void TextLabel::pushTransform(ScreenTransform& _transform) {
    if (!visibleState()) { return; }

    bool rotate = (m_transform.state.rotation.x != 1.f);
    glm::vec2 rotation = {m_transform.state.rotation.x, -m_transform.state.rotation.y};

    TextVertex::State state {
        m_fontAttrib.fill,
        m_fontAttrib.stroke,
        uint16_t(m_transform.state.alpha * TextVertex::alpha_scale),
        uint16_t(m_fontAttrib.fontScale),
    };

    auto it = m_textLabels.quads.begin() + m_vertexRange.start;
    auto end = it + m_vertexRange.length;
    auto& style = m_textLabels.style;

    if (m_type == Label::Type::curved) {

        float width = m_dim.x;

        if (m_sampler.sumLength() < width) { return; }

        float center = m_sampler.point(m_anchorPoint).length;

        glm::vec2 p1, p2;
        m_sampler.sample(center + it->quad[0].pos.x / TextVertex::position_scale, p1, rotation);
        // Check based on first charater whether labels needs to be flipped
        // sampler.sample(center + it->quad[2].pos.x, p2, rotation);
        m_sampler.sample(center + (end-1)->quad[2].pos.x / TextVertex::position_scale, p2, rotation);


        if (p1.x > p2.x) {
            m_sampler.reversePoints();
            center = m_sampler.sumLength() - center;
        }

        // if (center < width * 0.5f) {
        //     center = width * 0.5f;
        // } else if (sampler.sumLength() - center < width * 0.5f) {
        //     center = width * 0.5f;
        // }

        for (; it != end; ++it) {
            auto quad = *it;

            glm::vec2 origin = {(quad.quad[0].pos.x + quad.quad[2].pos.x) * 0.5f, 0 };
            glm::vec2 point;

            if (!m_sampler.sample(center + origin.x / TextVertex::position_scale, point, rotation)) {
                // break;
            }

            point *= TextVertex::position_scale;
            rotation = {rotation.x, -rotation.y};

            auto* quadVertices = style.getMesh(it->atlas).pushQuad();

            for (int i = 0; i < 4; i++) {
                TextVertex& v = quadVertices[i];

                v.pos = glm::i16vec2{point + rotateBy(glm::vec2(quad.quad[i].pos) - origin, rotation)};

                v.uv = quad.quad[i].uv;
                v.state = state;
            }
        }

    } else {
        glm::i16vec2 sp = glm::i16vec2(m_transform.state.screenPos * TextVertex::position_scale);

        for (; it != end; ++it) {
            auto quad = *it;
            auto* quadVertices = style.getMesh(it->atlas).pushQuad();

            for (int i = 0; i < 4; i++) {
                TextVertex& v = quadVertices[i];
                if (rotate) {
                    v.pos = sp + glm::i16vec2{rotateBy(quad.quad[i].pos, rotation)};
                } else {
                    v.pos = sp + quad.quad[i].pos;
                }
                v.uv = quad.quad[i].uv;
                v.state = state;
            }
        }
    }
}

TextLabels::~TextLabels() {
    style.context()->releaseAtlas(m_atlasRefs);
}

void TextLabels::setQuads(std::vector<GlyphQuad>&& _quads, std::bitset<FontContext::max_textures> _atlasRefs) {
    quads = std::move(_quads);
    m_atlasRefs = _atlasRefs;

}

}
