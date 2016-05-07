
// VideoTrackDlg.cpp : 实现文件
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

// CVideoTrackDlg 对话框


cv::Mat image;                                    //保存视频播放时当前帧的数据
const char* const windowName = "视频";          //播放视频窗口的名字
const char* const traceWindowName = "移动轨迹";     //播放视频窗口的名字
bool backprojMode = false; //表示是否要进入反向投影模式，ture表示准备进入反向投影模式
bool selectObject = false;//代表是否在选要跟踪的初始目标，true表示正在用鼠标选择
int trackObject = 0;      //代表跟踪目标数目
bool showHist = false;   //是否显示颜色直方图
cv::Point origin;       //用于保存鼠标选择第一次单击时点的位置
cv::Rect selection;     //用于保存运动物体的矩形框
int waitTime = 0;      //线程休眠时间  用于控制播放速度
bool exitreq = false;  //发送线程终止请求的标志
bool exited = true;    //线程已经终止的标志
bool pause = true;     //视频播放暂停的标志
int vmin = 10, vmax = 256, smin = 30;  //跟踪算法用到的设置

//重置变量
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


// CVideoTrackDlg 消息处理程序

BOOL CVideoTrackDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	//在此添加额外的初始化代码

	//显示窗口
	cvNamedWindow(windowName, 0);
	cvNamedWindow(traceWindowName, 0);

	//调整窗口大小
	HWND mainWnd = (HWND)cvGetWindowHandle(windowName);
	HWND hparent = ::GetParent(mainWnd);
	::SetParent(mainWnd, GetDlgItem(IDC_PIC)->GetSafeHwnd());
	::ShowWindow(hparent, HIDE_WINDOW);
	cvSetMouseCallback(windowName, onMouse, 0);
	CRect clientRect;
	GetClientRect(clientRect);
	moveWindow(windowName, 0, 0);
	resizeWindow(windowName, clientRect.Width(), clientRect.Height());//opencvWindow调整
	GetDlgItem(IDC_PIC)->MoveWindow(0, 0, clientRect.Width(), clientRect.Height());

	return TRUE;
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CVideoTrackDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);


	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CVideoTrackDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


