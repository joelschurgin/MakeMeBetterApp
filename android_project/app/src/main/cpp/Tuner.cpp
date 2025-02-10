//
// Created by joels on 4/18/2024.
//

#include "Tuner.h"
#include "kiss_fft/kiss_fft.h"
#include <math.h>

Tuner::Tuner(int _frameSize, int _sampleRate, int maxNumMelodies)
    : bufferSize{3 * _frameSize}
    , frameSize{_frameSize}
    , sampleRate{_sampleRate}
    , newEpochIdx(maxNumMelodies, frameSize)
    , bufferIn(bufferSize, 0.0f)
    , highpass(sampleRate) {
    bufferOut.reserve(maxNumMelodies);
    for (int i = 0; i < maxNumMelodies; i++) {
        bufferOut.emplace_back(bufferSize, 0.0f);
    }
}

Tuner::~Tuner() {
    bufferIn.clear();

    for (auto buf: bufferOut) {
        buf.clear();
    }
    bufferOut.clear();
}

void Tuner::writeInputSamples(const float *data) {
    // Shift our old data back to make room for the new data
    for (int i = 0; i < 2 * frameSize; i++) {
        bufferIn[i] = bufferIn[i + frameSize - 1];
    }

    // Finally, put in our new data.
    for (int i = 0; i < frameSize; i++) {
        bufferIn[i + 2 * frameSize - 1] = data[i];
    }
}

int Tuner::detectBufferPeriod() {
    float totalPower = 0;
    for (int i = 0; i < bufferSize; i++) {
        totalPower += bufferIn[i] * bufferIn[i];
    }

    if (totalPower < VOICED_THRESHOLD) {
        return -1;
    }

    // FFT is done using Kiss FFT engine. Remember to free(cfg) on completion
    kiss_fft_cfg cfg = kiss_fft_alloc(bufferSize, false, 0, 0);

    kiss_fft_cpx buffer_in[bufferSize];
    kiss_fft_cpx buffer_fft[bufferSize];

    for (int i = 0; i < bufferSize; i++) {
        buffer_in[i].r = bufferIn[i];
        buffer_in[i].i = 0;
    }

    kiss_fft(cfg, buffer_in, buffer_fft);
    free(cfg);


    // Autocorrelation is given by:
    // autoc = ifft(fft(x) * conj(fft(x))
    //
    // Also, (a + jb) (a - jb) = a^2 + b^2
    kiss_fft_cfg cfg_ifft = kiss_fft_alloc(bufferSize, true, 0, 0);

    kiss_fft_cpx multiplied_fft[bufferSize];
    kiss_fft_cpx autoc_kiss[bufferSize];

    for (int i = 0; i < bufferSize; i++) {
        multiplied_fft[i].r = (buffer_fft[i].r * buffer_fft[i].r)
                              + (buffer_fft[i].i * buffer_fft[i].i);
        multiplied_fft[i].i = 0;
    }

    kiss_fft(cfg_ifft, multiplied_fft, autoc_kiss);
    free(cfg_ifft);

    // Move to a normal float array rather than a struct array of r/i components
    float autoc[bufferSize];
    for (int i = 0; i < bufferSize; i++) {
        autoc[i] = autoc_kiss[i].r;
    }

    // We're only interested in pitches below 1000Hz.
    // Why does this line guarantee we only identify pitches below 1000Hz?
    int minIdx = sampleRate / 1000;
    int maxIdx = bufferSize / 2;

    int periodLen = findMaxArrayIdx(autoc, minIdx, maxIdx);
    float freq = ((float) sampleRate) / periodLen;

    if (freq < 50) {
        periodLen = -1;
    }

    return periodLen;
}

void Tuner::processBlock(std::vector<std::vector<float>>& data, std::vector<std::vector<PitchEvent>> pitchEventsList, int periodLen) {
    // The whole kit and kaboodle -- pitch shift
    const float fundamentalFreq = static_cast<float>(sampleRate) / static_cast<float>(periodLen);
    highpass.setG(highpass.calcG(fundamentalFreq));
    for (int i = 0; i < frameSize; i++) {
        highpass.processSample(bufferIn[i]);
    }

    const int numMelodies = std::min(pitchEventsList.size(), bufferOut.size());
    for (int melodyIdx = 0; melodyIdx < numMelodies; ++melodyIdx) {
        pitchShift(pitchEventsList[melodyIdx], periodLen, melodyIdx);
    }

    if (periodLen > 0) {
        for (int melodyIdx = 0; melodyIdx < numMelodies; ++melodyIdx) {
            for (int i = 0; i < frameSize; i++) {
                data[melodyIdx][i] = bufferOut[melodyIdx][i];
            }
        }
    }

    for (int melodyIdx = 0; melodyIdx < numMelodies; ++melodyIdx) {
        for (int i = 0; i < 2 * frameSize; i++) {
            bufferOut[melodyIdx][i] = bufferOut[melodyIdx][i + frameSize - 1];
        }

        for (int i = 0; i < frameSize; i++) {
            bufferOut[melodyIdx][i + 2 * frameSize - 1] = 0.0f;
        }
    }
}

