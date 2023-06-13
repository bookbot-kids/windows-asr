/*
* Author:    Volkodav Anton
* Date:      6/8/2023
* Modified   6/9/2023
* Copyright: Booktbot
*/

#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <fstream>
#include <stdio.h>
#include <conio.h>
#include <vector>
#include <stdlib.h>
#include <string.h>
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

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#include "c-api.h"

using namespace std;

//--------------------------------  Global Variables --------------------------------
const int FORMAT_TAG = WAVE_FORMAT_PCM;							 // Audio Type
const int NUMBER_OF_CHANNELS = 1;								 // Number of channels
const int BIT_PER_SAMPLE = 16;									 // Bit per samples
const int SAMPLE_RATE = 32000;									 // Sample Rate  32kHz
const double RESAMPLE_RATIO = 0.5;								 // Resample rate
const int BLOCK_ALIGN = NUMBER_OF_CHANNELS * BIT_PER_SAMPLE / 8; // Block Align
const int BYTE_PER_SECOND = SAMPLE_RATE * BLOCK_ALIGN;			 // Byte per Second
//------------------------------------------------------------------------------------

#define WAVBLOCK_SIZE      12800     // 200ms.  (int)(SAMPLE_RATE * BLOCK_ALIGN * 200 / 1000)
#define SAMPLEBLOCK_SIZE    6400     // Data Block Size for Recognition  (int)(WAVBLOCK_SIZE * RESAMPLE_RATIO)	

//---------------------------------- Structure ---------------------------------------
typedef struct {
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
} WAVHEADER;
//-------------------------------------------------------------------------------------


WWMFResampler iResampler;
IMFMediaType* pInputType = NULL;
IMFMediaType* pOutputType = NULL;

int InitializeResample()
{
	HRESULT hr = S_OK;

	cout << "Initializing Resample Library" << endl;

	// Core initialize
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = MFStartup(MF_VERSION);

	// Setup Input Media Type 
	hr = MFCreateMediaType(&pInputType);
	hr = pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	hr = pInputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = pInputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, NUMBER_OF_CHANNELS);
	hr = pInputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, SAMPLE_RATE);
	hr = pInputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, BLOCK_ALIGN);
	hr = pInputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, BYTE_PER_SECOND);
	hr = pInputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BIT_PER_SAMPLE);
	hr = pInputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

	// Setup Output Media Type
	hr = MFCreateMediaType(&pOutputType);
	hr = pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	hr = pOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, NUMBER_OF_CHANNELS);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, SAMPLE_RATE * RESAMPLE_RATIO);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, BLOCK_ALIGN);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, BYTE_PER_SECOND);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, BIT_PER_SAMPLE);
	hr = pOutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

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
	outputFormat.sampleRate = SAMPLE_RATE * RESAMPLE_RATIO;
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

int Resample(BYTE* Block, int nBytes, BYTE* SampleBlock, int* nSampleBytes)
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

int FinializeResample()
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

	MFShutdown();
	CoUninitialize();

	return S_OK;
}


SherpaNcnnRecognizer* recognizer;
SherpaNcnnStream* s;
SherpaNcnnDisplay* display;

int InitializeRecognition()
{
	cout << "Initializing Recognition audio library" << endl;

	SherpaNcnnRecognizerConfig config;
	config.model_config.tokens = "tokens.txt";
	config.model_config.encoder_param = "encoder_jit_trace-pnnx.ncnn.param";
	config.model_config.encoder_bin = "encoder_jit_trace-pnnx.ncnn.bin";
	config.model_config.decoder_param = "decoder_jit_trace-pnnx.ncnn.param";
	config.model_config.decoder_bin = "decoder_jit_trace-pnnx.ncnn.bin";
	config.model_config.joiner_param = "joiner_jit_trace-pnnx.ncnn.param";
	config.model_config.joiner_bin = "joiner_jit_trace-pnnx.ncnn.bin";

	int32_t num_threads = 4;
	//if (argc >= 10 && atoi(argv[9]) > 0) {
	//	num_threads = atoi(argv[9]);
	//}
	config.model_config.num_threads = num_threads;
	config.model_config.use_vulkan_compute = 0;

	config.decoder_config.decoding_method = "greedy_search";

	//if (argc == 11) {
	//	config.decoder_config.decoding_method = argv[10];
	//}

	config.decoder_config.num_active_paths = 4;
	config.enable_endpoint = 1; // 0
	config.rule1_min_trailing_silence = 2.0; // 2.4
	config.rule2_min_trailing_silence = 0.8; // 1.2
	config.rule3_min_utterance_length = 20; // 300

	config.feat_config.sampling_rate = 16000.0;
	config.feat_config.feature_dim = 80;

	recognizer = CreateRecognizer(&config);
	s = CreateStream(recognizer);
	display = CreateDisplay(1000);

	return S_OK;
}

