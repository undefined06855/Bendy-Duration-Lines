#include "DrawGridLayer.hpp"

HookedDrawGridLayer::Fields::Fields()
    : m_seenRotatedGameplayThisObject(false)
    , m_resolution(geode::Mod::get()->getSettingValue<double>("precision"))
    , m_cull(geode::Mod::get()->getSettingValue<bool>("cull-offscreen"))
    , m_limit(geode::Mod::get()->getSettingValue<int64_t>("draw-limit"))
    , m_debug(geode::Mod::get()->getSettingValue<bool>("debug")) {}

void HookedDrawGridLayer::draw() {
    auto fields = m_fields.self();

    bool orig = m_editorLayer->m_showDurationLines;
    m_editorLayer->m_showDurationLines = false;
    DrawGridLayer::draw(); // draw but never draw duration lines
    m_editorLayer->m_showDurationLines = orig;

    // these checks are done at the start of the snippet i replace
    // ideally replacing  0x2dcdc9 to 0x2dd182 (win)
    if (m_editorLayer->m_playbackMode == PlaybackMode::Playing || !m_editorLayer->m_showDurationLines) return;
    if (!m_editorLayer->m_durationObjects) return;
    
    auto durationObjectCount = m_editorLayer->m_durationObjects->count();
    if (durationObjectCount == 0) return;
    

    unsigned int count = 0;
    for (auto obj : geode::cocos::CCArrayExt<EffectGameObject*>(m_editorLayer->m_durationObjects)) {
        if (!obj) continue;
        if (!obj->getParent() && fields->m_cull) continue;

        drawLinesForObject(obj);

        if (++count > fields->m_limit) break;
    }

    // debug - draw speedStart
    // note: all things related to speeds are legacy names - also includes arrow
    // triggers and reverse triggers
    if (fields->m_debug) {
        for (auto obj : geode::cocos::CCArrayExt<EffectGameObject*>(m_speedObjects)) {
            cocos2d::ccDrawColor4B(255, 0, 0, 255);
            cocos2d::ccDrawCircle(obj->m_speedStart, 3, 0.f, 5, false);
        }
    }
}

void HookedDrawGridLayer::drawLinesForObject(EffectGameObject* obj) {
    auto fields = m_fields.self();
    std::vector<LinePoint> points = {};

    // should we actually draw this object?
    if (
        // 0121 - hide invisible checkbox in editor
        (GameManager::get()->getGameVariable("0121") && obj->m_isHide && !obj->m_isSelected)
        || obj->m_isSpawnTriggered || obj->m_isTouchTriggered
        || (obj->m_isGroupDisabled && !obj->m_isSelected)
    ) return;

    auto durations = durationForObject(obj);

    float startTime = LevelTools::timeForPos(
        obj->getRealPosition(), m_speedObjects,
        (int)m_editorLayer->m_levelSettings->m_startSpeed, obj->m_ordValue,
        obj->m_channelValue, false,
        m_editorLayer->m_levelSettings->m_platformerMode, true, false, 0
    );

    if (durations.m_fadeInDuration >= 0.01f) {
        for (float t = 0; t < durations.m_fadeInDuration; t += fields->m_resolution) {
            points.push_back(LinePoint{
                .m_col = colorForObject(obj, LinePart::FadeIn),
                .m_pos = posForTime(startTime + t, obj)
            });
        }
    }

    startTime += durations.m_fadeInDuration;

    if (durations.m_baseDuration >= 0.01f) {
        for (float t = 0; t < durations.m_baseDuration; t += fields->m_resolution) {
            points.push_back(LinePoint{
                .m_col = colorForObject(obj, LinePart::Base),
                .m_pos = posForTime(startTime + t, obj)
            });
        }
    }

    startTime += durations.m_baseDuration;

    if (durations.m_fadeOutDuration >= 0.01f) {
        for (float t = 0; t < durations.m_fadeOutDuration; t += fields->m_resolution) {
            points.push_back(LinePoint{
                .m_col = colorForObject(obj, LinePart::FadeOut),
                .m_pos = posForTime(startTime + t, obj)
            });
        }
    }

    drawLines(obj->getRealPosition(), points);
    m_fields->m_seenRotatedGameplayThisObject = false;
}

ObjectDuration HookedDrawGridLayer::durationForObject(EffectGameObject* obj) {
    int id = obj->m_objectID;
    ObjectDuration ret;

    ret.m_baseDuration = obj->m_duration;

    if (id == 1006) { // pulse trigger
        ret.m_baseDuration = obj->m_holdDuration;
        ret.m_fadeInDuration = obj->m_fadeInDuration;
        ret.m_fadeOutDuration = obj->m_fadeOutDuration;
    } else if (id == 3602) { // sfx trigger
        auto cast = static_cast<SFXTriggerGameObject*>(obj);
        ret.m_baseDuration = cast->m_soundDuration;
        ret.m_fadeInDuration = cast->m_fadeIn / 1000.f;
        ret.m_fadeOutDuration = cast->m_fadeOut / 1000.f;
    } else {
        ret.m_fadeInDuration = 0.f;
        ret.m_fadeOutDuration = 0.f;
    }

    return ret;
}

