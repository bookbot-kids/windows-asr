/*
* Author:    Volkodav Anton
* Date:      6/14/2023
* Copyright: Booktbot
*/

#include "SpeechRecognizer.h"
#include <thread>
 
#define FORMAT_TAG			WAVE_FORMAT_PCM								// Audio Type
#define NUMBER_OF_CHANNELS  1											// Number of channels
#define BIT_PER_SAMPLE      16											// Bit per samples
#define SAMPLE_RATE			32000										// Sample Rate  32kHz
#define TARGET_SAMPLE_RATE	16000										// Sample Rate  16kHz
#define BLOCK_ALIGN			NUMBER_OF_CHANNELS * BIT_PER_SAMPLE / 8     // Block Align
#define BYTE_PER_SECOND		SAMPLE_RATE * BLOCK_ALIGN 					// Byte per Second

#define WAVBLOCK_SIZE       12800										// 200ms.  (int)(SAMPLE_RATE * BLOCK_ALIGN * 200 / 1000)
#define SAMPLEBLOCK_SIZE    6400										// Sample rate is fixed to 16 kHz  TARGET_SAMPLE_RATE * BLOCK_ALIGN * 200 / 1000
#define RECOG_BLOCK_SIZ		3200										// SAMPLEBLOCK_SIZE / BlockAlign

static void ProcessResampleRecogThread(SpeechRecognizer * speechRecognizer)
{
	while (speechRecognizer->getRecognizerStatus() == SpeechRecognizerListen) {

		size_t maxWaveHdrListIndex = speechRecognizer->WaveHdrList.size() - 1;
		if (speechRecognizer->curRecogBockIndex < maxWaveHdrListIndex) {

			// Process the current buffer
			WAVEHDR* lastWaveHdr = speechRecognizer->WaveHdrList[speechRecognizer->curRecogBockIndex];
			if (lastWaveHdr->dwBytesRecorded != 0) {
				auto start = std::chrono::high_resolution_clock::now();

				BYTE SampleBlock[SAMPLEBLOCK_SIZE];
				int sampleCount;

				HRESULT hr;
				//cout << "Processing " << curProcessIndex << " Buffer index " << lastWaveHdr->dwBytesRecorded << endl;
				hr = speechRecognizer->Resample((BYTE*)lastWaveHdr->lpData, lastWaveHdr->dwBytesRecorded, SampleBlock, &sampleCount);
				if (hr != S_OK)
				{
					cout << "Resample failed" << endl;
				}
				else
				{
					//cout << "Recorded = " << lastWaveHdr->dwBytesRecorded << " Resampled bytes = " << sampleCount << endl;
					hr = speechRecognizer->Recognize(SampleBlock, sampleCount, speechRecognizer->curRecogBockIndex);
					if (hr != S_OK)
					{
						cout << "Recognition failed " << endl;
					}
				}

				auto end = std::chrono::high_resolution_clock::now();
				auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

				//cout << "Current Recording Buffer Index is " << WaveHdrList.size() - 1 << " and Cur Process Index is " << curProcessIndex << " processingTime is " << duration.count() << " ms" << endl;
				
				std::cout <<"index "<< speechRecognizer->curRecogBockIndex << "  Time taken by the operation: " << duration.count() << " microseconds" << endl;
				speechRecognizer->curRecogBockIndex++;
			}
		}
		else
			this_thread::sleep_for(chrono::milliseconds(1));
	}
}

static void CALLBACK RecoringWavInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	
	SpeechRecognizer* speechRecognizer = (SpeechRecognizer*)dwInstance;

	if (uMsg == MM_WIM_DATA && speechRecognizer->getRecognizerStatus() == SpeechRecognizerListen)
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

		// Insert the WaveHeader to list
		//WAVEHDR* RecordedWaveHdr = (WAVEHDR*)dwParam1;
		//speechRecognizer->WaveHdrList.push_back(RecordedWaveHdr);
		
		speechRecognizer->WaveHdrList.push_back(WaveHdr);

		cout << "index = " << speechRecognizer->WaveHdrList.size() - 1 << endl;
	}
}

