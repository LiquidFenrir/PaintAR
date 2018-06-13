#include "game.h"
#include "camera.h"
#include "sprites.h"
#include <cmath>

//http://www.pieter-jan.com/node/11
#define dt (double)(1.0f/30.0f)

#define GYROSCOPE_SENSITIVITY (double)14.375

typedef enum {
    DEADZONE_PITCH = 10,
    DEADZONE_YAW = 10,
    DEADZONE_ROLL = 25,
} GyroDeadzone;

void ComplementaryFilter(accelVector vector, angularRate rate, double *pitch, double *roll, double *yaw)
{
    if(abs(rate.x) < DEADZONE_PITCH)
        rate.x = 0;
    if(abs(rate.y) < DEADZONE_YAW)
        rate.x = 0;
    if(abs(rate.z) < DEADZONE_ROLL)
        rate.x = 0;

    // Integrate the gyroscope data -> int(angularSpeed) = angle
    *pitch += ((double)rate.x / GYROSCOPE_SENSITIVITY) * dt; // Angle around the X-axis
    *roll -= ((double)rate.z / GYROSCOPE_SENSITIVITY) * dt;  // Angle around the Z-axis
    // *yaw -= ((double)rate.y / GYROSCOPE_SENSITIVITY) * dt;   // Angle around the Y-axis

    // Compensate for drift with accelerometer data if !bullshit
    // Sensitivity = -2 to 2 G at 16Bit -> 2G = 32768 && 0.5G = 8192
    int forceMagnitudeApprox = abs(vector.x) + abs(vector.z) + abs(vector.y);
    if (forceMagnitudeApprox > 8192 && forceMagnitudeApprox < 32768)
    {
        // Turning around the X axis results in a vector on the Y-axis
        double pitchAcc = atan2((double)vector.y, (double)vector.z) * 180 / M_PI;
        *pitch = *pitch * 0.98 + pitchAcc * 0.02;

        // Turning around the Y axis results in a vector on the X-axis
        double rollAcc = atan2((double)vector.x, (double)vector.z) * 180 / M_PI;
        *roll = *roll * 0.98 + rollAcc * 0.02;
    }
}

static C2D_SpriteSheet spritesheet;
static C2D_Sprite paintSprite;

static double constrainAngle(double x)
{
    x = fmod(x + 180, 360);
    if (x < 0)
        x += 360;
    return x - 180;
}

static inline u32 randomColor()
{
    int randMax = 0x100-Game::colorBeforeDamageLower*2;
    return C2D_Color32(Game::colorBeforeDamageLower + rand() % randMax, Game::colorBeforeDamageLower + rand() % randMax, Game::colorBeforeDamageLower + rand() % randMax, 0xFF);
}

static double calculateModifier(u8 paintPart, u8 waterPart)
{
    if(abs(paintPart-waterPart) > Game::colorBeforeDamageLower)
    {
        double part = 1.0f;
        if(paintPart > waterPart)
        {
            part = (double)waterPart/(double)paintPart;
        }
        else
        {
            part = (double)paintPart/(double)waterPart;
        }
        return part;
    }
    else
        return 1.0f;
}

namespace Game
{
    static constexpr u32 fakeWhiteColor = C2D_Color32(0xFF-colorBeforeDamageLower, 0xFF-colorBeforeDamageLower, 0xFF-colorBeforeDamageLower, 0xFF);
    static constexpr u32 fakeBlackColor = C2D_Color32(colorBeforeDamageLower, colorBeforeDamageLower, colorBeforeDamageLower, 0xFF);

    static constexpr int KILLS_TO_BOSS = 10;
    static constexpr int SECONDS_TO_SPAWN = 10;

    static constexpr double angleVisible = 67.5f;
    static constexpr double angleCenter = 15.0f;
    static constexpr double BASE_HEALTH = 50;
    static constexpr double BOSS_HEALTH_MODIFIER = 10;
    static auto bossColors = std::array{clearWaterColor, fakeWhiteColor, fakeBlackColor};

