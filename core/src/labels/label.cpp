#include "label.h"

#include "util/geom.h"
#include "glm/gtx/rotate_vector.hpp"
#include "tangram.h"

namespace Tangram {

const float Label::activation_distance_threshold = 2;

Label::Label(Label::Transform _transform, glm::vec2 _size, Type _type, Options _options, LabelProperty::Anchor _anchor)
    : m_type(_type),
      m_transform(_transform),
      m_dim(_size),
      m_options(_options),
      m_anchorType(_anchor) {

    if (!m_options.collide || m_type == Type::debug){
        enterState(State::visible, 1.0);
    } else {
        m_state = State::wait_occ;
        m_transform.state.alpha = 0.0;
    }

    m_occludedLastFrame = false;
    m_occluded = false;
    m_parent = nullptr;
}

Label::~Label() {}

void Label::setParent(const Label& _parent, bool _definePriority) {
    m_parent = &_parent;

    glm::vec2 anchorDir = LabelProperty::anchorDirection(_parent.anchorType());
    glm::vec2 anchorOrigin = anchorDir * _parent.dimension() * 0.5f;
    applyAnchor(m_dim + _parent.dimension(), anchorOrigin, m_anchorType);

    if (_definePriority) {
        m_options.priority = _parent.options().priority + 0.5f;
    }

    m_options.offset += _parent.options().offset;
}

bool Label::offViewport(const glm::vec2& _screenSize) {
    // const auto& quad = m_obb.getQuad();

    // for (int i = 0; i < 4; ++i) {
    //     const auto& p = quad[i];
    //     if (p.x < _screenSize.x && p.y < _screenSize.y && p.x > 0 && p.y > 0) {
    //         return false;
    //     }
    // }

    // return true;
    return false;
}

bool Label::canOcclude() {
    if (!m_options.collide) {
        return false;
    }

    int occludeFlags = (State::visible |
                        State::wait_occ |
                        State::skip_transition |
                        State::fading_in |
                        State::sleep |
                        State::out_of_screen |
                        State::dead);

    return (occludeFlags & m_state) && !(m_type == Type::debug);
}

bool Label::visibleState() const {
    int visibleFlags = (State::visible |
                        State::fading_in |
                        State::fading_out |
                        State::skip_transition);

    return (visibleFlags & m_state);
}

void Label::skipTransitions() {
    enterState(State::skip_transition, 0.0);
}

glm::vec2 Label::center() const {
    return m_transform.state.screenPos;
}

void Label::enterState(const State& _state, float _alpha) {
    if (m_state == State::dead) { return; }

    m_state = _state;
    setAlpha(_alpha);
}

void Label::setAlpha(float _alpha) {
    m_transform.state.alpha = CLAMP(_alpha, 0.0, 1.0);
}

void Label::resetState() {
    if (m_state == State::dead) { return; }

    m_occludedLastFrame = false;
    m_occluded = false;
    enterState(State::wait_occ, 0.0);
}

bool Label::update(const glm::mat4& _mvp, const glm::vec2& _screenSize, float _zoomFract,
                   bool _allLabels, ScreenTransform& _transform) {

    m_occludedLastFrame = m_occluded;
    m_occluded = false;

    if (m_state == State::dead) {
        if (!_allLabels) {
            return false;
        } else {
            m_occluded = true;
        }
    }

    bool ruleSatisfied = updateScreenTransform(_mvp, _screenSize, !_allLabels, _transform);

    // one of the label rules has not been satisfied
    if (!ruleSatisfied) {
        enterState(State::sleep, 0.0);
        return false;
    }

    // update the view-space bouding box
    // updateBBoxes(_zoomFract);

    // // checks whether the label is out of the viewport
    // if (offViewport(_screenSize)) {
    //     enterState(State::out_of_screen, 0.0);
    //     if (m_occludedLastFrame) {
    //         m_occluded = true;
    //         return false;
    //     }
    // } else if (m_state == State::out_of_screen) {
    //     if (m_occludedLastFrame) {
    //         enterState(State::sleep, 0.0);
    //     } else {
    //         enterState(State::visible, 1.0);
    //     }
    // }

    return true;
}

bool Label::evalState(const glm::vec2& _screenSize, float _dt) {

    // if (Tangram::getDebugFlag(DebugFlags::all_labels)) {
    //     enterState(State::visible, 1.0);
    //     return false;
    // }

    bool animate = false;

    switch (m_state) {
        case State::visible:
            if (m_occluded) {
                m_fade = FadeEffect(false, m_options.hideTransition.ease,
                                    m_options.hideTransition.time);
                enterState(State::fading_out, 1.0);
                animate = true;
            }
            break;
        case State::fading_in:
            if (m_occluded) {
                enterState(State::sleep, 0.0);
                // enterState(State::fading_out, m_transform.state.alpha);
                // animate = true;
                break;
            }
            setAlpha(m_fade.update(_dt));
            animate = true;
            if (m_fade.isFinished()) {
                enterState(State::visible, 1.0);
            }
            break;
        case State::fading_out:
            if (!m_occluded) {
                enterState(State::fading_in, m_transform.state.alpha);
                animate = true;
                break;
            }
            setAlpha(m_fade.update(_dt));
            animate = true;
            if (m_fade.isFinished()) {
                enterState(State::sleep, 0.0);
            }
            break;
        case State::wait_occ:
            if (m_occluded) {
                enterState(State::sleep, 0.0);
            } else {
                m_fade = FadeEffect(true, m_options.showTransition.ease,
                                    m_options.showTransition.time);
                enterState(State::fading_in, 0.0);
                animate = true;
            }
            break;
        case State::skip_transition:
            if (m_occluded) {
                enterState(State::sleep, 0.0);
            } else {
                enterState(State::visible, 1.0);
            }
            break;
        case State::sleep:
            if (!m_occluded) {
                m_fade = FadeEffect(true, m_options.showTransition.ease,
                                    m_options.showTransition.time);
                enterState(State::fading_in, 0.0);
                animate = true;
            }
            break;
        case State::dead:
        case State::out_of_screen:
            break;
    }

    return animate;
}

}