SpeechRecognizer::SpeechRecognizer()
{
	speechText = "";
	recordingId = "0";

	configuration.modelDir = "";
	configuration.modelSampleRate = 16000;
	configuration.recordingDir = "";

	recognizerStatus = SpeechRecognizerNormal;
}

SpeechRecognizer::SpeechRecognizer(Configuration config)
{
	speechText = "";
	recordingId = "0";

	configuration.modelDir = config.modelDir;
	configuration.modelSampleRate = config.modelSampleRate;
	configuration.recordingDir = config.recordingDir;

	recognizerStatus = SpeechRecognizerNormal;
}

SpeechRecognizer::SpeechRecognizer(Configuration config, std::string speechText_s, std::string recordingId_s)
{
	speechText = speechText_s;
	recordingId = recordingId_s;

	configuration.modelDir = config.modelDir;
	configuration.modelSampleRate = config.modelSampleRate;
	configuration.recordingDir = config.recordingDir;

	recognizerStatus = SpeechRecognizerNormal;
}

SpeechRecognizer::~SpeechRecognizer()
{
	recognizerStatus = SpeechRecognizerNormal;
	release();
}

HRESULT
SpeechRecognizer::initialize(std::string recordingId_s)
{
	recordingId = recordingId_s;

	HRESULT hr = S_OK;
	
	// Core initialize
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (hr != S_OK) {
		cout << "Failed to CoInitializeEx()" << endl;
		return hr;
	}
	hr = MFStartup(MF_VERSION);
	if (hr != S_OK) {
		cout << "Failed to MFStartup()" << endl;
		return hr;
	}
	hr = InitializeResample();
	if (hr != S_OK) {
		cout << "Failed to InitializeResample()" << endl;
		return hr;
	}
	hr = InitializeRecognition();
	if (hr != S_OK) {
		cout << "Failed to InitializeRecognition()" << endl;
		return hr;
	}

	//thread processSampleRecogThread(ProcessResampleRecogThread, this);

	cout << "Initialize Library Done" << endl;
	recognizerStatus = SpeechRecognizerNormal;

	return hr;
}

HRESULT 
SpeechRecognizer::listen()
{
	if (recognizerStatus == SpeechRecognizerListen)
	{
		cout << "Recognizer is already listening";
		return S_FALSE;
	}

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
	if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfex, (DWORD_PTR)RecoringWavInProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
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

	WaveHdrList.push_back(WaveHdr);

	recognizerStatus = SpeechRecognizerListen;

	// Start listening
	if (waveInStart(hWaveIn) != MMSYSERR_NOERROR)
	{
		cout << "Error starting audio recording!" << endl;
		return S_FALSE;
	}
	

	return S_OK;
}

