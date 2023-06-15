// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the SPEECHRECOGNIZER_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// SPEECHRECOGNIZER_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef SPEECHRECOGNIZER_EXPORTS
#define SPEECHRECOGNIZER_API __declspec(dllexport)
#else
#define SPEECHRECOGNIZER_API __declspec(dllimport)
#endif

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <thread>
#include <functional>
#include <thread>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mftransform.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <dmo.h>
#include <evr.h>
#include <Objbase.h>
#include <assert.h>
#include <stdint.h>
#include <wmcodecdsp.h>
#include <chrono>
#include <thread>
#include <ctime>
#include <sstream>
#include <iomanip>

#include "WWMFResampler.h"
#include "c-api.h"

using namespace std;

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

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
    SpeechRecognizerMute,
    SpeechRecognizerRelease
};


// This class is exported from the dll
class SPEECHRECOGNIZER_API SpeechRecognizer
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
    void addListener(const std::function<void(const std::string&)>& listener);

    // Remove a callback ASR
    void removeListener(const std::function<void(const std::string&)>& listener);

    // Clear all listeners
    void removeAllListeners();

private:
    Configuration configuration;
    std::string speechText;
    std::string recordingId;
    SpeechRecognizerStatus recognizerStatus;

    vector < std::function<void(const std::string&)> > recogCallbackList;
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

    std::thread recogThread;
    void ProcessResampleRecogThread();

public:
    int curRecogBockIndex;
    HWAVEIN hWaveIn;
    list < WAVEHDR*> WaveHdrList;
    SpeechRecognizerStatus getRecognizerStatus();
    HRESULT Resample(BYTE* Block, int nBytes, BYTE* SampleBlock, int* nSampleBytes);
    HRESULT Recognize(BYTE* sampledBytes, int nBytes, int index);
};


extern SPEECHRECOGNIZER_API int nSpeechRecognizer;

SPEECHRECOGNIZER_API int fnSpeechRecognizer(void);