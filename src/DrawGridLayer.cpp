#include "DrawGridLayer.hpp"
#include "Geode/utils/cocos.hpp"
#include <cmath>

HookedDrawGridLayer::Fields::Fields()
    : m_seenRotatedGameplayThisObject(false)
    , m_lastPointWasRotatedThisObject(false)
    , m_adjustmentThisObject(0.f, 0.f)
    , m_lastPointThisObject(0.f, 0.f)
    
    , m_resolution(geode::Mod::get()->getSettingValue<double>("precision"))
    , m_cullOffscreen(geode::Mod::get()->getSettingValue<bool>("cull-offscreen"))
    , m_cullOtherLayers(geode::Mod::get()->getSettingValue<bool>("cull-other-layers"))
    , m_limit(geode::Mod::get()->getSettingValue<int64_t>("draw-limit"))
    , m_durationLimit(geode::Mod::get()->getSettingValue<int64_t>("duration-limit"))
    , m_ignoreSpawnTriggered(geode::Mod::get()->getSettingValue<bool>("ignore-spawn-triggered"))

    , m_stripOldArrowTriggers(geode::Mod::get()->getSettingValue<bool>("strip-old-arrow-triggers"))
    , m_dontOffsetSecondaryAxis(geode::Mod::get()->getSettingValue<bool>("dont-offset-secondary-axis"))
    , m_ignoreJumpedPoints(geode::Mod::get()->getSettingValue<bool>("ignore-jumped-points"))
    , m_debug(geode::Mod::get()->getSettingValue<bool>("debug")) {}

void HookedDrawGridLayer::draw() {
    auto fields = m_fields.self();

    bool orig = m_editorLayer->m_showDurationLines;
    m_editorLayer->m_showDurationLines = false;
    DrawGridLayer::draw(); // draw but never draw duration lines
    m_editorLayer->m_showDurationLines = orig;

    if (m_editorLayer->m_playbackMode == PlaybackMode::Playing || !m_editorLayer->m_showDurationLines) return;
    if (!m_editorLayer->m_durationObjects) return;

    auto durationObjectsCopy = m_editorLayer->m_durationObjects->shallowCopy();
    durationObjectsCopy->addObjectsFromArray(m_editorLayer->m_spawnTriggers);
    
    auto durationObjectsCount = durationObjectsCopy->count();
    if (durationObjectsCount == 0) return;
    
    glLineWidth(2.f);
    cocos2d::ccGLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int count = 0;
    for (auto obj : geode::cocos::CCArrayExt<EffectGameObject*>(durationObjectsCopy)) {
        if (!obj) continue;
        if (!obj->getParent() && fields->m_cullOffscreen) continue;
        if (
            m_editorLayer->m_currentLayer != obj->m_editorLayer
         && m_editorLayer->m_currentLayer != obj->m_editorLayer2
         && fields->m_cullOtherLayers
         && m_editorLayer->m_currentLayer != -1
        ) continue;

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
            cocos2d::ccDrawColor4B(255, 0, 0, 180);
            cocos2d::ccDrawCircle(obj->m_speedStart, 30, 0.f, 8, false);
        }
    }

    glLineWidth(1.f);
}

