/*
 * G2B Poets — UI (NanoVG): donkergroen paneel, één grote POETS-draai
 * + kleine OUTPUT, met een live oscilloscope (echte golfvorm van het
 * uitgaande signaal) en een glow die oplicht bij elke punch.
 * Sleep omhoog/omlaag = draaien; scroll = fijn.
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

static const char* kPresetNames[17] = {
    "G2B","DEMBOW","DRILL","RNB","HOUSE","AFRO","TRAP","BOOM","UKG","JERSEY","BOUNCE","SOUL","LOFI","FUNK","REGGAE","DANCE","KAW"
};
static const int kNumPresets = 17;



class PoetsUI : public UI
{
public:
    PoetsUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        std::memset(fValue, 0, sizeof(fValue));
        std::memset(fScope, 0, sizeof(fScope));
        fMin[0] = 0.0f;  fMax[0] = 100.0f;
        fMin[2]=0.0f;  fMax[2]=26.0f;  fValue[2]=9.0f;
        fMin[1] = -6.0f; fMax[1] = 6.0f;
        fMin[3] = 4000.0f; fMax[3] = 16000.0f; fValue[3] = 10000.0f;
        fMin[4] = 0.0f;    fMax[4] = 100.0f;   fValue[4] = 100.0f;
        fPunch = 0.0f;
        fScopePos = 0;
        fDrag = -1;
        fLastY = 0.0f;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index == 6) fOn = value;   // AAN/UIT
        for (int i = 0; i < 5; ++i)
            if (kParamIds[i] == (int)index) { fValue[i] = value; break; }
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "scope") == 0)
        {
            // "v1;v2;..." → append aan ringbuffer
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
        }
        else if (std::strcmp(key, "punch") == 0)
        {
            const float v = std::strtof(value, nullptr);
            if (v > fPunch) fPunch = v;
        }
        repaint();
    }

    void uiIdle() override
    {
        // glow dooft langzaam uit — elke idle-tick
        if (fPunch > 0.0f)
        {
            fPunch *= 0.92f;
            if (fPunch < 0.001f) fPunch = 0.0f;
            repaint();
        }
    }

    void onNanoDisplay() override
    {
        const float W = getWidth(), H = getHeight();

        // achtergrond
        beginPath();
        rect(0, 0, W, H);
        fillColor(13, 15, 14);
        fill();

        // titel
        fontSize(20.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(47, 191, 113);
        text(18.0f, 24.0f, "G2B POETS", nullptr);
        fontSize(11.5f);
        fillColor(143, 160, 152);
        text(18.0f, 43.0f, "punch + body + sizzle — één draai", nullptr);

        // ---- oscilloscope (rechtsboven) ----
        const float sx = 170.0f, sy = 12.0f, sw = W - 188.0f, sh = 46.0f;
        beginPath();
        rect(sx, sy, sw, sh);
        fillColor(8, 10, 9);
        fill();
        strokeColor(42, 47, 45);
        strokeWidth(1.0f);
        stroke();

        beginPath();
        const float mid = sy + sh * 0.5f;
        for (int i = 0; i < kScopeSize - 1; ++i)
        {
            const int idx0 = (fScopePos + i) % kScopeSize;
            const int idx1 = (fScopePos + i + 1) % kScopeSize;
            const float x0 = sx + (float)i / (kScopeSize - 1) * sw;
            const float x1 = sx + (float)(i + 1) / (kScopeSize - 1) * sw;
            float y0 = mid - fScope[idx0] * sh * 0.45f;
            float y1 = mid - fScope[idx1] * sh * 0.45f;
            if (y0 < sy + 1) y0 = sy + 1;
            if (y0 > sy + sh - 1) y0 = sy + sh - 1;
            if (y1 < sy + 1) y1 = sy + 1;
            if (y1 > sy + sh - 1) y1 = sy + sh - 1;
            if (i == 0) moveTo(x0, y0); else lineTo(x0, y0);
            lineTo(x1, y1);
        }
        strokeColor(47, 191, 113);
        strokeWidth(1.5f);
        stroke();

        // ---- grote POETS-knop (links-centraal) met punch-glow ----
        const float pcx = 120.0f, pcy = 175.0f, pR = 62.0f;
        const float norm0 = (fValue[0] - fMin[0]) / (fMax[0] - fMin[0]);
        const float ang0 = -2.4f + norm0 * 4.8f;

        // glow: groene waas die oplicht bij punch-transienten
        const float gl = fPunch * 2.2f;
        if (gl > 0.02f)
        {
            const float gg = gl > 1.0f ? 1.0f : gl;
            beginPath();
            circle(pcx, pcy, pR + 14.0f);
            const Paint grad = radialGradient(pcx, pcy, pR * 0.5f, pR + 14.0f,
                                              Color(47, 191, 113, (int)(gg * 90)),
                                              Color(47, 191, 113, 0));
            fillPaint(grad);
            fill();
        }

        beginPath();
        circle(pcx, pcy, pR);
        fillColor(30, 34, 33);
        fill();
        strokeColor(42, 47, 45);
        strokeWidth(2.5f);
        stroke();

        beginPath();
        arc(pcx, pcy, pR - 6.0f, -2.4f - 1.5708f, ang0 - 1.5708f, CW);
        strokeColor(47, 191, 113);
        strokeWidth(5.0f);
        stroke();

        beginPath();
        moveTo(pcx, pcy);
        lineTo(pcx + std::sin(ang0) * (pR - 16.0f),
               pcy - std::cos(ang0) * (pR - 16.0f));
        strokeColor(232, 236, 234);
        strokeWidth(4.0f);
        stroke();

        fontSize(14.0f);
        fillColor(143, 160, 152);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(pcx, pcy + pR + 10.0f, "POETS", nullptr);
        fontSize(16.0f);
        fillColor(232, 236, 234);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f %%", fValue[0]);
        text(pcx, pcy + pR + 27.0f, buf, nullptr);

        // ---- kleine OUTPUT-knop (rechts) ----
        const float ocx = W - 55.0f, ocy = 175.0f, oR = 30.0f;
        const float norm1 = (fValue[1] - fMin[1]) / (fMax[1] - fMin[1]);
        const float ang1 = -2.4f + norm1 * 4.8f;

        beginPath();
        circle(ocx, ocy, oR);
        fillColor(30, 34, 33);
        fill();
        strokeColor(42, 47, 45);
        strokeWidth(2.0f);
        stroke();

        beginPath();
        arc(ocx, ocy, oR - 5.0f, -2.4f - 1.5708f, ang1 - 1.5708f, CW);
        strokeColor(29, 122, 72);
        strokeWidth(3.5f);
        stroke();

        beginPath();
        moveTo(ocx, ocy);
        lineTo(ocx + std::sin(ang1) * (oR - 10.0f),
               ocy - std::cos(ang1) * (oR - 10.0f));
        strokeColor(232, 236, 234);
        strokeWidth(3.0f);
        stroke();

        fontSize(11.0f);
        fillColor(143, 160, 152);
        text(ocx, ocy + oR + 8.0f, "OUTPUT", nullptr);
        fontSize(12.5f);
        fillColor(232, 236, 234);
        std::snprintf(buf, sizeof(buf), "%.1f dB", fValue[1]);
        text(ocx, ocy + oR + 24.0f, buf, nullptr);

        // ---- kleine FOKUS-knop (rechts van GALAXY) ----
        const float fcx = 415.0f, fcy = 175.0f, fR = 30.0f;
        const float norm3 = (fValue[3] - fMin[3]) / (fMax[3] - fMin[3]);
        const float ang3 = -2.4f + norm3 * 4.8f;
        beginPath(); circle(fcx, fcy, fR); fillColor(30, 34, 33); fill();
        strokeColor(42, 47, 45); strokeWidth(2.0f); stroke();
        beginPath();
        arc(fcx, fcy, fR - 5.0f, -2.4f - 1.5708f, ang3 - 1.5708f, CW);
        strokeColor(80, 170, 200); strokeWidth(3.5f); stroke();
        beginPath(); moveTo(fcx, fcy);
        lineTo(fcx + std::sin(ang3) * (fR - 10.0f), fcy - std::cos(ang3) * (fR - 10.0f));
        strokeColor(232, 236, 234); strokeWidth(3.0f); stroke();
        fontSize(11.0f); fillColor(143, 160, 152);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(fcx, fcy + fR + 8.0f, "FOKUS", nullptr);
        fontSize(12.5f); fillColor(232, 236, 234);
        std::snprintf(buf, sizeof(buf), "%.1f kHz", fValue[3] / 1000.0f);
        text(fcx, fcy + fR + 24.0f, buf, nullptr);

        // ---- GALAXY-knop (midden) ----
        const float gcx = W / 2.0f, gcy = 175.0f, gR = 44.0f;
        const float norm2 = (fValue[2] - fMin[2]) / (fMax[2] - fMin[2]);
        const float ang2 = -2.4f + norm2 * 4.8f;
        beginPath(); circle(gcx, gcy, gR); fillColor(30, 34, 33); fill();
        strokeColor(42, 47, 45); strokeWidth(2.0f); stroke();
        beginPath();
        arc(gcx, gcy, gR - 5.0f, -2.4f - 1.5708f, ang2 - 1.5708f, CW);
        strokeColor(130, 90, 230); strokeWidth(4.0f); stroke();
        beginPath(); moveTo(gcx, gcy);
        lineTo(gcx + std::sin(ang2) * (gR - 11.0f), gcy - std::cos(ang2) * (gR - 11.0f));
        strokeColor(232, 236, 234); strokeWidth(3.0f); stroke();
        fontSize(13.0f); fillColor(143, 160, 152);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(gcx, gcy + gR + 8.0f, "GALAXY", nullptr);
        { int gp=(int)fValue[2]; std::snprintf(buf,sizeof(buf),"%s %d",gp<9?"DARK":(gp<18?"LIGHT":"GALAXY"),gp%9+1); }
        fontSize(13.5f); fillColor(200, 170, 255);
        text(gcx, gcy + gR + 23.0f, buf, nullptr);

        // ---- KRACHT-knop (tussen GALAXY en FOKUS) ----
        const float kcx = 354.0f, kcy = 175.0f, kR = 28.0f;
        const float norm4 = (fValue[4] - fMin[4]) / (fMax[4] - fMin[4]);
        const float ang4 = -2.4f + norm4 * 4.8f;
        beginPath(); circle(kcx, kcy, kR); fillColor(30, 34, 33); fill();
        strokeColor(42, 47, 45); strokeWidth(2.0f); stroke();
        beginPath();
        arc(kcx, kcy, kR - 5.0f, -2.4f - 1.5708f, ang4 - 1.5708f, CW);
        strokeColor(200, 170, 80); strokeWidth(3.0f); stroke();
        beginPath(); moveTo(kcx, kcy);
        lineTo(kcx + std::sin(ang4) * (kR - 9.0f), kcy - std::cos(ang4) * (kR - 9.0f));
        strokeColor(232, 236, 234); strokeWidth(3.0f); stroke();
        fontSize(11.0f); fillColor(143, 160, 152);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(kcx, kcy + kR + 8.0f, "KRACHT", nullptr);
        fontSize(12.0f); fillColor(232, 236, 234);
        std::snprintf(buf, sizeof(buf), "%.0f %%", fValue[4]);
        text(kcx, kcy + kR + 23.0f, buf, nullptr);

        // hulptekst
        fontSize(10.5f);
        fillColor(90, 100, 94);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        
        // preset-knoppen
        {
            for (int i = 0; i < kNumPresets; ++i)
            {
                const float bw = 40.0f, bh = 14.0f;
                const int prow = i / 9, pcol = i % 9;
                const int cols = (prow == 0 ? 9 : 8);
                const float px = W - 18.0f - (cols - pcol) * (bw + 3.0f), py = 64.0f + prow * 15.0f;
                const bool on = (i == fProg);
                beginPath(); roundedRect(px, py, bw, bh, 4.0f);
                if (on) { fillColor(29, 122, 72); fill(); }
                strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
                fontSize(7.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(on ? Color(232, 236, 234) : Color(143, 160, 152));
                text(px + bw * 0.5f, py + bh * 0.5f, kPresetNames[i], nullptr);
            }
        }

        text(W * 0.5f, H - 8.0f,
             "sleep omhoog/omlaag om te draaien — scroll = fijn", nullptr);

        {
            float bx2 = 14.0f;
            beginPath(); roundedRect(bx2, 48.0f, 26.0f, 17.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(11.0f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(bx2+13.0f, 56.5f, "\xF0\x9F\x8E\xB2", nullptr);
            const float tx = W - 40.0f;
            beginPath(); roundedRect(tx, 34.0f, 26.0f, 17.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(10.0f);
            text(tx+13.0f, 42.5f, fLight>0 ? "\xF0\x9F\x8C\x99" : "\xE2\x98\x80", nullptr);
        }

        // AAN/UIT-knop (rechtsboven, naast thema-knop)
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
        const float knobs[5][3] = {{120.0f, 175.0f, 62.0f},
                                   {W - 55.0f, 175.0f, 30.0f},
                                   {W / 2.0f, 175.0f, 44.0f},
                                   {415.0f, 175.0f, 30.0f},
                                   {354.0f, 175.0f, 28.0f}};
        if (ev.press)
        {
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-96.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-44.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 33.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fOn = fOn>0.5f?0.f:1.f; spv(6, fOn); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 40.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 48.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 65.0f)
            { spv(3, 1.0f); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-40.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-14.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 34.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fLight = (fLight+1)%8; repaint(); return true; }

            // preset-klik?
            {
                for (int i = 0; i < kNumPresets; ++i)
                {
                    const float bw = 40.0f, bh = 14.0f;
                    const int prow = i / 9, pcol = i % 9;
                    const int cols = (prow == 0 ? 9 : 8);
                    const float px = W - 18.0f - (cols - pcol) * (bw + 3.0f), py = 64.0f + prow * 15.0f;
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= px && (float)(ev.pos.getX()/getScaleFactor()) <= px + bw &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= py && (float)(ev.pos.getY()/getScaleFactor()) <= py + bh)
                    {
                        spv(4, (float)i);
                        fProg = i;
                        repaint();
                        return true;
                    }
                }
            }

            for (int i = 0; i < 5; ++i)
            {
                const float dx = (float)(ev.pos.getX()/getScaleFactor()) - knobs[i][0];
                const float dy = (float)(ev.pos.getY()/getScaleFactor()) - knobs[i][1];
                if (dx*dx + dy*dy <= knobs[i][2]*knobs[i][2] + 100.0f)
                {
                    fDrag = i;
                    fLastY = (float)(ev.pos.getY()/getScaleFactor());
                    editParameter(kParamIds[i], true);
                    return true;
                }
            }
        }
        else if (fDrag >= 0)
        {
            editParameter(kParamIds[fDrag], false);
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
        spv(kParamIds[fDrag], v);
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float W = getWidth();
        const float knobs[5][3] = {{120.0f, 175.0f, 62.0f},
                                   {W - 55.0f, 175.0f, 30.0f},
                                   {W / 2.0f, 175.0f, 44.0f},
                                   {415.0f, 175.0f, 30.0f},
                                   {354.0f, 175.0f, 28.0f}};
        for (int i = 0; i < 5; ++i)
        {
            const float dx = (float)(ev.pos.getX()/getScaleFactor()) - knobs[i][0];
            const float dy = (float)(ev.pos.getY()/getScaleFactor()) - knobs[i][1];
            if (dx*dx + dy*dy <= knobs[i][2]*knobs[i][2] + 100.0f)
            {
                const float range = fMax[i] - fMin[i];
                float v = fValue[i] + ev.delta.getY() * range / 100.0f;
                if (v < fMin[i]) v = fMin[i];
                if (v > fMax[i]) v = fMax[i];
                fValue[i] = v;
                editParameter(kParamIds[i], true);
                spv(kParamIds[i], v);
                editParameter(kParamIds[i], false);
                repaint();
                return true;
            }
        }
        return true;
    }

private:
    static const int kScopeSize = 256;
    float fValue[5], fMin[5], fMax[5];
    static const int kParamIds[5];
    float fOn = 1.f;
    float fScope[kScopeSize];
    int fScopePos;
    float fPunch;
    int fDrag;
    float fLastY;
    int fProg = -1;
    int fLight = 0;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PoetsUI)
};

// slot (UI) → param-id (DSP): POETS, OUTPUT, GALAXY, FOKUS
const int PoetsUI::kParamIds[5] = {0, 1, 2, 5, 7};

UI* createUI() { return new PoetsUI(); }

END_NAMESPACE_DISTRHO
