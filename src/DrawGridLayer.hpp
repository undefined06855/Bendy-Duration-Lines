#pragma once
#include <Geode/modify/DrawGridLayer.hpp>

struct PosForTime {
    cocos2d::CCPoint m_pos;
    bool m_hasJumped;
};

struct LinePoint {
    cocos2d::ccColor4B m_col;
    PosForTime m_pos;
};

struct ObjectDuration {
    float m_baseDuration;
    float m_fadeInDuration;
    float m_fadeOutDuration;
};

enum class LinePart {
    FadeIn, Base, FadeOut
};

class $modify(HookedDrawGridLayer, DrawGridLayer) {
    struct Fields {
        // these get reset for every object
        bool m_seenRotatedGameplayThisObject;
        bool m_lastPointWasRotatedThisObject;
        cocos2d::CCPoint m_adjustmentThisObject;
        cocos2d::CCPoint m_lastPointThisObject;

        // settings
        double m_resolution;
        bool m_cull;
        unsigned int m_limit;
        bool m_stripOldArrowTriggers;
        bool m_dontOffsetSecondaryAxis;
        bool m_ignoreJumpedPoints;
        bool m_debug;
        
        Fields();
    };

    void draw();

    void drawLinesForObject(EffectGameObject* obj);
    void drawLines(const cocos2d::CCPoint& start, const std::vector<LinePoint>& segments);
    void drawDashedLine(const cocos2d::CCPoint& start, const cocos2d::CCPoint& end);

    ObjectDuration durationForObject(EffectGameObject* obj);
    cocos2d::ccColor4B colorForObject(EffectGameObject* obj, LinePart part);

    PosForTime posForTime(float time, EffectGameObject* obj, cocos2d::CCArray* speedObjects);

    cocos2d::ccColor4B tintColor(const cocos2d::ccColor4B& col, int amount);
};

// // patch out random if block lol this might have fixed something you never know (it didnt)
// $execute {
//     // from 0x3180cc to 0x3180fb
//     long long start = geode::base::get() + 0x3180cc;
//     long long end = geode::base::get() + 0x3180fb + 1;
//     int length = end - start;

//     std::vector<uint8_t> bytes = {};
//     bytes.resize(length);
//     std::fill(bytes.begin(), bytes.end(), 0x90);

//     (void)geode::Mod::get()->patch((void*)start, bytes);
// }