void HookedDrawGridLayer::drawLinesForObject(EffectGameObject* obj) {
    auto fields = m_fields.self();
    std::vector<LinePoint> points = {};

    // should we actually draw this object?
    if (
        // 0121 - hide invisible checkbox in editor
        (GameManager::get()->getGameVariable("0121") && (obj->m_isHide || obj->m_isGroupDisabled) && !obj->m_isSelected)
        || (obj->m_isSpawnTriggered && fields->m_ignoreSpawnTriggered)
    ) return;

    auto durations = durationForObject(obj);

    auto speedObjectsCopy = m_speedObjects->shallowCopy();

    // remove all speed objects that are before this object
    // this could be buggy with rotated gameplay
    // TODO: perhaps make this more efficient? should be already sorted by x
    // position and channel or something like that
    if (fields->m_stripOldArrowTriggers) {
        auto speedObjectsToRemove = cocos2d::CCArray::create();
    
        for (auto speedObject : geode::cocos::CCArrayExt<EffectGameObject*>(speedObjectsCopy)) {
            if (speedObject->getRealPosition().x >= obj->getRealPosition().x) continue; // in front of trigger
            if (speedObject->m_speedModType >= -1) continue; // speed change or just reverse

            speedObjectsToRemove->addObject(speedObject);
        }
    
        speedObjectsCopy->removeObjectsInArray(speedObjectsToRemove);
    }


    float startTime = LevelTools::timeForPos(
        obj->getRealPosition(), speedObjectsCopy,
        (int)m_editorLayer->m_levelSettings->m_startSpeed,
        obj->m_ordValue, obj->m_channelValue,
        false,
        m_editorLayer->m_levelSettings->m_platformerMode,
        true, false, 69
    );

    float actualStartTime = startTime;

    // fade in line
    if (durations.m_fadeInDuration >= 0.01f) {
        auto col = colorForObject(obj, LinePart::FadeIn);
        for (float t = 0; t < durations.m_fadeInDuration; t += fields->m_resolution) {
            points.push_back(LinePoint{
                .m_col = col,
                .m_pos = posForTime(startTime + t, obj, speedObjectsCopy)
            });
        }
    }

    startTime += durations.m_fadeInDuration;

    // base duration line
    if (durations.m_baseDuration >= 0.01f) {
        auto col = colorForObject(obj, LinePart::Base);
        for (float t = 0; t < durations.m_baseDuration; t += fields->m_resolution) {
            points.push_back(LinePoint{
                .m_col = col,
                .m_pos = posForTime(startTime + t, obj, speedObjectsCopy)
            });
        }
    }

    startTime += durations.m_baseDuration;

    // fade out line
    if (durations.m_fadeOutDuration >= 0.01f) {
        auto col = colorForObject(obj, LinePart::FadeOut);
        for (float t = 0; t < durations.m_fadeOutDuration; t += fields->m_resolution) {
            points.push_back(LinePoint{
                .m_col = col,
                .m_pos = posForTime(startTime + t, obj, speedObjectsCopy)
            });
        }
    }

    startTime += durations.m_fadeOutDuration;

    bool isSpawnTrigger = obj->m_objectID == 1268;
    bool isShakeTrigger = obj->m_objectID == 1520;

    if (isShakeTrigger) {
        // draw with zigzag
        // pass in obj and not object position because we need to use it later
        // same with speedObjectsCopy
        drawLinesWithZigZag(obj, points, obj->m_shakeStrength, obj->m_shakeInterval, speedObjectsCopy, actualStartTime);
    } else if (isSpawnTrigger) {
        // draw small vertical line at the end
        auto cast = static_cast<SpawnTriggerGameObject*>(obj);
        
        auto col = colorForObject(obj, LinePart::Base);
        cocos2d::ccDrawColor4B(col.r, col.g, col.b, col.a);

        PosForTime end;
        if (points.size() != 0) {
            end = points.back().m_pos;
        } else {
            end = {
                .m_pos = obj->getRealPosition(),
                .m_isRotated = false
            };
        }

        if (cast->m_delayRange < 0.01f) {
            if (end.m_isRotated) {
                cocos2d::ccDrawLine(
                    { end.m_pos.x + 4, end.m_pos.y },
                    { end.m_pos.x - 4, end.m_pos.y }
                );
            } else {
                cocos2d::ccDrawLine(
                    { end.m_pos.x, end.m_pos.y + 4 },
                    { end.m_pos.x, end.m_pos.y - 4 }
                );
            }
        } else {
            auto left = posForTime(startTime - cast->m_delayRange, obj, speedObjectsCopy);
            auto right = posForTime(startTime + cast->m_delayRange, obj, speedObjectsCopy);

            // draw the lines for left, right and end, making sure they get
            // rotated if needed
            if (left.m_isRotated) {
                cocos2d::ccDrawLine(
                    { left.m_pos.x + 8, left.m_pos.y },
                    { left.m_pos.x - 8, left.m_pos.y }
                );
            } else {
                cocos2d::ccDrawLine(
                    { left.m_pos.x, left.m_pos.y + 8 },
                    { left.m_pos.x, left.m_pos.y - 8 }
                );
            }

            if (end.m_isRotated) {
                cocos2d::ccDrawLine(
                    { end.m_pos.x + 4, end.m_pos.y },
                    { end.m_pos.x - 4, end.m_pos.y }
                );
            } else {
                cocos2d::ccDrawLine(
                    { end.m_pos.x, end.m_pos.y + 4 },
                    { end.m_pos.x, end.m_pos.y - 4 }
                );
            }

            if (right.m_isRotated) {
                cocos2d::ccDrawLine(
                    { right.m_pos.x + 8, right.m_pos.y },
                    { right.m_pos.x - 8, right.m_pos.y }
                );    
            } else {
                cocos2d::ccDrawLine(
                    { right.m_pos.x, right.m_pos.y + 8 },
                    { right.m_pos.x, right.m_pos.y - 8 }
                );    
            }

            // draw dashed lines up to right
            points.push_back({
                .m_col = colorForObject(obj, LinePart::Base),
                .m_pos = right
            });

            // make all points after left half opacity
            for (auto& point : points) {
                if (point.m_pos.m_time > startTime - cast->m_delayRange) {
                    point.m_col.a /= 2;
                }
            }
        }

        // true makes the line forced as dashed for spawn triggers
        drawLines(obj->getRealPosition(), points, true);
    } else {
        // normal objects
        drawLines(obj->getRealPosition(), points, false);
    }

    fields->m_seenRotatedGameplayThisObject = false;
    fields->m_lastPointWasRotatedThisObject = false;
    fields->m_adjustmentThisObject = cocos2d::CCPoint{ 0.f, 0.f };
    fields->m_lastPointThisObject = cocos2d::CCPoint{ 0.f, 0.f };
}

