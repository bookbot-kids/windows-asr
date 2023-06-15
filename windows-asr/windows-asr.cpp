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

#include "SpeechRecognizer.h"

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")


int main()
{
	char ch;

	cout << "Welcome to our Windows-ASR" << endl;

	SpeechRecognizer recognizer;
	recognizer.initialize("MyRecording1");
		

	cout << "Press Any key to Start recording" << endl;
	ch = getchar();

	recognizer.listen();
	Sleep(2000);

	recognizer.mute();
	Sleep(1000);

	recognizer.unmute();

	cout << "Recording now ... press Any key to Stop recording" << endl;
	ch = getchar();

	recognizer.stopListening();

	return 0;
}
