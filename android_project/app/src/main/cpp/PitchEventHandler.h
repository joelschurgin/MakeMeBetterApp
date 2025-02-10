#pragma once

#include <vector>
#include "TextParser.h"

class PitchEventHandler {
public:
    PitchEventHandler();
    ~PitchEventHandler();

    void prepareForNextBuffer() { pitchEventIdx = 0; }
    bool setCurrPitchEvent(const int bufferPos, const std::vector<PitchEvent>& events); // returns true if event changed
    PitchEvent getCurrPitchEvent() { return currPitchEvent; }

private:
    PitchEvent currPitchEvent;

    int pitchEventIdx;
};