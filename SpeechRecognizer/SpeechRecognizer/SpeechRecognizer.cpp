// SpeechRecognizer.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "SpeechRecognizer.h"
#include <thread>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>



#define FORMAT_TAG          WAVE_FORMAT_PCM                             // Audio Type
#define NUMBER_OF_CHANNELS  1                                           // Number of channels
#define BIT_PER_SAMPLE      16                                          // Bit per samples
#define SAMPLE_RATE         48000                                       // Sample Rate  44100kHz
#define TARGET_SAMPLE_RATE  16000                                       // Sample Rate  16kHz
#define BLOCK_ALIGN         NUMBER_OF_CHANNELS * BIT_PER_SAMPLE / 8     // Block Align
#define BYTE_PER_SECOND     SAMPLE_RATE * BLOCK_ALIGN                   // Byte per Second

#define WAVBLOCK_SIZE       19200                                       // 200ms.  (int)(SAMPLE_RATE * BLOCK_ALIGN * 400 / 1000) 19200
#define SAMPLEBLOCK_SIZE    6400                                        // Sample rate is fixed to 16 kHz  TARGET_SAMPLE_RATE * BLOCK_ALIGN * 400 / 1000
#define RECOG_BLOCK_SIZ     3200                                        // SAMPLEBLOCK_SIZE / BlockAlign

// This is an example of an exported variable
SPEECHRECOGNIZER_API int nSpeechRecognizer=0;

// This is an example of an exported function.
SPEECHRECOGNIZER_API int fnSpeechRecognizer(void)
{
    return 0;
}


// This is the constructor of a class that has been exported.

SpeechRecognizer::SpeechRecognizer()
{
    speechText = "";
    recordingId = "0";
    recordingPath = "";

    configuration.modelDir = "";
    configuration.modelSampleRate = 16000;
    configuration.recordingDir = "";
    configuration.recordSherpaAudio = false;
    configuration.decodeMethod = "greedy_search";
    configuration.resultMode = "text";

    recognizerStatus = SpeechRecognizerStart;
}

SpeechRecognizer::SpeechRecognizer(Configuration config)
{
    speechText = "";
    recordingId = "0";
    recordingPath = "";

    configuration.modelDir = config.modelDir;
    configuration.modelSampleRate = config.modelSampleRate;
    configuration.recordingDir = config.recordingDir;
    configuration.recordSherpaAudio = config.recordSherpaAudio;
    configuration.decodeMethod = config.decodeMethod;
    configuration.resultMode = config.resultMode;

    recognizerStatus = SpeechRecognizerStart;
}

SpeechRecognizer::SpeechRecognizer(Configuration config, std::string speechText_s, std::string recordingId_s)
{
    speechText = speechText_s;
    recordingId = recordingId_s;
    recordingPath = "";

    configuration.modelDir = config.modelDir;
    configuration.modelSampleRate = config.modelSampleRate;
    configuration.recordingDir = config.recordingDir;
    configuration.recordSherpaAudio = config.recordSherpaAudio;
    configuration.decodeMethod = config.decodeMethod;
    configuration.resultMode = config.resultMode;

    recognizerStatus = SpeechRecognizerStart;
}

SpeechRecognizer::~SpeechRecognizer()
{
    if (recognizerStatus == SpeechRecognizerStart)
        return;
    if (recognizerStatus != SpeechRecognizerRelease)
        release();
}

HRESULT
SpeechRecognizer::initialize(std::string recordingId_s, std::string recordingPath_s)
{
    recordingId = recordingId_s;
    recordingPath = recordingPath_s;

    HRESULT hr = S_OK;

    // Core initialize
    /*hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr != S_OK) {
        cout << "Failed to CoInitializeEx()" << endl;
        return hr;
    }*/
    hr = MFStartup(MF_VERSION);
    if (hr != S_OK) {
        cout << "Failed to MFStartup()" << endl;
        return hr;
    }

    hr = InitializeRecognition();
    if (hr != S_OK) {
        cout << "Failed to InitializeRecognition()" << endl;
        return hr;
    }


    hr = InitializeResample();
    if (hr != S_OK) {
        cout << "Failed to InitializeResample()" << endl;
        return hr;
    }

    WaveHdrList.clear();
    WaveHdrSherpaList.clear();

    cout << "Initialize Library Done" << endl;
    recognizerStatus = SpeechRecognizerNormal;

    return hr;
}