cocos2d::CCPoint HookedDrawGridLayer::posForTime(float time, EffectGameObject* obj) {
    auto fields = m_fields.self();

    auto ret = LevelTools::posForTimeInternal(
        time,
        m_speedObjects,
        (int)m_editorLayer->m_levelSettings->m_startSpeed,
        m_editorLayer->m_levelSettings->m_platformerMode,
        false,
        true,
        m_editorLayer->m_gameState.m_rotateChannel,
        1 // unused
    );

    /*
     * right so fellas why is this needed
     * when leveltools ends up finishing posForTime on rotated gameplay,
     * getLastGameplayRotated returns true and the y value of the position given
     * is relative to the object, not 0, meaning i should add the y position of
     * the object whenever the calculations finish on unrotated gameplay
     * (2.2 new code, odd but works for rob and in most cases)
     *
     * now that's all fine and dandy except for some reason if leveltools
     * ends up on unrotated gameplay but the unrotated gameplay happens to also
     * be what it starts on (it starts on unrotated gameplay but also ends on
     * the same block of unrotated gameplay), the y position is ALWAYS relative
     * to zero, so i ALWAYS need to add y position
     * (2.1 legacy code?)
     *
     * tl;dr - dont add y if gameplay ends on unrotated (2.2) UNLESS we have not
     * seen rotated gameplay (2.1), in which case ALWAYS add y
     *
     * m_seenRotatedGameplay gets reset at the end of drawLinesForObject
     * note: this explanation may have contradicted itself or something, ask if
     * you want a diagram or if it doesnt make sense or something
     * also note the red channel of debug points store if rotated gameplay has
     * been seen on this object
    */

    if (LevelTools::getLastGameplayRotated()) {
        fields->m_seenRotatedGameplayThisObject = true;
    }

    if (!LevelTools::getLastGameplayRotated() && !fields->m_seenRotatedGameplayThisObject) {
        ret.y += obj->getRealPosition().y;
    }

    // debug - draw points
    if (fields->m_debug) {
        cocos2d::ccDrawColor4B(
            fields->m_seenRotatedGameplayThisObject ? 255 : 0,
            LevelTools::getLastGameplayRotated() ? 255 : 0,
            255,
            255
        );
        cocos2d::ccDrawCircle(ret, 5, 0.314159f, 5, false);
    }

    return ret;
}

cocos2d::ccColor4B HookedDrawGridLayer::colorForObject(EffectGameObject* obj, LinePart part) {
    if (obj->m_isColorTrigger || obj->m_objectID == 1006) {
        cocos2d::ccColor4B ret = {
            obj->m_triggerTargetColor.r,
            obj->m_triggerTargetColor.g,
            obj->m_triggerTargetColor.b,
            255
        };

        if (part == LinePart::FadeIn) {
            ret = tintColor(ret, +35);
        } else if (part == LinePart::FadeOut) {
            ret = tintColor(ret, -25);
        }

        return ret;
    }

    switch(part) {
        case LinePart::FadeIn:
            return { 75, 75, 75, 75 };
        case LinePart::Base:
            return { 100, 100, 100, 75 };
        case LinePart::FadeOut:
            return { 125, 125, 125, 75 };
    }
}

constexpr float hue2rgb(float p, float q, float t) {
    if (t < 0.f) t += 1.f;
    if (t > 1.f) t -= 1.f;
    if (t < 1.f / 6.f) return p + (q - p) * 6.f * t;
    if (t < 1.f / 2.f) return q;
    if (t < 2.f / 3.f) return p + (q - p) * (2.f / 3.f - t) * 6.f;
    return p;
}

cocos2d::ccColor4B HookedDrawGridLayer::tintColor(cocos2d::ccColor4B col, int amount) {
    // convert to hsl, tint, convert back
    // most of this taken from eclipse though it really couldve been taken from
    // anywhere

    auto floatCol = cocos2d::ccc4FFromccc4B(col);

    // convert to hsl
    auto max = std::max({floatCol.r, floatCol.g, floatCol.b});
    auto min = std::min({floatCol.r, floatCol.g, floatCol.b});
    float h, s, l = (max + min) / 2.f;

    if (max == min) {
        h = s = 0;
    } else {
        float d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);

        if (max == floatCol.r) {
            h = (floatCol.g - floatCol.b) / d + (floatCol.g < floatCol.b ? 6.f : 0.f);
        } else if (max == floatCol.g) {
            h = (floatCol.b - floatCol.r) / d + 2.f;
        } else {
            h = (floatCol.r - floatCol.g) / d + 4.f;
        }

        h /= 6.f;
    }

    // do the tint
    l = std::max(0.f, std::min(l + amount / 100.f, 1.f));

    // convert back to rgb
    float r, g, b;

    if (s == 0.f) {
        r = g = b = l;
    } else {
        auto q = l < .5f ? l * (1.f + s) : l + s - l * s;
        auto p = 2.f * l - q;
        r = hue2rgb(p, q, h + 1.f / 3.f);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.f / 3.f);
    }

    return { static_cast<GLubyte>(r*255), static_cast<GLubyte>(g*255), static_cast<GLubyte>(b*255), col.a };
}

void HookedDrawGridLayer::drawLines(const cocos2d::CCPoint& start, const std::vector<LinePoint>& segments) {
    std::vector<LinePoint> allSegments;
    allSegments.resize(segments.size() + 1);
    allSegments[0] = LinePoint{ .m_pos = start };
    std::copy(segments.begin(), segments.end(), allSegments.begin() + 1);

    for (int i = 0; i < allSegments.size() - 1; i++) {
        cocos2d::CCPoint from = allSegments[i].m_pos;
        cocos2d::CCPoint to = allSegments[i + 1].m_pos;
        
        auto col = allSegments[i + 1].m_col;
        cocos2d::ccDrawColor4B(col.r, col.g, col.b, col.a);
        cocos2d::ccDrawLine(from, to);
    }
}