void Tuner::pitchShift(std::vector<PitchEvent> pitchEvents, int periodLen, int melodyIdx) {
    // Lab 4 code is condensed into this function
    // If voiced
    if (periodLen > 0) {
        std::vector<int> epochLocations;
        findEpochLocations(epochLocations, bufferIn.data(), periodLen);

        int closestIdx = 1;
        const int P0 = periodLen;
        const int l = 2 * P0 + 1;
        float hWindowed[l];

        int P1 = 1;

        eventHandler.prepareForNextBuffer();
        PitchEvent currPitchEvent = eventHandler.getCurrPitchEvent();
        if (currPitchEvent.frequency > 0) {
            P1 = static_cast<int>(static_cast<float>(sampleRate) / currPitchEvent.frequency);
        }

        while (newEpochIdx[melodyIdx] < 2 * frameSize) {
            if (eventHandler.setCurrPitchEvent(newEpochIdx[melodyIdx] - frameSize, pitchEvents)) {
                currPitchEvent = eventHandler.getCurrPitchEvent();
                if (currPitchEvent.frequency > 0) {
                    P1 = static_cast<int>(static_cast<float>(sampleRate) /
                                          currPitchEvent.frequency);
                }
            }

            if (currPitchEvent.frequency > 0) {
                // Find the closest epoch in the original signal
                closestIdx = findClosestInVector(epochLocations, newEpochIdx[melodyIdx], closestIdx - 1,
                                                 epochLocations.size() - 2);

                // apply Hanning window
                float peak = 0.0f;
                for (int i = 0; i < l; i++) {
                    const int pos = (epochLocations[closestIdx] - P0 + i + bufferSize) % (bufferSize);
                    hWindowed[i] = getHanningCoef(l, i) * bufferIn[pos];
                    peak = std::max(peak, std::abs(hWindowed[i]));
                }

                const float scale = 4000.0f / peak;
                for (int i = 0; i < l; i++) {
                    hWindowed[i] *= scale;
                }

                // overlap
                overlapAddArray(bufferOut[melodyIdx].data(), hWindowed, newEpochIdx[melodyIdx] - P0, l);
                newEpochIdx[melodyIdx] += P1;
            } else {
                newEpochIdx[melodyIdx]++;
            }
        }
        // ************************ END YOUR CODE HERE  ***************************** //
    }

    // Final bookkeeping, move your new pointer back, because you'll be
    // shifting everything back now in your circular buffer
    newEpochIdx[melodyIdx] -= frameSize;
    if (newEpochIdx[melodyIdx] < frameSize) {
        newEpochIdx[melodyIdx] = frameSize;
    }
}

void Tuner::findEpochLocations(std::vector<int> &epochLocations, float *buffer, int periodLen) {
    // This algorithm requires that the epoch locations be pretty well marked

    int largestPeak = findMaxArrayIdx(bufferIn.data(), 0, bufferSize);
    epochLocations.push_back(largestPeak);

    // First go right
    int epochCandidateIdx = epochLocations[0] + periodLen;
    while (epochCandidateIdx < bufferSize) {
        epochLocations.push_back(epochCandidateIdx);
        epochCandidateIdx += periodLen;
    }

    // Then go left
    epochCandidateIdx = epochLocations[0] - periodLen;
    while (epochCandidateIdx > 0) {
        epochLocations.push_back(epochCandidateIdx);
        epochCandidateIdx -= periodLen;
    }

    // Sort in place so that we can more easily find the period,
    // where period = (epochLocations[t+1] + epochLocations[t-1]) / 2
    std::sort(epochLocations.begin(), epochLocations.end());

    // Finally, just to make sure we have our epochs in the right
    // place, ensure that every epoch mark (sans first/last) sits on a peak
    for (int i = 1; i < epochLocations.size() - 1; i++) {
        int minIdx = epochLocations[i] - EPOCH_PEAK_REGION_WIGGLE;
        int maxIdx = epochLocations[i] + EPOCH_PEAK_REGION_WIGGLE;

        int peakOffset = findMaxArrayIdx(bufferIn.data(), minIdx, maxIdx) - minIdx;
        peakOffset -= EPOCH_PEAK_REGION_WIGGLE;

        epochLocations[i] += peakOffset;
    }
}

void Tuner::overlapAddArray(float *dest, float *src, int startIdx, int len) {
    int idxLow = startIdx;
    int idxHigh = startIdx + len;

    int padLow = 0;
    int padHigh = 0;
    if (idxLow < 0) {
        padLow = -idxLow;
    }
    if (idxHigh > bufferSize) {
        padHigh = bufferSize - idxHigh;
    }

    // Finally, reconstruct the buffer
    for (int i = padLow; i < len + padHigh; i++) {
        dest[startIdx + i] += src[i];
    }
}