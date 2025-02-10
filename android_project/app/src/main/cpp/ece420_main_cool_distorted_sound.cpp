//
// Created by daran on 1/12/2017 to be used in ECE420 Sp17 for the first time.
// Modified by dwang49 on 1/1/2018 to adapt to Android 7.0 and Shield Tablet updates.
//

#include <jni.h>
#include "ece420_main.h"
#include "ece420_lib.h"
#include "kiss_fft/kiss_fft.h"

// Declare JNI function
extern "C" {
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_getFftBuffer(JNIEnv *env, jclass, jobject bufferPtr);
}

// JNI Function
extern "C" {
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_writeNewFreq(JNIEnv *env, jclass, jint);
}
// JNI Function
extern "C" {
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_writeNewSwitch(JNIEnv *env, jclass, jint);
}

// Student Variables
#define EPOCH_PEAK_REGION_WIGGLE 30
#define VOICED_THRESHOLD 100000000
#define FRAME_SIZE 1024
#define BUFFER_SIZE (3 * FRAME_SIZE)
#define F_S 48000
float bufferIn[BUFFER_SIZE] = {};
float bufferOut[BUFFER_SIZE] = {};
int newEpochIdx = FRAME_SIZE;

// We have two variables here to ensure that we never change the desired frequency while
// processing a frame. Thread synchronization, etc. Setting to 300 is only an initializer.
int FREQ_NEW_ANDROID = 300;
int FREQ_NEW = 300;
// Switch from UI (Use this to enable/disable TDPSOLA to display the spectrogram
bool IS_TDPSOLA_ANDROID = false;

// FRAME_SIZE is 1024 and we zero-pad it to 2048 to do FFT
#define FRAME_SIZE 1024
#define ZP_FACTOR 2
#define FFT_SIZE (FRAME_SIZE * ZP_FACTOR)
// Variable to store final FFT output
float fftOut[FFT_SIZE] = {};
bool isWritingFft = false;

// FFT Vars
kiss_fft_cfg kfftCfg = kiss_fft_alloc(FFT_SIZE, false, NULL,NULL);
kiss_fft_cpx kfftIn[FFT_SIZE] = {};
kiss_fft_cpx kfftOut[FFT_SIZE] = {};

void processFFT(float* in, float* out) {
    // 1. Apply hamming window to the entire FRAME_SIZE
    for (int i = 0; i < FRAME_SIZE; i++) {
        kfftIn[i] = { in[i] * getHanningCoef(FRAME_SIZE,i), 0.0f };
    }

    // 2. Zero padding to FFT_SIZE = FRAME_SIZE * ZP_FACTOR
    // probably not necessary
    for (int i = FRAME_SIZE; i < FFT_SIZE; i++) {
        kfftIn[i] = {0.0f, 0.0f};
    }

    // 3. Apply fft with KISS_FFT engine
    kiss_fft(kfftCfg, kfftIn, kfftOut);

    // 4. Scale fftOut[] to between 0 and 1 with log() and linear scaling
    for (int i = 0; i < FFT_SIZE; i++) {
        const float magSquared = kfftOut[i].r * kfftOut[i].r + kfftOut[i].i + kfftOut[i].i;
        out[i] = log10(magSquared) * 0.0000001;
    }
}