static void CALLBACK RecordingWavInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (uMsg == WIM_DATA)
    {
        SpeechRecognizer* speechRecognizer = (SpeechRecognizer*)dwInstance;
        WAVEHDR* RecordedWaveHdr = (WAVEHDR*)dwParam1;
        speechRecognizer->WaveHdrList.push_back(RecordedWaveHdr);

        if (speechRecognizer->getRecognizerStatus() == SpeechRecognizerListen)
        {
            // Create new buffer for recording
            WAVEHDR* WaveHdr = new WAVEHDR;
            BYTE* buffer = new BYTE[WAVBLOCK_SIZE];

            WaveHdr->lpData = (LPSTR)buffer;
            WaveHdr->dwBufferLength = WAVBLOCK_SIZE * NUMBER_OF_CHANNELS;
            WaveHdr->dwBytesRecorded = 0;
            WaveHdr->dwUser = 0L;
            WaveHdr->dwFlags = 0L;
            WaveHdr->dwLoops = 0L;

            waveInPrepareHeader(speechRecognizer->hWaveIn, WaveHdr, sizeof(WAVEHDR));
            waveInAddBuffer(speechRecognizer->hWaveIn, WaveHdr, sizeof(WAVEHDR));

            auto start = std::chrono::high_resolution_clock::now();

            int8_t sampleBytes[SAMPLEBLOCK_SIZE];
            int nBytes;

            speechRecognizer->Resample((BYTE*)RecordedWaveHdr->lpData, RecordedWaveHdr->dwBytesRecorded, (BYTE*)sampleBytes, &nBytes);
#if 0
            HRESULT hr;
            hr = speechRecognizer->Recognize(sampleBytes, nBytes, speechRecognizer->curRecogBockIndex);
            if (hr != S_OK)
            {
                cout << "Recognition failed " << endl;
            }
#endif
            
            WAVEHDR* WaveHdrSerpa = new WAVEHDR;
            BYTE* sherpaBuffer = new BYTE[nBytes];
            memcpy(sherpaBuffer, sampleBytes, nBytes);
            WaveHdrSerpa->lpData = (LPSTR)sherpaBuffer;
            WaveHdrSerpa->dwBufferLength = nBytes;
            WaveHdrSerpa->dwBytesRecorded = nBytes;
            WaveHdrSerpa->dwUser = 0L;
            WaveHdrSerpa->dwFlags = 0L;
            WaveHdrSerpa->dwLoops = 0L;
            speechRecognizer->WaveHdrSherpaList.push_back(WaveHdrSerpa);

                
            if (speechRecognizer->WaveHdrSherpaList.size() == 1)
            {
                speechRecognizer->recogIt = speechRecognizer->WaveHdrSherpaList.begin();
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            //std::cout << "index " << speechRecognizer->curRecogBockIndex << "  Time taken by the operation: " << duration.count() << " ms" << endl;
            speechRecognizer->curRecogBockIndex++;

        }
    }
}

