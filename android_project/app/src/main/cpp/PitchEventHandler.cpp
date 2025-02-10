//
// Created by joels on 4/22/2024.
//

#include "PitchEventHandler.h"

PitchEventHandler::PitchEventHandler()
    : currPitchEvent{0, 0}
    , pitchEventIdx{0} {

}

PitchEventHandler::~PitchEventHandler() {

}

bool PitchEventHandler::setCurrPitchEvent(const int bufferPos, const std::vector<PitchEvent>& events) {
    for (int i = pitchEventIdx; i < events.size(); ++i) {
        if (std::abs(static_cast<int>(events[i].position - bufferPos)) <= std::abs(static_cast<int>(bufferPos - currPitchEvent.position))) {
            currPitchEvent = events[i];
            pitchEventIdx = i;
            return true;
        }
    }
    return false;
}