bool lab5PitchShift(float *bufferIn) {
    // Lab 4 code is condensed into this function
    int periodLen = detectBufferPeriod(bufferIn);
    float freq = ((float) F_S) / periodLen;

    // If voiced
    if (periodLen > 0) {

        LOGD("Frequency detected: %f\r\n", freq);

        // Epoch detection - this code is written for you, but the principles will be quizzed
        std::vector<int> epochLocations;
        findEpochLocations(epochLocations, bufferIn, periodLen);

        // In this section, you will implement the algorithm given in:
        // https://courses.engr.illinois.edu/ece420/lab5/lab/#buffer-manipulation-algorithm
        //
        // Don't forget about the following functions! API given on the course page.
        //
        // getHanningCoef();
        // findClosestInVector();
        // overlapAndAdd();
        // *********************** START YOUR CODE HERE  **************************** //
        int closestIdx = 0;
        const int P0 = periodLen;
        const int P1 = F_S / FREQ_NEW;
        const int l = 2 * P0 + 1;
        float hWindowed[l];

        while (newEpochIdx < 2 * FRAME_SIZE) {
            // Find the closest epoch in the original signal
            closestIdx = findClosestInVector(epochLocations,newEpochIdx,newEpochIdx - 1,epochLocations.size() - 2);

            // apply Hanning window
            for(int i = 0; i < l; i++) {
                const int pos = (epochLocations[closestIdx] - P0 + i + BUFFER_SIZE) % (BUFFER_SIZE);
                hWindowed[i] = getHanningCoef(l,i) * bufferIn[pos];
            }

            // overlap
            overlapAddArray(bufferOut, hWindowed, newEpochIdx - P0,l);
            newEpochIdx += P1;
        }
        // ************************ END YOUR CODE HERE  ***************************** //
    }

    // Final bookkeeping, move your new pointer back, because you'll be
    // shifting everything back now in your circular buffer
    newEpochIdx -= FRAME_SIZE;
    if (newEpochIdx < FRAME_SIZE) {
        newEpochIdx = FRAME_SIZE;
    }

    return (periodLen > 0);
}


void ece420ProcessFrame(sample_buf *dataBuf) {
    isWritingFft = false;

    // Keep in mind, we only have 20ms to process each buffer!
    struct timeval start;
    struct timeval end;
    gettimeofday(&start, NULL);

    // Get the new desired frequency from android
    FREQ_NEW = FREQ_NEW_ANDROID;


    // Data is encoded in signed PCM-16, little-endian, mono
    int16_t data[FRAME_SIZE];
    for (int i = 0; i < FRAME_SIZE; i++) {
        data[i] = ((uint16_t) dataBuf->buf_[2 * i]) | (((uint16_t) dataBuf->buf_[2 * i + 1]) << 8);
    }

    // Shift our old data back to make room for the new data
    for (int i = 0; i < 2 * FRAME_SIZE; i++) {
        bufferIn[i] = bufferIn[i + FRAME_SIZE - 1];
    }

    // Finally, put in our new data.
    for (int i = 0; i < FRAME_SIZE; i++) {
        bufferIn[i + 2 * FRAME_SIZE - 1] = (float) data[i];
    }

    // The whole kit and kaboodle -- pitch shift
    bool isVoiced = lab5PitchShift(bufferIn);

    if (isVoiced) {
        for (int i = 0; i < FRAME_SIZE; i++) {
            int16_t newVal = (int16_t) bufferOut[i];

            uint8_t lowByte = (uint8_t) (0x00ff & newVal);
            uint8_t highByte = (uint8_t) ((0xff00 & newVal) >> 8);
            dataBuf->buf_[i * 2] = lowByte;
            dataBuf->buf_[i * 2 + 1] = highByte;
        }
    }

    // Very last thing, update your output circular buffer!
    for (int i = 0; i < 2 * FRAME_SIZE; i++) {
        bufferOut[i] = bufferOut[i + FRAME_SIZE - 1];
    }

    for (int i = 0; i < FRAME_SIZE; i++) {
        bufferOut[i + 2 * FRAME_SIZE - 1] = 0;
    }

    // Spectrogram is just a fancy word for short time fourier transform
    // 1. Apply hamming window to the entire FRAME_SIZE
    // 2. Zero padding to FFT_SIZE = FRAME_SIZE * ZP_FACTOR
    // 3. Apply fft with KISS_FFT engine
    // 4. Scale fftOut[] to between 0 and 1 with log() and linear scaling
    // NOTE: This code block is a suggestion to get you started. You will have to
    // add/change code outside this block to implement FFT buffer overlapping (extra credit part).
    // Keep all of your code changes within java/MainActivity and cpp/ece420_*
    // ********************* START YOUR CODE HERE *********************** //
    // ********************* RE-USE Lab3 CODE HERE *********************** //
    isWritingFft = true;
    processFFT(bufferOut, fftOut);
    // ********************* END YOUR CODE HERE ************************* //
    // Flip the flag so that the JNI thread will update the buffer
    isWritingFft = false;

    gettimeofday(&end, NULL);
    LOGD("Time delay: %ld us",  ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));
}