HRESULT
SpeechRecognizer::listen()
{
    if (recognizerStatus == SpeechRecognizerStart)
    {
        cout << "Initialize the library first" << endl;
        return S_FALSE;
    }
    if (recognizerStatus == SpeechRecognizerListen)
    {
        cout << "Recognizer is already listening";
        return S_FALSE;
    }

    resetSpeech();

    // Configure wave format
    WAVEFORMATEX wfex;
    wfex.wFormatTag = FORMAT_TAG;
    wfex.nChannels = NUMBER_OF_CHANNELS;
    wfex.wBitsPerSample = BIT_PER_SAMPLE;
    wfex.nSamplesPerSec = SAMPLE_RATE;
    wfex.nAvgBytesPerSec = SAMPLE_RATE * wfex.nChannels * wfex.wBitsPerSample / 8;
    wfex.nBlockAlign = wfex.nChannels * wfex.wBitsPerSample / 8;
    wfex.cbSize = 0;

    // Open audio device
    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfex, (DWORD_PTR)RecordingWavInProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
    {
        cout << "Error opening audio device!" << endl;
        return S_FALSE;
    }

    // Initialize audio header

    WAVEHDR* WaveHdr = new WAVEHDR;
    BYTE* buffer = new BYTE[WAVBLOCK_SIZE];

    WaveHdr->lpData = (LPSTR)buffer;
    WaveHdr->dwBufferLength = WAVBLOCK_SIZE * NUMBER_OF_CHANNELS;
    WaveHdr->dwBytesRecorded = 0;
    WaveHdr->dwUser = 0L;
    WaveHdr->dwFlags = 0L;
    WaveHdr->dwLoops = 0L;

    // Prepare audio header
    if (waveInPrepareHeader(hWaveIn, WaveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
    {
        cout << "Error preparing audio header!" << endl;
        return S_FALSE;
    }

    // Add audio header to queue
    if (waveInAddBuffer(hWaveIn, WaveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
    {
        cout << "Error adding audio header to queue!" << endl;
        return S_FALSE;
    }

    recognizerStatus = SpeechRecognizerListen;

    // Start listening
    if (waveInStart(hWaveIn) != MMSYSERR_NOERROR)
    {
        cout << "Error starting audio recording!" << endl;
        return S_FALSE;
    }

    WaveHdrList.clear();
    WaveHdrSherpaList.clear();
    recogIt = WaveHdrSherpaList.begin();

    recogThread = new std::thread(&SpeechRecognizer::ProcessResampleRecogThread, this);
    return S_OK;
}

LPCWSTR ConvertToLPCWSTR(const std::string& str)
{
    int numChars = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (numChars == 0)
    {
        // Failed to get the number of characters needed
        return nullptr;
    }

    wchar_t* wideStr = new wchar_t[numChars];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr, numChars);

    return wideStr;
}

HRESULT
SpeechRecognizer::stopListening()
{
    if (recognizerStatus == SpeechRecognizerStart)
    {
        cout << "Initialize the library first" << endl;
        return S_FALSE;
    }

    if (!(recognizerStatus == SpeechRecognizerListen || recognizerStatus == SpeechRecognizerMute))
    {
        cout << "Please listen to the audio first" << endl;
        return S_FALSE;
    }

    recognizerStatus = SpeechRecognizerStopListen;
    //recogThread.detach();

    if (waveInStop(hWaveIn) != MMSYSERR_NOERROR)
    {
        std::cout << "Failed to stop recording" << std::endl;
        return 1;
    }

    int totlRecordSize = 0;

    int i = 0;
    for (auto it = WaveHdrList.begin(); it != WaveHdrList.end(); ++it, i++) {
        WAVEHDR* wavHdr = (WAVEHDR*)*it;
        MMRESULT hr;

        hr = waveInUnprepareHeader(hWaveIn, wavHdr, sizeof(WAVEHDR));
        if (hr != MMSYSERR_NOERROR)
        {
            std::cout << "Failed to unprepareHeader code is " << hr << " index is " << i << " recorded = " << wavHdr->dwBytesRecorded << std::endl;
        }
        totlRecordSize += wavHdr->dwBytesRecorded;
    }

    // Close audio device
    if (waveInClose(hWaveIn) != MMSYSERR_NOERROR)
    {
        cout << "Error closing audio device!" << endl;
    }

    // Format audio header
    WavHeader wh;
    memcpy(wh.RIFF, "RIFF", 4);
    memcpy(wh.WAVE, "WAVE", 4);
    memcpy(wh.fmt, "fmt ", 4);
    memcpy(wh.data, "data", 4);

    wh.siz_wf = BIT_PER_SAMPLE;
    wh.wFormatTag = WAVE_FORMAT_PCM;
    wh.nChannels = NUMBER_OF_CHANNELS;
    wh.wBitsPerSample = BIT_PER_SAMPLE;
    wh.nSamplesPerSec = SAMPLE_RATE;
    wh.nAvgBytesPerSec = BYTE_PER_SECOND;
    wh.nBlockAlign = BLOCK_ALIGN;
    wh.pcmbytes = totlRecordSize;
    wh.bytes = wh.pcmbytes + 36;

    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = now_ms.time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);

    std::string filenameSpeech = configuration.recordingDir + recordingPath + recordingId + "_" + std::to_string(value.count()) + ".txt";
    std::ofstream outfile(filenameSpeech, ios::trunc);
    outfile << speechText << std::endl;
    outfile.close();

    // Save audio file
    std::string filenameWAV = configuration.recordingDir + recordingPath + recordingId + "_" + std::to_string(value.count()) + ".wav";
    std::ofstream outfileaac(filenameWAV, ios::binary | ios::trunc);

    outfileaac << std::string((const char*)&wh, (const char*)&wh + sizeof(WavHeader));
    for (auto it = WaveHdrList.begin(); it != WaveHdrList.end(); ++it) {
        WAVEHDR* wavHdr = (WAVEHDR*)*it;
        outfileaac << std::string((const char*)wavHdr->lpData, (const char*)wavHdr->lpData + wavHdr->dwBytesRecorded);

        if (wavHdr->lpData)
            delete wavHdr->lpData;
    }
    WaveHdrList.clear();

    outfileaac.close();


    std::string filenameAAC = configuration.recordingDir + recordingPath + recordingId + "_" + std::to_string(value.count()) + ".aac";

    ConvertWavToAac(ConvertToLPCWSTR(filenameWAV), ConvertToLPCWSTR(filenameAAC));

    if (configuration.recordSherpaAudio)
    {
        std::string filenameSherpa = configuration.recordingDir + recordingPath + recordingId + "_sherpa_" + std::to_string(value.count()) + ".wav";
        totlRecordSize = 0;
        for (auto it = WaveHdrSherpaList.begin(); it != WaveHdrSherpaList.end(); ++it) {
            WAVEHDR* wavHdr = (WAVEHDR*)*it;
            totlRecordSize += wavHdr->dwBytesRecorded;
        }

        wh.nSamplesPerSec = TARGET_SAMPLE_RATE;
        wh.pcmbytes = totlRecordSize;
        wh.bytes = wh.pcmbytes + 36;

        std::ofstream outfilesherpa(filenameSherpa, ios::binary | ios::trunc);

        outfilesherpa << std::string((const char*)&wh, (const char*)&wh + sizeof(WavHeader));
        for (auto it = WaveHdrSherpaList.begin(); it != WaveHdrSherpaList.end(); ++it) {
            WAVEHDR* wavHdr = (WAVEHDR*)*it;
            outfilesherpa << std::string((const char*)wavHdr->lpData, (const char*)wavHdr->lpData + wavHdr->dwBytesRecorded);

            if (wavHdr->lpData)
                delete wavHdr->lpData;
        }
        WaveHdrSherpaList.clear();
        outfilesherpa.close();
    }

    if (recogThread) {
        recogThread->join();
        delete recogThread;
    }

    return S_OK;
}

HRESULT
SpeechRecognizer::mute()
{
    if (recognizerStatus == SpeechRecognizerStart)
    {
        cout << "Initialize the library first" << endl;
        return S_FALSE;
    }

    if (recognizerStatus != SpeechRecognizerListen)
    {
        std::cout << "Please listen to audio first" << std::endl;
        return S_OK;
    }

    recognizerStatus = SpeechRecognizerMute;

    if (waveInStop(hWaveIn) != MMSYSERR_NOERROR)
    {
        std::cout << "Failed to mute" << std::endl;
        return S_FALSE;
    }

    cout << "the status changed to Mute" << endl;

    return S_OK;
}

HRESULT
SpeechRecognizer::unmute()
{
    if (recognizerStatus == SpeechRecognizerStart)
    {
        cout << "Initialize the library first" << endl;
        return S_FALSE;
    }
    if (recognizerStatus != SpeechRecognizerMute)
    {
        std::cout << "Please mute the audio first" << std::endl;
        return S_OK;
    }

    recognizerStatus = SpeechRecognizerListen;

    // Create new buffer for recording
    WAVEHDR* WaveHdr = new WAVEHDR;
    BYTE* buffer = new BYTE[WAVBLOCK_SIZE];

    WaveHdr->lpData = (LPSTR)buffer;
    WaveHdr->dwBufferLength = WAVBLOCK_SIZE * NUMBER_OF_CHANNELS;
    WaveHdr->dwBytesRecorded = 0;
    WaveHdr->dwUser = 0L;
    WaveHdr->dwFlags = 0L;
    WaveHdr->dwLoops = 0L;

    waveInPrepareHeader(hWaveIn, WaveHdr, sizeof(WAVEHDR));
    waveInAddBuffer(hWaveIn, WaveHdr, sizeof(WAVEHDR));

    // Start listening
    if (waveInStart(hWaveIn) != MMSYSERR_NOERROR)
    {
        cout << "Error to unmute!" << endl;
        return S_FALSE;
    }
    return S_OK;
}

void
SpeechRecognizer::release()
{
    if (recognizerStatus == SpeechRecognizerStart)
    {
        cout << "Initialize the library first" << endl;
        return;
    }

    if (recognizerStatus == SpeechRecognizerListen)
    {
        stopListening();
    }

    recognizerStatus = SpeechRecognizerRelease;

    MFShutdown();
    //CoUninitialize();

    FinalizeResample();
    FinializeRecognition();

    removeAllListeners();
}

void
SpeechRecognizer::resetSpeech()
{
    //DestroyStream(sherpaStream);
    //DestroyRecognizer(sherpaRecognizer);
    //InitializeRecognition();

    Reset(sherpaRecognizer, sherpaStream);

}

void
SpeechRecognizer::flushSpeech(std::string speechText_s)
{
    speechText = speechText_s;
}

template<typename T, typename... U>
size_t getAddress(std::function<T(U...)> f) {
    typedef T(fnType)(U...);
    fnType** fnPointer = f.template target<fnType*>();
    return (size_t)*fnPointer;
}

void
SpeechRecognizer::addListener(const std::function<void(const std::string&, bool, bool)>& listener)
{
    recogCallbackList.push_back(listener);
}

void SpeechRecognizer::addLevelListener(const std::function<void(float)>& listener) 
{
    levelCallbackList.push_back(listener);
}

// Remove a callback level
void SpeechRecognizer::removeLevelListener(const std::function<void(float)>& listener)
{
    for (auto it = levelCallbackList.begin(); it != levelCallbackList.end(); it++) {
        if (getAddress(*it) == getAddress(listener)) {
            it = levelCallbackList.erase(it);
        }
        else {
            ++it;
        }
    }
}

// Clear all levels listeners
void SpeechRecognizer::removeAllLevelListeners() 
{
    levelCallbackList.clear();
}


void
SpeechRecognizer::removeListener(const std::function<void(const std::string&, bool, bool)>& listener)
{
    for (auto it = recogCallbackList.begin(); it != recogCallbackList.end(); it++) {
        if (getAddress(*it) == getAddress(listener)) {
            it = recogCallbackList.erase(it);
        }
        else {
            ++it;
        }
    }
}

void
SpeechRecognizer::removeAllListeners()
{
    recogCallbackList.clear();
}

HRESULT
SpeechRecognizer::InitializeResample()
{
    HRESULT hr = S_OK;

    cout << "Initializing Resample Library" << endl;

    // Setup Input Media Type 
    hr = MFCreateMediaType(&pInputType);
    hr = pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    hr = pInputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    hr = pInputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, NUMBER_OF_CHANNELS);
    hr = pInputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, SAMPLE_RATE);
    hr = pInputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, BLOCK_ALIGN);
    hr = pInputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, BYTE_PER_SECOND);
    hr = pInputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BIT_PER_SAMPLE);
    hr = pInputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
    hr = pInputType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);

    // Setup Output Media Type
    hr = MFCreateMediaType(&pOutputType);
    hr = pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    hr = pOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, NUMBER_OF_CHANNELS);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, TARGET_SAMPLE_RATE);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, BLOCK_ALIGN);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, BYTE_PER_SECOND);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BIT_PER_SAMPLE);
    hr = pOutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
    hr = pInputType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);

    // Setup the translation.
    WWMFPcmFormat inputFormat;

    inputFormat.sampleFormat = WWMFBitFormatInt;
    inputFormat.nChannels = NUMBER_OF_CHANNELS;
    inputFormat.sampleRate = SAMPLE_RATE;
    inputFormat.bits = BIT_PER_SAMPLE;
    inputFormat.validBitsPerSample = BIT_PER_SAMPLE;

    // System mix format.
    WWMFPcmFormat outputFormat;
    outputFormat.sampleFormat = WWMFBitFormatInt;
    outputFormat.nChannels = NUMBER_OF_CHANNELS;
    outputFormat.sampleRate = TARGET_SAMPLE_RATE;
    outputFormat.bits = BIT_PER_SAMPLE;
    outputFormat.validBitsPerSample = BIT_PER_SAMPLE;

    if (iResampler.Initialize(inputFormat, outputFormat, 60) == S_OK)
    {
        cout << "Initialize OK" << endl;
        return S_OK;
    }
    else
    {
        cout << "Initialize Failed" << endl;
        return S_FALSE;
    }
}

