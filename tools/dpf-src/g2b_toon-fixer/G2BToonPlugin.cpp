#include "DistrhoPlugin.hpp"
#include <cmath>
#include <cstring>
#include <vector>

START_NAMESPACE_DISTRHO

// G2B Toon-Fixer: real-time pitch correction (autotune-style)
// - autocorrelation pitch detect (~every 30ms)
// - snap detected pitch to nearest note in chosen key/scale
// - dual-head delay-line pitch shifter with Hann crossfade
// - KRACHT = correction speed (subtle -> robot)
// - DIMENTIES 27: natural (0-8) / modern (9-17) / hard-tune robot (18-26)

class G2BToonPlugin : public Plugin
{
public:
    enum Param { kOn=0, kKracht, kKey, kScale, kShift, kDimensie, kSkin, kPreset, kParamCount };

    G2BToonPlugin()
        : Plugin(kParamCount, 0, 0),
          v_(kParamCount, 0.f),
          writePos_(0), phase_(0.f), ratio_(1.f), targetRatio_(1.f),
          detBuf_(2048, 0.f), detPos_(0), detCnt_(0),
          pitchHz_(0.f), conf_(0.f),
          note_(60), snapHz_(0.f), valid_(false)
    {
        v_[kOn] = 1.f; v_[kKracht] = 60.f; v_[kKey] = 0.f; v_[kScale] = 0.f;
        v_[kShift] = 0.f; v_[kDimensie] = 9.f; v_[kSkin] = 0.f;
        const int N = kShiftBuf;
        bufL_.assign(N, 0.f);
        bufR_.assign(N, 0.f);
    }

protected:
    const char* getLabel()   const override { return "G2B Toon-Fixer"; }
    const char* getMaker()   const override { return "Good2Better"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t    getVersion() const override { return d_version(1,0,0); }
    int64_t     getUniqueId() const override { return d_cconst('G','2','T','f'); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        switch (index)
        {
        case kOn:
            p.name = "Aan"; p.symbol = "on"; p.hints = kParameterIsBoolean;
            p.ranges.def = 1.f; p.ranges.min = 0.f; p.ranges.max = 1.f; break;
        case kKracht:
            p.name = "Kracht"; p.symbol = "kracht"; p.unit = "%";
            p.ranges.def = 60.f; p.ranges.min = 0.f; p.ranges.max = 100.f; break;
        case kKey:
            p.name = "Toonsoort"; p.symbol = "key";
            p.ranges.def = 0.f; p.ranges.min = 0.f; p.ranges.max = 11.f; break;
        case kScale:
            p.name = "Ladder"; p.symbol = "scale";
            p.ranges.def = 0.f; p.ranges.min = 0.f; p.ranges.max = 3.f; break;
        case kShift:
            p.name = "Shift"; p.symbol = "shift"; p.unit = "st";
            p.ranges.def = 0.f; p.ranges.min = -12.f; p.ranges.max = 12.f; break;
        case kDimensie:
            p.name = "Dimensies"; p.symbol = "dimensie";
            p.ranges.def = 9.f; p.ranges.min = 0.f; p.ranges.max = 26.f; break;
        case kSkin:
            p.name = "Skin"; p.symbol = "skin";
            p.ranges.def = 0.f; p.ranges.min = 0.f; p.ranges.max = 7.f; break;
        case kPreset:
            p.name = "Preset"; p.symbol = "pre"; p.hints |= kParameterIsInteger;
            p.ranges.def = 0.f; p.ranges.min = 0.f; p.ranges.max = 15.f; break;
        default: break;
        }
    }

    float getParameterValue(uint32_t index) const override { return v_[index]; }

    void setParameterValue(uint32_t index, float value) override
    {
        if(index==kPreset && (int)value!=(int)v_[kPreset])applyPreset((int)value);
        v_[index] = value;
    }
    void applyPreset(int p){static const float PR[16][3]={{0,0,0},{9,1,0},{7,0,0},{2,1,0},{0,0,-12},{9,1,3},{5,0,-2},{10,1,0},{4,0,5},{7,1,-4},{0,2,0},{9,3,0},{3,1,7},{6,0,-6},{1,1,2},{11,0,-1}};const float*q=PR[p&15];v_[kKey]=q[0];v_[kScale]=q[1];v_[kShift]=q[2];}

    void activate() override
    {
        std::fill(bufL_.begin(), bufL_.end(), 0.f);
        std::fill(bufR_.begin(), bufR_.end(), 0.f);
        std::fill(detBuf_.begin(), detBuf_.end(), 0.f);
        writePos_ = 0; phase_ = 0.f; ratio_ = 1.f; targetRatio_ = 1.f;
        detPos_ = 0; detCnt_ = 0; pitchHz_ = 0.f; conf_ = 0.f;
        snapHz_ = 0.f; valid_ = false;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* inL  = inputs[0];
        const float* inR  = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];

        if (v_[kOn] < 0.5f)
        {
            if (inL != outL) std::memcpy(outL, inL, frames * sizeof(float));
            if (inR != outR) std::memcpy(outR, inR, frames * sizeof(float));
            return;
        }

        const float sr   = (float)getSampleRate();
        const float dimN = v_[kDimensie];
        // correction slew per dimensie-band
        float dimSlew = (dimN < 9.f) ? 0.5f : (dimN < 18.f ? 1.0f : 1.6f);
        const float kracht = v_[kKracht] / 100.f;
        // slew: kracht 0 -> very slow natural, 100 -> near-instant robot
        const float slew = (0.0008f + kracht * kracht * 0.04f) * dimSlew;
        const float shiftSt = v_[kShift];
        const float shiftR  = std::pow(2.f, shiftSt / 12.f);
        const int   N = kShiftBuf;
        const float W = kShiftWin;

        for (uint32_t i = 0; i < frames; ++i)
        {
            const float xL = inL[i], xR = inR[i];

            // detection buffer (mono)
            detBuf_[detPos_] = 0.5f * (xL + xR);
            detPos_ = (detPos_ + 1) & 2047;
            if (++detCnt_ >= 1024)
            {
                detCnt_ = 0;
                detectPitch(sr);
            }

            // manual shift always applies; correction on top when pitch found
            float want = shiftR;
            if (valid_ && pitchHz_ > 0.f && snapHz_ > 0.f)
                want *= snapHz_ / pitchHz_;
            // hard-tune dimensies quantize the target harder (snap stays, slew faster)
            targetRatio_ = want;
            ratio_ += (targetRatio_ - ratio_) * slew;
            if (ratio_ < 0.5f) ratio_ = 0.5f;
            if (ratio_ > 2.0f) ratio_ = 2.0f;

            // write input
            bufL_[writePos_] = xL;
            bufR_[writePos_] = xR;
            writePos_ = (writePos_ + 1) % N;

            // phase drifts so read speed = ratio
            phase_ += (1.f - ratio_);
            while (phase_ < 0.f)       phase_ += W;
            while (phase_ >= W)        phase_ -= W;

            // two heads staggered by W/2, Hann-windowed
            float accL = 0.f, accR = 0.f;
            for (int h = 0; h < 2; ++h)
            {
                float d = phase_ + h * (W * 0.5f);
                if (d >= W) d -= W;
                const float w = 0.5f - 0.5f * std::cos(6.2831853f * d / W);
                const float rd = (float)writePos_ - 1.f - d;
                float rp = std::fmod(rd + N * 4.f, (float)N);
                const int p0 = (int)rp;
                const int p1 = (p0 + 1) % N;
                const float fr = rp - (float)p0;
                accL += w * (bufL_[p0] + fr * (bufL_[p1] - bufL_[p0]));
                accR += w * (bufR_[p0] + fr * (bufR_[p1] - bufR_[p0]));
            }

            // robot band adds a touch of saturation for the classic hard edge
            if (dimN >= 18.f)
            {
                const float sat = 0.08f + (dimN - 18.f) * 0.02f;
                accL = accL * (1.f - sat) + std::tanh(accL * 2.f) * sat;
                accR = accR * (1.f - sat) + std::tanh(accR * 2.f) * sat;
            }

            outL[i] = accL;
            outR[i] = accR;
        }
    }

private:
    static constexpr int kShiftBuf = 8192;
    static constexpr int kShiftWin = 2048;