ObjectDuration HookedDrawGridLayer::durationForObject(EffectGameObject* obj) {
    auto fields = m_fields.self();

    int id = obj->m_objectID;
    ObjectDuration ret;

    ret.m_baseDuration = obj->m_duration;

    switch(id) {
        case 1006: { // pulse
            ret.m_baseDuration = obj->m_holdDuration;
            ret.m_fadeInDuration = obj->m_fadeInDuration;
            ret.m_fadeOutDuration = obj->m_fadeOutDuration;
            break;
        }
        
        case 3602: { // sfx
            auto cast = static_cast<SFXTriggerGameObject*>(obj);
            ret.m_baseDuration = cast->m_soundDuration;
            ret.m_fadeInDuration = cast->m_fadeIn / 1000.f;
            ret.m_fadeOutDuration = cast->m_fadeOut / 1000.f;
            break;
        }

        case 1268: // spawn
            ret.m_baseDuration = static_cast<SpawnTriggerGameObject*>(obj)->m_spawnDelay;
        default:
            ret.m_fadeInDuration = 0.f;
            ret.m_fadeOutDuration = 0.f;
    }

    auto total = ret.m_fadeInDuration + ret.m_baseDuration + ret.m_fadeOutDuration;
    if (total > fields->m_durationLimit) {
        float excess = total - fields->m_durationLimit;

        float toRemove = std::min(ret.m_fadeOutDuration, excess);
        ret.m_fadeOutDuration -= toRemove;
        excess -= toRemove;

        if (excess > 0.f) {
            toRemove = std::min(ret.m_baseDuration, excess);
            ret.m_baseDuration -= toRemove;
            excess -= toRemove;
        }

        if (excess > 0.f) {
            ret.m_fadeInDuration -= std::min(ret.m_fadeInDuration, excess);
        }
    }

    return ret;
}

