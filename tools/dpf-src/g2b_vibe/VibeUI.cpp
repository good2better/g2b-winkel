/*
 * G2B TAPE — UI (NanoVG): 8 knoppen + 16 presets + scope.
 * Sleep = draaien, scroll = fijn, klik preset = knoppen goed zetten.
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// G2B skins (8 thema's): bg,pn,tx,gy,ed,GR,DG
static const float G2BSK[8][7][3]={
    {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
    {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
    {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
    {{24,14,20},{34,20,28},{250,230,240},{150,105,125},{60,38,50},{255,110,180},{180,60,120}},
    {{228,230,232},{240,242,244},{30,34,38},{105,112,118},{205,210,214},{40,140,100},{20,100,70}},
    {{18,10,10},{28,16,16},{240,225,220},{140,100,95},{55,35,32},{200,80,60},{130,45,35}},
    {{14,14,10},{22,22,16},{245,240,220},{120,115,95},{50,48,35},{230,190,80},{160,125,45}},
    {{12,14,20},{20,24,32},{235,240,250},{105,115,135},{36,44,58},{80,160,255},{45,110,200}}};
static const char* G2BSKN[8]={"MELIOR","SPECTRUM","LAGOON","CANDY","GHOST","MAROON","GOLD","STUDIO"};
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

static const int kNumPrograms = 16;
static const char* kProgramNames[kNumPrograms] = {
    "G2B","CASSETTE","VINYL","BOOMBAP","DUB","JAREN-70","LOFI","WALKMAN","NACHT","RADIO","SOUL","TRAP","DRILL","BOOM","NEON","WARM"
};
// spiegel van kPresetParams in VibePlugin.cpp — UI pusht alle 11 knopwaarden zelf (ook GALAXY + KRACHT)
static const float kUiPresetParams[16][11] = {
    {0.50f,1.f,0.f,0.30f,0.35f,0.20f,0.25f,0.30f,0.30f, 9,100},
    {0.40f,1.f,0.f,0.55f,0.55f,0.45f,0.30f,0.55f,0.65f, 4,100},
    {0.50f,1.f,0.f,0.25f,0.30f,0.70f,0.15f,0.35f,0.45f, 5,100},
    {0.65f,0.f,0.f,0.40f,0.50f,0.55f,0.20f,0.60f,0.60f, 6,100},
    {0.55f,2.f,0.f,0.30f,0.40f,0.25f,0.70f,0.45f,0.50f, 15,95},
    {0.30f,2.f,0.f,0.70f,0.60f,0.50f,0.35f,0.70f,0.85f, 12,90},
    {0.50f,0.f,1.f,0.50f,0.55f,0.60f,0.20f,0.75f,0.70f, 8,90},
    {0.40f,1.f,0.f,0.65f,0.45f,0.40f,0.25f,0.55f,0.75f, 3,90},
    {0.60f,3.f,0.f,0.35f,0.45f,0.30f,0.45f,0.65f,0.55f, 14,95},
    {0.50f,1.f,0.f,0.45f,0.50f,0.35f,0.30f,0.85f,0.60f, 4,100},
    {0.50f,2.f,0.f,0.35f,0.45f,0.25f,0.35f,0.50f,0.55f, 11,100},
    {0.65f,0.f,1.f,0.30f,0.40f,0.20f,0.30f,0.40f,0.40f, 2,100},
    {0.70f,0.f,2.f,0.35f,0.45f,0.20f,0.35f,0.45f,0.45f, 2,100},
    {0.60f,1.f,0.f,0.40f,0.45f,0.35f,0.30f,0.50f,0.55f, 9,100},
    {0.60f,4.f,2.f,0.20f,0.35f,0.15f,0.50f,0.25f,0.30f, 18,95},
    {0.40f,2.f,0.f,0.30f,0.50f,0.15f,0.30f,0.40f,0.45f, 10,100},
};

class VibeUI : public UI
{
public:
    VibeUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[11]  = {0,0,0,0,0,0,0,0,0,0,0};
        static const float maxs[11]  = {1,15,3,1,1,1,1,1,1,26,100};
        static const float defs[11]  = {0.40f,1,0,0.30f,0.35f,0.20f,0.25f,0.30f,0.30f,9,100};
        static const char* lbl[11]   = {"POMP","CURVE","RATE","WOW","TAPE","STOF","ECHO","DARK","AGE","DIMENTIES","KRACHT"};
        static const char* unt[11]   = {"%","","","%","%","%","%","%","%","","%"};
        std::memcpy(fMin, mins, sizeof(mins));
        std::memcpy(fMax, maxs, sizeof(maxs));
        std::memcpy(fValue, defs, sizeof(defs));
        std::memcpy(fLabel, lbl, sizeof(lbl));
        std::memcpy(fUnit, unt, sizeof(unt));
        std::memset(fScope, 0, sizeof(fScope));
        fScopePos = 0; fDrag = -1; fLastY = 0.0f; fProg = -1;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < 9) fValue[index] = value;
        else if (index == 9) fValue[9] = value;
        else if (index == 11) {
            fProg = (int)(value + 0.5f);
            if (fProg < 0) fProg = 0; if (fProg > 15) fProg = 15;
            for (int p = 0; p < 11; ++p) fValue[p] = kUiPresetParams[fProg][p];
        }
        else if (index == 12) fOn = value;
        else if (index == 13) fValue[10] = value;
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "scope") == 0)
        {
            const char* p = value;
            while (*p != '\0')
            {
                char* end = nullptr;
                const float v = std::strtof(p, &end);
                if (end == p) break;
                fScope[fScopePos] = v;
                fScopePos = (fScopePos + 1) % kScopeSize;
                p = (*end == ';') ? end + 1 : end;
            }
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float W = getWidth(), H = getHeight();
        beginPath(); rect(0, 0, W, H); fillColor(g2bsk(fLight,0)); fill();

        fontSize(20.0f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(230, 190, 80);
        text(18.0f, 22.0f, "G2B VIBE", nullptr);
        fontSize(11.5f); fillColor(143, 160, 152);
        text(18.0f, 41.0f, "pomp + band in een: pomp · curve · rate · wow · tape · stof · echo · dark · age", nullptr);

        // preset-knoppen (2 rijen: 8 + 8)
        for (int i = 0; i < kNumPrograms; ++i)
        {
            const int row = i < 8 ? 0 : 1, col = i % 8;
            const float bw = 66.0f, bh = 17.0f, gap = 5.0f;
            const float rowW = 8 * bw + 7 * gap;
            const float bx = (W - rowW) * 0.5f + col * (bw + gap);
            const float by = row == 0 ? 56.0f : 76.0f;
            const bool on = (i == fProg);
            beginPath(); roundedRect(bx, by, bw, bh, 4.0f);
            if (on) { fillColor(160, 125, 45); fill(); }
            strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
            fontSize(9.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(on ? Color(232, 236, 234) : Color(143, 160, 152));
            text(bx + bw * 0.5f, by + bh * 0.5f, kProgramNames[i], nullptr);
        }

        // scope (onder presets)
        const float sx = 18.0f, sy = 100.0f, sw = W - 36.0f, sh = 30.0f;
        beginPath(); rect(sx, sy, sw, sh);
        fillColor(8, 10, 9); fill();
        strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
        beginPath();
        const float mid = sy + sh * 0.5f;
        for (int i = 0; i < kScopeSize - 1; ++i)
        {
            const int i0 = (fScopePos + i) % kScopeSize;
            const int i1 = (fScopePos + i + 1) % kScopeSize;
            const float x0 = sx + (float)i / (kScopeSize - 1) * sw;
            const float x1 = sx + (float)(i + 1) / (kScopeSize - 1) * sw;
            float y0 = mid - fScope[i0] * sh * 0.45f;
            float y1 = mid - fScope[i1] * sh * 0.45f;
            if (y0 < sy + 1) y0 = sy + 1; if (y0 > sy + sh - 1) y0 = sy + sh - 1;
            if (y1 < sy + 1) y1 = sy + 1; if (y1 > sy + sh - 1) y1 = sy + sh - 1;
            if (i == 0) moveTo(x0, y0); else lineTo(x0, y0);
            lineTo(x1, y1);
        }
        strokeColor(230, 190, 80); strokeWidth(1.5f); stroke();

        // 8 knoppen
        const int n = 11;
        const float step = (W - 40.0f) / n;
        for (int i = 0; i < n; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 178.0f;
            const float R  = 21.0f;
            const float norm = (fValue[i] - fMin[i]) / (fMax[i] - fMin[i]);
            const float ang = -2.4f + norm * 4.8f;

            beginPath(); circle(cx, cy, R);
            fillColor(30, 34, 33); fill();
            strokeColor(42, 47, 45); strokeWidth(2.0f); stroke();

            beginPath();
            arc(cx, cy, R - 4.0f, -2.4f - 1.5708f, ang - 1.5708f, CW);
            strokeColor(230, 190, 80); strokeWidth(3.0f); stroke();

            beginPath();
            moveTo(cx, cy);
            lineTo(cx + std::sin(ang) * (R - 8.0f),
                   cy - std::cos(ang) * (R - 8.0f));
            strokeColor(232, 236, 234); strokeWidth(3.0f); stroke();

            fontSize(10.0f); fillColor(143, 160, 152);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            text(cx, cy + R + 8.0f, fLabel[i], nullptr);
            fontSize(12.0f); fillColor(232, 236, 234);
            char buf[24];
            if (std::strcmp(fUnit[i], "%") == 0)
                std::snprintf(buf, sizeof(buf), "%.0f %%", fMax[i] <= 2.0f ? fValue[i] * 100.0f : fValue[i]);
            else
                std::snprintf(buf, sizeof(buf), "%.0f", fValue[i]);
            text(cx, cy + R + 23.0f, buf, nullptr);
        }

        fontSize(10.5f); fillColor(90, 100, 94);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        text(W * 0.5f, H - 8.0f,
             "klik een preset = instant vibe — POMP duikt op je beat · AGE: 0 = nieuw, 100 = band uit 1978", nullptr);

        {
            float bx2 = 14.0f;
            beginPath(); roundedRect(bx2, 140.0f, 30.0f, 17.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(8.0f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(bx2+15.0f, 148.5f, "DICE", nullptr);
            const float tx = W - 40.0f;
            beginPath(); roundedRect(tx, 34.0f, 26.0f, 17.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            {   // zon/maan icoon
                const float cx = tx + 13.0f, cy = 42.5f;
                if (fLight > 0) {
                    fillColor(g2bsk(fLight,2));
                    beginPath(); arc(cx+1.5f, cy, 5.0f, 0.f, 6.2832f, CW); fill();
                    fillColor(g2bsk(fLight,1));
                    beginPath(); arc(cx-1.8f, cy, 4.4f, 0.f, 6.2832f, CW); fill();
                } else {
                    fillColor(g2bsk(fLight,2));
                    beginPath(); arc(cx, cy, 4.2f, 0.f, 6.2832f, CW); fill();
                    strokeColor(g2bsk(fLight,2)); strokeWidth(1.2f);
                    for (int r = 0; r < 8; ++r) {
                        const float a = r * 0.7854f;
                        beginPath();
                        moveTo(cx + cosf(a) * 5.6f, cy + sinf(a) * 5.6f);
                        lineTo(cx + cosf(a) * 7.4f, cy + sinf(a) * 7.4f);
                        stroke();
                    }
                }
            }
        }

        // AAN/UIT-knop
        {
            const bool aan = fOn > 0.5f;
            beginPath(); roundedRect(W - 96.0f, 33.0f, 52.0f, 18.0f, 9.0f);
            fillColor(aan ? g2bsk(fLight,5) : g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(10.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(aan ? g2bsk(fLight,0) : g2bsk(fLight,2));
            text(W - 70.0f, 42.0f, aan ? "AAN" : "UIT", nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        const float W = getWidth();
        if (ev.press)
        {
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-96.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-44.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 33.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fOn = fOn>0.5f?0.f:1.f; setParameterValue(12, fOn); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 44.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 140.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 157.0f)
            { setParameterValue(10, 1.0f); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-40.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-14.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 34.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fLight = (fLight+1)%8; repaint(); return true; }
            {
                for (int i = 0; i < kNumPrograms; ++i)
                {
                    const int row = i < 8 ? 0 : 1, col = i % 8;
                    const float bw = 66.0f, bh = 17.0f, gap = 5.0f;
                    const float rowW = 8 * bw + 7 * gap;
                    const float bx = (W - rowW) * 0.5f + col * (bw + gap);
                    const float by = row == 0 ? 56.0f : 76.0f;
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= bx && (float)(ev.pos.getX()/getScaleFactor()) <= bx + bw &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= by && (float)(ev.pos.getY()/getScaleFactor()) <= by + bh)
                    {
                        setParameterValue(11, (float)i);
                        for (int p = 0; p < 11; ++p)
                            setParameterValue(parOf(p), kUiPresetParams[i][p]);
                        for (int p = 0; p < 11; ++p)
                            fValue[p] = kUiPresetParams[i][p];
                        fProg = i;
                        repaint();
                        return true;
                    }
                }
            }
        }
        const float step = (W - 40.0f) / 11;
        if (ev.press)
        {
            for (int i = 0; i < 11; ++i)
            {
                const float cx = 20.0f + step * (i + 0.5f);
                const float cy = 178.0f;
                const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
                const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
                if (dx*dx + dy*dy <= 21.0f*21.0f + 80.0f)
                {
                    fDrag = i; fLastY = (float)(ev.pos.getY()/getScaleFactor());
                    editParameter(parOf(i), true);
                    return true;
                }
            }
        }
        else if (fDrag >= 0)
        {
            editParameter(parOf(fDrag), false);
            fDrag = -1;
        }
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fDrag < 0) return false;
        const float dy = fLastY - (float)(ev.pos.getY()/getScaleFactor());
        fLastY = (float)(ev.pos.getY()/getScaleFactor());
        const float range = fMax[fDrag] - fMin[fDrag];
        float v = fValue[fDrag] + dy * range / 200.0f;
        if (v < fMin[fDrag]) v = fMin[fDrag];
        if (v > fMax[fDrag]) v = fMax[fDrag];
        fValue[fDrag] = v;
        setParameterValue(parOf(fDrag), v);
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float W = getWidth();
        const float step = (W - 40.0f) / 11;
        for (int i = 0; i < 11; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 178.0f;
            const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
            const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
            if (dx*dx + dy*dy <= 21.0f*21.0f + 80.0f)
            {
                const float range = fMax[i] - fMin[i];
                float v = fValue[i] + ev.delta.getY() * range / 100.0f;
                if (v < fMin[i]) v = fMin[i];
                if (v > fMax[i]) v = fMax[i];
                fValue[i] = v;
                const int pi = parOf(i);
                editParameter(pi, true);
                setParameterValue(pi, v);
                editParameter(pi, false);
                repaint();
                return true;
            }
        }
        return true;
    }

private:
    // knop-slot → param: 0-5 modules, 6 DIMENTIES(7), 7 KRACHT(11)
    static int parOf(int s){ static const int m[11]={0,1,2,3,4,5,6,7,8,9,13}; return m[s]; }
    static const int kScopeSize = 256;
    float fValue[11], fMin[11], fMax[11];
    const char* fLabel[11];
    const char* fUnit[11];
    float fOn = 1.f;
    float fScope[kScopeSize];
    int fScopePos;
    int fDrag;
    float fLastY;
    int fProg;
    int fLight = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeUI)
};

UI* createUI() { return new VibeUI(); }

END_NAMESPACE_DISTRHO
