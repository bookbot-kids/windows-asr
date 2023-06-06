
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
#include "include/soxr.h"


#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mftransform.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

#include <vector>
#include <dmo.h>
#include <evr.h>
#include <atlbase.h>
#include <assert.h>
#include <stdint.h>
#include <wmcodecdsp.h>
#include "WWMFResampler.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

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

int reSampleRate_libsamplerate(const char* inputFilename, const char* outputFilename)
{
	cout << "ReSampleing audio with libsamplerate..." << endl;

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

int reSampleRate_libsoxr(const char* inputFilename, const char* outputFilename)
{
	cout << "ReSampleing audio with libsoxr..." << endl;
	// Open the input WAV file
	SF_INFO inputInfo;
	SNDFILE* inputFile = sf_open(inputFilename, SFM_READ, &inputInfo);
	if (!inputFile)
	{
		// Handle the error
		return -1;
	}

	// Get the input WAV file format
	
	sf_command(inputFile, SFC_GET_CURRENT_SF_INFO, &inputInfo, sizeof(inputInfo));

	// Calculate the output sample rate and number of samples
	const double inputSampleRate = static_cast<double>(inputInfo.samplerate);
	const double outputSampleRate = static_cast<double>(inputInfo.samplerate * RESAMPLE_RATIO);
	const size_t numSamples = static_cast<size_t>(inputInfo.frames * (outputSampleRate / inputSampleRate));

	// Allocate memory for the input and output buffers
	float* inputBuffer = new float[inputInfo.frames * inputInfo.channels];
	float* outputBuffer = new float[numSamples * inputInfo.channels];

	// Read the input WAV file data into the input buffer
	const sf_count_t numFrames = sf_readf_float(inputFile, inputBuffer, inputInfo.frames);
	if (numFrames != inputInfo.frames)
	{
		// Handle the error
		delete[] inputBuffer;
		delete[] outputBuffer;
		sf_close(inputFile);
		return -1;
	}

	// Create the libsoxr resampler
	soxr_error_t error;
	soxr_quality_spec_t quality = soxr_quality_spec(SOXR_HQ, 0);
	soxr_io_spec_t ioSpec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
	soxr_t resampler = soxr_create(inputSampleRate, outputSampleRate, inputInfo.channels, &error, &ioSpec, &quality, nullptr);
	if (error)
	{
		// Handle the error
		delete[] inputBuffer;
		delete[] outputBuffer;
		sf_close(inputFile);
		return -1;
	}

	// Resample the input buffer to the output buffer
	size_t numInputSamples = inputInfo.frames;
	size_t numOutputSamples = numSamples;
	size_t numInputProcessed = 0;
	size_t numOutputProcessed = 0;
	soxr_process(resampler, inputBuffer, numInputSamples, &numInputProcessed, outputBuffer, numOutputSamples, &numOutputProcessed);

	// Open the output WAV file
	SF_INFO outputInfo = inputInfo;
	outputInfo.samplerate = static_cast<int>(outputSampleRate);
	SNDFILE* outputFile = sf_open(outputFilename, SFM_WRITE, &outputInfo);
	if (!outputFile)
	{
		// Handle the error
		soxr_delete(resampler);
		delete[] inputBuffer;
		delete[] outputBuffer;
		sf_close(inputFile);
		return -1;
	}

	// Write the output WAV file data from the output buffer
	const sf_count_t numOutputFrames = numOutputProcessed / inputInfo.channels;
	const sf_count_t numOutputWritten = sf_writef_float(outputFile, outputBuffer, numOutputFrames);
	if (numOutputWritten != numOutputFrames)
	{
		// Handle the error
		sf_close(outputFile);
		soxr_delete(resampler);
		delete[] inputBuffer;
		delete[] outputBuffer;
		sf_close(inputFile);
		return -1;
	}

	// Close the input and output WAV files
	sf_close(outputFile);
	soxr_delete(resampler);
	delete[] inputBuffer;
	delete[] outputBuffer;
	sf_close(inputFile);

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

int reSampleRate_wmf(LPCWSTR inputFilename, LPCWSTR outputFilename)
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

	cout << "ReSampleing audio with wmf..." << endl;

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = MFStartup(MF_VERSION);

	hr = MFCreateSourceReaderFromURL(inputFilename, NULL, &pReader);
	hr = MFCreateSinkWriterFromURL(outputFilename, NULL, NULL, &pWriter);

	hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pInputType);
	hr = pInputType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &inputSampleRate);
	hr = pInputType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &numChannels);
	hr = pInputType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bytepersecond);
	hr = pInputType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign);
	hr = pInputType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitPerSample);
	UINT32 outputSampleRate = static_cast<UINT32>(inputSampleRate * RESAMPLE_RATIO);   //Sampe rate conversion ratio

	hr = MFCreateMediaType(&pOutputType);
	hr = pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	hr = pOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, numChannels);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, outputSampleRate);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytepersecond);
	hr = pOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitPerSample);
	hr = pOutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

	// Setup the translation.
	WWMFResampler iResampler;
	WWMFPcmFormat inputFormat;

	inputFormat.sampleFormat = WWMFBitFormatInt;
	inputFormat.nChannels = numChannels;
	inputFormat.sampleRate = inputSampleRate;
	inputFormat.bits = bitPerSample;
	inputFormat.validBitsPerSample = bitPerSample;

	// System mix format.
	WWMFPcmFormat outputFormat;
	outputFormat.sampleFormat = WWMFBitFormatInt;
	outputFormat.nChannels = numChannels;
	outputFormat.sampleRate = outputSampleRate;
	outputFormat.bits = bitPerSample;
	outputFormat.validBitsPerSample = bitPerSample;

	if (iResampler.Initialize(inputFormat, outputFormat, 60) == S_OK)
	{
		cout << "Initialize OK" << endl;
	}

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
		//printf("%I64d\n", timestamp);
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
			CComPtr<IMFMediaBuffer> spBuffer;
			hr = pSample->ConvertToContiguousBuffer(&spBuffer);
			DWORD cbBytes = 0;
			hr = spBuffer->GetCurrentLength(&cbBytes);

			BYTE* pByteBuffer = NULL;
			hr = spBuffer->Lock(&pByteBuffer, NULL, NULL);

			BYTE* to = new BYTE[cbBytes]; //< output PCM data
			DWORD toBytes = cbBytes;            //< output PCM data size
			memcpy(to, pByteBuffer, cbBytes);

			spBuffer->Unlock();

			iResampler.Resample(to, toBytes, &sampleData_return);
			//cout << "Original bytes = " << toBytes << endl;
			//cout << "sampleData_return.bytes : " << sampleData_return.bytes << endl;

			IMFSample* pOutputSample = NULL;

			// create a new memory buffer
			IMFMediaBuffer* pBuffer = NULL;
			hr = MFCreateMemoryBuffer(sampleData_return.bytes, &pBuffer);

			// lock the buffer and copy the data into the buffer
			BYTE* pData = NULL;
			hr = pBuffer->Lock(&pData, NULL, NULL);

			memcpy_s(pData, sampleData_return.bytes, sampleData_return.data, sampleData_return.bytes);

			hr = pBuffer->Unlock();

			// set buffer data length
			hr = pBuffer->SetCurrentLength(sampleData_return.bytes);

			// Create a Media Sample and add buffers to it
			hr = MFCreateSample(&pOutputSample);
			hr = pOutputSample->AddBuffer(pBuffer);
			hr = pOutputSample->SetSampleTime(timestamp);

			if (SUCCEEDED(hr))
			{
				hr = pOutputSample->SetSampleDuration(duration);
			}

			hr = pWriter->WriteSample(streamIndex, pOutputSample);
			pOutputSample->Release();

			pSample->Release();
}
	}

	hr = pWriter->SendStreamTick(streamIndex, duration);

	iResampler.Finalize();

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

	// Checking sample rate
	hr = MFCreateSourceReaderFromURL(outputFilename, NULL, &pReader);

	hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pInputType);
	hr = pInputType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &inputSampleRate);
	cout << "output Audio's sample rate is " << inputSampleRate << endl;
	if (pReader)
	{
		pReader->Release();
	}

	if (pInputType)
	{
		pInputType->Release();
	}

	return hr;
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

	reSampleRate_libsamplerate("recording.wav", "resample_out_libsamplrate.wav");
	
	reSampleRate_libsoxr("recording.wav", "resample_out_libsoxr.wav");

	reSampleRate_wmf(L"recording.wav", L"resample_out_wmf.wav");

	getchar();

	return 0;
}
