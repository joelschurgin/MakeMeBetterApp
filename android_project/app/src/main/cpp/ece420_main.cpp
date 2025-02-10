//
// Created by daran on 1/12/2017 to be used in ECE420 Sp17 for the first time.
// Modified by dwang49 on 1/1/2018 to adapt to Android 7.0 and Shield Tablet updates.
//

#include <jni.h>
#include "ece420_main.h"
#include "ece420_lib.h"
#include "kiss_fft/kiss_fft.h"
#include "TextParser.h"
#include "Tuner.h"
#include "Filter.h"
#include "EnvelopeGenerator.h"
#include "Recorder.h"

extern "C" {
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_writeNewTempo(JNIEnv *env, jclass, jint);
}

extern "C" {
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_getNotesInput(JNIEnv *env, jclass clazz, jstring input);
}

// Student Variables
#define FRAME_SIZE 1024
#define F_S 48000

#define MAX_NUM_MELODIES 3

bool recordMode = false;

TextParser parser(FRAME_SIZE, F_S);
Tuner tuner(FRAME_SIZE, F_S, 1);
Filter filter(F_S, FRAME_SIZE, 2, 4.0, 8.0);
EnvelopeGenerator envGenerator(FRAME_SIZE);
Recorder recorder(F_S, FRAME_SIZE, 8.0);

void synthesize(const float* inputData, float *outputData) {
    std::vector<std::vector<PitchEvent>> pitchEventsList;
    std::vector<std::vector<float>> noteData(1, std::vector<float>(FRAME_SIZE, 0));

    tuner.writeInputSamples(inputData);
    const int period = tuner.detectBufferPeriod();
    const float userFreq = static_cast<float>(F_S) / static_cast<float>(period);

    parser.calcPitchEvents(userFreq);
    pitchEventsList = {parser.getPitchEventsForNextBuffer()};

    tuner.processBlock(noteData, pitchEventsList, period);

    envGenerator.setSamplesPerNote(parser.getSamplesPerNote());
    const float *envelope = envGenerator.getNextBlock(pitchEventsList[0]);
    filter.processBlock(noteData[0].data(), envelope, pitchEventsList[0]);

    for (int i = 0; i < FRAME_SIZE; i++) {
        outputData[i] = envelope[i] * noteData[0][i];
    }
}

void ece420ProcessFrame(sample_buf *dataBuf) {
    // Keep in mind, we only have 20ms to process each buffer!
    struct timeval start;
    struct timeval end;
    gettimeofday(&start, NULL);

    // Data is encoded in signed PCM-16, little-endian, mono
    float data[FRAME_SIZE];
    for (int i = 0; i < FRAME_SIZE; i++) {
        const int16_t value = ((uint16_t) dataBuf->buf_[2 * i]) | (((uint16_t) dataBuf->buf_[2 * i + 1]) << 8);
        data[i] = value;
    }

    if (recordMode) {
        if (recorder.isRecording()) {
            for (int i = 0; i < FRAME_SIZE; i++) {
                const int16_t value = ((uint16_t) dataBuf->buf_[2 * i]) | (((uint16_t) dataBuf->buf_[2 * i + 1]) << 8);
                data[i] = value;
            }
            recorder.writeData(data);
        } else {
            if (recorder.isPlaying()) {
                synthesize(recorder.getNextBuffer(), data);
            } else {
                for (int i = 0; i < FRAME_SIZE; i++) {
                    data[i] = 0.0f;
                }
            }
        }
    } else {
        synthesize(data, data);
    }

    for (int i = 0; i < FRAME_SIZE; i++) {
        const int16_t value = data[i];
        uint8_t lowByte = (uint8_t) (0x00ff & value);
        uint8_t highByte = (uint8_t) ((0xff00 & value) >> 8);
        dataBuf->buf_[i * 2] = lowByte;
        dataBuf->buf_[i * 2 + 1] = highByte;
    }

    gettimeofday(&end, NULL);
    LOGD("Time delay: %ld us",  ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));

}

extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_writeNewTempo(JNIEnv *env, jclass, jint newTempo) {
    const int tempo = std::max(40, newTempo);
    parser.setTempo(tempo);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_writeNewEnvelopePeakPosition(JNIEnv *env, jclass, jfloat newEnvelopePeakPosition) {
    envGenerator.setShape(newEnvelopePeakPosition, parser.getSamplesPerNote());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_getNotesInput(JNIEnv *env, jclass clazz, jstring input) {
    const char *cstr = env->GetStringUTFChars(input, NULL);
    std::string str = std::string(cstr);
    env->ReleaseStringUTFChars(input, cstr);
    parser.parse(str);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_setFilterCutoff(JNIEnv *env, jclass clazz, jfloat cutoff) {
    filter.setMaxFrequencyInOctaves(cutoff);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_setFilterQ(JNIEnv *env, jclass clazz, jfloat q) {
    filter.setRes(q);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_startRecord(JNIEnv *env, jclass clazz) {
    recorder.start();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_stopRecord(JNIEnv *env, jclass clazz) {
    recorder.stop();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_setRecordMode(JNIEnv *env, jclass clazz,
                                                jboolean record_mode_on) {
    recordMode = record_mode_on;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_ece420_lab3_MainActivity_playRecording(JNIEnv *env, jclass clazz) {
    recorder.play();
}