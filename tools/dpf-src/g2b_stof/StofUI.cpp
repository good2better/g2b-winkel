/*
 * G2B Knip — UI (NanoVG): 5 draaiknoppen + preset-rij + live scope.
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

static const int kNumPrograms = 17;
static const char* kProgramNames[kNumPrograms] = {
    "G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"
};

static const float kUiPresetParams[17][6] = {
    {0.25f,0.20f,0.35f,0.15f,1.0f,0.f},   // G2B — subtiel stof
    {0.30f,0.25f,0.45f,0.20f,1.0f,0.f},   // DEMBOW
    {0.40f,0.30f,0.55f,0.30f,1.0f,-1.f},  // DRILL — donker lo-fi
    {0.20f,0.15f,0.30f,0.10f,1.0f,0.f},   // RNB — zacht
    {0.15f,0.10f,0.20f,0.10f,1.0f,0.f},   // HOUSE — licht
    {0.35f,0.35f,0.45f,0.25f,1.0f,0.f},   // AFRO
    {0.45f,0.30f,0.50f,0.35f,1.0f,-1.f}, // TRAP
    {0.30f,0.25f,0.40f,0.20f,1.0f,0.f},   // BOOM
    {0.28f,0.22f,0.38f,0.18f,1.0f,0.f},   // UKG
    {0.32f,0.28f,0.42f,0.22f,1.0f,0.f},   // JERSEY
    {0.35f,0.30f,0.45f,0.25f,1.0f,0.f},   // BOUNCE
    {0.22f,0.18f,0.32f,0.12f,1.0f,0.f},   // SOUL
    {0.60f,0.55f,0.65f,0.50f,1.0f,-2.f},  // LOFI — maximale stof
    {0.30f,0.35f,0.40f,0.28f,1.0f,0.f},   // FUNK
    {0.38f,0.32f,0.44f,0.26f,1.0f,0.f},   // REGGAE
    {0.20f,0.12f,0.28f,0.14f,1.0f,0.f},   // DANCE
    {0.33f,0.28f,0.42f,0.22f,1.0f,0.f},   // KAWINA

};

class StofUI : public UI
{
public:
    StofUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[8]  = {0,0,0,0,0,-24,0,0};
        static const float maxs[8]  = {1,1,1,1,1,12,26,100};
        static const float defs[8]  = {0.25f,0.2f,0.35f,0.15f,1.0f,0,9,100};
        static const char* lbl[8]   = {"KRAAK","WOBBLE","TOON","CRUSH","MIX","OUTPUT","GALAXY","KRACHT"};
        static const char* unt[8]   = {"%","%","%","%","%","dB","","%"};
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
        if (index < 7) fValue[index] = value;
        else if (index == 9) fOn = value;
        else if (index == 10) fValue[7] = value;
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
        else if (std::strcmp(key, "knobs") == 0)
        {
            const char* p = value;
            for (int i = 0; i < 8 && *p != '\0'; ++i)
            {
                char* end = nullptr;
                const float v = std::strtof(p, &end);
                if (end == p) break;
                fValue[i] = v;
                p = (*end == ';') ? end + 1 : end;
            }
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float W = getWidth(), H = getHeight();
        beginPath(); rect(0, 0, W, H); fillColor(g2bsk(fLight,0)); fill();

        fontSize(13.0f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(47, 191, 113);
        text(14.0f, H - 12.0f, "G2B STOF", nullptr);
        fontSize(10.0f); fillColor(143, 160, 152);
        text(76.0f, 53.0f, "vinyl-stof: kraak·wobble·crush", nullptr);

        // preset-knoppen (2 rijen rechtsboven: 9 + 8)
        const float pbw = 52.0f, pbh = 15.0f;
        for (int i = kNumPrograms - 1; i >= 0; --i)
        {
            const int row = (i < 9) ? 0 : 1;
            const int col = (i < 9) ? i : i - 9;
            const int cnt = (row == 0) ? 9 : 8;
            const float bx = W - 14.0f - (float)(cnt - col) * (pbw + 5.0f);
            const float by = (row == 0) ? 12.0f : 31.0f;
            const bool on = (i == fProg);
            beginPath(); roundedRect(bx, by, pbw, pbh, 4.0f);
            if (on) { fillColor(29, 122, 72); fill(); }
            strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
            fontSize(9.0f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(on ? Color(232, 236, 234) : Color(143, 160, 152));
            text(bx + pbw * 0.5f, by + pbh * 0.5f, kProgramNames[i], nullptr);
        }

        // scope (onder presets)
        const float sx = 18.0f, sy = 58.0f, sw = W - 36.0f, sh = 44.0f;
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
        strokeColor(47, 191, 113); strokeWidth(1.5f); stroke();

        // 5 knoppen
        const int n = 8;
        const float step = (W - 40.0f) / n;
        for (int i = 0; i < n; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 175.0f;
            const float R  = 28.0f;
            const float norm = (fValue[i] - fMin[i]) / (fMax[i] - fMin[i]);
            const float ang = -2.4f + norm * 4.8f;

            beginPath(); circle(cx, cy, R);
            fillColor(30, 34, 33); fill();
            strokeColor(42, 47, 45); strokeWidth(2.0f); stroke();

            beginPath();
            arc(cx, cy, R - 4.0f, -2.4f - 1.5708f, ang - 1.5708f, CW);
            strokeColor(47, 191, 113); strokeWidth(3.0f); stroke();

            beginPath();
            moveTo(cx, cy);
            lineTo(cx + std::sin(ang) * (R - 8.0f),
                   cy - std::cos(ang) * (R - 8.0f));
            strokeColor(232, 236, 234); strokeWidth(3.0f); stroke();

            fontSize(10.5f); fillColor(143, 160, 152);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            text(cx, cy + R + 8.0f, fLabel[i], nullptr);
            fontSize(12.0f); fillColor(232, 236, 234);
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%.1f %s", fValue[i], fUnit[i]);
            text(cx, cy + R + 23.0f, buf, nullptr);
        }

        fontSize(10.5f); fillColor(90, 100, 94);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        text(W * 0.5f, H - 8.0f,
             "klik een preset = knoppen staan gelijk goed — sleep = draaien", nullptr);

        {
            float bx2 = 14.0f;
            beginPath(); roundedRect(bx2, 12.0f, 26.0f, 15.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(11.0f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(bx2+13.0f, 19.5f, "\xF0\x9F\x8E\xB2", nullptr);
            const float tx = 46.0f;
            beginPath(); roundedRect(tx, 12.0f, 26.0f, 15.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(10.0f);
            text(tx+13.0f, 19.5f, fLight>0 ? "\xF0\x9F\x8C\x99" : "\xE2\x98\x80", nullptr);
        }

        // AAN/UIT-knop (rechtsboven, naast thema-knop)
        {
            const bool aan = fOn > 0.5f;
            beginPath(); roundedRect(14.0f, 31.0f, 52.0f, 15.0f, 7.0f);
            fillColor(aan ? g2bsk(fLight,5) : g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(10.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(aan ? g2bsk(fLight,0) : g2bsk(fLight,2));
            text(40.0f, 38.5f, aan ? "AAN" : "UIT", nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        const float W = getWidth();
        // preset-klik?
        if (ev.press)
        {
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 66.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 31.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 46.0f)
            { fOn = fOn>0.5f?0.f:1.f; spv(9, fOn); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 40.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 12.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 27.0f)
            {
            fProg = -1;
            fRndUi=fRndUi*1664525u+1013904223u; { const float v=0.0f+((fRndUi>>8)%100)/100.f*(0.6f-(0.0f)); fValue[0]=v; spv(0,v); }
            fRndUi=fRndUi*1664525u+1013904223u; { const float v=0.0f+((fRndUi>>8)%100)/100.f*(0.5f-(0.0f)); fValue[1]=v; spv(1,v); }
            fRndUi=fRndUi*1664525u+1013904223u; { const float v=0.1f+((fRndUi>>8)%100)/100.f*(0.7f-(0.1f)); fValue[2]=v; spv(2,v); }
            fRndUi=fRndUi*1664525u+1013904223u; { const float v=0.0f+((fRndUi>>8)%100)/100.f*(0.4f-(0.0f)); fValue[3]=v; spv(3,v); }
            repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 46.0 && (float)(ev.pos.getX()/getScaleFactor()) <= 72.0 &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 12.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 27.0f)
            { fLight = (fLight+1)%8; repaint(); return true; }

            {
                const float hbw = 52.0f, hbh = 15.0f;
                for (int i = kNumPrograms - 1; i >= 0; --i)
                {
                    const int row = (i < 9) ? 0 : 1;
                    const int col = (i < 9) ? i : i - 9;
                    const int cnt = (row == 0) ? 9 : 8;
                    const float hbx = W - 14.0f - (float)(cnt - col) * (hbw + 5.0f);
                    const float hby = (row == 0) ? 12.0f : 31.0f;
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= hbx && (float)(ev.pos.getX()/getScaleFactor()) <= hbx + hbw &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= hby && (float)(ev.pos.getY()/getScaleFactor()) <= hby + hbh)
                    {
                        fProg = i;
                        for (int p = 0; p < 6; ++p){ fValue[p] = kUiPresetParams[i][p]; spv(p, fValue[p]); }
                        spv(8, (float)i);
                        repaint();
                        return true;
                    }
                }
            }
        }
        const float step = (W - 40.0f) / 8;
        if (ev.press)
        {
            for (int i = 0; i < 8; ++i)
            {
                const float cx = 20.0f + step * (i + 0.5f);
                const float cy = 175.0f;
                const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
                const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
                if (dx*dx + dy*dy <= 28.0f*28.0f + 80.0f)
                {
                    fDrag = i; fLastY = (float)(ev.pos.getY()/getScaleFactor());
                    editParameter(i == 7 ? 10 : i, true);
                    return true;
                }
            }
        }
        else if (fDrag >= 0)
        {
            editParameter(fDrag == 7 ? 10 : fDrag, false);
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
        spv(fDrag == 7 ? 10 : fDrag, v);
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float W = getWidth();
        const float step = (W - 40.0f) / 8;
        for (int i = 0; i < 8; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 175.0f;
            const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
            const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
            if (dx*dx + dy*dy <= 28.0f*28.0f + 80.0f)
            {
                const float range = fMax[i] - fMin[i];
                float v = fValue[i] + ev.delta.getY() * range / 100.0f;
                if (v < fMin[i]) v = fMin[i];
                if (v > fMax[i]) v = fMax[i];
                fValue[i] = v;
                editParameter(i == 7 ? 10 : i, true);
                spv(i == 7 ? 10 : i, v);
                editParameter(i == 7 ? 10 : i, false);
                repaint();
                return true;
            }
        }
        return true;
    }

private:
    static const int kScopeSize = 256;
    float fValue[8], fMin[8], fMax[8];
    const char* fLabel[8];
    const char* fUnit[8];
    float fOn = 1.f;
    float fScope[kScopeSize];
    int fScopePos;
    int fDrag;
    float fLastY;
    int fProg;
    unsigned fRndUi=987654u;
    int fLight = 0;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StofUI)
};

UI* createUI() { return new StofUI(); }

END_NAMESPACE_DISTRHO
