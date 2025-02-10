#include "TextParser.h"

TextParser::TextParser(int _bufferSize, int _sampleRate)
    : bufferSize{_bufferSize}
    , sampleRate{_sampleRate}
    , prevUserFreq{0.0f}
    , samplesPerNote{0.0}
    , bufferOffset{0ul}
    , n{0} {}

TextParser::~TextParser() {
    pitches.clear();
    events.clear();
}

void TextParser::parse(std::string input) {
    if (prevInput == input) {
        return;
    }

    bufferOffset = 0ul;

    if (!pitches.empty()) {
        pitches.clear();
    }

    pitches.reserve(input.size());

    for (char c : input) {
        if (c == 'x' || c == 'X') {
            pitches.push_back(-1);
        } else {
            if (c >= '1' && c <= '8') {
                int num = -1;
                switch (c) {
                    case '1':
                        num = 0; // c
                        break;
                    case '2':
                        num = 2; // d
                        break;
                    case '3':
                        num = 4; // e
                        break;
                    case '4':
                        num = 5; // f
                        break;
                    case '5':
                        num = 7; // g
                        break;
                    case '6':
                        num = 9; // a
                        break;
                    case '7':
                        num = 11; // b
                        break;
                    case '8':
                        num = 12; // c +oct
                        break;
                }
                pitches.push_back(num);
            }
        }
    }
}

void TextParser::calcPitchEvents(float userFreq) {
    if (bufferOffset >= bufferSize || userFreq == 0) return;

    n = getNearestNote(userFreq) % 12;
    prevUserFreq = userFreq;

    if (!events.empty()) {
        events.clear();
    }
    events.reserve(pitches.size());

    samplesPerNote = 30.0 * sampleRate / tempo; // 30 = 60 sec / 2 notes per sec

    double pos = 0;
    for (Pitch p : pitches) {
        if (p < 0) {
            events.emplace_back(pos, 0.0);
        } else {
            events.emplace_back(pos, 440.0 * pow(2.0, static_cast<double>(n + p) / 12.0));
        }
        pos += samplesPerNote;
    }

    if (events.size() > 0) {
        events.emplace_back(pos, 0.0);
    }
}

int TextParser::getNearestNote(float freq) const {
    return static_cast<int>(0.5 + 12.0 * log2(freq / 440.0));
}

std::vector<PitchEvent> TextParser::getPitchEventsForNextBuffer() {
    std::vector<PitchEvent> bufferEvents;
    const unsigned long nextBufferPos = bufferOffset + bufferSize;

    for (PitchEvent event : events) {
        if (event.position >= bufferOffset && event.position < nextBufferPos) {
            bufferEvents.emplace_back(event.position - bufferOffset, event.frequency);
        }
    }

    bufferOffset = nextBufferPos;
    melodyDone();
    return bufferEvents;
}

bool TextParser::melodyDone() {
    bool done = events.size() == 0 || (bufferOffset + bufferSize > events[events.size() - 1].position);
    if (done) bufferOffset = 0ul;
    return done;
}

double TextParser::getSamplesPerNote() {
    return samplesPerNote;
}

