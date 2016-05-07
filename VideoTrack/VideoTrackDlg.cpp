
// VideoTrackDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "VideoTrack.h"
#include "VideoTrackDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace cv;
using namespace std;

// CVideoTrackDlg �Ի���


cv::Mat image;                                    //������Ƶ����ʱ��ǰ֡������
const char* const windowName = "��Ƶ";          //������Ƶ���ڵ�����
const char* const traceWindowName = "�ƶ��켣";     //������Ƶ���ڵ�����
bool backprojMode = false; //��ʾ�Ƿ�Ҫ���뷴��ͶӰģʽ��ture��ʾ׼�����뷴��ͶӰģʽ
bool selectObject = false;//�����Ƿ���ѡҪ���ٵĳ�ʼĿ�꣬true��ʾ���������ѡ��
int trackObject = 0;      //�������Ŀ����Ŀ
bool showHist = false;   //�Ƿ���ʾ��ɫֱ��ͼ
cv::Point origin;       //���ڱ������ѡ���һ�ε���ʱ���λ��
cv::Rect selection;     //���ڱ����˶�����ľ��ο�
int waitTime = 0;      //�߳�����ʱ��  ���ڿ��Ʋ����ٶ�
bool exitreq = false;  //�����߳���ֹ����ı�־
bool exited = true;    //�߳��Ѿ���ֹ�ı�־
bool pause = true;     //��Ƶ������ͣ�ı�־
int vmin = 10, vmax = 256, smin = 30;  //�����㷨�õ�������

//���ñ���
void ResignValues()
{
	backprojMode = false;
	selectObject = false;
	trackObject = 0;
	showHist = false;
	exitreq = false;
	exited = true;
}



CVideoTrackDlg::CVideoTrackDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CVideoTrackDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CVideoTrackDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CVideoTrackDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_SIZE()
	ON_COMMAND(ID_32773, &CVideoTrackDlg::OnOpenFile)
	ON_COMMAND(ID_32774, &CVideoTrackDlg::OnOpenCamera)
	ON_COMMAND(ID_1111, &CVideoTrackDlg::OnSpeedUp)
	ON_COMMAND(ID_1112, &CVideoTrackDlg::OnSpeedDown)
	ON_COMMAND(ID_1113, &CVideoTrackDlg::OnPaused)
END_MESSAGE_MAP()


// CVideoTrackDlg ��Ϣ�������

BOOL CVideoTrackDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ���ô˶Ի����ͼ�ꡣ  ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	//�ڴ���Ӷ���ĳ�ʼ������

	//��ʾ����
	cvNamedWindow(windowName, 0);
	cvNamedWindow(traceWindowName, 0);

	//�������ڴ�С
	HWND mainWnd = (HWND)cvGetWindowHandle(windowName);
	HWND hparent = ::GetParent(mainWnd);
	::SetParent(mainWnd, GetDlgItem(IDC_PIC)->GetSafeHwnd());
	::ShowWindow(hparent, HIDE_WINDOW);
	cvSetMouseCallback(windowName, onMouse, 0);
	CRect clientRect;
	GetClientRect(clientRect);
	moveWindow(windowName, 0, 0);
	resizeWindow(windowName, clientRect.Width(), clientRect.Height());//opencvWindow����
	GetDlgItem(IDC_PIC)->MoveWindow(0, 0, clientRect.Width(), clientRect.Height());

	return TRUE;
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ  ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CVideoTrackDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);


	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CVideoTrackDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


