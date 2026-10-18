/*
 * G2B SampleX — UI (NanoVG): LAAD SAMPLE knop + preset-rij + scope +
 * 7 draaiknoppen (PITCH/FILTER/DRIVE/DROP/START/LEVEL + preset-knoppen).
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>

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

static void b64enc(const std::vector<unsigned char>& in, std::string& out)
{
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0;
    out.clear();
    while (i + 2 < in.size())
    {
        const unsigned v = (in[i] << 16) | (in[i+1] << 8) | in[i+2];
        out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63];
        out += T[(v >> 6) & 63];  out += T[v & 63];
        i += 3;
    }
    if (i < in.size())
    {
        unsigned v = in[i] << 16;
        const bool two = (i + 1 < in.size());
        if (two) v |= in[i+1] << 8;
        out += T[(v >> 18) & 63]; out += T[(v >> 12) & 63];
        out += two ? T[(v >> 6) & 63] : '=';
        out += '=';
    }
}

class SampleXUI : public UI
{
public:
    SampleXUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        static const float mins[9]  = {-12, 500, 0, 0, 0, 0,0,0,0};
        static const float maxs[9]  = { 12, 15000, 100, 100, 90, 100,26,100,2};
        static const float defs[9]  = {  0, 8000, 30, 40, 0, 85,9,100,1};
        static const char* lbl[9]   = {"PITCH","FILTER","DRIVE","DROP","START","LEVEL","DIMENTIES","KRACHT","SFEER"};
        std::memcpy(fMin, mins, sizeof(mins));
        std::memcpy(fMax, maxs, sizeof(maxs));
        std::memcpy(fValue, defs, sizeof(defs));
        std::memcpy(fLabel, lbl, sizeof(lbl));
        std::memset(fScope, 0, sizeof(fScope));
        fScopePos = 0; fDrag = -1; fLastY = 0.0f; fProg = 0; fSnapOn=0.f; fChopN=0;
        fName = "geen sample geladen — klik LAAD SAMPLE";
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < 7) fValue[index] = value;
        if (index == 13) fChopN = (int)value;
        if (index == 14) fSnapOn = value;
        if (index == 16) fOn = (int)(value + 0.5f);
        if (index == 17) fValue[7] = value; // KRACHT
        if (index == 18) fValue[8] = value; // SFEER
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "autofit")==0 || std::strcmp(key, "wav")==0) { fName=value; repaint(); return; }
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

    void uiFileBrowserSelected(const char* filename) override
    {
        std::ifstream in(filename, std::ios::binary);
        if (!in) { fName = "kon bestand niet openen"; repaint(); return; }
        std::vector<unsigned char> buf((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
        // hard cap ~4MB wav → ~5.4MB base64
        if (buf.size() > 4 * 1024 * 1024) buf.resize(4 * 1024 * 1024);
        std::string b64;
        b64enc(buf, b64);
        setState("sample", b64.c_str());
        const char* base = std::strrchr(filename, '/');
        if (!base) base = std::strrchr(filename, '\\');
        fName = base ? base + 1 : filename;
        if (fName.size() > 44) fName = fName.substr(0, 41) + "...";
        repaint();
    }

    void onNanoDisplay() override
    {
        const float W = getWidth(), H = getHeight();
        beginPath(); rect(0, 0, W, H); fillColor(g2bsk(fLight,0)); fill();

        fontSize(20.0f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(47, 191, 113);
        text(18.0f, 22.0f, "G2B SAMPLEX", nullptr);
        fontSize(11.5f); fillColor(143, 160, 152);
        text(18.0f, 41.0f, "jouw one-shot/loop → jouw eigen instrument", nullptr);

        // LAAD SAMPLE knop
        const bool hover = false;
        beginPath(); roundedRect(18.0f, 56.0f, 150.0f, 24.0f, 5.0f);
        fillColor(hover ? Color(47, 191, 113) : Color(29, 122, 72)); fill();
        strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
        fontSize(11.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(232, 236, 234);
        text(18.0f + 75.0f, 56.0f + 12.0f, "LAAD SAMPLE", nullptr);

        fontSize(10.0f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(143, 160, 152);
        text(178.0f, 68.0f, fName.c_str(), nullptr);

        // preset-knoppen rechtsboven
        float bx = W - 14.0f;
        for (int i = kNumPresets - 1; i >= 0; --i)
        {
            const float bw = 62.0f, bh = 17.0f;
            bx -= bw + 5.0f;
            const bool on = (i == fProg);
            beginPath(); roundedRect(bx, 12.0f, bw, bh, 4.0f);
            if (on) { fillColor(29, 122, 72); fill(); }
            strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
            fontSize(10.0f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(on ? Color(232, 236, 234) : Color(143, 160, 152));
            text(bx + bw * 0.5f, 12.0f + bh * 0.5f, kPresetNames[i], nullptr);
        }

        // scope (onder de knoppenbalk)
        const float sx = 18.0f, sy = 90.0f, sw = W - 36.0f, sh = 42.0f;
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

        // AAN/UIT pil (naast thema-knop)
        {
            const float ax = W - 96.0f, ay = 34.0f;
            beginPath(); roundedRect(ax, ay, 50.0f, 17.0f, 4.0f);
            fillColor(fOn ? g2bsk(fLight,5) : g2bsk(fLight,4)); fill();
            strokeColor(g2bsk(fLight,3)); strokeWidth(1.0f); stroke();
            fontSize(8.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(fOn ? Color(232,236,234) : g2bsk(fLight,3));
            text(ax + 25.0f, ay + 8.5f, fOn ? "AAN" : "UIT", nullptr);
        }

        // 9 knoppen
        const int n = 9;
        const float step = (W - 40.0f) / n;
        for (int i = 0; i < n; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 200.0f;
            const float R  = 24.0f;
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

            fontSize(8.5f); fillColor(143, 160, 152);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            text(cx, cy + R + 8.0f, fLabel[i], nullptr);
            fontSize(11.0f); fillColor(232, 236, 234);
            char buf[24];
            if (i == 0)
                std::snprintf(buf, sizeof(buf), "%+.0f st", fValue[i]);
            else if (i == 1)
                std::snprintf(buf, sizeof(buf), "%.0f Hz", fValue[i]);
            else if (i == 6)
                std::snprintf(buf, sizeof(buf), "%.0f/26", fValue[i]);
            else if (i == 7)
                std::snprintf(buf, sizeof(buf), "%.0f%%", fValue[i]);
            else if (i == 8)
                std::snprintf(buf, sizeof(buf), "%s", (int)(fValue[i]+0.5f)==0?"DONKER":((int)(fValue[i]+0.5f)==2?"LICHT":"NEUTR"));
            else
                std::snprintf(buf, sizeof(buf), "%.0f %%", fValue[i]);
            text(cx, cy + R + 23.0f, buf, nullptr);
        }

        // XY-pad: X = KRACHT (0-100), Y = DIMENTIES (0-26)
        fXX = 18.0f; fXY_ = 258.0f; fXW = W - 36.0f; fXH = 46.0f;
        {
            const float px = fXX, py = fXY_, pw = fXW, ph = fXH;
            beginPath(); roundedRect(px, py, pw, ph, 6.0f);
            fillColor(g2bsk(fLight,4)); fill();
            strokeColor(g2bsk(fLight,3)); strokeWidth(1.0f); stroke();
            const float nx = fValue[7] / 100.0f, ny = fValue[6] / 26.0f;
            const float dx = px + nx * pw, dy = py + (1.0f - ny) * ph;
            beginPath();
            moveTo(dx, py); lineTo(dx, py + ph);
            moveTo(px, dy); lineTo(px + pw, dy);
            strokeColor(g2bsk(fLight,3)); strokeWidth(0.5f); stroke();
            beginPath(); circle(dx, dy, 5.0f);
            fillColor(g2bsk(fLight,5)); fill();
            fontSize(9.0f); fillColor(g2bsk(fLight,3));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(px + 6.0f, py + 4.0f, "DIMENTIES ", nullptr);
            textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
            text(px + pw - 6.0f, py + ph - 4.0f, "KRACHT →", nullptr);
        }

        // LANDR-rij: AUTO-FIT / EXPORT / SNAP / CHOP pills
        {
            const char* pl[4]; char pb[4][24];
            std::snprintf(pb[0],24,"AUTO-FIT"); std::snprintf(pb[1],24,"EXPORT WAV");
            std::snprintf(pb[2],24,"SNAP:%s",fSnapOn>0.5f?"AAN":"UIT");
            std::snprintf(pb[3],24,"CHOP:%d",fChopN);
            for(int i=0;i<4;i++)pl[i]=pb[i];
            const float pw=86.f,ph=20.f,py=H-46.f;
            for(int i=0;i<4;i++){
                const float px=18.f+i*(pw+8.f);
                const bool on=(i==2&&fSnapOn>0.5f)||(i==3&&fChopN>0);
                beginPath(); roundedRect(px,py,pw,ph,5.f);
                if(on){fillColor(29,122,72);fill();} else {fillColor(30,34,33);fill();}
                strokeColor(42,47,45); strokeWidth(1.f); stroke();
                fontSize(10.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
                fillColor(on?Color(232,236,234):Color(143,160,152));
                text(px+pw*0.5f,py+ph*0.5f,pl[i],nullptr);
            }
        }
        fontSize(10.5f); fillColor(90, 100, 94);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        text(W * 0.5f, H - 8.0f,
             "LAAD SAMPLE = jouw one-shot/loop erin — speel met MIDI/piano-roll", nullptr);

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
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        const float W = getWidth();
        if (ev.press)
        {
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 40.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 48.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 65.0f)
            { spv(7, 1.0f); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-40.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-14.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 34.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fLight = (fLight+1)%8; repaint(); return true; }

            // LAAD SAMPLE knop
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 18.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 168.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 56.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 80.0f)
            {
                openFileBrowser();
                return true;
            }
            // LANDR-pills
            {
                const float pw=86.f,ph=20.f,py=getHeight()-46.f;
                for(int i=0;i<4;i++){
                    const float px=18.f+i*(pw+8.f);
                    if((float)(ev.pos.getX()/getScaleFactor())>=px&&(float)(ev.pos.getX()/getScaleFactor())<=px+pw&&(float)(ev.pos.getY()/getScaleFactor())>=py&&(float)(ev.pos.getY()/getScaleFactor())<=py+ph){
                        if(i==0)spv(12,1.f);
                        else if(i==1)spv(15,1.f);
                        else if(i==2){fSnapOn=fSnapOn>0.5f?0.f:1.f;spv(14,fSnapOn);}
                        else {fChopN=(fChopN+4)%20;if(fChopN>16)fChopN=0;spv(13,(float)fChopN);}
                        repaint(); return true;
                    }
                }
            }
            // preset-klik — zelfde geometrie als tekening (bx vanaf rechts, 62x17 op y=12)
            {
                float hbx = W - 14.0f;
                for (int i = kNumPresets - 1; i >= 0; --i)
                {
                    const float bw = 62.0f, bh = 17.0f;
                    hbx -= bw + 5.0f;
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= hbx && (float)(ev.pos.getX()/getScaleFactor()) <= hbx + bw &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= 12.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 12.0f + bh)
                    {
                        spv(8, (float)i);
                        fProg = i;
                        repaint();
                        return true;
                    }
                }
            }
            // AAN/UIT pil
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-96.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-46.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 34.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fOn = 1 - fOn; spv(16, (float)fOn); repaint(); return true; }
            // XY-pad
            if ((float)(ev.pos.getX()/getScaleFactor()) >= fXX && (float)(ev.pos.getX()/getScaleFactor()) <= fXX + fXW &&
                (float)(ev.pos.getY()/getScaleFactor()) >= fXY_ && (float)(ev.pos.getY()/getScaleFactor()) <= fXY_ + fXH)
            { fXYdrag = true; xySet((float)(ev.pos.getX()/getScaleFactor()), (float)(ev.pos.getY()/getScaleFactor())); return true; }
            // knoppen
            const float step = (W - 40.0f) / 9;
            for (int i = 0; i < 9; ++i)
            {
                const float cx = 20.0f + step * (i + 0.5f);
                const float cy = 200.0f;
                const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
                const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
                if (dx*dx + dy*dy <= 24.0f*24.0f + 70.0f)
                {
                    fDrag = i; fLastY = (float)(ev.pos.getY()/getScaleFactor());
                    editParameter(paramOf(i), true);
                    return true;
                }
            }
        }
        else if (fXYdrag)
        {
            fXYdrag = false;
            return true;
        }
        else if (fDrag >= 0)
        {
            editParameter(paramOf(fDrag), false);
            fDrag = -1;
        }
        return true;
    }

    // knop-slots: 0-6 = params 0-6 (slot6=DIMENTIES→param6), 7 KRACHT(17), 8 SFEER(18)
    static int paramOf(int slot) { return slot < 7 ? slot : slot + 10; }
    void xySet(double mx, double my)
    {
        float nx = (float)((mx - fXX) / fXW), ny = (float)(1.0 - (my - fXY_) / fXH);
        if (nx < 0) nx = 0; if (nx > 1) nx = 1;
        if (ny < 0) ny = 0; if (ny > 1) ny = 1;
        const float kr = nx * 100.0f, dm = ny * 26.0f;
        fValue[7] = kr; fValue[6] = dm;
        editParameter(17, true); spv(17, kr); editParameter(17, false);
        editParameter(6, true);  spv(6, dm);  editParameter(6, false);
        repaint();
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fXYdrag) { xySet((float)(ev.pos.getX()/getScaleFactor()), (float)(ev.pos.getY()/getScaleFactor())); return true; }
        if (fDrag < 0) return false;
        const float dy = fLastY - (float)(ev.pos.getY()/getScaleFactor());
        fLastY = (float)(ev.pos.getY()/getScaleFactor());
        const float range = fMax[fDrag] - fMin[fDrag];
        float v = fValue[fDrag] + dy * range / 200.0f;
        if (v < fMin[fDrag]) v = fMin[fDrag];
        if (v > fMax[fDrag]) v = fMax[fDrag];
        fValue[fDrag] = v;
        spv(paramOf(fDrag), v);
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float W = getWidth();
        const float step = (W - 40.0f) / 9;
        for (int i = 0; i < 9; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 200.0f;
            const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
            const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
            if (dx*dx + dy*dy <= 24.0f*24.0f + 70.0f)
            {
                const float range = fMax[i] - fMin[i];
                float v = fValue[i] + ev.delta.getY() * range / 100.0f;
                if (v < fMin[i]) v = fMin[i];
                if (v > fMax[i]) v = fMax[i];
                fValue[i] = v;
                const int pi = paramOf(i);
                editParameter(pi, true);
                spv(pi, v);
                editParameter(pi, false);
                repaint();
                return true;
            }
        }
        return true;
    }

private:
    static const int kScopeSize = 256;
    float fValue[9], fMin[9], fMax[9];
    const char* fLabel[9];
    float fScope[kScopeSize];
    int fScopePos;
    int fDrag;
    float fLastY;
    int fProg;
    float fSnapOn; int fChopN;
    int fLight = 0;
    int fOn = 1; bool fXYdrag = false;
    float fXX = 18.0f, fXY_ = 258.0f, fXW = 524.0f, fXH = 46.0f;
    std::string fName;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleXUI)
};

UI* createUI() { return new SampleXUI(); }

END_NAMESPACE_DISTRHO
