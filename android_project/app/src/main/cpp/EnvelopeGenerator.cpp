#include "EnvelopeGenerator.h"

EnvelopeGenerator::EnvelopeGenerator(int _blockSize)
    : blockSize{_blockSize}
    , state{OFF}
    , shape{0.5}
    , value{0.0}
    , envelope(blockSize) {
}

EnvelopeGenerator::~EnvelopeGenerator() {
    envelope.clear();
}

float EnvelopeGenerator::getNextSample() {
    switch (state) {
        case ATTACK:
            value += attackInc;
            if (value >= 1.0) {
                value = 1.0;
                state = DECAY;
            }
            break;
        case DECAY:
            value -= decayInc;
            if (value <= 0.0) {
                value = 0.0;
                state = OFF;
            }
            break;
        case OFF:
            break;
    }
    return value;
}

void EnvelopeGenerator::setShape(double newShape, double samplesPerNote) {
    const double pad = 0.00001; // so we don't divide by 0
    shape = newShape * (1.0 - pad * 2.0) + pad;
    const double shapeSamples = samplesPerNote * shape;
    attackInc = 1.0 / (shapeSamples);
    decayInc = 1.0 / (samplesPerNote - shapeSamples);
}

void EnvelopeGenerator::setSamplesPerNote(double samplesPerNote) {
    setShape(shape, samplesPerNote);
}

const float* EnvelopeGenerator::getNextBlock(std::vector<PitchEvent> pitchEvents) {
    PitchEvent currEvent = eventHandler.getCurrPitchEvent();
    for (int i = 0; i < blockSize; ++i) {
        if (eventHandler.setCurrPitchEvent(i, pitchEvents)) {
            currEvent = eventHandler.getCurrPitchEvent();
            startNote();
        }
        envelope[i] = getNextSample();
    }

    return envelope.data();
}