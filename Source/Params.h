#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// id, display name, min, max, default, step
// ids and ranges MUST match the D table at the top of ui/index.html
#define SHZ_PARAMS(X) \
 X(ingain,   "Input Gain",        -12,  12,    0,    0.1) \
 X(gate,     "Gate Threshold",    -80, -20,  -55,    1)   \
 X(transpose,"Transpose",         -12,  12,    0,    1)   \
 X(out,      "Output",            -24,  12,   -6,    0.1) \
 X(gain,     "Amp Gain",            0,  10,    6,    0.1) \
 X(tight,    "Amp Tight",           0,  10,    5,    0.1) \
 X(bass,     "Amp Bass",            0,  10,    5,    0.1) \
 X(mid,      "Amp Mid",             0,  10,    6,    0.1) \
 X(treble,   "Amp Treble",          0,  10,    5,    0.1) \
 X(pres,     "Amp Presence",        0,  10,    5,    0.1) \
 X(depth,    "Amp Depth",           0,  10,    4,    0.1) \
 X(master,   "Amp Master",          0,  10,    6,    0.1) \
 X(d_drive,  "Drive Drive",         0,  10,    4,    0.1) \
 X(d_tone,   "Drive Tone",          0,  10,    5,    0.1) \
 X(d_level,  "Drive Level",         0,  10,    6,    0.1) \
 X(d_tight,  "Drive Tight",         0,  10,    6,    0.1) \
 X(b_gain,   "Boost Gain",          0,  10,    5,    0.1) \
 X(b_tone,   "Boost Tone",          0,  10,    5,    0.1) \
 X(b_level,  "Boost Level",         0,  10,    5,    0.1) \
 X(c_sus,    "Comp Sustain",        0,  10,    5,    0.1) \
 X(c_att,    "Comp Attack",         0,  10,    5,    0.1) \
 X(c_level,  "Comp Level",          0,  10,    5,    0.1) \
 X(t_pitch,  "Drop Pitch",        -12,  12,   -2,    1)   \
 X(t_blend,  "Drop Blend",          0,  10,   10,    0.1) \
 X(s_buzz,   "Sitar Buzz",          0,  10,    6,    0.1) \
 X(s_res,    "Sitar Sympathy",      0,  10,    5,    0.1) \
 X(s_tone,   "Sitar Tone",          0,  10,    6,    0.1) \
 X(s_mix,    "Sitar Mix",           0,  10,    7,    0.1) \
 X(cabmix,   "Cab IR Mix",          0, 100,  100,    1)   \
 X(dist,     "Mic Distance",        0,  10,    3,    0.1) \
 X(pos,      "Mic Position",        0,  10,    4,    0.1) \
 X(angle,    "Mic Angle",           0,  10,    2,    0.1) \
 X(room,     "Room",                0,  10,    1,    0.1) \
 X(e1,       "EQ Low Gain",       -15,  15,    0,    0.1) \
 X(e2,       "EQ Low Mid Gain",   -15,  15,    0,    0.1) \
 X(e3,       "EQ Mid Gain",       -15,  15,    0,    0.1) \
 X(e4,       "EQ High Mid Gain",  -15,  15,    0,    0.1) \
 X(e5,       "EQ High Gain",      -15,  15,    0,    0.1) \
 X(f1,       "EQ Low Freq",        30, 300,   80,    1)   \
 X(f2,       "EQ Low Mid Freq",   120, 800,  300,    1)   \
 X(f3,       "EQ Mid Freq",       400,3000, 1000,    1)   \
 X(f4,       "EQ High Mid Freq", 1500,8000, 3000,   10)   \
 X(f5,       "EQ High Freq",     3000,16000,8000,   10)   \
 X(hpf,      "EQ HPF",             20, 300,   40,    1)   \
 X(lpf,      "EQ LPF",           3000,16000,12000,  10)   \
 X(mode,     "Amp Mode",            0,   4,    2,    1)   \
 X(cab,      "Cabinet",             0,   2,    2,    1)   \
 X(mic,      "Microphone",          0,   2,    0,    1)   \
 X(gateon,   "Gate On",             0,   1,    1,    1)   \
 X(drvon,    "Drive On",            0,   1,    1,    1)   \
 X(bston,    "Boost On",            0,   1,    0,    1)   \
 X(cmpon,    "Comp On",             0,   1,    0,    1)   \
 X(tunon,    "Drop On",             0,   1,    0,    1)   \
 X(stron,    "Sitar On",            0,   1,    0,    1)

namespace Id
{
    enum
    {
       #define X(id, n, mn, mx, df, st) id,
        SHZ_PARAMS(X)
       #undef X
        Count
    };
}

struct PDef { const char* id; const char* name; float mn, mx, df, st; };

inline const PDef kParams[Id::Count] =
{
   #define X(id, n, mn, mx, df, st) { #id, n, (float) (mn), (float) (mx), (float) (df), (float) (st) },
    SHZ_PARAMS(X)
   #undef X
};

inline juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : kParams)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p.id, 1 }, p.name,
            juce::NormalisableRange<float> (p.mn, p.mx, p.st), p.df));
    return layout;
}
