#pragma once

#include "labels/label.h"

#include <glm/glm.hpp>

namespace Tangram {

class TextLabels;
class TextStyle;

struct GlyphQuad {
    size_t atlas;
    struct {
        glm::i16vec2 pos;
        glm::u16vec2 uv;
    } quad[4];
};

struct TextVertex {
    glm::i16vec2 pos;
    glm::u16vec2 uv;
    struct State {
        uint32_t color;
        uint32_t stroke;
        uint16_t alpha;
        uint16_t scale;
    } state;

    const static float position_scale;
    const static float alpha_scale;
};

class TextLabel : public Label {

public:

    struct FontVertexAttributes {
        uint32_t fill;
        uint32_t stroke;
        uint8_t fontScale;
    };

    TextLabel(Label::Transform _transform, Type _type, Label::Options _options,
              LabelProperty::Anchor _anchor, TextLabel::FontVertexAttributes _attrib,
              glm::vec2 _dim, TextLabels& _labels, Range _vertexRange);

    void updateBBoxes(float _zoomFract) override;

    Range& quadRange() { return m_vertexRange; }

protected:

    void pushTransform() override;

private:
    void applyAnchor(const glm::vec2& _dimension, const glm::vec2& _origin,
                     LabelProperty::Anchor _anchor) override;

    // Back-pointer to owning container
    const TextLabels& m_textLabels;
    // first vertex and count in m_textLabels quads
    Range m_vertexRange;

    FontVertexAttributes m_fontAttrib;
};

}
