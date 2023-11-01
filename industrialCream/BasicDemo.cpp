#include <stdio.h>
#include <iostream>
#include <string.h>

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>

#include <MvCameraControl.h>


/*

	����ֻ���ÿ���ģʽ
	// usleep(200000);  // 1s = 1000000us   1s = 1000ms
	PixelType_Gvsp_BayerRG8 = 17301513

*/


#define INFO_STREAM( stream ) std::cout << stream << std::endl;
#define ERROR_STREAM( stream, erroeCode ) \
    std::cout << __FILE__ << " (" << __LINE__ << " line): " << \
    std::hex << std::showbase << stream << "[" << erroeCode << "]" << std::dec << std::endl;




unsigned int nFrames = 5000;

bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo) {
	if (NULL == pstMVDevInfo) {
		printf("The Pointer of pstMVDevInfo is NULL!\n");
		return false;
	}
	if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE) {
		int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
		int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
		int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
		int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

		// ch:��ӡ��ǰ���ip���û��Զ������� | en:print current ip and user defined name
		printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
		printf("CurrentIp: %d.%d.%d.%d\n", nIp1, nIp2, nIp3, nIp4);
		printf("UserDefinedName: %s\n\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
	}
	else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE) {
		printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
		printf("UserDefinedName: %s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
	}
	else {
		printf("Not support.\n");
	}

	return true;
}

