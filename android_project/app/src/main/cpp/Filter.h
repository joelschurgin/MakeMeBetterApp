#pragma once

#include <vector>
#include "TextParser.h"
#include "PitchEventHandler.h"

class SinglePoleLPF {
public:
    SinglePoleLPF(double _sampleRate);
    ~SinglePoleLPF();

    void reset();
    float calcG(float cutoff);
    void setG(float newG) { g = newG; }

    float processSample(float sample);

private:
    double sampleRate;

    double g;
    const double gExpCoeff;
    const double Coeff1 = 1.0 / 1.3;
    const double Coeff2 = 0.3 / 1.3;

    double prevInputSample;
    double prevOutputSample;
};

class SinglePoleHPF : public SinglePoleLPF {
public:
    SinglePoleHPF(double _sampleRate) : SinglePoleLPF(_sampleRate) {}
    float processSample(float sample);
};

class Filter {
public:
    Filter(double _sampleRate, int _bufferSize, int nCascades, double _res = 0.707, double _maxFrequencyInOctaves = 2.0);
    ~Filter();

    void processBlock(float *data, const float *envelope, std::vector<PitchEvent> pitchEvents);
    void setRes(double newRes) { res = newRes; }
    void reset();

    void setMaxFrequencyInOctaves(double newFreq) {
        relativeMaxFreqInOctaves = newFreq;
        maxFreqExponentBase = pow(2.0, relativeMaxFreqInOctaves);
    }

private:
    double FrequencyCurve(double x, double freq) const { return freq * pow(maxFreqExponentBase, x); }
private:
    double sampleRate;
    int bufferSize;

    std::vector<SinglePoleLPF> lpf;

    const double comp = 0.5;
    double res;
    double nonLinFeedback;

    double relativeMaxFreqInOctaves;
    double maxFreqExponentBase;

    PitchEventHandler eventHandler;
};