int Recognize (BYTE* sampledBytes, int nBytes, int index)
{
#define NumberSample 16000 * 200 / 1000  // 200ms. Sample rate is fixed to 16 kHz 

	float samples[NumberSample];

	//cout << " Recognize audio" << endl;

	int32_t segment_id = -1;
	int nSamples = 0;

	for (int i = 0; i < nBytes - 1 ; i += 2, nSamples ++ ) {
		samples[nSamples] = ((sampledBytes[i + 1] << 8) + sampledBytes[i]) / 32768.;
	}

	AcceptWaveform(s, 16000, samples, nSamples);
	//cout << "nSamples " << nSamples << endl;
	
	//for (int i = 0; i < 10; i++)
	//	cout << samples[i] << " ";
	//cout << endl;

	while (IsReady(recognizer, s)) {
		Decode(recognizer, s);
	}

	SherpaNcnnResult* r = GetResult(recognizer, s);
	if (strlen(r->text)) {
		cout << "buffer index : " << index << "  ";
		SherpaNcnnPrint(display, segment_id, r->text);
	}
	DestroyResult(r);

#if 0
	// add some tail padding
	float tail_paddings[4800] = { 0 };  // 0.3 seconds at 16 kHz sample rate
	AcceptWaveform(s, 16000, tail_paddings, 4800);

	InputFinished(s);

	while (IsReady(recognizer, s)) {
		Decode(recognizer, s);
	}
	SherpaNcnnResult* r = GetResult(recognizer, s);
	if (strlen(r->text)) {
		SherpaNcnnPrint(display, segment_id, r->text);
	}
#endif

	return S_OK;
}

int FinializeRecognition()
{
	DestroyDisplay(display);
	DestroyStream(s);
	DestroyRecognizer(recognizer);
	return S_OK;
}

vector < WAVEHDR*> WaveHdrList;
HWAVEIN hWaveIn;
int recordingStatus = 0;

void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg == WIM_DATA && recordingStatus == 1)
	{

		// Send the buffer back to the device
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

		// Insert the WaveHeader to list
		WaveHdrList.push_back(WaveHdr);
	}
}