//������Ƶ �����㷨�ڴˣ�camshift��
DWORD WINAPI work(LPVOID capIn)
{
	//�õ��򿪵���Ƶ������ͷ����
	VideoCapture cap = *(VideoCapture *)capIn;

	Rect trackWindow;
	//����һ����ת�ľ��������,���ڴ洢ʹ�ø����㷨��������ľ���
	RotatedRect trackBox;
	int hsize = 16;
	//hranges�ں���ļ���ֱ��ͼ������Ҫ�õ�
	float hranges[] = { 0, 180 };
	const float* phranges = hranges;


	Mat frame, image0, hsv, hue, mask, hist, histimg = Mat::zeros(200, 320, CV_8UC3), backproj, traceimg;
	pause = false;

	//����Ƶ������ͷ��ȡ��֡ͼ��
	cap >> frame;
	frame.copyTo(image0);
	cap >> frame;
	frame.copyTo(image);
	//����֡�����������ͼ��Ĳ�ͬ���Զ�ʶ���˶����壨������С���ɸѡ�ˣ�
	AutoDetectPeopleByDiff(image0, image);
	//trackObject��ֵΪ-1�����Ѿ�ѡ�����˶��������
	trackObject = -1;


	for (; !exitreq;)
	{
		exited = false;
		if (!pause)
		{
			//�߳�˯��һ��ʱ�䣬���ڿ��Ʋ����ٶ�
			Sleep(waitTime);
			//������ͷץȡһ֡ͼ�������frame��
			cap >> frame;
			if (frame.empty())
				break;
		}

		frame.copyTo(image);

		//���û����ͣ��������³���
		if (!pause)//û�а���ͣ��
		{
			//��rgb��ɫ�ռ�ת����hsv�ռ�
			cvtColor(image, hsv, CV_BGR2HSV);

			//trackObject��ʼ��Ϊ0, ��ѡ������˶�������κ�Ϊ-1
			if (trackObject)
			{
				int _vmin = vmin, _vmax = vmax;

				//inRange�����Ĺ����Ǽ����������ÿ��Ԫ�ش�С�Ƿ���2��������ֵ֮�䣬�����ж�ͨ��,mask����0ͨ������Сֵ��Ҳ����h����
				//����������hsv��3��ͨ�����Ƚ�h,0~180,s,smin~256,v,min(vmin,vmax),max(vmin,vmax)�����3��ͨ�����ڶ�Ӧ�ķ�Χ�ڣ���
				//mask��Ӧ���Ǹ����ֵȫΪ1(0xff)������Ϊ0(0x00).
				inRange(hsv, Scalar(0, smin, MIN(_vmin, _vmax)),
					Scalar(180, 256, MAX(_vmin, _vmax)), mask);
				int ch[] = { 0, 0 };
				//hue��ʼ��Ϊ��hsv��С���һ���ľ���ɫ���Ķ������ýǶȱ�ʾ�ģ�������֮�����120�ȣ���ɫ���180��
				hue.create(hsv.size(), hsv.depth());
				//��hsv��һ��ͨ��(Ҳ����ɫ��)�������Ƶ�hue�У�0��������
				mixChannels(&hsv, 1, &hue, 1, ch, 1);

				//ѡ�����˶�������κ�������³���
				//�ú����ڲ��ֽ��丳ֵ1,����ֻ����һ��
				if (trackObject == -1)
				{
					//�˴��Ĺ��캯��roi�õ���Mat hue�ľ���ͷ����roi������ָ��ָ��hue����������ͬ�����ݣ�selectΪ�����Ȥ������
					Mat roi(hue, selection), maskroi(mask, selection);//mask�����hsv����Сֵ

					//calcHist()������һ������Ϊ����������У���2��������ʾ����ľ�����Ŀ����3��������ʾ��������ֱ��ͼά��ͨ�����б���4��������ʾ��ѡ�����뺯��
					//��5��������ʾ���ֱ��ͼ����6��������ʾֱ��ͼ��ά������7������Ϊÿһάֱ��ͼ����Ĵ�С����8������Ϊÿһάֱ��ͼbin�ı߽�
					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);//��roi��0ͨ������ֱ��ͼ��ͨ��mask����hist�У�hsizeΪÿһάֱ��ͼ�Ĵ�С
					normalize(hist, hist, 0, 255, CV_MINMAX);//��hist����������鷶Χ��һ��������һ����0~255

					trackWindow = selection;
					trackObject = 1;

					histimg = Scalar::all(0);//�����all(0)��ʾ���Ǳ���ȫ����0
					int binW = histimg.cols / hsize;  //histing��һ��200*300�ľ���hsizeӦ����ÿһ��bin�Ŀ�ȣ�Ҳ����histing�����ֳܷ�����bin����
					Mat buf(1, hsize, CV_8UC3);//����һ�����嵥bin����
					for (int i = 0; i < hsize; i++)//saturate_case����Ϊ��һ����ʼ����׼ȷ�任����һ����ʼ����
						buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);//Vec3bΪ3��charֵ������
					cvtColor(buf, buf, CV_HSV2BGR);//��hsv��ת����bgr

					for (int i = 0; i < hsize; i++)
					{
						//at����Ϊ����һ��ָ������Ԫ�صĲο�ֵ
						int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
						//��һ������ͼ���ϻ�һ���򵥳�ľ��Σ�ָ�����ϽǺ����½ǣ���������ɫ����С�����͵�
						rectangle(histimg, Point(i*binW, histimg.rows),
							Point((i + 1)*binW, histimg.rows - val),
							Scalar(buf.at<Vec3b>(i)), -1, 8);
					}
				}

				//����ֱ��ͼ�ķ���ͶӰ������hueͼ��0ͨ��ֱ��ͼhist�ķ���ͶӰ��������backproj��
				calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
				backproj &= mask;


				//trackWindowΪ���ѡ�������TermCriteriaΪȷ��������ֹ��׼��
				RotatedRect trackBox = CamShift(backproj, trackWindow,
					TermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1));//CV_TERMCRIT_EPS��ͨ��forest_accuracy,CV_TERMCRIT_ITER��ͨ��max_num_of_trees_in_the_forest 
				if (trackWindow.area() <= 1)
				{
					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
						trackWindow.x + r, trackWindow.y + r) &
						Rect(0, 0, cols, rows);//Rect����Ϊ�����ƫ�ƺʹ�С������һ��������Ϊ��������Ͻǵ����꣬�����ĸ�����Ϊ����Ŀ�͸�
				}

				if (backprojMode)
					cvtColor(backproj, image, CV_GRAY2BGR);//���ͶӰģʽ����ʾ��Ҳ��rgbͼ

				//���յļ������Ѿ�����trackBox�У�ʹ����Բ��ͼ���б�ǳ���
				ellipse(image, trackBox, Scalar(0, 0, 255), 3, CV_AA);


				//���켣ͼ  �켣������ȡtraceBox����������
				Point center;
				center.x = trackBox.center.x;
				center.y = trackBox.center.y;
				traceimg = Mat::zeros(image.rows, image.cols, CV_8UC3);
				//�켣�������Ѿ�����center�У�ʹ��ʵ��Բ��ͼ�ϱ�ǳ���
				circle(traceimg, center, 15, Scalar(0, 0, 255), -1, CV_AA, 0);
				cv::imshow(traceWindowName, traceimg);
			}
		}

		//����Ĵ����ǲ���pauseΪ�滹��Ϊ�ٶ�Ҫִ�е�
		else if (trackObject < 0)//ͬʱҲ���ڰ�����ͣ��ĸ�Ժ�
			pause = false;

		//���´������� ��ʹ�����ѡ������ʱ��ʾ�෴��ɫ
		if (selectObject && selection.width > 0 && selection.height > 0)
		{
			Mat roi(image, selection);
			bitwise_not(roi, roi);//bitwise_notΪ��ÿһ��bitλȡ��
		}

		//��ʾ���յ�ͼ��
		cv::imshow(windowName, image);
	}
	//��־�߳��˳�
	exited = true;
	return 0;
}





