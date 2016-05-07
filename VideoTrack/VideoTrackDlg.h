
// VideoTrackDlg.h : 头文件
//

#pragma once
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/objdetect.hpp"

// CVideoTrackDlg 对话框
class CVideoTrackDlg : public CDialogEx
{
// 构造
public:
	CVideoTrackDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_VIDEOTRACK_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
//	afx_msg void Ontrack();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnOpenFile();
	afx_msg void OnOpenCamera();
	afx_msg void OnPause();
	afx_msg void OnSpeedUp();
	afx_msg void OnSpeedDown();
	afx_msg void OnPaused();
};



void onMouse(int event, int x, int y, int, void*);
void AutoDetectPeople(const cv::Mat &img);
void AutoDetectPeopleByDiff(const cv::Mat &img0, const cv::Mat& img1);