#pragma once

#include "common.h"
#include <vector>
#include <array>
#include <tuple>

namespace Game
{
    constexpr u32 backgroundColor = C2D_Color32(0x20, 0x20, 0x20, 0xFF); // Some nice gray, taken from QRaken
    constexpr u32 textColor = C2D_Color32f(1,1,1,1); // White

    constexpr u32 clearWaterColor = C2D_Color32(0x00, 0x94, 0xFF, 0xFF);

    constexpr double textScale = 0.5f;

    constexpr int colorBeforeDamageLower = 0x20;

    typedef struct
    {
        u32 color;
        int damage;
    } WaterProperty;

    class PaintSplash
    {
        public:
            PaintSplash(bool boss);
            PaintSplash(double tX, double tY, double tZ);

            bool isInCenter(double tX, double tY, double tZ);
            bool hit(const WaterProperty& water, int* damage);

            bool isVisible(double tX, double tY, double tZ);
            void draw(double tX, double tY, double tZ);

        private:
            double tX, tY, tZ; // angle from normal
            double health;
            u32 color;
            bool boss;
    };

    class Game
    {
        public:
            Game(int argc, char* argv[]);
            ~Game();

            void update();

            bool running;

        private:
            void drawCameraImage();
            void drawPaintSplashes();
            void drawHitCounter();
            void drawOverlay();
            void drawText();

            void draw();

            void addText(C2D_TextBuf textBuf, const char* text);

            std::vector<WaterProperty> waterProperties;

            int selectedWater;
            u32 waterLevel;
            bool firing;
            bool overloaded;
            int hitCounter;
            int lastDamage;

            angularRate rate;
            accelVector vector;

            double tX, tY, tZ; // Camera angle from normal
            std::vector<PaintSplash*> paintSplashes;

            u32 old_time_limit;

            int frameCounter;
            u64 lastTime;

            C3D_RenderTarget *top, *bottom;
            C2D_TextBuf staticBuf, dynamicBuf;
            std::vector<C2D_Text*> text;
    };
}