HRESULT
SpeechRecognizer::Resample(BYTE* Block, int nBytes, BYTE* SampleBlock, int* nSampleBytes)
{
    if (Block == NULL || SampleBlock == NULL || nSampleBytes == NULL)
        return S_FALSE;

    //cout << "Resampling the buffer" << endl;
    HRESULT hr;
    WWMFSampleData sampleData_return;
    hr = iResampler.Resample(Block, nBytes, &sampleData_return);
    if (hr != S_OK)
        return S_FALSE;

    *nSampleBytes = sampleData_return.bytes;
    memcpy_s(SampleBlock, sampleData_return.bytes, sampleData_return.data, sampleData_return.bytes);
    sampleData_return.Release();

    return S_OK;
}

HRESULT
SpeechRecognizer::FinalizeResample()
{
    cout << "Finalizing Resample Library" << endl;
    iResampler.Finalize();
    if (pInputType)
    {
        pInputType->Release();
    }
    if (pOutputType)
    {
        pOutputType->Release();
    }

    return S_OK;
}

HRESULT
SpeechRecognizer::InitializeRecognition()
{
    cout << "Initializing Recognition audio library" << endl;

    sherpaConfig.tokens = configuration.modelDir + "tokens.txt";
    sherpaConfig.encoder_param = configuration.modelDir + "encoder_jit_trace-pnnx.ncnn.param";
    sherpaConfig.encoder_bin = configuration.modelDir + "encoder_jit_trace-pnnx.ncnn.bin";
    sherpaConfig.decoder_param = configuration.modelDir + "decoder_jit_trace-pnnx.ncnn.param";
    sherpaConfig.decoder_bin = configuration.modelDir + "decoder_jit_trace-pnnx.ncnn.bin";
    sherpaConfig.joiner_param = configuration.modelDir + "joiner_jit_trace-pnnx.ncnn.param";
    sherpaConfig.joiner_bin = configuration.modelDir + "joiner_jit_trace-pnnx.ncnn.bin";
    
    if (configuration.decodeMethod.empty())
        sherpaConfig.decoding_method = "greedy_search"; // greedy_search or modified_beam_search
    else
        sherpaConfig.decoding_method = configuration.decodeMethod;

    sherpaConfig.num_threads = 4;
    sherpaConfig.use_vulkan_compute = 0;
    sherpaConfig.num_active_paths = 4;
    sherpaConfig.enable_endpoint = true;
    sherpaConfig.rule1_min_trailing_silence = 2.4f;
    sherpaConfig.rule2_min_trailing_silence = 1.2f;
    sherpaConfig.rule3_min_utterance_length = 300.0f;
    sherpaConfig.sampling_rate = (float)configuration.modelSampleRate;
    sherpaConfig.feature_dim = 80;

    config.model_config.tokens = sherpaConfig.tokens.c_str();
    config.model_config.encoder_param = sherpaConfig.encoder_param.c_str();
    config.model_config.encoder_bin = sherpaConfig.encoder_bin.c_str();
    config.model_config.decoder_param = sherpaConfig.decoder_param.c_str();
    config.model_config.decoder_bin = sherpaConfig.decoder_bin.c_str();
    config.model_config.joiner_param = sherpaConfig.joiner_param.c_str();
    config.model_config.joiner_bin = sherpaConfig.joiner_bin.c_str();
    config.decoder_config.decoding_method = sherpaConfig.decoding_method.c_str();
    config.model_config.num_threads = sherpaConfig.num_threads;
    config.model_config.use_vulkan_compute = sherpaConfig.use_vulkan_compute;
    config.decoder_config.num_active_paths = sherpaConfig.num_active_paths;
    config.enable_endpoint = sherpaConfig.enable_endpoint;
    config.rule1_min_trailing_silence = sherpaConfig.rule1_min_trailing_silence;
    config.rule2_min_trailing_silence = sherpaConfig.rule2_min_trailing_silence;
    config.rule3_min_utterance_length = sherpaConfig.rule3_min_utterance_length;
    config.feat_config.sampling_rate = sherpaConfig.sampling_rate;
    config.feat_config.feature_dim = (int32_t)sherpaConfig.feature_dim;

    sherpaRecognizer = CreateRecognizer(&config);
    sherpaStream = CreateStream(sherpaRecognizer);

    return S_OK;
}

