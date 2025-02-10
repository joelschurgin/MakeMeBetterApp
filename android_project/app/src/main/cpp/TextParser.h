#pragma once

#include <jni.h>
#include <string>
#include <vector>

// 1 - 8 for corresponding note in a major scale, -1 for rest
// Converted to semitones where 0 is root and 12 is an octave up
typedef int Pitch;

struct PitchEvent {
    unsigned long position; // in samples
    float frequency; // hz

    PitchEvent(unsigned long _position, float _frequency) : position{_position}, frequency{_frequency} {}
};

class TextParser {
public:
    TextParser(int _bufferSize, int _sampleRate);
    ~TextParser();

    void parse(std::string input);
    void calcPitchEvents(float userFreq);
    std::vector<PitchEvent> getPitchEventsForNextBuffer();

    void setTempo(float _tempo) {
        tempo = _tempo;
        calcPitchEvents(prevUserFreq);
    }

    bool melodyDone();
    double getSamplesPerNote();

private:
    int getNearestNote(float freq) const;

private:
    std::string prevInput;
    int bufferSize;
    int sampleRate;
    float tempo;
    float prevUserFreq;
    double samplesPerNote;

    unsigned long bufferOffset;

    int n;

    std::vector<Pitch> pitches;
    std::vector<PitchEvent> events;
};
