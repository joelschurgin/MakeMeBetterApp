#pragma once

#include <vector>
#include <math.h>

class Recorder {
public:
    Recorder(int _sampleRate, int _frameSize, double maxRecordLength);
    ~Recorder();

    void start();
    void stop();
    void play();
    bool isRecording() { return recording; }
    bool isPlaying() { return playing; }

    void writeData(float *data);
    float* getNextBuffer();

private:
    void applyFades();

private:
    int sampleRate;
    int frameSize;
    double maxRecordLength;
    size_t pos;
    bool recording;
    bool playing;

    const int fadeLength;

//    float attack;
//    float release;

    std::vector<float> signal;
    std::vector<float> nextBuffer;
};