    enum WaterInfo
    {
        WATER_LEVEL_MAX = 100,
        FRAMES_TO_RELOAD = 3,
        FRAMES_TO_SPEND = 2,
    };

    PaintSplash::PaintSplash(bool boss)
    {
        double (*random_deg_angle)() = [](){ return (rand() % 360) - 180.0; };
        this->tX = random_deg_angle();
        this->tY = random_deg_angle();
        this->tZ = 0;

        this->boss = boss;
        if(boss)
        {
            size_t color = rand() % bossColors.size();
            this->color = bossColors[color];
            this->health = BASE_HEALTH*BOSS_HEALTH_MODIFIER;
        }
        else
        {
            this->color = randomColor();
            this->health = BASE_HEALTH;
        }
    }

    PaintSplash::PaintSplash(double tX, double tY, double tZ)
    {
        double (*dispersion)(double) = [](double cur) { return cur - (rand() % 10) + (rand() % 10);};
        this->tX = dispersion(tX);
        this->tY = dispersion(tY);
        this->tZ = dispersion(tZ);
        this->color = randomColor();
        this->health = BASE_HEALTH;
        this->boss = false;
    }

    bool PaintSplash::isVisible(double tX, double tY, double tZ)
    {
        if(this->tX - angleVisible <= tX && tX <= this->tX + angleVisible)
            if(this->tY - angleVisible <= tY && tY <= this->tY + angleVisible)
                if(this->tZ - angleVisible <= tZ && tZ <= this->tZ + angleVisible)
                    return true;
        return false;
    }

    void PaintSplash::draw(double tX, double tY, double tZ)
    {
        double x_orig = 200;
        double y_orig = 120;

        double y = 1.0f;
        double x = 1.0f;

        double y_coeff = 1.0f;
        double x_coeff = 1.0f;

        y = sin(C3D_AngleFromDegrees(this->tX-tX))*y_coeff;
        x = sin(C3D_AngleFromDegrees(this->tY-tY))*x_coeff;

        if(this->boss)
        {
            C2D_SpriteSetPos(&paintSprite, (x+1.0f)*x_orig - 32, (y+1.0f)*y_orig - 32);
            C2D_SpriteSetScale(&paintSprite, 2.0f, 2.0f);
        }
        else
            C2D_SpriteSetPos(&paintSprite, (x+1.0f)*x_orig - 16, (y+1.0f)*y_orig - 16);

        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, this->color, 1.0f);
        C2D_DrawSpriteTinted(&paintSprite, &tint);

