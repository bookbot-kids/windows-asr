#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "WWMFResampler.h"
#include "c-api.h"

using namespace std;

typedef void (*SpeechRecognizerCallbackFunc)(const std::string&);

struct Configuration {
    std::string modelDir; // model folder path
    int modelSampleRate; // default 16khz
    std::string recordingDir; //folder to save aac recording
};

struct WavHeader {
    char RIFF[4];
    DWORD bytes;
    char WAVE[4];
    char fmt[4];
    int siz_wf;
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    char data[4];
    DWORD pcmbytes;
};

enum SpeechRecognizerStatus {
    SpeechRecognizerNormal,
    SpeechRecognizerListen,
    SpeechRecognizerMute
};

class SpeechRecognizer
{
public:
    // Constructor & Destructor
    SpeechRecognizer();
    SpeechRecognizer(Configuration config);
    SpeechRecognizer(Configuration config, std::string speechText, std::string recordingId);
    ~SpeechRecognizer();
    
    // Start to listen audio
    HRESULT listen();

    // Stop to listen audio
    HRESULT stopListening();

    // Pause listen
    HRESULT mute();

    // Resume listen
    HRESULT unmute();

    // Setup microphone and ASR model
    HRESULT initialize(std::string recordingId); // set recordingId property

    // Release all resources
    void release();

    // Reset ASR speech
    void resetSpeech();

    // flush speech 
    void flushSpeech(std::string speechText); // set speech text property

    // Add a callback to receive string value from ASR
    void addListener(SpeechRecognizerCallbackFunc listener);

    // Remove a callback ASR
    void removeListener(SpeechRecognizerCallbackFunc listener);

    // Clear all listeners
    void removeAllListeners();

private:
    Configuration configuration;
    std::string speechText;
    std::string recordingId;
    SpeechRecognizerStatus recognizerStatus;

private:
    // resample variables and functions
    WWMFResampler iResampler;
    IMFMediaType* pInputType;
    IMFMediaType* pOutputType;
    HRESULT InitializeResample();
    HRESULT FinializeResample();

    // recognition variables and functions
    SherpaNcnnRecognizer* sherpaRecognizer;
    SherpaNcnnStream* sherpaStream;
    SherpaNcnnDisplay* sherapDisplay;

    HRESULT InitializeRecognition();
    HRESULT FinializeRecognition();

public:
    int curRecogBockIndex;
    HWAVEIN hWaveIn;
    vector < WAVEHDR*> WaveHdrList;
    SpeechRecognizerStatus getRecognizerStatus();
    HRESULT Resample(BYTE* Block, int nBytes, BYTE* SampleBlock, int* nSampleBytes);
    HRESULT Recognize(BYTE* sampledBytes, int nBytes, int index);
};