//Ѱ��ǰ��ͼƬ�����������������ģ�֡���Ҫ���ô˺�����
void findMaxCountours(Mat& img, RotatedRect& rect)
{
	img.convertTo(img, CV_8UC1);
	Mat dst = Mat(img.size(), CV_8UC3, Scalar::all(0));

	vector<vector<Point>> contours;
	vector<Point> people;
	Point centerForContour;
	vector<Vec4i> hierarchy;
	//findContours����Ѱ��img�е������������浽contours������
	findContours(img, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);
	//������ȡ��ÿһ������������ɸѡ�ȴ���
	for (int i = 0; i < contours.size(); i++)
	{
		//���������㼯ȷ���������
		RotatedRect rect = minAreaRect(contours[i]);
		//�����������ȷ����������ͳ����
		Size size = rect.size;
		float area = size.area();
		float whRate;
		if (area != 0)
			whRate = size.width / size.height;
		//��������������ɸѡ����
		if (area > 60 && whRate < 4 && whRate > 1 / 3)
		{
			/*	findGeometryCenter(contours[i], centerForContour);
			centers.push_back(centerForContour);*/
			//������������������뵽����������
			for (int j = 0; j < contours[i].size(); j++)
				people.push_back(contours[i][j]);
			//drawContours(dst, contours, i, Scalar(0,255,0),FILLED, 8, hierarchy);
		}
	}
	//�ж��������Ƿ���
	if (people.size() != 0)
	{
		//findGeometryCenter(centers, centerForContour);
		//circle(dst, centerForContour, 2, Scalar(0, 255, 0), -1);
		//��������������������Ӿ���
		rect = minAreaRect(people);
		//��img�������������ĵ�
		circle(dst, rect.center, 2, Scalar(0, 255, 0), -1);
	}

	img = Mat(dst);
}


