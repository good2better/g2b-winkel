/*
 * G2B OchoOcho — UI (NanoVG): donkergroen paneel, 7 draaiknoppen
 * + live sub-golfvorm bovenin. Sleep = draaien, scroll = fijn.
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

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
static inline Color g2bsk(int sk,int r){if(sk<0)sk=0;if(sk>7)sk=7;const float*c=G2BSK[sk&7][r];return Color((int)c[0],(int)c[1],(int)c[2]);}

START_NAMESPACE_DISTRHO

static const char* kPresetNames[16] = {"DONDER","KICKBAS","TRAP","BOOM","AFRO","DEMBOW","DRILL","SUB808","SKRATJI","BANGI","KOTI","AGOGO","SEKI","SURDO","TIMBAL","KLAP"};
static const int kNumPresets = 16;



class OchoUI : public UI
{
public:
    OchoUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources();
        std::memset(fValue, 0, sizeof(fValue));
        std::memset(fScope, 0, sizeof(fScope));
        static const float mins[12]  = {0,0,0,0,0,-12,0,0,0,0,0,0};
        static const float maxs[12]  = {100,100,100,100,100,12,100,11,1,100,26,2};
        static const char* lbl[12]   = {"DECAY","GLIDE","SUB","CLICK","DROP","TUNE","LEVEL","KEY","SCALE","KRACHT","DIMENTIES","SFEER"};
        std::memcpy(fMin, mins, sizeof(mins));
        std::memcpy(fMax, maxs, sizeof(maxs));
        std::memcpy(fLabel, lbl, sizeof(lbl));
        fScopePos = 0; fDrag = -1; fLastY = 0.0f;
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < 9) fValue[index] = value;
        else if (index == 10) fProg = (int)(value + 0.5f);
        else if (index == 11) fOn = (int)(value + 0.5f);
        else if (index >= 12 && index <= 14) fValue[index - 3] = value; // 12=KRACHT→9,13=DIMENTIES→10,14=SFEER→11
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "usrsmp") == 0)
        {
            fUsrNames.clear();
            const char* p = value;
            while (*p != '\0')
            {
                const char* bar = std::strchr(p, '|');
                fUsrNames.push_back(bar ? std::string(p, bar - p) : std::string(p));
                if (!bar) break;
                p = bar + 1;
            }
            repaint();
            return;
        }
        if (std::strcmp(key, "usrset") == 0)
        {
            fUsrSets.clear();
            const char* p = value;
            while (*p != '\0')
            {
                const char* bar = std::strchr(p, '|');
                fUsrSets.push_back(bar ? std::string(p, bar - p) : std::string(p));
                if (!bar) break;
                p = bar + 1;
            }
            repaint();
            return;
        }
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

        fontSize(20.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(47, 191, 113);
        text(18.0f, 24.0f, "G2B OCHO-OCHO", nullptr);
        fontSize(10.0f);
        fillColor(143, 160, 152);
        text(178.0f, 24.0f, "v6.3", nullptr);
        fontSize(11.5f);
        fillColor(143, 160, 152);
        text(18.0f, 43.0f, "jouw Surinaamse one-shot machine — speel hem als een instrument", nullptr);

        // scope (rechtsboven)
        const float sx = 300.0f, sy = 12.0f, sw = W - 318.0f, sh = 46.0f;
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
            if (y0 < sy + 1) y0 = sy + 1;
            if (y0 > sy + sh - 1) y0 = sy + sh - 1;
            if (y1 < sy + 1) y1 = sy + 1;
            if (y1 > sy + sh - 1) y1 = sy + sh - 1;
            if (i == 0) moveTo(x0, y0); else lineTo(x0, y0);
            lineTo(x1, y1);
        }
        strokeColor(47, 191, 113); strokeWidth(1.5f); stroke();

        // AAN/UIT pil (vrij vlak tussen titel en scope)
        {
            const float ax = 250.0f, ay = 16.0f;
            beginPath(); roundedRect(ax, ay, 44.0f, 17.0f, 4.0f);
            fillColor(fOn ? g2bsk(fLight,5) : g2bsk(fLight,4)); fill();
            strokeColor(g2bsk(fLight,3)); strokeWidth(1.0f); stroke();
            fontSize(8.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(fOn ? Color(232,236,234) : g2bsk(fLight,3));
            text(ax + 22.0f, ay + 8.5f, fOn ? "AAN" : "UIT", nullptr);
        }

        // knoppen in een rij
        const int n = 12;
        const float step = (W - 40.0f) / n;
        for (int i = 0; i < n; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 160.0f;
            const float R  = 17.0f;
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

            // klik-flits: groene ring om de knop die je net klikte
            if (fFlashTick > 0 && i == fFlash)
            {
                beginPath(); circle(cx, cy, R + 3.0f);
                strokeColor(47, 191, 113); strokeWidth(2.0f); stroke();
            }
            if (fFlashTick > 0 && i == 11) --fFlashTick;
            fontSize(11.0f); fillColor(232, 236, 234);
            char buf[24];
            if (fMin[i] < 0.0f)
                std::snprintf(buf, sizeof(buf), "%+.0f st", fValue[i]);
            else if (i == 7)
            { static const char* kn[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
              std::snprintf(buf, sizeof(buf), "%s", kn[(int)fValue[i]%12]); }
            else if (i == 8)
                std::snprintf(buf, sizeof(buf), "%s", fValue[i]>0.5f?"major":"minor");
            else if (i == 9)
                std::snprintf(buf, sizeof(buf), "%.0f%%", fValue[i]);
            else if (i == 10)
                std::snprintf(buf, sizeof(buf), "%.0f/26", fValue[i]);
            else if (i == 11)
                std::snprintf(buf, sizeof(buf), "%s", (int)(fValue[i]+0.5f)==0?"DONKER":((int)(fValue[i]+0.5f)==2?"LICHT":"NEUTR"));
            else
                std::snprintf(buf, sizeof(buf), "%.0f %%", fValue[i]);
            text(cx, cy + R + 23.0f, buf, nullptr);
        }

        // XY-pad: X = KRACHT (0-100), Y = DIMENTIES (0-26)
        fXX = 18.0f; fXY_ = 225.0f; fXW = W - 36.0f; fXH = 105.0f;
        {
            const float px = fXX, py = fXY_, pw = fXW, ph = fXH;
            beginPath(); roundedRect(px, py, pw, ph, 6.0f);
            fillColor(g2bsk(fLight,4)); fill();
            strokeColor(g2bsk(fLight,3)); strokeWidth(1.0f); stroke();
            const float nx = fValue[9] / 100.0f, ny = fValue[10] / 26.0f;
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

        fontSize(10.5f);
        fillColor(90, 100, 94);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        
        // smaak-knoppen: 2 rijen van 6 rechtsboven
        {
            const float bw = 56.0f, bh = 17.0f, gap = 4.0f;
            for (int i = 0; i < kNumPresets; ++i)
            {
                const int row = i / 8, col = i % 8;
                const float bx = W - 14.0f - (8 - col) * (bw + gap);
                const float by = 62.0f + row * (bh + 4.0f);
                const bool on = (i == fProg);
                beginPath(); roundedRect(bx, by, bw, bh, 4.0f);
                if (on) { fillColor(29, 122, 72); fill(); }
                strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
                fontSize(9.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(on ? Color(232, 236, 234) : Color(143, 160, 152));
                text(bx + bw * 0.5f, by + bh * 0.5f, kPresetNames[i], nullptr);
            }
        }

        // eigen samples: knoppen voor de shots van je HUIDIGE set (max 8)
        {
            const float bw = 56.0f, bh = 17.0f, gap = 4.0f;
            const int cur0 = (fProg >= 0 && fProg < kNumPresets + (int)fUsrNames.size()) ? fProg : 0;
            const char* cs = setOf(cur0);
            fBtnCount = 0;
            for (int u = 0; u < (int)fUsrNames.size() && fBtnCount < 8; ++u)
            {
                const char* s = (u < (int)fUsrSets.size()) ? fUsrSets[u].c_str() : "MIJN";
                if (std::strcmp(s, cs)) continue;
                const int slot = kNumPresets + u;
                fBtnSlots[fBtnCount] = slot;
                const int i = fBtnCount;
                const int col = i % 8;
                const float bx = W - 14.0f - (8 - col) * (bw + gap);
                const float by = 128.0f;
                const bool on = (slot == fProg);
                beginPath(); roundedRect(bx, by, bw, bh, 4.0f);
                if (on) { fillColor(29, 122, 72); fill(); }
                strokeColor(42, 47, 45); strokeWidth(1.0f); stroke();
                fontSize(8.0f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(on ? Color(232, 236, 234) : Color(143, 160, 152));
                text(bx + bw * 0.5f, by + bh * 0.5f, fUsrNames[u].c_str(), nullptr);
                ++fBtnCount;
            }
        }

        // huidige smaak + ‹ › pijltjes
        {
            const float ay = 106.0f;
            beginPath(); roundedRect(14.0f, ay, 26.0f, 18.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(13.0f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(27.0f, ay+9.0f, "<", nullptr);
            beginPath(); roundedRect(212.0f, ay, 26.0f, 18.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            text(225.0f, ay+9.0f, ">", nullptr);
            fontSize(13.0f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
            fillColor(232,236,234);
            const int total = kNumPresets + (int)fUsrNames.size();
            const int cur = (fProg >= 0 && fProg < total) ? fProg : 0;
            // grote naam = de SMAAK (blijft staan); erboven de shot-naam
            const char* nm = cur < kNumPresets ? kPresetNames[cur] : setOf(cur);
            text(48.0f, ay+9.0f, nm, nullptr);
            {
                fontSize(8.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
                fillColor(29, 122, 72);
                if (cur < kNumPresets)
                {
                    // toon ook hoeveel eigen shots hij vond — direct zichtbaar
                    // of de mappen geladen zijn (0 = mappen niet gevonden)
                    char ob[32]; std::snprintf(ob, sizeof(ob), "SET: OCHO · %d eigen", (int)fUsrNames.size());
                    text(48.0f, ay - 3.0f, ob, nullptr);
                }
                else
                    text(48.0f, ay - 3.0f, fUsrNames[cur - kNumPresets].c_str(), nullptr);
            }
            // teller binnen de set + aantal shots in set
            {
                int lo, hi; setRange(cur, lo, hi);
                char cb[24]; std::snprintf(cb, sizeof(cb), "%d/%d in set", cur - lo + 1, hi - lo);
                fontSize(8.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
                fillColor(g2bsk(fLight,3));
                text(158.0f, ay - 3.0f, cb, nullptr);
            }
            // SET> knop: spring naar de volgende geluiden-set
            beginPath(); roundedRect(158.0f, ay, 46.0f, 18.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(29, 122, 72); strokeWidth(1.0f); stroke();
            fontSize(9.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(181.0f, ay+9.0f, "SET>", nullptr);
            // "808 ✓": maak de shot een echte 808 (sub + val + lengte + aanslag)
            beginPath(); roundedRect(246.0f, ay, 46.0f, 18.0f, 4.0f);
            fillColor(29, 122, 72); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(9.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(232,236,234);
            text(269.0f, ay+9.0f, "808!", nullptr);
            if (fFxTick > 0)
            {
                beginPath(); roundedRect(244.0f, ay - 2.0f, 50.0f, 22.0f, 5.0f);
                strokeColor(232, 190, 60); strokeWidth(2.0f); stroke();
                --fFxTick;
            }
        }

        text(W * 0.5f, H - 8.0f,
             "MIDI-keyboard of piano-roll nodig — sleep = draaien · < > = smaak", nullptr);
        {
            // dobbelsteen: vierkantje met 5 stippen (geen emoji-font nodig)
            float bx2 = 14.0f;
            beginPath(); roundedRect(bx2, 46.0f, 30.0f, 20.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(g2bsk(fLight,5)); strokeWidth(1.2f); stroke();
            fillColor(g2bsk(fLight,5));
            {
                const float dx2[5] = {bx2+7.5f, bx2+22.5f, bx2+15.0f, bx2+7.5f, bx2+22.5f};
                const float dy2[5] = {52.0f,    52.0f,     56.0f,     60.0f,    60.0f};
                for (int i = 0; i < 5; ++i)
                { beginPath(); circle(dx2[i], dy2[i], 1.7f); fill(); }
            }
            // dobbel-feedback: flits + "GEROLD" zodat je ziet dat hij rolde
            if (fDiceTick > 0)
            {
                beginPath(); roundedRect(bx2 - 2.0f, 44.0f, 34.0f, 24.0f, 5.0f);
                strokeColor(47, 191, 113); strokeWidth(2.0f); stroke();
                fontSize(8.5f); textAlign(ALIGN_LEFT|ALIGN_MIDDLE);
                fillColor(47, 191, 113);
                text(bx2 + 36.0f, 56.0f, "GEROLD", nullptr);
                --fDiceTick;
            }
            const float tx = W - 40.0f;
            beginPath(); roundedRect(tx, 34.0f, 26.0f, 17.0f, 4.0f);
            fillColor(g2bsk(fLight,1)); fill();
            strokeColor(42,47,45); strokeWidth(1.0f); stroke();
            fontSize(6.5f); textAlign(ALIGN_CENTER|ALIGN_MIDDLE);
            fillColor(g2bsk(fLight,2));
            text(tx+13.0f, 42.5f, "SKIN", nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1) return false;
        const float W = getWidth();
        const float step = (W - 40.0f) / 12;
        if (ev.press)
        {
            // AAN/UIT pil
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 250.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 294.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 16.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 33.0f)
            { fOn = 1 - fOn; spv(11, (float)fOn); repaint(); return true; }
            // dobbelsteen (links onder titel)
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 44.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 46.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 66.0f)
            { spv(9, 1.0f); fDiceTick = 18; repaint(); return true; }
            // 🌙 skin (rechtsboven)
            if ((float)(ev.pos.getX()/getScaleFactor()) >= W - 40.0f && (float)(ev.pos.getX()/getScaleFactor()) <= W - 14.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 34.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 51.0f)
            { fLight = (fLight + 1) % 8; repaint(); return true; }
            // ‹ › pijltjes: vorige / volgende shot BINNEN je set
            {
                const int total = kNumPresets + (int)fUsrNames.size();
                if (total > 0)
                {
                    int cur = (fProg >= 0 && fProg < total) ? fProg : 0;
                    int lo, hi; setRange(cur, lo, hi);
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= 14.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 40.0f &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= 106.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 124.0f)
                    {
                        cur = cur - 1 < lo ? hi - 1 : cur - 1;
                        spv(10, (float)cur); fProg = cur; repaint(); return true;
                    }
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= 212.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 238.0f &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= 106.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 124.0f)
                    {
                        cur = cur + 1 >= hi ? lo : cur + 1;
                        spv(10, (float)cur); fProg = cur; repaint(); return true;
                    }
                    // SET>: spring naar de eerste shot van de volgende set
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= 158.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 204.0f &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= 106.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 124.0f)
                    {
                        cur = hi >= total ? 0 : hi;
                        spv(10, (float)cur); fProg = cur; repaint(); return true;
                    }
                }
            }
            // "808 ✓": perfecte 808-instellingen voor de gekozen shot
            if ((float)(ev.pos.getX()/getScaleFactor()) >= 246.0f && (float)(ev.pos.getX()/getScaleFactor()) <= 292.0f &&
                (float)(ev.pos.getY()/getScaleFactor()) >= 106.0f && (float)(ev.pos.getY()/getScaleFactor()) <= 124.0f)
            {
                // kick-punchy 808: korte staart, harde aanslag, snelle val
                static const float v808[6] = {50.0f, 55.0f, 100.0f, 55.0f, 75.0f, 100.0f};
                static const int   p808[6] = {0, 1, 2, 3, 4, 6};
                for (int i = 0; i < 6; ++i)
                {
                    const int p = p808[i];
                    editParameter(p, true); spv(p, v808[i]); editParameter(p, false);
                }
                fFxTick = 18;
                repaint(); return true;
            }
            // XY-pad
            if ((float)(ev.pos.getX()/getScaleFactor()) >= fXX && (float)(ev.pos.getX()/getScaleFactor()) <= fXX + fXW &&
                (float)(ev.pos.getY()/getScaleFactor()) >= fXY_ && (float)(ev.pos.getY()/getScaleFactor()) <= fXY_ + fXH)
            { fXYdrag = true; xySet((float)(ev.pos.getX()/getScaleFactor()), (float)(ev.pos.getY()/getScaleFactor())); return true; }
            // smaak-klik? (2 rijen van 8) — springt meteen in die SET
            // als er een gelijknamige gebruikers-set bestaat (TRAP → TRAP-set)
            {
                const float bw = 56.0f, bh = 17.0f, gap = 4.0f;
                for (int i = 0; i < kNumPresets; ++i)
                {
                    const int row = i / 8, col = i % 8;
                    const float bx = W - 14.0f - (8 - col) * (bw + gap);
                    const float by = 62.0f + row * (bh + 4.0f);
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= bx && (float)(ev.pos.getX()/getScaleFactor()) <= bx + bw &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= by && (float)(ev.pos.getY()/getScaleFactor()) <= by + bh)
                    {
                        int slot = i;
                        for (int u = 0; u < (int)fUsrSets.size(); ++u)
                            if (eqNoCase(fUsrSets[u].c_str(), kPresetNames[i]))
                            { slot = kNumPresets + u; break; }
                        spv(10, (float)slot);
                        fProg = slot;
                        repaint();
                        return true;
                    }
                }
            }

            // knoppen voor de shots van je huidige set (fBtnSlots uit onNanoDisplay)
            {
                const float bw = 56.0f, bh = 17.0f, gap = 4.0f;
                for (int i = 0; i < fBtnCount && i < 8; ++i)
                {
                    const int col = i % 8;
                    const float bx = W - 14.0f - (8 - col) * (bw + gap);
                    const float by = 128.0f;
                    if ((float)(ev.pos.getX()/getScaleFactor()) >= bx && (float)(ev.pos.getX()/getScaleFactor()) <= bx + bw &&
                        (float)(ev.pos.getY()/getScaleFactor()) >= by && (float)(ev.pos.getY()/getScaleFactor()) <= by + bh)
                    {
                        const int slot = fBtnSlots[i];
                        spv(10, (float)slot);
                        fProg = slot;
                        repaint();
                        return true;
                    }
                }
            }
            for (int i = 0; i < 12; ++i)
            {
                const float cx = 20.0f + step * (i + 0.5f);
                const float cy = 160.0f;
                const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
                const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
                if (dx*dx + dy*dy <= 17.0f*17.0f + 60.0f)
                {
                    // discrete knoppen (KEY 0-11, SCALE 0-1, SFEER 0-2):
                    // klik = volgende optie — slepen over zo'n klein bereik
                    // komt nooit over de hele stap (zijn bug: "key werkt niet")
                    if (i == 7 || i == 8 || i == 11)
                    {
                        float v = fValue[i] + 1.0f;
                        if (v > fMax[i] + 0.5f) v = fMin[i];
                        fValue[i] = v;
                        const int pi = paramOf(i);
                        editParameter(pi, true); spv(pi, v); editParameter(pi, false);
                        fFlash = i; fFlashTick = 18;
                        repaint();
                        return true;
                    }
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

    // knop-slots: 0-8 = params 0-8, 9 KRACHT(12), 10 DIMENTIES(13), 11 SFEER(14)
    static int paramOf(int slot) { return slot < 9 ? slot : slot + 3; }
    void xySet(double mx, double my)
    {
        float nx = (float)((mx - fXX) / fXW), ny = (float)(1.0 - (my - fXY_) / fXH);
        if (nx < 0) nx = 0; if (nx > 1) nx = 1;
        if (ny < 0) ny = 0; if (ny > 1) ny = 1;
        const float kr = nx * 100.0f, dm = ny * 26.0f;
        fValue[9] = kr; fValue[10] = dm;
        editParameter(12, true); spv(12, kr); editParameter(12, false);
        editParameter(13, true); spv(13, dm); editParameter(13, false);
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
        const float step = (W - 40.0f) / 12;
        for (int i = 0; i < 12; ++i)
        {
            const float cx = 20.0f + step * (i + 0.5f);
            const float cy = 160.0f;
            const float dx = (float)(ev.pos.getX()/getScaleFactor()) - cx;
            const float dy = (float)(ev.pos.getY()/getScaleFactor()) - cy;
            if (dx*dx + dy*dy <= 17.0f*17.0f + 60.0f)
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
    float fValue[12], fMin[12], fMax[12];
    const char* fLabel[12];
    float fScope[kScopeSize];
    int fScopePos;
    int fDrag;
    float fLastY;
    int fProg = -1;
    int fDiceTick = 0;
    int fFxTick = 0;
    std::vector<std::string> fUsrNames;
    std::vector<std::string> fUsrSets;
    // set-helpers: gebakken 16 = set "OCHO"; eigen shots dragen hun mapnaam
    const char* setOf(int slot) const
    {
        if (slot < kNumPresets) return "OCHO";
        const int u = slot - kNumPresets;
        return (u >= 0 && u < (int)fUsrSets.size()) ? fUsrSets[u].c_str() : "MIJN";
    }
    void setRange(int slot, int& lo, int& hi) const
    {
        const int total = kNumPresets + (int)fUsrNames.size();
        if (slot < kNumPresets) { lo = 0; hi = kNumPresets; return; }
        const char* s = setOf(slot);
        lo = slot;
        while (lo > kNumPresets && std::strcmp(setOf(lo - 1), s) == 0) --lo;
        hi = slot + 1;
        while (hi < total && std::strcmp(setOf(hi), s) == 0) ++hi;
    }
    int fLight = 0;
    int fOn = 1; bool fXYdrag = false;
    int fBtnSlots[8] = {0,0,0,0,0,0,0,0};
    int fBtnCount = 0;
    int fFlash = -1, fFlashTick = 0;
    static bool eqNoCase(const char* a, const char* b)
    {
        while (*a && *b)
        {
            char ca = *a, cb = *b;
            if (ca >= 'a' && ca <= 'z') ca -= 32;
            if (cb >= 'a' && cb <= 'z') cb -= 32;
            if (ca != cb) return false;
            ++a; ++b;
        }
        return *a == *b;
    }
    float fXX = 18.0f, fXY_ = 225.0f, fXW = 524.0f, fXH = 105.0f;

    void spv(uint32_t i,float v){setParameterValue(i,v);parameterChanged(i,v);}
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OchoUI)
};

UI* createUI() { return new OchoUI(); }

END_NAMESPACE_DISTRHO