HRESULT
SpeechRecognizer::stopListening()
{
	if (!(recognizerStatus == SpeechRecognizerListen || recognizerStatus == SpeechRecognizerMute))
	{
		cout << "Please listen to the audio first" << endl;
		return S_FALSE;
	}
	
	recognizerStatus = SpeechRecognizerNormal;
	if (waveInStop(hWaveIn) != MMSYSERR_NOERROR)
	{
		std::cout << "Failed to stop recording" << std::endl;
		return 1;
	}

	for (int i = 0; i < WaveHdrList.size(); i++)
	{
		MMRESULT hr;

		hr = waveInUnprepareHeader(hWaveIn, WaveHdrList[i], sizeof(WAVEHDR));
		if (hr != MMSYSERR_NOERROR)
		{
			std::cout << "Failed to unprepareHeader code is " << hr << " index is " << i << " recorded = " << WaveHdrList[i]->dwBytesRecorded << std::endl;
		}
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
	wh.pcmbytes = WaveHdrList.size() * WAVBLOCK_SIZE;
	wh.bytes = wh.pcmbytes + 36;

	// Save audio file
	HANDLE hFile = CreateFileW(L"new_recording.wav", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile)
	{
		DWORD dwWrite;
		WriteFile(hFile, &wh, sizeof(WavHeader), &dwWrite, NULL);
		for (int i = 0; i < WaveHdrList.size(); i++)
		{
			WriteFile(hFile, WaveHdrList[i]->lpData, WaveHdrList[i]->dwBufferLength, &dwWrite, NULL);
		}
		CloseHandle(hFile);
	}

	return S_OK;
}

void
SpeechRecognizer::mute()
{

}

void
SpeechRecognizer::unmute()
{

}

void
SpeechRecognizer::release()
{

	MFShutdown();
	CoUninitialize();
}

void
SpeechRecognizer::resetSpeech()
{

}

void
SpeechRecognizer::flushSpeech(std::string speechText)
{

}

void
SpeechRecognizer::addListener(SpeechRecognizerCallbackFunc listener)
{

}

void
SpeechRecognizer::removeListener(SpeechRecognizerCallbackFunc listener)
{

}

void
SpeechRecognizer::removeAllListeners()
{

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
SpeechRecognizer::FinializeResample()
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

	SherpaNcnnRecognizerConfig config;
	config.model_config.tokens = "tokens.txt";
	config.model_config.encoder_param = "encoder_jit_trace-pnnx.ncnn.param";
	config.model_config.encoder_bin = "encoder_jit_trace-pnnx.ncnn.bin";
	config.model_config.decoder_param = "decoder_jit_trace-pnnx.ncnn.param";
	config.model_config.decoder_bin = "decoder_jit_trace-pnnx.ncnn.bin";
	config.model_config.joiner_param = "joiner_jit_trace-pnnx.ncnn.param";
	config.model_config.joiner_bin = "joiner_jit_trace-pnnx.ncnn.bin";
	config.model_config.num_threads = 4;
	config.model_config.use_vulkan_compute = 0;
	config.decoder_config.decoding_method = "greedy_search"; // greedy_search
	config.decoder_config.num_active_paths = 4;
	config.enable_endpoint = 1;
	config.rule1_min_trailing_silence = 2.4f;
	config.rule2_min_trailing_silence = 1.2f;
	config.rule3_min_utterance_length = 300.0f;

	config.feat_config.sampling_rate = 16000.0;
	config.feat_config.feature_dim = 80;

	sherpaRecognizer = CreateRecognizer(&config);
	sherpaStream = CreateStream(sherpaRecognizer);
	sherapDisplay = CreateDisplay(100);

	return S_OK;
}

HRESULT 
SpeechRecognizer::Recognize(BYTE* sampledBytes, int nBytes, int index)
{
	float samples[RECOG_BLOCK_SIZ];

	int32_t segment_id = -1;
	int nSamples = 0;

	for (int i = 0; i < nBytes - 1; i += 2, nSamples++) {
		samples[nSamples] = ((sampledBytes[i + 1] << 8) + sampledBytes[i]) / 32768.0f;
	}

	AcceptWaveform(sherpaStream, 16000, samples, nSamples);

	while (IsReady(sherpaRecognizer, sherpaStream)) {
		Decode(sherpaRecognizer, sherpaStream);
	}

	SherpaNcnnResult* r = GetResult(sherpaRecognizer, sherpaStream);
	if (strlen(r->text)) {
		//cout << "buffer index : " << index << "  ";
		SherpaNcnnPrint(sherapDisplay, segment_id, r->text);
	}
	DestroyResult(r);

	return S_OK;
}

HRESULT 
SpeechRecognizer::FinializeRecognition()
{
	DestroyDisplay(sherapDisplay);
	DestroyStream(sherpaStream);
	DestroyRecognizer(sherpaRecognizer);
	return S_OK;
}



SpeechRecognizerStatus 
SpeechRecognizer::getRecognizerStatus()
{
	return recognizerStatus;
}