// Returns lag l that maximizes sum(x[n] x[n-k])
int detectBufferPeriod(float *buffer) {

    float totalPower = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        totalPower += buffer[i] * buffer[i];
    }

    if (totalPower < VOICED_THRESHOLD) {
        return -1;
    }

    // FFT is done using Kiss FFT engine. Remember to free(cfg) on completion
    kiss_fft_cfg cfg = kiss_fft_alloc(BUFFER_SIZE, false, 0, 0);

    kiss_fft_cpx buffer_in[BUFFER_SIZE];
    kiss_fft_cpx buffer_fft[BUFFER_SIZE];

    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer_in[i].r = bufferIn[i];
        buffer_in[i].i = 0;
    }

    kiss_fft(cfg, buffer_in, buffer_fft);
    free(cfg);


    // Autocorrelation is given by:
    // autoc = ifft(fft(x) * conj(fft(x))
    //
    // Also, (a + jb) (a - jb) = a^2 + b^2
    kiss_fft_cfg cfg_ifft = kiss_fft_alloc(BUFFER_SIZE, true, 0, 0);

    kiss_fft_cpx multiplied_fft[BUFFER_SIZE];
    kiss_fft_cpx autoc_kiss[BUFFER_SIZE];

    for (int i = 0; i < BUFFER_SIZE; i++) {
        multiplied_fft[i].r = (buffer_fft[i].r * buffer_fft[i].r)
                              + (buffer_fft[i].i * buffer_fft[i].i);
        multiplied_fft[i].i = 0;
    }

    kiss_fft(cfg_ifft, multiplied_fft, autoc_kiss);
    free(cfg_ifft);

    // Move to a normal float array rather than a struct array of r/i components
    float autoc[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++) {
        autoc[i] = autoc_kiss[i].r;
    }

    // We're only interested in pitches below 1000Hz.
    // Why does this line guarantee we only identify pitches below 1000Hz?
    int minIdx = F_S / 1000;
    int maxIdx = BUFFER_SIZE / 2;

    int periodLen = findMaxArrayIdx(autoc, minIdx, maxIdx);
    float freq = ((float) F_S) / periodLen;

    // TODO: tune
    if (freq < 50) {
        periodLen = -1;
    }

    return periodLen;
}


void findEpochLocations(std::vector<int> &epochLocations, float *buffer, int periodLen) {
    // This algorithm requires that the epoch locations be pretty well marked

    int largestPeak = findMaxArrayIdx(bufferIn, 0, BUFFER_SIZE);
    epochLocations.push_back(largestPeak);

    // First go right
    int epochCandidateIdx = epochLocations[0] + periodLen;
    while (epochCandidateIdx < BUFFER_SIZE) {
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

        int peakOffset = findMaxArrayIdx(bufferIn, minIdx, maxIdx) - minIdx;
        peakOffset -= EPOCH_PEAK_REGION_WIGGLE;

        epochLocations[i] += peakOffset;
    }
}

void overlapAddArray(float *dest, float *src, int startIdx, int len) {
    int idxLow = startIdx;
    int idxHigh = startIdx + len;

    int padLow = 0;
    int padHigh = 0;
    if (idxLow < 0) {
        padLow = -idxLow;
    }
    if (idxHigh > BUFFER_SIZE) {
        padHigh = BUFFER_SIZE - idxHigh;
    }

    // Finally, reconstruct the buffer
    for (int i = padLow; i < len + padHigh; i++) {
        dest[startIdx + i] += src[i];
    }
}

// http://stackoverflow.com/questions/34168791/ndk-work-with-floatbuffer-as-parameter
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_getFftBuffer(JNIEnv *env, jclass, jobject bufferPtr) {
    jfloat *buffer = (jfloat *) env->GetDirectBufferAddress(bufferPtr);
    // thread-safe, kinda
    while (isWritingFft) {}
    // We will only fetch up to FRAME_SIZE data in fftOut[] to draw on to the screen
    for (int i = 0; i < FRAME_SIZE; i++) {
        buffer[i] = fftOut[i];
    }
}

JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_writeNewFreq(JNIEnv *env, jclass, jint newFreq) {
FREQ_NEW_ANDROID = (int) newFreq;
return;
}


JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_writeNewSwitch(JNIEnv *env, jclass, jint newState) {
IS_TDPSOLA_ANDROID = (bool) newState;
return;
}