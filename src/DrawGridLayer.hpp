#pragma once
#include <Geode/modify/DrawGridLayer.hpp>

struct LinePoint {
    cocos2d::ccColor4B m_col;
    cocos2d::CCPoint m_pos;
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
        bool m_seenRotatedGameplayThisObject;

        double m_resolution;
        bool m_cull;
        unsigned int m_limit;

        bool m_debug;

        Fields();
    };

    void draw();

    void drawLinesForObject(EffectGameObject* obj);
    void drawLines(const cocos2d::CCPoint& start, const std::vector<LinePoint>& segments);

    ObjectDuration durationForObject(EffectGameObject* obj);
    cocos2d::ccColor4B colorForObject(EffectGameObject* obj, LinePart part);

    cocos2d::CCPoint posForTime(float time, EffectGameObject* obj);

    cocos2d::ccColor4B tintColor(cocos2d::ccColor4B col, int amount);
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
