
// VideoTrack.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CVideoTrackApp: 
// �йش����ʵ�֣������ VideoTrack.cpp
//

class CVideoTrackApp : public CWinApp
{
public:
	CVideoTrackApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};
extern CVideoTrackApp theApp;