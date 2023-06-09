#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <thread>
#include <functional>
#include "WWMFResampler.h"
#include "c-api.h"

using namespace std;
#pragma warning(disable: 4251)
struct Configuration {
    std::string modelDir; // model folder path
    int modelSampleRate; // default 16khz
    std::string recordingDir; // folder to save aac recording
    std::string decodeMethod; // greedy_search or modified_beam_search
    bool recordSherpaAudio; // turn on/off recording sherpa audio
};

struct SherpaConfig {
    std::string tokens;
    std::string encoder_param;
    std::string encoder_bin;
    std::string decoder_param;
    std::string decoder_bin;
    std::string joiner_param;
    std::string joiner_bin;
    std::string decoding_method;

    int num_threads;
    int use_vulkan_compute;
    int num_active_paths;
    bool enable_endpoint;
    float rule1_min_trailing_silence;
    float rule2_min_trailing_silence;
    float rule3_min_utterance_length;
    float sampling_rate;
    float feature_dim;
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
    SpeechRecognizerStart,
    SpeechRecognizerNormal,
    SpeechRecognizerListen,
    SpeechRecognizerStopListen,
    SpeechRecognizerMute,
    SpeechRecognizerRelease
};

// This class is exported from the dll
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
    HRESULT initialize(std::string recordingId, std::string recordingPath_s); // set recordingId property

    // Release all resources
    void release();

    // Reset ASR speech
    void resetSpeech();

    // flush speech 
    void flushSpeech(std::string speechText); // set speech text property

    // Add a callback to receive string value from ASR
    void addListener(const std::function<void(const std::string&, bool)>& listener);

    // Remove a callback ASR
    void removeListener(const std::function<void(const std::string&, bool)>& listener);

    // Clear all listeners
    void removeAllListeners();

    // Recognize from file
    void recognizeAudio(std::string audio_path, std::string output_path);

    // Convert wav to aac 
    HRESULT ConvertWavToAac(LPCWSTR inputFilePath, LPCWSTR outputFilePath);

private:
    SherpaConfig sherpaConfig;
    Configuration configuration;
    std::string speechText;
    std::string recordingId;
    std::string recordingPath;
    SpeechRecognizerStatus recognizerStatus;

    vector < std::function<void(const std::string&, bool)> > recogCallbackList;
private:
    // resample variables and functions
    WWMFResampler iResampler;
    IMFMediaType* pInputType;
    IMFMediaType* pOutputType;
    HRESULT InitializeResample();
    HRESULT FinalizeResample();

    // recognition variables and functions
    SherpaNcnnRecognizer* sherpaRecognizer;
    SherpaNcnnStream* sherpaStream;

    HRESULT InitializeRecognition();
    HRESULT FinializeRecognition();

    std::thread * recogThread;
    void ProcessResampleRecogThread();

public:
    SherpaNcnnRecognizerConfig config;
    int curRecogBockIndex;
    HWAVEIN hWaveIn;
    list < WAVEHDR*> WaveHdrList;
    list < WAVEHDR*> WaveHdrSherpaList;
    SpeechRecognizerStatus getRecognizerStatus();
    bool isSerpaRecording();
    HRESULT Resample(BYTE* Block, int nBytes, BYTE* SampleBlock, int* nSampleBytes);
    HRESULT Recognize(int8_t* sampledBytes, int nBytes, int index);

    std::list <WAVEHDR* >::iterator recogIt;
};

