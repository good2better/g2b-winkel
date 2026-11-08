/*
 * G2B DREN — UI (NanoVG): droom-machine.
 * 10 macro-knoppen (2 rijen van 5), aflevering-kiezer ‹ › + ZAP,
 * XY-pad (OUTPUT x DIMENTIES), live scope.
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

// G2B skins (8 thema's)
static const float G2BSK[8][7][3]={
    {{13,15,14},{23,26,25},{232,236,234},{100,110,103},{42,47,45},{47,191,113},{29,122,72}},
    {{14,14,22},{22,22,34},{230,232,255},{110,110,150},{40,40,58},{120,110,255},{70,60,170}},
    {{10,20,22},{16,30,32},{225,245,245},{95,130,130},{30,55,55},{40,200,210},{20,120,130}},
    {{24,14,20},{34,20,28},{250,230,240},{150,105,125},{60,38,50},{255,110,180},{180,60,120}},
    {{228,230,232},{240,242,244},{30,34,38},{105,112,118},{205,210,214},{40,140,100},{20,100,70}},
    {{18,10,10},{28,16,16},{240,225,220},{140,100,95},{55,35,32},{200,80,60},{130,45,35}},
    {{14,14,10},{22,22,16},{245,240,220},{120,115,95},{50,48,35},{230,190,80},{160,125,45}},
    {{12,14,20},{20,24,32},{235,240,250},{105,115,135},{36,44,58},{80,160,255},{45,110,200}}};
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

// param-indexen (zie DrenPlugin.cpp)
enum { P_TOON, P_WARM, P_DRIFT, P_WARP, P_RUW, P_ECHO, P_RUIMTE, P_SUB,
       P_DECAY, P_SPOOK, P_MIX, P_LEVEL, P_KEY, P_SCALE, P_DICE, P_PRESET,
       P_ON, P_GALAXY, P_SFEER, P_ZAPBACK, P_TIJD, P_CATCH, P_LOCKS };
#ifdef DREN_LITE
static const int kNumPres = 64;
#define DREN_TITLE "G2B DREN LITE"
#else
static const int kNumPres = 320;
#define DREN_TITLE "G2B DREN"
#endif

class DrenUI : public UI
{
public:
    DrenUI() : UI(560, 372)
    {
        loadSharedResources();
        static const int    par[10] = {P_TOON,P_WARM,P_DRIFT,P_WARP,P_RUW,P_ECHO,P_RUIMTE,P_SUB,P_SPOOK,P_MIX};
        static const float  defs[10] = {0.2f,0.5f,0.15f,0.3f,0.f,0.35f,0.55f,0.25f,0.2f,0.55f};
        static const char*  lbl[10]  = {"TOON","WARM","DRIFT","WARP","RUW","ECHO","RUIMTE","SUB","SPOOK","MIX"};
        std::memcpy(fPar, par, sizeof(par));
        std::memcpy(fValue, defs, sizeof(defs));
        std::memcpy(fLabel, lbl, sizeof(lbl));
        std::memset(fScope, 0, sizeof(fScope));
        std::memset(fLocked, 0, sizeof(fLocked));
        fScopePos = 0; fDrag = -1; fLastY = 0.0f;
        fTijd = 0.f; fTijdDrag = false;
        std::strcpy(fPName, "GOUD-UR-01");
    }

protected:
    bool onKeyboard(const KeyboardEvent& ev) override {
        if(!ev.press)return true;
        static const char kKeys[]="awsedftgyhujkolp;";
        const char* hit=std::strchr(kKeys,(char)std::tolower((int)ev.key));
        if(hit){sendNote(0,(uint8_t)(48+(int)(hit-kKeys)),100);repaint();return true;}
        return false;
    }

    void parameterChanged(uint32_t index, float value) override
    {
        for (int s = 0; s < 10; ++s)
            if (fPar[s] == (int)index) { fValue[s] = value; repaint(); return; }
        if (index == P_ON) { fOn = (int)(value + 0.5f); repaint(); }
        else if (index == P_LEVEL) { fLv = value; repaint(); }
        else if (index == P_GALAXY) { fDm = value; repaint(); }
        else if (index == P_PRESET) { fProg = (int)(value + 0.5f); repaint(); }
        else if (index == P_TIJD) { fTijd = value; repaint(); }
        else if (index == P_LOCKS) { int m = (int)(value + 0.5f);
            for (int j = 0; j < 10; ++j) fLocked[j] = (m >> j) & 1; repaint(); }
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
        else if (std::strcmp(key, "presetname") == 0)
        {
            std::strncpy(fPName, value, sizeof(fPName) - 1);
            fPName[sizeof(fPName) - 1] = 0;
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float W = getWidth(), H = getHeight();
        beginPath(); rect(0, 0, W, H); fillColor(g2bsk(fLight,0)); fill();

        fontSize(20.0f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(47, 191, 113);
        text(18.0f, 20.0f, DREN_TITLE, nullptr);
        fontSize(11.0f); fillColor(143, 160, 152);
        text(18.0f, 38.0f, "de droom-radio die je bespeelt", nullptr);

        // aflevering-kiezer: ‹ naam › + aflevering-nummer
        {
            const float ny = 12.0f, nw = 150.0f, nx = W - nw - 60.0f;
            beginPath(); roundedRect(nx, ny, nw, 34.0f, 6.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(g2bsk(fLight,3)); strokeWidth(1.0f); stroke();
            fontSize(9.0f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,3));
            char pb[24]; std::snprintf(pb, sizeof(pb), "AFLEVERING %03d/%03d", fProg + 1, kNumPres);
            text(nx + nw * 0.5f, ny + 9.0f, pb, nullptr);
            fontSize(12.5f); fillColor(g2bsk(fLight,2));
            text(nx + nw * 0.5f, ny + 23.0f, fPName, nullptr);
            // ‹ › knoppen
            beginPath(); rect(nx + 2, ny + 2, 20.0f, 30.0f);
            fillColor(g2bsk(fLight,4)); fill();
            fontSize(14.0f); fillColor(g2bsk(fLight,2));
            text(nx + 12.0f, ny + 17.0f, "<", nullptr);
            beginPath(); rect(nx + nw - 22.0f, ny + 2, 20.0f, 30.0f);
            fillColor(g2bsk(fLight,4)); fill();
            text(nx + nw - 12.0f, ny + 17.0f, ">", nullptr);
        }

        // ZAP + AAN/UIT
        {
            beginPath(); roundedRect(14.0f, 46.0f, 40.0f, 16.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(9.0f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,5));
            text(34.0f, 54.0f, "ZAP", nullptr);

            const float ax = W - 48.0f, ay = 12.0f;
            beginPath(); roundedRect(ax, ay, 44.0f, 17.0f, 4.0f);
            fillColor(fOn ? g2bsk(fLight,5) : g2bsk(fLight,4)); fill();
            strokeColor(g2bsk(fLight,3)); strokeWidth(1.0f); stroke();
            fontSize(8.5f);
            fillColor(fOn ? Color(232,236,234) : g2bsk(fLight,3));
            text(ax + 22.0f, ay + 8.5f, fOn ? "AAN" : "UIT", nullptr);

            const float tx = W - 48.0f, ty = 33.0f;
            beginPath(); roundedRect(tx, ty, 44.0f, 15.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(9.0f);
            text(tx + 22.0f, ty + 7.5f, fLight>0 ? "\xF0\x9F\x8C\x99" : "\xE2\x98\x80", nullptr);
        }

        // scope
        const float sx = 18.0f, sy = 68.0f, sw = W - 36.0f, sh = 38.0f;
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

        // knoppen: 2 rijen van 5
        for (int i = 0; i < 10; ++i)
        {
            const int row = i / 5, col = i % 5;
            const float step = (W - 40.0f) / 5;
            const float cx = 20.0f + step * (col + 0.5f);
            const float cy = 150.0f + row * 66.0f;
            const float R  = 21.0f;
            const float norm = fValue[i];
            const float ang = -2.4f + norm * 4.8f;

            beginPath(); circle(cx, cy, R);
            fillColor(30, 34, 33); fill();
            strokeColor(42, 47, 45); strokeWidth(2.0f); stroke();
            beginPath();
            arc(cx, cy, R - 4.0f, -2.4f - 1.5708f, ang - 1.5708f, CW);
            strokeColor(47, 191, 113); strokeWidth(3.0f); stroke();
            beginPath();
            moveTo(cx, cy);
            lineTo(cx + std::sin(ang) * (R - 8.0f), cy - std::cos(ang) * (R - 8.0f));
            strokeColor(232, 236, 234); strokeWidth(3.0f); stroke();

            const float lx = cx + R - 9.0f, ly = cy - R - 3.0f;
            beginPath(); roundedRect(lx, ly, 9.0f, 9.0f, 2.0f);
            fillColor(fLocked[i] ? Color(80, 220, 235) : g2bsk(fLight, 4)); fill();

            fontSize(8.5f); fillColor(143, 160, 152);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            text(cx, cy + R + 6.0f, fLabel[i], nullptr);
            fontSize(10.5f); fillColor(232, 236, 234);
            char buf[16]; std::snprintf(buf, sizeof(buf), "%d%%", (int)(fValue[i] * 100.f));
            text(cx, cy + R + 20.0f, buf, nullptr);
        }

        // XY-pad: X = OUTPUT, Y = DIMENTIES
        fXX = 18.0f; fXY_ = 296.0f; fXW = W - 36.0f; fXH = 56.0f;
        {
            const float px = fXX, py = fXY_, pw = fXW, ph = fXH;
            beginPath(); roundedRect(px, py, pw, ph, 6.0f);
            fillColor(g2bsk(fLight,4)); fill();
            strokeColor(g2bsk(fLight,3)); strokeWidth(1.0f); stroke();
            const float nx = (fLv + 24.0f) / 36.0f;
            const float ny = fDm / 26.0f;
            const float dx = px + nx * pw, dy = py + (1.0f - ny) * ph;
            beginPath();
            moveTo(dx, py); lineTo(dx, py + ph);
            moveTo(px, dy); lineTo(px + pw, dy);
            strokeColor(g2bsk(fLight,3)); strokeWidth(0.5f); stroke();
            beginPath(); circle(dx, dy, 5.0f);
            fillColor(g2bsk(fLight,5)); fill();
            fontSize(9.0f); fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(px + 6.0f, py + 4.0f, "DIMENTIES", nullptr);
            textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
            text(px + pw - 6.0f, py + ph - 4.0f, "OUTPUT →", nullptr);
        }

        fontSize(10.0f); fillColor(90, 100, 94);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        text(W * 0.5f, H - 6.0f, "< > = aflevering · ZAP <- = terug · CATCH = bewaar 30s · slotjes = lock", nullptr);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        const float W = getWidth();
        if (ev.press)
        {
            // ZAP
            if (ev.pos.getX() >= 14 && ev.pos.getX() <= 54 && ev.pos.getY() >= 46 && ev.pos.getY() <= 62)
            { editParameter(P_DICE, true); spv(P_DICE, 1.f); editParameter(P_DICE, false); return true; }
            // ZAP-TERUG
            if (ev.pos.getX() >= 58 && ev.pos.getX() <= 84 && ev.pos.getY() >= 46 && ev.pos.getY() <= 62)
            { spv(P_ZAPBACK, 1.f); repaint(); return true; }
            // DREAM CATCHER
            if (ev.pos.getX() >= 88 && ev.pos.getX() <= 132 && ev.pos.getY() >= 46 && ev.pos.getY() <= 62)
            { spv(P_CATCH, 1.f); repaint(); return true; }
            // TIJDREIS strip
            if (ev.pos.getX() >= 136 && ev.pos.getX() <= 220 && ev.pos.getY() >= 46 && ev.pos.getY() <= 62)
            { fTijdDrag = true; tijdSet(ev.pos.getX()); return true; }
#ifdef DREN_LITE
            // upsell-knop
            if (ev.pos.getX() >= 224 && ev.pos.getX() <= 344 && ev.pos.getY() >= 46 && ev.pos.getY() <= 62)
            {
#ifdef _WIN32
                std::system("start \"\" \"https://good2better.com/p.html?id=g2bdren&utm_source=dren_lite\"");
#else
                std::system("xdg-open 'https://good2better.com/p.html?id=g2bdren&utm_source=dren_lite' &");
#endif
                return true;
            }
#endif
            // aflevering ‹ ›
            const float nx = W - 150.0f - 60.0f, ny = 12.0f, nw = 150.0f;
            if (ev.pos.getY() >= ny && ev.pos.getY() <= ny + 34.0f)
            {
                if (ev.pos.getX() >= nx + 2 && ev.pos.getX() <= nx + 22)
                { int np = (fProg - 1 + kNumPres) % kNumPres; spv(P_PRESET, (float)np); return true; }
                if (ev.pos.getX() >= nx + nw - 22 && ev.pos.getX() <= nx + nw - 2)
                { int np = (fProg + 1) % kNumPres; spv(P_PRESET, (float)np); return true; }
            }
            // thema
            if (ev.pos.getX() >= W-48 && ev.pos.getX() <= W-4 && ev.pos.getY() >= 33 && ev.pos.getY() <= 48)
            { fLight = (fLight+1)%8; repaint(); return true; }
            // AAN/UIT
            if (ev.pos.getX() >= W-48 && ev.pos.getX() <= W-4 && ev.pos.getY() >= 12 && ev.pos.getY() <= 29)
            { fOn = 1 - fOn; spv(P_ON, (float)fOn); repaint(); return true; }
            // XY
            if (ev.pos.getX() >= fXX && ev.pos.getX() <= fXX + fXW &&
                ev.pos.getY() >= fXY_ && ev.pos.getY() <= fXY_ + fXH)
            { fXYdrag = true; xySet(ev.pos.getX(), ev.pos.getY()); return true; }
            // slotjes + knoppen
            for (int i = 0; i < 10; ++i)
            {
                const int row = i / 5, col = i % 5;
                const float step = (W - 40.0f) / 5;
                const float cx = 20.0f + step * (col + 0.5f);
                const float cy = 150.0f + row * 66.0f;
                const float lx = cx + 21.0f - 9.0f, ly = cy - 21.0f - 3.0f;
                if (ev.pos.getX() >= lx && ev.pos.getX() <= lx + 9 && ev.pos.getY() >= ly && ev.pos.getY() <= ly + 9)
                { fLocked[i] = !fLocked[i];
                int m = 0; for (int j = 0; j < 10; ++j) if (fLocked[j]) m |= (1 << j);
                spv(P_LOCKS, (float)m); repaint(); return true; }
            }
        }
        else if (fXYdrag) { fXYdrag = false; return true; }
        else if (fTijdDrag) { fTijdDrag = false; return true; }

        if (ev.press)
        {
            for (int i = 0; i < 10; ++i)
            {
                const int row = i / 5, col = i % 5;
                const float step = (W - 40.0f) / 5;
                const float cx = 20.0f + step * (col + 0.5f);
                const float cy = 150.0f + row * 66.0f;
                const float dx = ev.pos.getX() - cx;
                const float dy = ev.pos.getY() - cy;
                if (dx*dx + dy*dy <= 21.0f*21.0f + 80.0f)
                { fDrag = i; fLastY = ev.pos.getY(); editParameter(fPar[i], true); return true; }
            }
        }
        else if (fDrag >= 0) { editParameter(fPar[fDrag], false); fDrag = -1; }
        return true;
    }

    void xySet(double mx, double my)
    {
        float nx = (float)((mx - fXX) / fXW);
        float ny = (float)(1.0 - (my - fXY_) / fXH);
        if (nx < 0) nx = 0; if (nx > 1) nx = 1;
        if (ny < 0) ny = 0; if (ny > 1) ny = 1;
        fLv = -24.f + nx * 36.f; fDm = ny * 26.f;
        editParameter(P_LEVEL, true);  spv(P_LEVEL, fLv);  editParameter(P_LEVEL, false);
        editParameter(P_GALAXY, true); spv(P_GALAXY, fDm); editParameter(P_GALAXY, false);
        repaint();
    }

    void tijdSet(double mx)
    {
        float t = (float)((mx - 136.0) / 84.0);
        if (t < 0.f) t = 0.f; if (t > 1.f) t = 1.f;
        fTijd = t;
        spv(P_TIJD, t);
        repaint();
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fXYdrag) { xySet(ev.pos.getX(), ev.pos.getY()); return true; }
        if (fTijdDrag) { tijdSet(ev.pos.getX()); return true; }
        if (fDrag < 0) return false;
        const float dy = fLastY - (float)ev.pos.getY();
        fLastY = (float)ev.pos.getY();
        float v = fValue[fDrag] + dy / 200.0f;
        if (v < 0.f) v = 0.f; if (v > 1.f) v = 1.f;
        fValue[fDrag] = v;
        spv(fPar[fDrag], v);
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float W = getWidth();
        for (int i = 0; i < 10; ++i)
        {
            const int row = i / 5, col = i % 5;
            const float step = (W - 40.0f) / 5;
            const float cx = 20.0f + step * (col + 0.5f);
            const float cy = 150.0f + row * 66.0f;
            const float dx = ev.pos.getX() - cx;
            const float dy = ev.pos.getY() - cy;
            if (dx*dx + dy*dy <= 21.0f*21.0f + 80.0f)
            {
                float v = fValue[i] + ev.delta.getY() / 100.0f;
                if (v < 0.f) v = 0.f; if (v > 1.f) v = 1.f;
                fValue[i] = v;
                editParameter(fPar[i], true); spv(fPar[i], v); editParameter(fPar[i], false);
                repaint();
                return true;
            }
        }
        return true;
    }

private:
    static const int kScopeSize = 256;
    int   fPar[10];
    float fValue[10];
    bool  fLocked[10];
    const char* fLabel[10];
    float fScope[kScopeSize];
    int   fScopePos;
    int   fDrag;
    float fLastY;
    int   fLight = 0;
    int   fOn = 1;
    int   fProg = 0;
    float fLv = 0.f, fDm = 9.f;
    bool  fXYdrag = false;
    float fXX = 18.0f, fXY_ = 296.0f, fXW = 524.0f, fXH = 56.0f;
    char  fPName[24];
    float fTijd = 0.f;
    bool  fTijdDrag = false;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrenUI)
};

UI* createUI() { return new DrenUI(); }
END_NAMESPACE_DISTRHO