int StartRecording()
{
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
	if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfex, (DWORD_PTR)waveInProc, 0L, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
	{
		cout << "Error opening audio device!" << endl;
		return -1;
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
		return -1;
	}

	// Add audio header to queue
	if (waveInAddBuffer(hWaveIn, WaveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
	{
		cout << "Error adding audio header to queue!" << endl;
		return -1;
	}

	WaveHdrList.push_back(WaveHdr);

	recordingStatus = 1;

	// Start recording
	if (waveInStart(hWaveIn) != MMSYSERR_NOERROR)
	{
		cout << "Error starting audio recording!" << endl;
		return -1;
	}

	return S_OK;
}

int StopRecording()
{
	// Stop recording
	cout << "Stop recording and save to file" << endl;
	recordingStatus = 0;

	if (waveInStop(hWaveIn) != MMSYSERR_NOERROR)
	{
		std::cout << "Failed to stop recording" << std::endl;
		return 1;
	}

	for (int i = 0; i < WaveHdrList.size(); i++)
	{
		MMRESULT hr;
		//cout << "UnprepareHeader index = " << i << endl;
		hr = waveInUnprepareHeader(hWaveIn, WaveHdrList[i], sizeof(WAVEHDR));
		if (hr != MMSYSERR_NOERROR)
		{
			std::cout << "Failed to unprepareHeader " << hr << "index is " << i << std::endl;
		}
	}

	// Close audio device
	if (waveInClose(hWaveIn) != MMSYSERR_NOERROR)
	{
		cout << "Error closing audio device!" << endl;
		//return 1;
	}

	// Format audio header
	WAVHEADER wh;
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

	wh.pcmbytes = WaveHdrList.size() * WAVBLOCK_SIZE;
	wh.bytes = wh.pcmbytes + 36;

	// Save audio file
	HANDLE hFile = CreateFileW(L"recording.wav", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile)
	{
		DWORD dwWrite;
		WriteFile(hFile, &wh, sizeof(WAVHEADER), &dwWrite, NULL);
		for (int i = 0; i < WaveHdrList.size(); i++)
		{
			WriteFile(hFile, WaveHdrList[i]->lpData, WaveHdrList[i]->dwBufferLength, &dwWrite, NULL);
		}
		CloseHandle(hFile);
	}
	return S_OK;
}

int curProcessIndex = 0;

static void ProcessResampleRecogThread() {
	
	time_t now = time(nullptr); // Get current time
	tm timeinfo;
	localtime_s(&timeinfo, &now); // Convert time to struct tm
	stringstream filename; // Create stringstream to build filename
	filename << "file_" << setfill('0') << setw(2) << timeinfo.tm_hour << "-"
		<< setfill('0') << setw(2) << timeinfo.tm_min << "-"
		<< setfill('0') << setw(2) << timeinfo.tm_sec << ".csv";
	ofstream outfile(filename.str()); // Open file for writing

	outfile << "CurProcessingBufferIndex, CurRecordingBurfferIndex, ProcessingTime(microsecond)" << endl;

	while (recordingStatus) {

		int maxWaveHdrListIndex = WaveHdrList.size() - 1;
		if (curProcessIndex <= maxWaveHdrListIndex) {

			// Process the current buffer
			WAVEHDR* lastWaveHdr = WaveHdrList[curProcessIndex];
			if (lastWaveHdr->dwBytesRecorded != 0) {
				auto start = std::chrono::high_resolution_clock::now();

				BYTE SampleBlock[SAMPLEBLOCK_SIZE];
				int sampleCount;

				HRESULT hr;
				//cout << "Processing " << curProcessIndex << " Buffer index " << lastWaveHdr->dwBytesRecorded << endl;
				hr = Resample((BYTE*)lastWaveHdr->lpData, lastWaveHdr->dwBytesRecorded, SampleBlock, &sampleCount);
				if (hr != S_OK)
				{
					cout << "Resample failed" << endl;
				}
				else
				{
					//cout << "Recorded = " << lastWaveHdr->dwBytesRecorded << " Resampled bytes = " << sampleCount << endl;

					hr = Recognize(SampleBlock, sampleCount, curProcessIndex);
					if (hr != S_OK)
					{
						cout << "Recognition failed " << endl;
					}

				}

				//std::cout << "curProcessIndex is " << curProcessIndex << "and recorded buffer index is " << WaveHdrList.size() - 1 << std::endl;

				auto end = std::chrono::high_resolution_clock::now();
				auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

				//cout << "Current Recording Buffer Index is " << WaveHdrList.size() - 1 << " and Cur Process Index is " << curProcessIndex << " processingTime is " << duration.count() << " ms" << endl;
				//std::cout <<"index "<< curProcessIndex << "  Time taken by the operation: " << duration.count() << " microseconds" << endl;
				outfile << curProcessIndex << ", " << WaveHdrList.size() - 1 <<", " << duration.count() << endl;
				curProcessIndex++;
			}
		} else
			this_thread::sleep_for(chrono::milliseconds(1));
	}

	outfile.close();
}

int main()
{
	char ch;

	cout << "Welcome to our Windows-ASR" << endl;

	InitializeResample();
	InitializeRecognition();

	cout << "Press Any key to start recording ... " << endl;

	ch = getchar();

	StartRecording();
	thread processSampleRecogThread(ProcessResampleRecogThread);
	
	// Wait for user input to stop recording
	std::cout << "Recording and Recognizing ... wait for a second..." << endl;
	cout << "Press any key to stop" << std::endl;

	std::cin.get();

	StopRecording();

	processSampleRecogThread.join();

	cout << "Recording Finished" << endl << endl;
	ch = getchar();

	return 0;
}