//播放视频 跟踪算法在此（camshift）
DWORD WINAPI work(LPVOID capIn)
{
	//得到打开的视频过摄像头对象
	VideoCapture cap = *(VideoCapture *)capIn;

	Rect trackWindow;
	//定义一个旋转的矩阵类对象,用于存储使用跟踪算法计算出来的矩形
	RotatedRect trackBox;
	int hsize = 16;
	//hranges在后面的计算直方图函数中要用到
	float hranges[] = { 0, 180 };
	const float* phranges = hranges;


	Mat frame, image0, hsv, hue, mask, hist, histimg = Mat::zeros(200, 320, CV_8UC3), backproj, traceimg;
	pause = false;

	//从视频或摄像头获取两帧图像
	cap >> frame;
	frame.copyTo(image0);
	cap >> frame;
	frame.copyTo(image);
	//调用帧差法，根据两幅图像的不同，自动识别运动物体（经过大小面积筛选了）
	AutoDetectPeopleByDiff(image0, image);
	//trackObject赋值为-1代表已经选好了运动物体矩形
	trackObject = -1;


	for (; !exitreq;)
	{
		exited = false;
		if (!pause)
		{
			//线程睡眠一段时间，用于控制播放速度
			Sleep(waitTime);
			//从摄像头抓取一帧图像并输出到frame中
			cap >> frame;
			if (frame.empty())
				break;
		}

		frame.copyTo(image);

		//如果没有暂停则进入以下程序
		if (!pause)//没有按暂停键
		{
			//将rgb颜色空间转化成hsv空间
			cvtColor(image, hsv, CV_BGR2HSV);

			//trackObject初始化为0, 当选择好了运动物体矩形后为-1
			if (trackObject)
			{
				int _vmin = vmin, _vmax = vmax;

				//inRange函数的功能是检查输入数组每个元素大小是否在2个给定数值之间，可以有多通道,mask保存0通道的最小值，也就是h分量
				//这里利用了hsv的3个通道，比较h,0~180,s,smin~256,v,min(vmin,vmax),max(vmin,vmax)。如果3个通道都在对应的范围内，则
				//mask对应的那个点的值全为1(0xff)，否则为0(0x00).
				inRange(hsv, Scalar(0, smin, MIN(_vmin, _vmax)),
					Scalar(180, 256, MAX(_vmin, _vmax)), mask);
				int ch[] = { 0, 0 };
				//hue初始化为与hsv大小深度一样的矩阵，色调的度量是用角度表示的，红绿蓝之间相差120度，反色相差180度
				hue.create(hsv.size(), hsv.depth());
				//将hsv第一个通道(也就是色调)的数复制到hue中，0索引数组
				mixChannels(&hsv, 1, &hue, 1, ch, 1);

				//选择了运动物体矩形后进入以下程序
				//该函数内部又将其赋值1,所以只进入一次
				if (trackObject == -1)
				{
					//此处的构造函数roi用的是Mat hue的矩阵头，且roi的数据指针指向hue，即共用相同的数据，select为其感兴趣的区域
					Mat roi(hue, selection), maskroi(mask, selection);//mask保存的hsv的最小值

					//calcHist()函数第一个参数为输入矩阵序列，第2个参数表示输入的矩阵数目，第3个参数表示将被计算直方图维数通道的列表，第4个参数表示可选的掩码函数
					//第5个参数表示输出直方图，第6个参数表示直方图的维数，第7个参数为每一维直方图数组的大小，第8个参数为每一维直方图bin的边界
					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);//将roi的0通道计算直方图并通过mask放入hist中，hsize为每一维直方图的大小
					normalize(hist, hist, 0, 255, CV_MINMAX);//将hist矩阵进行数组范围归一化，都归一化到0~255

					trackWindow = selection;
					trackObject = 1;

					histimg = Scalar::all(0);//这里的all(0)表示的是标量全部清0
					int binW = histimg.cols / hsize;  //histing是一个200*300的矩阵，hsize应该是每一个bin的宽度，也就是histing矩阵能分出几个bin出来
					Mat buf(1, hsize, CV_8UC3);//定义一个缓冲单bin矩阵
					for (int i = 0; i < hsize; i++)//saturate_case函数为从一个初始类型准确变换到另一个初始类型
						buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);//Vec3b为3个char值的向量
					cvtColor(buf, buf, CV_HSV2BGR);//将hsv又转换成bgr

					for (int i = 0; i < hsize; i++)
					{
						//at函数为返回一个指定数组元素的参考值
						int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
						//在一幅输入图像上画一个简单抽的矩形，指定左上角和右下角，并定义颜色，大小，线型等
						rectangle(histimg, Point(i*binW, histimg.rows),
							Point((i + 1)*binW, histimg.rows - val),
							Scalar(buf.at<Vec3b>(i)), -1, 8);
					}
				}

				//计算直方图的反向投影，计算hue图像0通道直方图hist的反向投影，并存入backproj中
				calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
				backproj &= mask;


				//trackWindow为鼠标选择的区域，TermCriteria为确定迭代终止的准则
				RotatedRect trackBox = CamShift(backproj, trackWindow,
					TermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1));//CV_TERMCRIT_EPS是通过forest_accuracy,CV_TERMCRIT_ITER是通过max_num_of_trees_in_the_forest 
				if (trackWindow.area() <= 1)
				{
					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
						trackWindow.x + r, trackWindow.y + r) &
						Rect(0, 0, cols, rows);//Rect函数为矩阵的偏移和大小，即第一二个参数为矩阵的左上角点坐标，第三四个参数为矩阵的宽和高
				}

				if (backprojMode)
					cvtColor(backproj, image, CV_GRAY2BGR);//因此投影模式下显示的也是rgb图

				//最终的计算结果已经存在trackBox中，使用椭圆在图像中标记出来
				ellipse(image, trackBox, Scalar(0, 0, 255), 3, CV_AA);


				//画轨迹图  轨迹点坐标取traceBox的中心坐标
				Point center;
				center.x = trackBox.center.x;
				center.y = trackBox.center.y;
				traceimg = Mat::zeros(image.rows, image.cols, CV_8UC3);
				//轨迹点坐标已经存入center中，使用实心圆在图上标记出来
				circle(traceimg, center, 15, Scalar(0, 0, 255), -1, CV_AA, 0);
				cv::imshow(traceWindowName, traceimg);
			}
		}

		//后面的代码是不管pause为真还是为假都要执行的
		else if (trackObject < 0)//同时也是在按了暂停字母以后
			pause = false;

		//以下代码用于 当使用鼠标选择区域时显示相反颜色
		if (selectObject && selection.width > 0 && selection.height > 0)
		{
			Mat roi(image, selection);
			bitwise_not(roi, roi);//bitwise_not为将每一个bit位取反
		}

		//显示最终的图像
		cv::imshow(windowName, image);
	}
	//标志线程退出
	exited = true;
	return 0;
}