//֡��Զ�ʶ�����ˣ������������selection�У�selection���ڸ����㷨camshift��
void AutoDetectPeopleByDiff(const Mat &img0, const Mat& img1)
{
	//��ʾ����
	Mat pFrImg, pBkImg;
	//�������ԭʼ�����������
	Mat pFrameMat, pFrMat, pBkMat;
	//��ɫͼ��ת���ɵ�ͨ��ͼ��
	cvtColor(img0, pBkImg, CV_BGR2GRAY);
	cvtColor(img0, pFrImg, CV_BGR2GRAY);
	//��ʼ����һ֡�Ĵ������ԭʼ�����������
	pFrImg.convertTo(pFrameMat, CV_32FC1);
	pFrImg.convertTo(pBkMat, CV_32FC1);
	pFrImg.convertTo(pFrMat, CV_32FC1);
	//��ɫͼ��ת���ɵ�ͨ��ͼ��
	cvtColor(img1, pFrImg, CV_BGR2GRAY);
	//��ʼ����֡ԭʼͼ��
	pFrImg.convertTo(pFrameMat, CV_32FC1);

	//��ǰԭʼ֡����һ����֡���
	absdiff(pFrameMat, pBkMat, pFrMat);

	//��ֵ�����ͼ����ֵΪ60�����жȡ�
	threshold(pFrMat, pFrImg, 60, 255.0, CV_THRESH_BINARY);
	////Ѱ������
	//Point center;
	//findGeometryCenter(pFrImg, center);
	//Ѱ�ҽ��ͼ���ڵ���������
	RotatedRect rect;
	findMaxCountours(pFrImg, rect);
	//��ȡ��Χ����
	Rect outerRect = rect.boundingRect();
	//���ηŴ����
	int a = 5;//�Ŵ��Χ���Σ��������
	Point tl = outerRect.tl() - Point(a*outerRect.width, a*outerRect.height);
	Point br = outerRect.br() + Point(a*outerRect.width, a*outerRect.height);
	//��ֵ���پ���
	selection = Rect(tl, br);
}