PosForTime HookedDrawGridLayer::posForTime(float time, EffectGameObject* obj, cocos2d::CCArray* speedObjects) {
    auto fields = m_fields.self();

    PosForTime ret;

    // the "current" posForTime/timeForPos position, global var in LevelTools
    // (*(cocos2d::CCPoint*)(geode::base::get() + 0x6a41c0))

    auto point = LevelTools::posForTimeInternal(
        time,
        speedObjects,
        (int)m_editorLayer->m_levelSettings->m_startSpeed,
        m_editorLayer->m_levelSettings->m_platformerMode,
        false,
        true,
        m_editorLayer->m_gameState.m_rotateChannel,
        69 // unused
    );

    /*
     * right so fellas why is point.y += obj->getRealPosition().y; needed
     * 
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
     * also note that if 2.2 rotated gameplay has been seen before at ANY time,
     * new 2.2 stuff will be used to START on, though this is "fixed" by
     * forcibly removing all arrow triggers before the current trigger
     *
     * tl;dr - dont add y if gameplay ends on unrotated (2.2) UNLESS we have not
     * seen rotated gameplay (2.1), in which case ALWAYS add y
     * if we're rotated, never add y, never needed!
     */

    if (!LevelTools::getLastGameplayRotated() && !fields->m_seenRotatedGameplayThisObject) {
        point.y += obj->getRealPosition().y;
    }

    if (LevelTools::getLastGameplayRotated()) {
        fields->m_seenRotatedGameplayThisObject = true;
    }

    /*
     * right so fellas why is THIS needed now
     * 
     * when we hit an arrow trigger, in LevelTools::posForTimeInternal the
     * "current" position snaps either on x or y depending on if we've rotated
     * now snapping isnt what we want, because then the actual length of the
     * line will not be correct!
     * so we need to adjust for the snapping, by detecting if we've just snapped
     * to a new m_speedStart position (wrongly named as all things are to do
     * with this - although yes it is the start for speed changer objects, it is
     * also the start for arrow triggers) and if we have just snapped, compare
     * either the X or the Y, and compensate for that - a
     * LevelTools::posForTimeInternal rewritten version is in the geode discord
     * somewhere just ping me for it - i think the if statement on 98 - 126
     * "breaks" stuff
     */

    // just changed rotation or reversed, meaning we've hit a new arrow trigger
    // dont need to check for reverse, they work fine and itll break them
    if (LevelTools::getLastGameplayRotated() != fields->m_lastPointWasRotatedThisObject) {
        fields->m_adjustmentThisObject = cocos2d::CCPoint{ 0.f, 0.f };
        // compare y positions if rotated, else compare x
        // remember we want to adjust x in terms of y and adjust y in terms of x
        // since we've just rotated
        // the primary adjustment is the main one, the secondary is just for
        // offset relative to the current axis to make it look nice
        if (LevelTools::getLastGameplayRotated()) {
            if (!fields->m_dontOffsetSecondaryAxis) fields->m_adjustmentThisObject.x += point.y - fields->m_lastPointThisObject.y; // secondary
            fields->m_adjustmentThisObject.y -= point.y - fields->m_lastPointThisObject.y; // primary
        } else {
            fields->m_adjustmentThisObject.x -= point.x - fields->m_lastPointThisObject.x; // primary
            if (!fields->m_dontOffsetSecondaryAxis) fields->m_adjustmentThisObject.y += point.x - fields->m_lastPointThisObject.x; // secondary
        }

        // just changed rotation, the duration line might seem to "jump" here -
        // so mark it so it's visible in the editor!
        ret.m_hasJumped = true;
    } else {
        ret.m_hasJumped = false;
    }
    
    if (fields->m_ignoreJumpedPoints) ret.m_hasJumped = false;

    point += fields->m_adjustmentThisObject;

    // debug - draw points
    if (fields->m_debug) {
        cocos2d::ccDrawColor4B(
            fields->m_seenRotatedGameplayThisObject ? 255 : 0,
            LevelTools::getLastGameplayRotated() ? 255 : 0,
            LevelTools::getLastGameplayRotated() != fields->m_lastPointWasRotatedThisObject ? 255 : 0,
            255
        );
        cocos2d::ccDrawCircle(point, 5, 0.314159f, 5, false);
    }

    fields->m_lastPointWasRotatedThisObject = LevelTools::getLastGameplayRotated();
    fields->m_lastPointThisObject = point;

    ret.m_pos = point;
    ret.m_time = time;
    ret.m_isRotated = LevelTools::getLastGameplayRotated();

    return ret;
}

