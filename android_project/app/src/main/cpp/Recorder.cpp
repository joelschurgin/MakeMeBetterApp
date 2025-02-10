#include "Recorder.h"
#include "Filter.h"
#include "Tuner.h"

Recorder::Recorder(int _sampleRate, int _frameSize, double _maxRecordLength)
    : sampleRate{_sampleRate}
    , frameSize{_frameSize}
    , maxRecordLength{_maxRecordLength}
    , pos{0}
    , recording{false}
    , playing{false}
    , fadeLength{24}
    , signal(static_cast<size_t>(sampleRate * maxRecordLength), 0.0f)
    , nextBuffer(frameSize){

}

Recorder::~Recorder() {
    signal.clear();
}

void Recorder::start() {
    pos = 0;
    recording = true;
    signal.resize(static_cast<size_t>(sampleRate * maxRecordLength));
    for (size_t i = 0; i < signal.size(); ++i) {
        signal[i] = 0.0f;
    }
}

void Recorder::stop() {
    pos = 0;
    if (playing) {
        nextBuffer.resize(nextBuffer.size(), 0.0f);
    }
    playing = false;

    if (recording) {
        recording = false;
        applyFades();
    }
}

void Recorder::play() {
    pos = 0;
    recording = false;
    playing = true;
}

void Recorder::writeData(float *data) {
    for (int i = 0; i < frameSize; ++i) {
        if (pos >= signal.size()) {
            stop();
            return;
        }

        signal[pos] = data[i];
        ++pos;
    }
}

float *Recorder::getNextBuffer() {
    if (!playing) return nextBuffer.data();

    for (int i = 0; i < frameSize; ++i) {
        nextBuffer[i] = signal[pos];
        ++pos;
        if (pos >= signal.size()) {
            pos -= signal.size();
        }
    }
    return nextBuffer.data();
}

void Recorder::applyFades() {
    for (size_t i = 0; i < fadeLength; ++i) {
        signal[pos] *= static_cast<float>(pos) / static_cast<float>(fadeLength);
    }

    for (size_t i = signal.size() - fadeLength; i < signal.size(); ++i) {
        signal[pos] *= (1.0f - static_cast<float>(pos) / static_cast<float>(fadeLength));
    }
}