int main() {
	int nRet = MV_OK;
	void* handle = nullptr;
	unsigned char *pData = nullptr;
	unsigned char *pDataForBGR = nullptr;
	
	do
	{	
		MV_CC_DEVICE_INFO_LIST stDeviceList;
		memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
	
		// ö���豸
		// enum device
		nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
		if (MV_OK != nRet) {
			printf("MV_CC_EnumDevices fail! nRet [%x]\n", nRet);
			break;
		}
	
		if (stDeviceList.nDeviceNum > 0) {
			for (int i = 0; i < stDeviceList.nDeviceNum; i++) {
				printf("[device %d]:\n", i);
				MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
				if (NULL == pDeviceInfo) {
					break;
				}
				PrintDeviceInfo(pDeviceInfo);
			}
		}
		else {
			printf("Find No Devices!\n");
			break;
		}
	
		printf("Please Intput camera index: ");
		unsigned int nIndex = 0;
		scanf_s("%d", &nIndex);
	
		if (nIndex >= stDeviceList.nDeviceNum) {
			printf("Intput error!\n");
			break;
		}
	
		// ѡ���豸���������
		// select device and create handle
		nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
		if (MV_OK != nRet) {
			printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
			break;
		}
	
		// ���豸
		// open device
		nRet = MV_CC_OpenDevice(handle, MV_ACCESS_Control);
		if (MV_OK != nRet) {
			printf("MV_CC_OpenDevice fail! nRet [%x]\n", nRet);
			break;
		}


		// ch:̽��������Ѱ���С(ֻ��GigE�����Ч)
		int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
		if (nPacketSize > 0) {
			nRet = MV_CC_SetIntValue(handle, "GevSCPSPacketSize", nPacketSize);
			if (nRet != MV_OK) {
				ERROR_STREAM("Warning: Set Packet Size fail nRet", nRet);
			}
		}
		else {
			ERROR_STREAM("Warning: Get Packet Size fail nRet", nRet);
		}

		// �����ļ����Ѿ�����
		// // ch:���ô���ģʽΪoff | en:Set trigger mode as off
		// nRet = MV_CC_SetEnumValue(handle, "TriggerMode", MV_TRIGGER_MODE_OFF);
		// if (MV_OK != nRet)
		// {
		//     printf("Set Trigger Mode fail! nRet [0x%x]\n", nRet);
		//     break;
		// }

		// ch:��ȡ���ݰ���С
		MVCC_INTVALUE stParam;
		memset(&stParam, 0, sizeof(MVCC_INTVALUE));
		nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParam);
		if (nRet != MV_OK) {
			ERROR_STREAM("Get PayloadSize fail! nRet", nRet);
			break;
		}



		// ch:��ʼȡ��
		nRet = MV_CC_StartGrabbing(handle);
		if (nRet != MV_OK) {
			ERROR_STREAM("Start Grabbing fail! nRet", nRet);
			break;
		}

		// ����һ֡��ȡͼ������,����ǰ�����ڴ�
		MV_FRAME_OUT_INFO_EX stImageInfo = { 0 };
		memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));

		// pData = (unsigned char *)malloc(sizeof(unsigned char) * stParam.nCurValue);
		pData = new unsigned char[stParam.nCurValue];

		if (pData == nullptr) {
			std::cout << __LINE__ << " line: " << "malloc failed!" << std::endl;
			break;
		}
		nRet = MV_CC_GetOneFrameTimeout(handle, pData, stParam.nCurValue, &stImageInfo, 1000);
		if (nRet != MV_OK) {
			ERROR_STREAM("No data! nRet", nRet);
			break;
		}
		unsigned int nDstBufferSize = stImageInfo.nWidth * stImageInfo.nHeight * 4 + 2048;

		// pDataForBGR = (unsigned char *)malloc(nDstBufferSize);
		pDataForBGR = new unsigned char[nDstBufferSize];

		if (pDataForBGR == nullptr) {
			std::cout << __LINE__ << " line: " << "malloc failed!" << std::endl;
			break;
		}

		cv::Mat srcImage(cv::Size(stImageInfo.nWidth, stImageInfo.nHeight), CV_8UC3, pDataForBGR);
		double t = 0.0, fps = 0.0;
		char fps_string[10];  // ���ڴ��֡�ʵ��ַ���
		
		try {
			while (nFrames--) {
				t = (double)cv::getTickCount();
				auto begin = std::chrono::high_resolution_clock::now();
				nRet = MV_CC_GetImageForBGR(handle, pDataForBGR, nDstBufferSize, &stImageInfo, 1000);
				auto end = std::chrono::high_resolution_clock::now();
				std::cout << "��ʱ: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms" << std::endl;

				if (nRet != MV_OK) {
					ERROR_STREAM("No data! nRet", nRet);
					break;
				}
				/*
					Ȼ��ʼ�����
				*/
				//std::this_thread::sleep_for(std::chrono::milliseconds(200));   // ģ������ʱ

				t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
				fps = 1.0 / t;
				sprintf_s(fps_string, "%.2f", fps);
				std::string fpsString("fps: ");
				fpsString += fps_string;
				cv::putText(srcImage, fpsString, cv::Point(10, 30), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

				cv::imshow("haiKang", srcImage);
				if ((cv::waitKey(145) & 0xFF) != 255) {    // 145ms�����֡������,��Ϊ��7֡
					break;
				}
			}
		}
		catch (...) {
			continue;
		}
		// free(pData);
		// free(pDataForBGR);
		delete[] pData;
		delete[] pDataForBGR;

		cv::destroyAllWindows();
		
		// ֹͣȡ��
		nRet = MV_CC_StopGrabbing(handle);
		if (nRet != MV_OK) {
			ERROR_STREAM("MV_CC_StopGrabbing fail! nRet", nRet);
			break;
		}
		// �ر��豸
		nRet = MV_CC_CloseDevice(handle);
		if (nRet != MV_OK) {
			ERROR_STREAM("MV_CC_CloseDevice fail! nRet", nRet);
			break;
		}

		// ���پ��
		// destroy handle
		nRet = MV_CC_DestroyHandle(handle);
		if (nRet != MV_OK) {
			ERROR_STREAM("MV_CC_DestroyHandle fail! nRet", nRet);
			break;
		}

	} while (0);

	if (nRet != MV_OK) {
		if (handle != nullptr) {
			MV_CC_DestroyHandle(handle);
			handle = nullptr;
		}
	}
	cv::destroyAllWindows();
}