        if(this->boss)
            C2D_SpriteSetScale(&paintSprite, 1.0f, 1.0f);
    }

    bool PaintSplash::isInCenter(double tX, double tY, double tZ)
    {
        double actualAngleCenter = this->boss ? angleCenter*2 : angleCenter;
        if(this->tX - actualAngleCenter <= tX && tX <= this->tX + actualAngleCenter)
            if(this->tY - actualAngleCenter <= tY && tY <= this->tY + actualAngleCenter)
                if(this->tZ - actualAngleCenter <= tZ && tZ <= this->tZ + actualAngleCenter)
                    return true;
        return false;
    }

    bool PaintSplash::hit(const WaterProperty& water, int* damage)
    {
        double modifier = 1.0f;
        if(water.color != clearWaterColor)
        {
            DEBUG("%.8lx : %.8lx\n", this->color, water.color);
            u8 thisR = this->color >> 24 & 0xFF;
            u8 thisG = this->color >> 16 & 0xFF;
            u8 thisB = this->color >> 8 & 0xFF;

            u8 waterR = water.color >> 24 & 0xFF;
            u8 waterG = water.color >> 16 & 0xFF;
            u8 waterB = water.color >> 8 & 0xFF;

            double modifierR = calculateModifier(thisR, waterR);
            double modifierG = calculateModifier(thisG, waterG);
            double modifierB = calculateModifier(thisB, waterB);

            modifier *= modifierR*modifierG*modifierB;
        }

        double actual_damage = water.damage * modifier;
        DEBUG("modifier: %f, damage: %f\n", modifier, actual_damage);
        *damage = actual_damage;
        this->health -= *damage;
        return this->health <= 0;
    }

    Game::Game(int argc, char* argv[])
    {
        APT_GetAppCpuTimeLimit(&this->old_time_limit);
        APT_SetAppCpuTimeLimit(30);

        romfsInit();
        HIDUSER_EnableAccelerometer();
        HIDUSER_EnableGyroscope();

        gfxInitDefault();
        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        C2D_Prepare();

        this->top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        this->bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

        spritesheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
        C2D_SpriteFromSheet(&paintSprite, spritesheet, sprites_paint_idx);
        C2D_SpriteSetDepth(&paintSprite, 0.55f);

        this->staticBuf = C2D_TextBufNew(512);
        this->dynamicBuf = C2D_TextBufNew(512);

        this->addText(this->staticBuf, "Press \uE000 to fire a water beam!");
        this->addText(this->staticBuf, "Press \uE004 or \uE005 to change water type!");
        this->addText(this->staticBuf, "Press START to exit.");

        startCameraThread();

        this->waterLevel = WATER_LEVEL_MAX;
        this->firing = false;
        this->overloaded = false;

        this->lastTime = osGetTime();
        this->paintSplashes.push_back(new PaintSplash(0, 0, 0));
        this->paintSplashes.push_back(new PaintSplash(45, 45, 0));
        this->paintSplashes.push_back(new PaintSplash(-45, -45, 0));
        this->paintSplashes.push_back(new PaintSplash(-45, 45, 0));
        this->paintSplashes.push_back(new PaintSplash(45, -45, 0));

        this->running = true;
        this->frameCounter = 0;

        this->selectedWater = 0;
        this->waterProperties.push_back({clearWaterColor, 1});
        this->waterProperties.push_back({fakeWhiteColor, 5});
        this->waterProperties.push_back({fakeBlackColor, 5});

        this->tX = this->tY = this->tZ = 0.0f;
    }

    void Game::addText(C2D_TextBuf textBuf, const char* text)
    {
        C2D_Text* c2dtext = new C2D_Text;
        C2D_TextParse(c2dtext, textBuf, text);
        C2D_TextOptimize(c2dtext);
        this->text.push_back(c2dtext);
    }

    Game::~Game()
    {
        closeCameraThread();

        for(auto paintSplash : this->paintSplashes)
            delete paintSplash;

        for(auto text : this->text)
            delete text;

        C2D_TextBufDelete(this->dynamicBuf);
        C2D_TextBufDelete(this->staticBuf);

        C2D_Fini();
        C3D_Fini();
        gfxExit();

        DEBUG("%.8lx\n", HIDUSER_DisableGyroscope());
        DEBUG("%.8lx\n", HIDUSER_DisableAccelerometer());
        romfsExit();

        if(this->old_time_limit != UINT32_MAX)
        {
            APT_SetAppCpuTimeLimit(this->old_time_limit);
        }
    }

    void Game::drawCameraImage()
    {
        svcWaitSynchronization(arg->mutex, UINT64_MAX);
        convertCameraBuffer();
        C2D_DrawImageAt(arg->image, 0.0f, 0.0f, 0.5f, NULL, 1.0f, 1.0f);
        svcReleaseMutex(arg->mutex);
    }

    void Game::drawPaintSplashes()
    {
        for(auto const& paintSplash : this->paintSplashes)
            if(paintSplash->isVisible(this->tX, this->tY, this->tZ))
                paintSplash->draw(this->tX, this->tY, this->tZ);
    }

    void Game::drawHitCounter()
    {
        float start_x = 400 - 128 - 8;
        float y = 199.0f;
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet, sprites_counter_overlay_idx), start_x, y, 0.7f, NULL, 1.0f, 1.0f);
        constexpr size_t DIGITS_AMOUNT = 8;
        start_x += 128 - 21;
        for(size_t i = 0, big = 1; i != DIGITS_AMOUNT; i++, big *= 10)
        {
            int cnt = (this->hitCounter % (big*10))/big;
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet, sprites_0_idx+cnt), start_x - ((12+2)*i), y+5, 0.8f, NULL, 1.0f, 1.0f);
        }
    }

    void Game::drawOverlay()
    {
        C2D_DrawRectSolid(182.0f, 169.0f, 0.55f, 36.0f, 18.0f, this->waterProperties[this->selectedWater].color);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet, sprites_gun_idx), 0.0f, 0.0f, 0.6f, NULL, 1.0f, 1.0f);

        C2D_DrawRectSolid(15.0f, 205.0f, 0.7f, this->waterLevel, 20, this->waterProperties[this->selectedWater].color);
        if(this->overloaded)
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet, sprites_water_overloaded_idx), 13.0f, 203.0f, 0.8f, NULL, 1.0f, 1.0f);
        else
            C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet, sprites_water_overlay_idx), 13.0f, 203.0f, 0.8f, NULL, 1.0f, 1.0f);

        float start_x = 200 - ((this->waterProperties.size()*32)/2.0f);
        float y = 199.0f;
        for(size_t i = 0; i < this->waterProperties.size(); i++)
        {
            C2D_DrawRectSolid(start_x + 4 + 32*i, y+4, 0.7f, 24, 24, this->waterProperties[i].color);
            if(i == (size_t)this->selectedWater)
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet, sprites_water_selected_idx), start_x + 32*i, y, 0.8f, NULL, 1.0f, 1.0f);
            else
                C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet, sprites_water_type_idx), start_x + 32*i, y, 0.8f, NULL, 1.0f, 1.0f);
        }

        this->drawHitCounter();

        if(this->firing)
        {
            C2D_DrawRectSolid(190.0f, 120.0f, 0.7f, 20, 40, this->waterProperties[this->selectedWater].color);
            if(this->lastDamage != -1)
            {
                char buffer[128] = {0};
                DEBUG("damage: %i\n", this->lastDamage);
                sprintf(buffer, "%i", this->lastDamage);

                this->addText(dynamicBuf, buffer);
                C2D_DrawText(this->text.back(), C2D_WithColor, 220.0f, 110.0f, 0.65f, textScale, textScale, textColor);
                delete this->text.back();
                this->text.pop_back();

                this->lastDamage = -1;
            }
        }
    }

    void Game::drawText()
    {
        constexpr size_t INFO_LINES = 3;
        char buffer[INFO_LINES][128] = {0};
        sprintf(buffer[0], "Roll: %d\nYaw: %d\nPitch: %d\n", this->rate.x, this->rate.y, this->rate.z);
        sprintf(buffer[1], "x: %d\ny: %d\nz: %d\n", this->vector.x, this->vector.y, this->vector.z);
        sprintf(buffer[2], "tX: %f\ntY: %f\ntZ: %f\n", this->tX, this->tY, this->tZ);   

        size_t i = 0;
        for(i = 0; i < this->text.size(); i++)
        {
            C2D_DrawText(this->text[i], C2D_WithColor, 5, 5+i*15, 0.5f, textScale, textScale, textColor);
        }

        float y = 5+i*15;
        for(i = 0; i != INFO_LINES; i++)
        {
            this->addText(dynamicBuf, buffer[i]);
            C2D_DrawText(this->text.back(), C2D_WithColor, 5+80*i, y, 0.5f, textScale, textScale, textColor);
            delete this->text.back();
            this->text.pop_back();
        }

        y += 15*3;
        sprintf(buffer[0], "Paint splats left: %u", this->paintSplashes.size());
        this->addText(dynamicBuf, buffer[0]);
        C2D_DrawText(this->text.back(), C2D_WithColor, 5, y, 0.5f, textScale, textScale, textColor);
        delete this->text.back();
        this->text.pop_back();
        #undef INFO_LINES
    }

    void Game::draw()
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TextBufClear(dynamicBuf);

        C2D_SceneBegin(top);
        C2D_TargetClear(top, backgroundColor);

        this->drawCameraImage();
        this->drawPaintSplashes();
        this->drawOverlay();

        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, backgroundColor);

        this->drawText();

        C3D_FrameEnd(0);
    }

    void Game::update()
    {   
        hidScanInput();

        hidAccelRead(&this->vector);
        hidGyroRead(&this->rate);
        ComplementaryFilter(this->vector, this->rate, &this->tX, &this->tY, &this->tZ);

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if(kDown & KEY_START)
        {
            this->running = false;
            return;
        }

        this->draw();

        if(kDown & KEY_L)
        {
            this->selectedWater--;
            if(this->selectedWater < 0)
                this->selectedWater += this->waterProperties.size();
        }

        if(kDown & KEY_R)
        {
            this->selectedWater++;
            if(this->selectedWater >= (int)this->waterProperties.size())
                this->selectedWater = 0;
        }

        if(kHeld & KEY_CPAD_LEFT || kDown & KEY_LEFT)
            this->tY -= 2.0f;
        else if(kHeld & KEY_CPAD_RIGHT || kDown & KEY_RIGHT)
            this->tY += 2.0f;

        if(kHeld & KEY_CPAD_UP || kDown & KEY_UP)
            this->tX -= 2.0f;
        else if(kHeld & KEY_CPAD_DOWN || kDown & KEY_DOWN)
            this->tX += 2.0f;

        this->tY = constrainAngle(this->tY);
        this->tX = constrainAngle(this->tX);
        this->tZ = constrainAngle(this->tZ);

        if(!this->overloaded && kHeld & KEY_A && this->waterLevel > 0)
            this->firing = true;
        else
            this->firing = false;

        if(firing && this->frameCounter % FRAMES_TO_SPEND == 0)
            this->waterLevel--;

        if(this->waterLevel == 0)
            this->overloaded = true;

        if((this->overloaded || !(kHeld & KEY_A)) && !firing && this->waterLevel < WATER_LEVEL_MAX && frameCounter % FRAMES_TO_RELOAD == 0)
            this->waterLevel++;

        if(this->overloaded && this->waterLevel == WATER_LEVEL_MAX)
            this->overloaded = false;

        if(firing)
        {
            size_t i = 0;
            for(; i < this->paintSplashes.size(); i++)
            {
                if(this->paintSplashes[i]->isInCenter(this->tX, this->tY, this->tZ))
                {
                    if(this->paintSplashes[i]->hit(this->waterProperties[this->selectedWater], &this->lastDamage))
                    {

                        DEBUG("killed!\n");
                        delete this->paintSplashes[i];
                        this->paintSplashes.erase(this->paintSplashes.begin()+i);
                        this->hitCounter++;
                        break;
                    }
                }
            }
            if(i != this->paintSplashes.size()) // If break happened == 1 kill
            {
                if(this->hitCounter % KILLS_TO_BOSS == 0)
                {
                    DEBUG("adding boss\n");
                    this->paintSplashes.push_back(new PaintSplash(true));
                }
            }
        }

        this->frameCounter++;
        this->frameCounter %= 60;

        if(osGetTime() >= this->lastTime + SECONDS_TO_SPAWN*1000) //every 10 seconds
        {
            this->lastTime = osGetTime();
            this->paintSplashes.push_back(new PaintSplash(false));
            DEBUG("adding\n");
        }
    }
}