float calculateRMS(const float* samples, int nSamples) {
    float sum = 0.0f;
    for (int i = 0; i < nSamples; i++) {
        sum += samples[i] * samples[i];
    }
    return sqrt(sum / nSamples);
}

HRESULT
SpeechRecognizer::Recognize(int8_t* sampledBytes, int nBytes, int index)
{
    if (sherpaStream == NULL || sherpaRecognizer == NULL)
        return S_FALSE;

    float samples[RECOG_BLOCK_SIZ];

    int32_t segment_id = -1;
    int nSamples = 0;

    for (int i = 0; i < nBytes; i += 2, nSamples++) {
        samples[nSamples] = ((static_cast<int16_t>(sampledBytes[i + 1]) << 8) | (uint8_t)sampledBytes[i]) / 32768.f;
    }

    float volumeLevel = calculateRMS(samples, nSamples);
    for (auto& it : levelCallbackList) {
        it(volumeLevel);
    }

    AcceptWaveform(sherpaStream, 16000, samples, nSamples);

    while (IsReady(sherpaRecognizer, sherpaStream)) {
        Decode(sherpaRecognizer, sherpaStream);
    }

    static std::string lastText;
    bool is_endpoint = IsEndpoint(sherpaRecognizer, sherpaStream);
    SherpaNcnnResult* r = GetResult(sherpaRecognizer, sherpaStream);

    std::string recogText;
    if (configuration.resultMode == "tokens") {
        auto p = r->tokens;
        auto count = r->count;
        std::ostringstream oss;
        for (int32_t i = 0; i < count; ++i) {
            if (i != 0) {
                oss << " "; // add a space between tokens
            }
            oss << p;
            p += strlen(p) + 1;
        }

        recogText = oss.str();
    }
    else {
        recogText = r->text;
    }

    if (!recogText.empty() && lastText != recogText) {
        lastText = recogText;
        std::transform(recogText.begin(), recogText.end(), recogText.begin(),
            [](auto c) { return std::tolower(c); });

        for (auto& it : recogCallbackList) {
            it(recogText, is_endpoint, false);
        }
    }

    if (is_endpoint) {
        for (auto& it : recogCallbackList) {
            it(recogText, is_endpoint, true);
        }
        resetSpeech();
    }

    DestroyResult(r);

    return S_OK;
}

