#pragma once
#include <iostream>
#include <Windows.h>

wchar_t* multi_Byte_To_Wide_Char(std::string& pKey) {
	// string ת char*
	const char* pCStrKey = pKey.c_str();
	// ��һ�ε��÷���ת������ַ������ȣ�����ȷ��Ϊwchar_t*���ٶ����ڴ�ռ�
	int pSize = MultiByteToWideChar(CP_OEMCP, 0, pCStrKey, strlen(pCStrKey) + 1, NULL, 0);
	wchar_t* pWCStrKey = new wchar_t[pSize];
	// �ڶ��ε��ý����ֽ��ַ���ת����˫�ֽ��ַ���
	MultiByteToWideChar(CP_OEMCP, 0, pCStrKey, strlen(pCStrKey) + 1, pWCStrKey, pSize);
	// ��Ҫ������ʹ����wchar_t*��delete[]�ͷ��ڴ�
	return pWCStrKey;
}