//寻找前景图片的人轮廓并绘制形心（帧差法需要调用此函数）
void findMaxCountours(Mat& img, RotatedRect& rect)
{
	img.convertTo(img, CV_8UC1);
	Mat dst = Mat(img.size(), CV_8UC3, Scalar::all(0));

	vector<vector<Point>> contours;
	vector<Point> people;
	Point centerForContour;
	vector<Vec4i> hierarchy;
	//findContours函数寻找img中的轮廓，并保存到contours向量中
	findContours(img, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);
	//遍历获取的每一个轮廓，进行筛选等处理
	for (int i = 0; i < contours.size(); i++)
	{
		//根据轮廓点集确定外包矩形
		RotatedRect rect = minAreaRect(contours[i]);
		//根据外包矩形确定区域面积和长宽比
		Size size = rect.size;
		float area = size.area();
		float whRate;
		if (area != 0)
			whRate = size.width / size.height;
		//根据面积、长宽比筛选轮廓
		if (area > 60 && whRate < 4 && whRate > 1 / 3)
		{
			/*	findGeometryCenter(contours[i], centerForContour);
			centers.push_back(centerForContour);*/
			//符合条件的轮廓点加入到人轮廓集合
			for (int j = 0; j < contours[i].size(); j++)
				people.push_back(contours[i][j]);
			//drawContours(dst, contours, i, Scalar(0,255,0),FILLED, 8, hierarchy);
		}
	}
	//判断人轮廓是否检出
	if (people.size() != 0)
	{
		//findGeometryCenter(centers, centerForContour);
		//circle(dst, centerForContour, 2, Scalar(0, 255, 0), -1);
		//根据轮廓计算人轮廓外接矩形
		rect = minAreaRect(people);
		//在img绘制人轮廓中心点
		circle(dst, rect.center, 2, Scalar(0, 255, 0), -1);
	}

	img = Mat(dst);
}


//帧差法自动识别行人，并将结果存入selection中（selection用于跟踪算法camshift）
void AutoDetectPeopleByDiff(const Mat &img0, const Mat& img1)
{
	//显示矩阵
	Mat pFrImg, pBkImg;
	//处理矩阵：原始，结果，背景
	Mat pFrameMat, pFrMat, pBkMat;
	//彩色图像转化成单通道图像
	cvtColor(img0, pBkImg, CV_BGR2GRAY);
	cvtColor(img0, pFrImg, CV_BGR2GRAY);
	//初始化上一帧的处理矩阵，原始，背景，结果
	pFrImg.convertTo(pFrameMat, CV_32FC1);
	pFrImg.convertTo(pBkMat, CV_32FC1);
	pFrImg.convertTo(pFrMat, CV_32FC1);
	//彩色图像转化成单通道图像
	cvtColor(img1, pFrImg, CV_BGR2GRAY);
	//初始化本帧原始图像
	pFrImg.convertTo(pFrameMat, CV_32FC1);

	//当前原始帧跟上一背景帧相减
	absdiff(pFrameMat, pBkMat, pFrMat);

	//二值化结果图像【阈值为60，敏感度】
	threshold(pFrMat, pFrImg, 60, 255.0, CV_THRESH_BINARY);
	////寻找形心
	//Point center;
	//findGeometryCenter(pFrImg, center);
	//寻找结果图像内的轮廓矩形
	RotatedRect rect;
	findMaxCountours(pFrImg, rect);
	//获取包围矩形
	Rect outerRect = rect.boundingRect();
	//矩形放大比例
	int a = 5;//放大包围矩形，方便跟踪
	Point tl = outerRect.tl() - Point(a*outerRect.width, a*outerRect.height);
	Point br = outerRect.br() + Point(a*outerRect.width, a*outerRect.height);
	//赋值跟踪矩形
	selection = Rect(tl, br);
}


