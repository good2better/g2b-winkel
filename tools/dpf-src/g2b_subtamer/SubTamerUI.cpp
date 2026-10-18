/*
 * G2B SubTamer — UI (NanoVG): donkergroen paneel, 5 draaiknoppen.
 * Zelfde G2B-huisstijl als Glans. Verticaal slepen = waarde; scroll = fijn.
 */

#include "DistrhoUI.hpp"

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



class SubTamerUI : public UI
{
public:
    SubTamerUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        std::memset(fValue, 0, sizeof(fValue));
        std::memset(fMin,   0, sizeof(fMin));
        std::memset(fMax,   0, sizeof(fMax));
        fDrag = -1;
        fLastY = 0.0f;

        // gespiegelde ranges (dezelfde als in de plugin)
        fMin[0]=0.0f;    fMax[0]=100.0f;
        fMin[1]=60.0f;   fMax[1]=160.0f;
        fMin[2]=-12.0f;  fMax[2]=6.0f;
        fMin[3]=0.0f;    fMax[3]=100.0f;
        fMin[4]=-6.0f;   fMax[4]=6.0f;
        fLabel[0]="TEM";      fUnit[0]="%";
        fLabel[1]="VANAF";    fUnit[1]="Hz";
        fLabel[2]="STERKTE";  fUnit[2]="dB";
        fLabel[3]="MONO";     fUnit[3]="%";
        fLabel[4]="OUTPUT";   fUnit[4]="dB";
        fMin[5]=0.0f;    fMax[5]=26.0f;
        fLabel[5]="GALAXY";  fUnit[5]="";
        fMin[6]=0.0f;    fMax[6]=100.0f; fValue[6]=100.0f;
        fLabel[6]="KRACHT";  fUnit[6]="%";
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < 6) fValue[index] = value;
        else if (index == 8) fOn = value;
        else if (index == 9) fValue[6] = value;
        repaint();
    }

    void onNanoDisplay() override
    {
        const float W = getWidth(), H = getHeight();

        beginPath();
        rect(0, 0, W, H);
        fillColor(13, 15, 14);
        fill();

        fontSize(20.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(47, 191, 113);
        text(18.0f, 26.0f, "G2B SUBTAMER", nullptr);
        fontSize(12.0f);
        fillColor(143, 160, 152);
        text(18.0f, 46.0f, "sub always on the right spot — G2B", nullptr);

        const int n = 7;
        const float step = (W - 40.0f) / n;
        for (int i = 0; i < n; ++i)
        {
            const float cx = 40.0f + step * (i + 0.5f);
            const float cy = 120.0f;
            const float R  = 34.0f;
            const float norm = (fValue[i] - fMin[i]) / (fMax[i] - fMin[i]);
            const float ang = -2.4f + norm * 4.8f;

            beginPath();
            circle(cx, cy, R);
            fillColor(30, 34, 33);
            fill();
            strokeColor(42, 47, 45);
            strokeWidth(2.0f);
            stroke();

            beginPath();
            arc(cx, cy, R - 4.0f, -2.4f - 1.5708f, ang - 1.5708f, CW);
            strokeColor(47, 191, 113);
            strokeWidth(3.0f);
            stroke();

            beginPath();
            moveTo(cx, cy);
            lineTo(cx + std::sin(ang) * (R - 9.0f),
                   cy - std::cos(ang) * (R - 9.0f));
            strokeColor(232, 236, 234);
            strokeWidth(3.0f);
            stroke();

            fontSize(11.0f);
            fillColor(143, 160, 152);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            text(cx, cy + R + 8.0f, fLabel[i], nullptr);

            fontSize(12.5f);
            fillColor(232, 236, 234);
            char buf[32];
            if(i==5){int gp=(int)fValue[i];snprintf(buf,sizeof(buf),"%s %d",gp<9?"DARK":(gp<18?"LIGHT":"GALAXY"),gp%9+1);}else
            if (fMax[i] <= 2.5f)
                snprintf(buf, sizeof(buf), "%.2f %s", fValue[i], fUnit[i]);
            else
                snprintf(buf, sizeof(buf), "%.0f %s", fValue[i], fUnit[i]);
            text(cx, cy + R + 24.0f, buf, nullptr);
        }

        fontSize(10.5f);
        fillColor(90, 100, 94);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        
        // preset-knoppen
        {
            float bx = W - 14.0f;
            for (int i = kNumPresets - 1; i >= 0; --i)
            {
                const float bw = 62.0f, bh = 17.0f;
                bx -= bw + 5.0f;
                const bool on = (i == fProg);
                beginPath(); roundedRect(bx, 62.0f, bw, bh, 4.0f);
                if (on) { fillColor(29, 122, 72); fill(); }
                strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
                fontSize(10.0f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(on ? Color(232, 236, 234) : Color(143, 160, 152));
                text(bx + bw * 0.5f, 62.0f + bh * 0.5f, kPresetNames[i], nullptr);
            }
        }

        text(W * 0.5f, H - 10.0f,
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
        const float step = (W - 40.0f) / 7;
        if (ev.press)
        {
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-96.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-44.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 33.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fOn = fOn>0.5f?0.f:1.f; spv(8, fOn); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 40.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 48.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 65.0f)
            { spv(6, 1.0f); repaint(); return true; }
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W-40.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W-14.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 34.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fLight = (fLight+1)%8; repaint(); return true; }

            // preset-klik?
            {
                {
                float hbx = W - 14.0f;
                for (int i = kNumPresets - 1; i >= 0; --i)
                {
                    const float bw = 62.0f, bh = 17.0f;
                    hbx -= bw + 5.0f;
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= hbx && (float)(ev.pos.getX()/getScaleFactor()) <= hbx + bw &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= 62.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 62.0f + bh)
                    {
                        spv(7, (float)i);
                        fProg = i;
                        repaint();
                        return true;
                    }
                }
            }
            }

            for (int i = 0; i < 7; ++i)
            {
                const float cx = 40.0f + step * (i + 0.5f);
                const float cy = 120.0f;
                const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
                const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
                if (dx*dx + dy*dy <= 34.0f * 34.0f + 64.0f)
                {
                    fDrag = i;
                    fLastY = (float)(ev.pos.getY()/getScaleFactor());
                    editParameter(i == 6 ? 9 : i, true);
                    return true;
                }
            }
        }
        else if (fDrag >= 0)
        {
            editParameter(fDrag == 6 ? 9 : fDrag, false);
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
        spv(fDrag == 6 ? 9 : fDrag, v);
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float W = getWidth();
        const float step = (W - 40.0f) / 7;
        for (int i = 0; i < 7; ++i)
        {
            const float cx = 40.0f + step * (i + 0.5f);
            const float cy = 120.0f;
            const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
            const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
            if (dx*dx + dy*dy <= 34.0f * 34.0f + 64.0f)
            {
                const float range = fMax[i] - fMin[i];
                float v = fValue[i] + ev.delta.getY() * range / 100.0f;
                if (v < fMin[i]) v = fMin[i];
                if (v > fMax[i]) v = fMax[i];
                fValue[i] = v;
                editParameter(i == 6 ? 9 : i, true);
                spv(i == 6 ? 9 : i, v);
                editParameter(i == 6 ? 9 : i, false);
                repaint();
                return true;
            }
        }
        return true;
    }

private:
    float fValue[7], fMin[7], fMax[7];
    float fOn = 1.f;
    const char* fLabel[6];
    const char* fUnit[6];
    int fDrag;
    float fLastY;
    int fProg = -1;
    int fLight = 0;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubTamerUI)
};

UI* createUI() { return new SubTamerUI(); }

END_NAMESPACE_DISTRHO