//���û��ڴ���ʹ���������������ʱ���á� ps���Զ�ʶ���ƶ�����ʱ���Բ�������ǳ�����
void onMouse(int event, int x, int y, int, void*)
{
	//ֻ�е�����������ȥʱ����Ч��Ȼ��ͨ��if�������Ϳ���ȷ����ѡ��ľ�������selection��
	if (selectObject)
	{
		selection.x = MIN(x, origin.x);//�������ϽǶ�������
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);//���ο�
		selection.height = std::abs(y - origin.y);//���θ�

		selection &= Rect(0, 0, image.cols, image.rows);//����ȷ����ѡ�ľ���������ͼƬ��Χ��
	}

	//�ж�����ǰ��»����ɿ���
	switch (event)
	{
		//�����갴�����¼��ʼ����
	case CV_EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		break;
		//����ɿ�ʱ�����û�ѡ��ľ��ο��С����Ϊ�����㷨�����ݡ�
	case CV_EVENT_LBUTTONUP:
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			trackObject = -1;
		break;
	}
}


//���û��ı䴰�ڴ�Сʱ����
void CVideoTrackDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	//������С
	if (windowName && (HWND)cvGetWindowHandle(windowName))
	{
		//ͨ���ƶ����ں͸ı䴰�ڴ�С��ɵ�������
		moveWindow(windowName, 0, 0);
		resizeWindow(windowName, cx, cy);
		GetDlgItem(IDC_PIC)->MoveWindow(0, 0, cx, cy);
	}
}


//����˵� ����Ƶ ��ťʱ����
void CVideoTrackDlg::OnOpenFile()
{
	//�����ļ�ѡ�񴰿�
	CFileDialog fileDialog(TRUE, NULL, NULL, NULL, "All Files(*.*)|*.*||");
	//���ѡ����һ���ļ�
	if (fileDialog.DoModal() == IDOK)
	{
		//����ʹ��VideoCapture������̽�ܷ�ɹ�����Ƶ�ļ�
		VideoCapture *cap = new VideoCapture();
		cap->open(fileDialog.GetPathName().GetBuffer());
		if (!cap->isOpened())
		{
			MessageBox("����Ƶʧ��", NULL);
			return;
		}
		//�ܴ򿪳ɹ�����ֹ֮ǰ�Ĳ����߳�
		if (!exited)
		{
			//������ֹ����
			exitreq = true;
			//�ȴ��߳��˳�
			while (!exited);
		}
		//���ñ�����Ϊ��һ����Ƶ����׼������
		ResignValues();
		//�������̲߳�����Ƶ�ļ�
		CreateThread(NULL, NULL, work, cap, NULL, NULL);
	}
}

//����˵� ������ͷ ��ťʱ����
void CVideoTrackDlg::OnOpenCamera()
{
	//����ʹ��VideoCapture������̽�ܷ�򿪳ɹ�
	VideoCapture *cap = new VideoCapture();
	cap->open(0);
	if (!cap->isOpened())
	{
		MessageBox("������ͷʧ��", NULL);
		return;
	}

	//�ܴ򿪳ɹ�����ֹ֮ǰ�Ĳ����߳�
	if (!exited)
	{
		//������ֹ����
		exitreq = true;
		//�ȴ��߳��˳�
		while (!exited);
	}
	//���ñ�����Ϊ��һ����Ƶ����׼������
	ResignValues();
	//�������̲߳�������ͷ�ɼ�����Ƶ
	CreateThread(NULL, NULL, work, cap, NULL, NULL);
}


//����˵� �����ٶ�+ ��ťʱ����
void CVideoTrackDlg::OnSpeedUp()
{
	//���ٲ��ż��˯��ʱ��  ��С˯��ʱ��Ϊ0 ��������ٶ�
	waitTime >= 10 ? waitTime -= 10 : waitTime = 0;
}


//����˵� �����ٶ�- ��ťʱ����
void CVideoTrackDlg::OnSpeedDown()
{
	//���Ӳ��ż��˯��ʱ��
	waitTime += 10;
}


//����˵� ��ͣ ��ťʱ����
void CVideoTrackDlg::OnPaused()
{
	//������ͣ�ͼ���
	pause = !pause;
}





