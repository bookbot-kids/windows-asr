// windows-asr.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <conio.h>
#include <vector>

#include "include/samplerate.h"
#include "include/sndfile.h"

using namespace std;

//--------------------------------  Global Variables --------------------------------
const int FORMAT_TAG = WAVE_FORMAT_PCM;	// Audio Type
const int NUMBER_OF_CHANNELS = 1;		// Number of channels
const int BIT_PER_SAMPLE = 16;			// Bit per samples
const int SAMPLE_RATE = 44100;			// Sample Rate 
const int DURATION = 5;					// Recording Duration
const double RESAMPLE_RATIO = 0.5;		// Resample rate
//------------------------------------------------------------------------------------


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

int reSampleRate(const char* inputFilename, const char* outputFilename)
{
	cout << "ReSampleing audio..." << endl;

	// Open input file
	SF_INFO inputInfo;
	SNDFILE* inputFile = sf_open(inputFilename, SFM_READ, &inputInfo);
	if (!inputFile) {
		std::cerr << "Failed to open input file" << std::endl;
		return 1;
	}

	cout << "Current Audio's sample rate is " << inputInfo.samplerate << endl;

	// Open output file
	SF_INFO outputInfo = inputInfo;
	outputInfo.samplerate = static_cast<int>(inputInfo.samplerate * 0.5);
	SNDFILE* outputFile = sf_open(outputFilename, SFM_WRITE, &outputInfo);

	if (!outputFile) {
		std::cerr << "Failed to open output file" << std::endl;
		sf_close(inputFile);
		return 1;
	}

	// Set up resampler
	SRC_STATE* resampler = src_new(SRC_SINC_BEST_QUALITY, inputInfo.channels, NULL);
	if (!resampler) {
		std::cerr << "Failed to create resampler" << std::endl;
		sf_close(inputFile);
		sf_close(outputFile);
		return 1;
	}
	src_set_ratio(resampler, RESAMPLE_RATIO);

	// Allocate input buffer
	const int inputSize = 1024;
	std::vector<float> inputData(inputSize * inputInfo.channels);

	// Allocate output buffer
	const int outputSize = static_cast<int>(inputSize * RESAMPLE_RATIO);
	std::vector<float> outputData(outputSize * inputInfo.channels);

	// Read and resample data
	sf_count_t readCount = 0;
	SRC_DATA data;
	std::memset(&data, 0, sizeof(data));
	data.data_in = inputData.data();
	data.input_frames = inputSize;
	data.data_out = outputData.data();
	data.output_frames = outputSize;
	data.src_ratio = RESAMPLE_RATIO;

	while ((readCount = sf_readf_float(inputFile, inputData.data(), inputSize)) > 0) {
		int err = src_process(resampler, &data);
		if (err != 0) {
			std::cerr << "Resampling failed" << std::endl;
			src_delete(resampler);
			sf_close(inputFile);
			sf_close(outputFile);
			return 1;
		}
		sf_writef_float(outputFile, outputData.data(), data.output_frames_gen);
	}

	// Clean up
	src_delete(resampler);
	sf_close(inputFile);
	sf_close(outputFile);

	// Checking sample rate
	inputFile = sf_open(outputFilename, SFM_READ, &inputInfo);
	if (!inputFile) {
		std::cerr << "Failed to open input file" << std::endl;
		return 1;
	}
	cout << "output Audio's sample rate is " << inputInfo.samplerate << endl;
	sf_close(inputFile);

	return 0;
}

int main()
{
	const int SAMPLE_RATE = 44100;
	const int N = SAMPLE_RATE * DURATION * BIT_PER_SAMPLE / 8;
	short int buffer[N];

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
	HWAVEIN hWaveIn;
	if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfex, 0L, 0L, WAVE_FORMAT_DIRECT) != MMSYSERR_NOERROR)
	{
		cout << "Error opening audio device!" << endl;
		return -1;
	}

	// Initialize audio header
	WAVEHDR WaveHdr;
	WaveHdr.lpData = (LPSTR)buffer;
	WaveHdr.dwBufferLength = N * NUMBER_OF_CHANNELS;
	WaveHdr.dwBytesRecorded = 0;
	WaveHdr.dwUser = 0L;
	WaveHdr.dwFlags = 0L;
	WaveHdr.dwLoops = 0L;

	// Prepare audio header
	if (waveInPrepareHeader(hWaveIn, &WaveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
	{
		cout << "Error preparing audio header!" << endl;
		return -1;
	}

	// Add audio header to queue
	if (waveInAddBuffer(hWaveIn, &WaveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
	{
		cout << "Error adding audio header to queue!" << endl;
		return -1;
	}

	// Start recording
	if (waveInStart(hWaveIn) != MMSYSERR_NOERROR)
	{
		cout << "Error starting audio recording!" << endl;
		return -1;
	}

	cout << "Recording Audio ... " << endl;

	while (waveInUnprepareHeader(hWaveIn, &WaveHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING);

	// Close audio device
	if (waveInClose(hWaveIn) != MMSYSERR_NOERROR)
	{
		cout << "Error closing audio device!" << endl;
		return -1;
	}

	// Format audio header
	WAVHEADER wh;
	memcpy(wh.RIFF, "RIFF", 4);
	memcpy(wh.WAVE, "WAVE", 4);
	memcpy(wh.fmt, "fmt ", 4);
	memcpy(wh.data, "data", 4);

	wh.siz_wf = BIT_PER_SAMPLE;
	wh.wFormatTag = WAVE_FORMAT_PCM;
	wh.nChannels = wfex.nChannels;
	wh.wBitsPerSample = wfex.wBitsPerSample;
	wh.nSamplesPerSec = SAMPLE_RATE;
	wh.nAvgBytesPerSec = SAMPLE_RATE * wh.nChannels * wh.wBitsPerSample / 8;
	wh.nBlockAlign = wh.nChannels * wh.wBitsPerSample / 8;
	wh.bytes = N * NUMBER_OF_CHANNELS + 36;
	wh.pcmbytes = N * NUMBER_OF_CHANNELS;

	// Save audio file
	HANDLE hFile = CreateFileW(L"recording.wav", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile)
	{
		DWORD dwWrite;
		WriteFile(hFile, &wh, sizeof(WAVHEADER), &dwWrite, NULL);
		WriteFile(hFile, WaveHdr.lpData, WaveHdr.dwBufferLength, &dwWrite, NULL);
		CloseHandle(hFile);
	}

	cout << "Recording Finished" << endl;

	reSampleRate("recording.wav", "resample_out.wav");

	getchar();

	return 0;
}