HRESULT
SpeechRecognizer::FinializeRecognition()
{
    if (sherpaStream)
        DestroyStream(sherpaStream);
    if (sherpaRecognizer)
        DestroyRecognizer(sherpaRecognizer);
    sherpaStream = NULL;
    sherpaRecognizer = NULL;
    return S_OK;
}

SpeechRecognizerStatus
SpeechRecognizer::getRecognizerStatus()
{
    return recognizerStatus;
}

bool
SpeechRecognizer::isSerpaRecording()
{
    return configuration.recordSherpaAudio;
}

void
SpeechRecognizer::ProcessResampleRecogThread()
{
    while (1)
    {
        if (recognizerStatus == SpeechRecognizerListen)
        {
            HRESULT hr;
            std::list <WAVEHDR* >::iterator tempIt;
            tempIt = recogIt;

            if (recogIt._Ptr == NULL) {
                Sleep(10);
                continue;
            }

            if (*recogIt == NULL) {
                Sleep(10);
                continue;
            }

            if (++tempIt == WaveHdrSherpaList.end()) {
                Sleep(10);
                continue;
            }

            WAVEHDR* wavHdr = (WAVEHDR*)*recogIt;

            if (wavHdr->lpData == NULL) {
                Sleep(10);
                continue;
            }

            hr = Recognize((int8_t*)wavHdr->lpData, wavHdr->dwBytesRecorded, curRecogBockIndex);

            if (hr != S_OK)
            {
                cout << "Recognition failed " << endl;
            }

            recogIt++;
            curRecogBockIndex++;
        }
        else if (recognizerStatus == SpeechRecognizerRelease || recognizerStatus == SpeechRecognizerStopListen)
            break;
        else {
            Sleep(10);
            continue;
        }
    }

    cout << "Break the thread" << endl;
}