cocos2d::ccColor4B HookedDrawGridLayer::colorForObject(EffectGameObject* obj, LinePart part) {
    bool isOnOtherLayer = m_editorLayer->m_currentLayer != obj->m_editorLayer && m_editorLayer->m_currentLayer != obj->m_editorLayer2;
    if (m_editorLayer->m_currentLayer == -1) isOnOtherLayer = false; // all layers

    GLubyte opacity = isOnOtherLayer ? 30 : 230;

    if (obj->m_isColorTrigger || obj->m_objectID == 1006) {
        cocos2d::ccColor4B ret = {
            obj->m_triggerTargetColor.r,
            obj->m_triggerTargetColor.g,
            obj->m_triggerTargetColor.b,
            opacity
        };

        if (part != LinePart::Base) {
            ret = tintColor(ret, -17);
        }

        return ret;
    }

    switch(part) {
        case LinePart::FadeIn:
        case LinePart::FadeOut:
            return { 131, 131, 131, opacity };
        case LinePart::Base:
            return { 175, 175, 175, opacity };
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

cocos2d::ccColor4B HookedDrawGridLayer::tintColor(const cocos2d::ccColor4B& col, int amount) {
    // convert to hsl, tint, convert back
    // most of this taken from eclipse though it really couldve been taken from
    // anywhere
    // TODO: maybe make this more efficient? seems a lot to be running for every
    // pulse trigger every frame, twice

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

void HookedDrawGridLayer::drawLines(const cocos2d::CCPoint& start, const std::vector<LinePoint>& segments, bool forceDashed) {
    // could most likely be improved by using ccDrawLines for segments next to
    // each other that have the same colour

    std::vector<LinePoint> allSegments;
    allSegments.resize(segments.size() + 1);
    allSegments[0] = LinePoint{ .m_pos = { .m_pos = start } };
    std::copy(segments.begin(), segments.end(), allSegments.begin() + 1);

    for (int i = 0; i < allSegments.size() - 1; i++) {
        PosForTime from = allSegments[i].m_pos;
        PosForTime to = allSegments[i + 1].m_pos;
        bool hasJumped = to.m_hasJumped;
        
        auto col = allSegments[i + 1].m_col;

        cocos2d::ccDrawColor4B(col.r, col.g, col.b, col.a);
        if (hasJumped || forceDashed) {
            // if this isnt forced, turn down opacity
            if (!forceDashed) cocos2d::ccDrawColor4B(col.r, col.g, col.b, col.a / 2);
            drawDashedLine(from.m_pos, to.m_pos);
        } else {
            cocos2d::ccDrawLine(from.m_pos, to.m_pos);
        }
    }
}

void HookedDrawGridLayer::drawDashedLine(const cocos2d::CCPoint& start, const cocos2d::CCPoint& end) {
    float dashLength = 2.f;
    float gapLength = 4.f;
    float totalLength = dashLength + gapLength;

    auto vec = end - start;
    vec = vec.normalize();
    
    cocos2d::CCPoint pen = start;
    float dist = start.getDistance(end);
    // subtract totalLength to remove last two lines to then be drawn later
    for (float i = 0; i < dist - totalLength; i += totalLength) {
        cocos2d::ccDrawLine(pen, pen + (vec * dashLength));
        pen += vec * totalLength;
    }

    cocos2d::ccDrawLine(pen, end);

    // debug - draw dashed line star and end
    if (m_fields->m_debug) {
        cocos2d::ccDrawColor4B(0, 255, 0, 255);
        cocos2d::ccDrawCircle(start, 16, 0.f, 3, false);
        cocos2d::ccDrawColor4B(0, 255, 255, 255);
        cocos2d::ccDrawCircle(end, 16, 0.785398f, 3, false);
    }
}

constexpr float HookedDrawGridLayer::calculateZigZag(float time, float strength, float interval) {
    float inverseInterval = 1.f / (2.f * interval);
    float inverseIntervalTime = inverseInterval * time;
    return strength * (std::abs(2.f * (inverseIntervalTime - std::floor(inverseIntervalTime + .5f))) - .5f) * 30.f;
}

void HookedDrawGridLayer::drawLinesWithZigZag(EffectGameObject* start, const std::vector<LinePoint>& segments, float strength, float interval, cocos2d::CCArray* speedObjects, float startTime) {
    auto fields = m_fields.self();

    std::vector<LinePoint> allSegments;
    allSegments.resize(segments.size() + 1);
    allSegments[0] = LinePoint{ .m_pos = { .m_pos = start->getRealPosition() } };
    std::copy(segments.begin(), segments.end(), allSegments.begin() + 1);

    // small or zero interval
    if (interval < 0.01f) {
        interval = 0.01f;
    }

    // reset stuff to ensure any posForTime calls are correct
    fields->m_seenRotatedGameplayThisObject = false;
    fields->m_lastPointWasRotatedThisObject = false;
    fields->m_adjustmentThisObject = cocos2d::CCPoint{ 0.f, 0.f };
    fields->m_lastPointThisObject = cocos2d::CCPoint{ 0.f, 0.f };

    // add in peaks and troughs
    int peakTroughCount = start->m_duration / interval;
    allSegments.reserve(allSegments.size() + peakTroughCount);
    int lastInsertionPoint = -1;
    auto col = colorForObject(start, LinePart::Base);
    for (int i = 0; i < peakTroughCount; i++) {
        float time = startTime + (i * interval);

        // FIXME: a few bugs here since posForTime sets stuff for this object
        // inconsistently with the normal posForTime usage - makes this somewhat
        // buggy
        auto extraPoint = LinePoint{
            .m_col = col,
            .m_pos = posForTime(time, start, speedObjects)
        };

        // find where to insert
        for (int j = lastInsertionPoint + 1; j < allSegments.size(); j++) {
            auto& el = allSegments[j];
            if (el.m_pos.m_time > time) {
                lastInsertionPoint = j;
                allSegments.insert(allSegments.begin() + j, extraPoint);
                break;
            }
        }

        if (fields->m_debug) {
            auto pos = extraPoint.m_pos;
            if (pos.m_isRotated) {
                pos.m_pos.x += calculateZigZag(pos.m_time - startTime, strength, interval);
            } else {
                pos.m_pos.y += calculateZigZag(pos.m_time - startTime, strength, interval);
            }
            cocos2d::ccDrawColor4B(255, 255, 0, 255);
            cocos2d::ccDrawCircle(pos.m_pos, 3, 0.f, 4, false);
        }
    }

    for (int i = 0; i < allSegments.size() - 1; i++) {
        PosForTime from = allSegments[i].m_pos;
        PosForTime to = allSegments[i + 1].m_pos;
        bool hasJumped = to.m_hasJumped;

        from.m_time -= startTime;
        to.m_time -= startTime;

        auto col = allSegments[i + 1].m_col;

        // multiply x or y for from and to by the zig zag amount
        if (from.m_isRotated) {
            from.m_pos.x += calculateZigZag(from.m_time, strength, interval);
        } else {
            from.m_pos.y += calculateZigZag(from.m_time, strength, interval);
        }

        if (to.m_isRotated) {
            to.m_pos.x += calculateZigZag(to.m_time, strength, interval);
        } else {
            to.m_pos.y += calculateZigZag(to.m_time, strength, interval);
        }

        // then draw like normal point
        if (hasJumped) {
            cocos2d::ccDrawColor4B(col.r, col.g, col.b, col.a / 2);
            drawDashedLine(from.m_pos, to.m_pos);
        } else {
            cocos2d::ccDrawColor4B(col.r, col.g, col.b, col.a);
            cocos2d::ccDrawLine(from.m_pos, to.m_pos);
        }
    }
}
