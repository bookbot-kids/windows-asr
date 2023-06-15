// test-asr.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <iostream>

using namespace std;

#include "SpeechRecognizer.h"


void displayRecognition1(const std::string& str)
{
	cout << " displayRecognition1 -> " << str << endl;
}
void displayRecognition2(const std::string& str)
{
	cout << " displayRecognition2 -> " << str << endl;
}

int main()
{
	char ch;

	cout << "Welcome to our Windows-ASR" << endl;

	SpeechRecognizer recognizer;
	recognizer.initialize("MyRecording1");

	recognizer.addListener(displayRecognition1);
	recognizer.addListener(displayRecognition2);

	recognizer.removeListener(displayRecognition1);

	recognizer.flushSpeech("Hello! Nice to meet you");

	cout << "Press Any key to Start recording" << endl;
	ch = getchar();

	recognizer.listen();
	Sleep(5000);

	recognizer.mute();
	Sleep(3000);

	recognizer.resetSpeech();
	recognizer.unmute();

	cout << "Recording now ... press Any key to Stop recording" << endl;
	ch = getchar();

	recognizer.stopListening();

	recognizer.release();

}