void
SpeechRecognizer::recognizeAudio(std::string audio_path, std::string output_path)
{
    const char* wav_filename = audio_path.c_str();
    FILE* fp = NULL;
    fopen_s(&fp, wav_filename, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", wav_filename);
        return;
    }

    // Assume the wave header occupies 44 bytes.
    fseek(fp, 44, SEEK_SET);

    // simulate streaming

    int8_t buffer[RECOG_BLOCK_SIZ * 2];
    float samples[RECOG_BLOCK_SIZ];

    SherpaNcnnStream* s = CreateStream(sherpaRecognizer);

    int32_t segment_id = -1;

    std::ofstream output(output_path.c_str(), ios::trunc);

    while (!feof(fp)) {
        size_t n = fread((void*)buffer, sizeof(int8_t), RECOG_BLOCK_SIZ * 2, fp);
        if (n > 0) {
            int sampleCount = 0;
            for (size_t i = 0; i < n; i += 2, sampleCount++) {
                samples[sampleCount] = ((static_cast<int16_t>(buffer[i + 1]) << 8) | (uint8_t)buffer[i]) / 32768.f;
            }

            AcceptWaveform(sherpaStream, 16000, samples, sampleCount);
            while (IsReady(sherpaRecognizer, sherpaStream)) {
                Decode(sherpaRecognizer, sherpaStream);
            }

            SherpaNcnnResult* r = GetResult(sherpaRecognizer, sherpaStream);
            if (strlen(r->text)) {
                output << r->text << endl;
            }
            DestroyResult(r);
        }
    }
    fclose(fp);
    output.close();
}


