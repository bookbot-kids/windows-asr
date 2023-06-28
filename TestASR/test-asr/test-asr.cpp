// test-asr.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <iostream>

using namespace std;

#include "SpeechRecognizer.h"


void displayRecognition1(const std::string& str, bool isEndpoint)
{
	cout << " displayRecognition1 -> " << str << isEndpoint << endl;
}
void displayRecognition2(const std::string& str, bool isEndpoint)
{
	cout << " displayRecognition2 -> " << str << isEndpoint << endl;
}

int main()
{

	char ch;

	cout << "Welcome to our Windows-ASR" << endl;

	Configuration config;
	config.modelDir = "C:\\asrmodel\\";
	config.recordingDir = "C:\\recordings\\";
	config.modelSampleRate = 16000;
	config.recordSherpaAudio = true;
	//config.resultMode = "tokens";
	

	std::unique_ptr<SpeechRecognizer> recognizer = std::make_unique<SpeechRecognizer>(config);

	recognizer->initialize("MyRecording1", "");

	recognizer->addListener(displayRecognition1);
	recognizer->addListener(displayRecognition2);
	recognizer->removeListener(displayRecognition1);

	recognizer->flushSpeech("Hello! Nice to meet you");

	cout << "Press Any key to Start recording" << endl;
	ch = getchar();

	recognizer->listen();
#if 0

	Sleep(5000);

	recognizer.mute();
	Sleep(3000);

	recognizer.resetSpeech();
	recognizer.unmute();

#endif
	cout << "Recording and Recognize now ... press Any key to Stop ..." << endl;
	ch = getchar();

	recognizer->stopListening();


	//recognizer.recognizeAudio("E:\\recordings\\sample1.wav", "E:\\recordings\\");
	cout << "press Any key to Record again .... " << endl;
	ch = getchar();
	recognizer->listen();

	cout << "Recording and Recognize now ... press Any key to Stop ..." << endl;
	ch = getchar();

	recognizer->stopListening();

	cout << "press Any key to Record again .... " << endl;
	ch = getchar();
	recognizer->listen();

	cout << "Recording and Recognize now ... press Any key to Stop ..." << endl;
	ch = getchar();

	recognizer->stopListening();
}