#pragma once
#include <Geode/modify/DrawGridLayer.hpp>

struct PosForTime {
    cocos2d::CCPoint m_pos;
    bool m_hasJumped;
    bool m_isRotated;
    float m_time;
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
        bool m_cullOffscreen;
        bool m_cullOtherLayers;
        unsigned int m_limit;
        bool m_ignoreSpawnTriggered;
        
        bool m_stripOldArrowTriggers;
        bool m_dontOffsetSecondaryAxis;
        bool m_ignoreJumpedPoints;
        int m_durationLimit;
        bool m_debug;
        
        Fields();
    };

    void draw();

    void drawLinesForObject(EffectGameObject* obj);
    void drawLines(const cocos2d::CCPoint& start, const std::vector<LinePoint>& segments, bool forceDashed);
    void drawDashedLine(const cocos2d::CCPoint& start, const cocos2d::CCPoint& end);
    constexpr float calculateZigZag(float time, float strength, float interval);
    void drawLinesWithZigZag(EffectGameObject* start, const std::vector<LinePoint>& segments, float strength, float interval, cocos2d::CCArray* speedObjects, float startTime);

    ObjectDuration durationForObject(EffectGameObject* obj);
    cocos2d::ccColor4B colorForObject(EffectGameObject* obj, LinePart part);

    PosForTime posForTime(float time, EffectGameObject* obj, cocos2d::CCArray* speedObjects);

    cocos2d::ccColor4B tintColor(const cocos2d::ccColor4B& col, int amount);
};