HRESULT
SpeechRecognizer::ConvertWavToAac(LPCWSTR wavFilePath, LPCWSTR aacFilePath)
{
    IMFSourceReader* pReader = NULL;
    IMFMediaType* pInputType = NULL;
    IMFMediaType* pOutputType = NULL;
    IMFSinkWriter* pWriter = NULL;

    DWORD streamIndex = 0;
    LONGLONG duration = 0;
    UINT32 inputSampleRate;
    UINT32 numChannels = 0;
    UINT32 bytepersecond = 0;
    UINT32 blockAlign = 0;
    UINT32 bitPerSample = 0;

    HRESULT hr = S_OK;

    hr = MFCreateSourceReaderFromURL(wavFilePath, NULL, &pReader);
    hr = MFCreateSinkWriterFromURL(aacFilePath, NULL, NULL, &pWriter);

    hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pInputType);
    hr = pInputType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &inputSampleRate);
    hr = pInputType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
    hr = pInputType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bytepersecond);
    hr = pInputType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign);
    hr = pInputType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitPerSample);

    hr = MFCreateMediaType(&pOutputType);
    hr = pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    hr = pOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitPerSample);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, inputSampleRate);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, numChannels);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 20000);
    hr = pOutputType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
    hr = pOutputType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
    hr = pOutputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
    hr = pOutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 0);
    hr = pOutputType->SetUINT32(MF_MT_AVG_BITRATE, 16000);

    hr = pWriter->AddStream(pOutputType, &streamIndex);
    hr = pWriter->SetInputMediaType(streamIndex, pInputType, NULL);
    hr = pWriter->BeginWriting();

    DWORD flags = 0;
    LONGLONG timestamp = 0;

    while (true)
    {
        WWMFSampleData sampleData_return;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = NULL;

        hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, &timestamp, &pSample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            break;
        }

        if (pSample)
        {
            hr = pSample->SetSampleTime(timestamp);

            if (SUCCEEDED(hr))
            {
                hr = pSample->SetSampleDuration(duration);
            }

            hr = pWriter->WriteSample(streamIndex, pSample);

            pSample->Release();
        }
    }

    hr = pWriter->SendStreamTick(streamIndex, duration);

    if (pWriter)
    {
        pWriter->Finalize();
    }

    if (pReader)
    {
        pReader->Release();
    }

    if (pInputType)
    {
        pInputType->Release();
    }

    if (pOutputType)
    {
        pOutputType->Release();
    }

    return hr;
}