//当用户在窗口使用鼠标标记物体轮廓时调用。 ps：自动识别移动物体时可以不用鼠标标记出来。
void onMouse(int event, int x, int y, int, void*)
{
	//只有当鼠标左键按下去时才有效，然后通过if里面代码就可以确定所选择的矩形区域selection了
	if (selectObject)
	{
		selection.x = MIN(x, origin.x);//矩形左上角顶点坐标
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);//矩形宽
		selection.height = std::abs(y - origin.y);//矩形高

		selection &= Rect(0, 0, image.cols, image.rows);//用于确保所选的矩形区域在图片范围内
	}

	//判断鼠标是按下还是松开。
	switch (event)
	{
		//如果鼠标按下则记录初始坐标
	case CV_EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		break;
		//鼠标松开时计算用户选择的矩形框大小，作为跟踪算法的依据。
	case CV_EVENT_LBUTTONUP:
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			trackObject = -1;
		break;
	}
}


//当用户改变窗口大小时调用
void CVideoTrackDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	//调整大小
	if (windowName && (HWND)cvGetWindowHandle(windowName))
	{
		//通过移动窗口和改变窗口大小完成调整动作
		moveWindow(windowName, 0, 0);
		resizeWindow(windowName, cx, cy);
		GetDlgItem(IDC_PIC)->MoveWindow(0, 0, cx, cy);
	}
}


//点击菜单 打开视频 按钮时调用
void CVideoTrackDlg::OnOpenFile()
{
	//弹出文件选择窗口
	CFileDialog fileDialog(TRUE, NULL, NULL, NULL, "All Files(*.*)|*.*||");
	//如果选择了一个文件
	if (fileDialog.DoModal() == IDOK)
	{
		//首先使用VideoCapture对象试探能否成功打开视频文件
		VideoCapture *cap = new VideoCapture();
		cap->open(fileDialog.GetPathName().GetBuffer());
		if (!cap->isOpened())
		{
			MessageBox("打开视频失败", NULL);
			return;
		}
		//能打开成功则终止之前的播放线程
		if (!exited)
		{
			//发出终止请求
			exitreq = true;
			//等待线程退出
			while (!exited);
		}
		//重置变量，为下一个视频播放准备环境
		ResignValues();
		//开启新线程播放视频文件
		CreateThread(NULL, NULL, work, cap, NULL, NULL);
	}
}

//点击菜单 打开摄像头 按钮时调用
void CVideoTrackDlg::OnOpenCamera()
{
	//首先使用VideoCapture对象试探能否打开成功
	VideoCapture *cap = new VideoCapture();
	cap->open(0);
	if (!cap->isOpened())
	{
		MessageBox("打开摄像头失败", NULL);
		return;
	}

	//能打开成功则终止之前的播放线程
	if (!exited)
	{
		//发出终止请求
		exitreq = true;
		//等待线程退出
		while (!exited);
	}
	//重置变量，为下一个视频播放准备环境
	ResignValues();
	//开启新线程播放摄像头采集的视频
	CreateThread(NULL, NULL, work, cap, NULL, NULL);
}


//点击菜单 播放速度+ 按钮时调用
void CVideoTrackDlg::OnSpeedUp()
{
	//减少播放间隔睡眠时间  最小睡眠时间为0 代表最快速度
	waitTime >= 10 ? waitTime -= 10 : waitTime = 0;
}


//点击菜单 播放速度- 按钮时调用
void CVideoTrackDlg::OnSpeedDown()
{
	//增加播放间隔睡眠时间
	waitTime += 10;
}


//点击菜单 暂停 按钮时调用
void CVideoTrackDlg::OnPaused()
{
	//交替暂停和继续
	pause = !pause;
}





