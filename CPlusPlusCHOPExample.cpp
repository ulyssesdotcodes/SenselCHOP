/* Shared Use License: This file is owned by Derivative Inc. (Derivative) and
 * can only be used, and/or modified for use, in conjunction with 
 * Derivative's TouchDesigner software, and only if you are a licensee who has
 * accepted Derivative's TouchDesigner license or assignment agreement (which
 * also govern the use of this file).  You may share a modified version of this
 * file with another authorized licensee of Derivative's TouchDesigner software.
 * Otherwise, no redistribution or sharing of this file, with or without
 * modification, is permitted.
 */

#include "CPlusPlusCHOPExample.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <cmath>
#include <assert.h>
#include <iostream>
#include <algorithm>

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
int32_t
GetCHOPAPIVersion(void)
{
	// Always return CHOP_CPLUSPLUS_API_VERSION in this function.
	return CHOP_CPLUSPLUS_API_VERSION;
}

DLLEXPORT
CHOP_CPlusPlusBase*
CreateCHOPInstance(const OP_NodeInfo* info)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per CHOP that is using the .dll
	return new CPlusPlusCHOPExample(info);
}

DLLEXPORT
void
DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the CHOP using that instance is deleted, or
	// if the CHOP loads a different DLL
	delete (CPlusPlusCHOPExample*)instance;
}

};


CPlusPlusCHOPExample::CPlusPlusCHOPExample(const OP_NodeInfo* info) : myNodeInfo(info)
{
	initSensel();
	lastcook = std::chrono::system_clock::now();
}

CPlusPlusCHOPExample::~CPlusPlusCHOPExample()
{
	if(frame) {
		senselFreeFrameData(handle, frame);
	}
	if (handle) {
		senselStopScanning(handle);
		senselClose(handle);
	}
}

void
CPlusPlusCHOPExample::getGeneralInfo(CHOP_GeneralInfo* ginfo)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrame = true;
	ginfo->timeslice = false;
	ginfo->inputMatchIndex = 0;
}

bool
CPlusPlusCHOPExample::getOutputInfo(CHOP_OutputInfo* info)
{
	// If there is an input connected, we are going to match it's channel names etc
	// otherwise we'll specify our own.
	info->numChannels = 2;

	// Since we are outputting a timeslice, the system will dictate
	// the numSamples and startIndex of the CHOP data
	if (handle) {
		info->numSamples = sensor_info.num_rows * sensor_info.num_cols;;
	}
	//info->numSamples = 1;
	info->startIndex = 0;

	// For illustration we are going to output 120hz data
	//info->sampleRate = 10;
	return true;
}

const char*
CPlusPlusCHOPExample::getChannelName(int32_t index, void* reserved)
{
	return "chan";
}

void
CPlusPlusCHOPExample::execute(const CHOP_Output* output,
							  OP_Inputs* inputs,
							  void* reserved)
{
	auto now = std::chrono::system_clock::now();
	if ((std::chrono::duration_cast<std::chrono::milliseconds>(now - lastcook)).count() > 2500) {
		if(frame) {
			senselFreeFrameData(handle, frame);
		}
		if (handle) {
			senselStopScanning(handle);
			senselClose(handle);
		}

		initSensel();
		lastcook = std::chrono::system_clock::now();
	}
	else if (handle) {
		unsigned int num_frames = 0;
		//Read all available data from the Sensel device
		senselReadSensor(handle);
		//Get number of frames available in the data read from the sensor
		senselGetNumAvailableFrames(handle, &num_frames);

		for (int f = 0; f < std::min<int>(num_frames, 5); f++)
		{
			//Read one frame of data
			senselGetFrame(handle, frame);
		}

		lastcook = std::chrono::system_clock::now();

		if (frame) {
			//Calculate the total force
			for (int i = 0; i < output->numSamples; i++)
			{
				output->channels[0][i] = frame->force_array[i];
			}

			int idx = 0;
			for (int i = 0; i < frame->n_contacts; i++) {
				SenselContact contact = frame->contacts[i];
				output->channels[1][idx++] = contact.x_pos;
				output->channels[1][idx++] = contact.y_pos;
				output->channels[1][idx++] = contact.total_force;
			}

			for (int i = idx; i < output->numSamples; i++) {
				output->channels[1][i] = -1;
			}
		}
	}


}

int32_t
CPlusPlusCHOPExample::getNumInfoCHOPChans()
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send one channel.
	return 1;
}

void
CPlusPlusCHOPExample::getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan* chan)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name = "sensel conected";
		chan->value = 1;
	}
}

bool		
CPlusPlusCHOPExample::getInfoDATSize(OP_InfoDATSize* infoSize)
{
	infoSize->rows = 1;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
CPlusPlusCHOPExample::getInfoDATEntries(int32_t index,
										int32_t nEntries,
										OP_InfoDATEntries* entries)
{
	// It's safe to use static buffers here because Touch will make it's own
	// copies of the strings immediately after this call returns
	// (so the buffers can be reuse for each column/row)
	static char tempBuffer1[4096];
	static char tempBuffer2[4096];
	if (index == 0)
	{
		// Set the value for the first column
#ifdef WIN32
		strcpy_s(tempBuffer1, "Sensel device");
#else // macOS
        //strlcpy(tempBuffer1, "executeCount", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

		// Set the value for the second column
#ifdef WIN32
		sprintf_s(tempBuffer2, "%s" , senseldev);
#else // macOS
		sprintf_s(tempBuffer2, "%s" , senseldev);
#endif
		entries->values[1] = tempBuffer2;
	}
}

void
CPlusPlusCHOPExample::setupParameters(OP_ParameterManager* manager)
{

}

void 
CPlusPlusCHOPExample::pulsePressed(const char* name)
{
}

void CPlusPlusCHOPExample::initSensel()
{
	//Handle that references a Sensel device
	handle = NULL;
	frame = NULL;
	//List of all available Sensel devices
    SenselDeviceList list;
	//Firmware info from the Sensel device
	SenselFirmwareInfo fw_info;
    
	//Get a list of avaialble Sensel devices 
	senselGetDeviceList(&list);
	//senselOpen(&handle);
	if (list.num_devices == 0)
	{
		std::cout << "No device found" << std::endl;
	}
	else {
		std::cout << "We have a device!\n" << std::endl;

		//Open a Sensel device by the id in the SenselDeviceList, handle initialized 
		senselOpenDeviceByID(&handle, list.devices[0].idx);
		//Get the firmware info
		senselGetFirmwareInfo(handle, &fw_info);
		//Get the sensor info
		senselGetSensorInfo(handle, &sensor_info);

		//Set the frame content to scan force data
		senselSetFrameContent(handle, FRAME_CONTENT_PRESSURE_MASK | FRAME_CONTENT_CONTACTS_MASK);
		//Allocate a frame of data, must be done before reading frame data
		senselAllocateFrameData(handle, &frame);
		//Start scanning the Sensel device
		senselStartScanning(handle);

		fprintf(stdout, "\nSensel Device: %s\n", list.devices[0].serial_num);
		fprintf(stdout, "Firmware Version: %d.%d.%d\n", fw_info.fw_version_major, fw_info.fw_version_minor, fw_info.fw_version_build);
		fprintf(stdout, "Width: %fmm\n", sensor_info.width);
		fprintf(stdout, "Height: %fmm\n", sensor_info.height);
		fprintf(stdout, "Cols: %d\n", sensor_info.num_cols);
		fprintf(stdout, "Rows: %d\n", sensor_info.num_rows);
	}
}

