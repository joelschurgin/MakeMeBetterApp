#pragma once

#include <vector>
#include "TextParser.h"
#include "PitchEventHandler.h"

class EnvelopeGenerator {
public:
    EnvelopeGenerator(int _blockSize);
    ~EnvelopeGenerator();

    const float* getNextBlock(std::vector<PitchEvent> pitchEvents);

    void setShape(double newShape, double samplesPerNote);
    void setSamplesPerNote(double samplesPerNote);

private:
    void startNote() {
        value = 0.0;
        state = ATTACK;
    }
    float getNextSample();

private:
    enum State {
        ATTACK,
        DECAY,
        OFF
    };

    const int blockSize;

    State state;

    double shape; // equal to attack value
    double value;
    double attackInc, decayInc;

    std::vector<float> envelope;

    PitchEventHandler eventHandler;
};