    std::vector<float> v_;
    std::vector<float> bufL_, bufR_;
    int   writePos_;
    float phase_, ratio_, targetRatio_;

    std::vector<float> detBuf_;
    int   detPos_, detCnt_;
    float pitchHz_, conf_, snapHz_;
    int   note_;
    bool  valid_;

    // scale tables (semitone sets within octave)
    static bool inScale(int scale, int pc)
    {
        static const bool maj[12]  = {1,0,1,0,1,1,0,1,0,1,0,1};
        static const bool min_[12] = {1,0,1,1,0,1,0,1,1,0,1,0};
        static const bool pma[12]  = {1,0,1,0,1,0,0,1,0,1,0,0};
        static const bool pmi[12]  = {1,0,0,1,0,1,0,1,0,0,1,0};
        switch (scale)
        {
        case 0: return maj[pc];
        case 1: return min_[pc];
        case 2: return pma[pc];
        default: return pmi[pc];
        }
    }

    void detectPitch(float sr)
    {
        // autocorrelation on the last 2048 samples of detBuf_
        // lag range: 55..1100 -> ~40..800 Hz
        const int Wn = 2048;
        // energy / noise gate
        float rms = 0.f;
        for (int i = 0; i < Wn; i += 8)
        {
            const int idx = (detPos_ - Wn + i + 2 * Wn) & 2047;
            rms += detBuf_[idx] * detBuf_[idx];
        }
        rms = std::sqrt(rms / (Wn / 8));
        if (rms < 0.01f) { valid_ = false; pitchHz_ = 0.f; return; }

        int bestLag = 0; float bestVal = 0.f; float base = 0.f;
        const int lagMin = (int)(sr / 800.f), lagMax = (int)(sr / 40.f);
        for (int lag = lagMin; lag < lagMax; lag += 2)
        {
            float ac = 0.f;
            for (int i = 0; i < 1024; i += 2)
            {
                const int i0 = (detPos_ - 1024 + i + 4 * Wn) & 2047;
                const int i1 = (i0 - lag + 4 * Wn) & 2047;
                ac += detBuf_[i0] * detBuf_[i1];
            }
            if (lag == lagMin) base = ac;
            if (ac > bestVal) { bestVal = ac; bestLag = lag; }
        }
        if (bestLag == 0 || base <= 0.f) { valid_ = false; return; }
        conf_ = bestVal / base;
        if (conf_ < 0.35f) { valid_ = false; return; }

        pitchHz_ = sr / (float)bestLag;
        // nearest in-scale note
        const float midi = 69.f + 12.f * std::log2(pitchHz_ / 440.f);
        const int key = (int)(v_[kKey] + 0.5f);
        const int scl = (int)(v_[kScale] + 0.5f);
        int bestN = (int)std::round(midi); int bestD = 100;
        for (int n = bestN - 6; n <= bestN + 6; ++n)
        {
            const int pc = ((n - key) % 12 + 12) % 12;
            if (!inScale(scl, pc)) continue;
            const int d = std::abs(n - (int)std::round(midi * 4) / 4 * 4); // rough
            const int dd = std::abs(n - (int)std::round(midi));
            if (dd < bestD) { bestD = dd; bestN = n; }
            (void)d;
        }
        note_ = bestN;
        snapHz_ = 440.f * std::pow(2.f, (bestN - 69) / 12.f);
        // ignore absurd corrections (>7 st away -> likely bad detect)
        if (std::fabs(12.f * std::log2(snapHz_ / pitchHz_)) > 7.f) { valid_ = false; return; }
        valid_ = true;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G2BToonPlugin)
};

Plugin* createPlugin() { return new G2BToonPlugin(); }

END_NAMESPACE_DISTRHO
