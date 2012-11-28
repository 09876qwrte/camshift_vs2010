#include "stdafx.h"

CvxText text("simhei.ttf");
float p = 0.5;//͸����

IplImage *image = 0, *hsv = 0, *hue = 0, *mask = 0, *backproject = 0, *histimg = 0,*img_out = 0;

CvHistogram *hist = 0;
CvHistogram *hist_0 = 0;
CvHistogram *hist_1 = 0;
CvHistogram *hist_2 = 0;

int backproject_mode = 0;
int select_object = 0;
int track_object = 0;
int show_hist = 1;  
CvPoint origin;
CvRect selection;
CvRect track_window,track_window_temp;
CvBox2D track_box;  // tracking ���ص����� box�����Ƕ�
CvConnectedComp track_comp;
int hdims = 80;     // ����HIST�ĸ�����Խ��Խ��ȷ
float hranges_arr[] = {0,180};
float* hranges = hranges_arr;
int vmin = 40, vmax = 256, smin = 30;

//--------------------------------------
CvPoint mousePosition;//������ڴ��� camshift �õ��� track_box.center.x and y

CvPoint predict_pt;//������� kalman ��Ԥ������

const int winHeight=640;//������ǲɼ�������Ƶ��С�����д�̶�320 * 240  640 * 480
const int winWidth=480;

BOOL	bOnceSave = TRUE;//��������ֻ����һ��
int	minWidth = 0;//�����ʼ��ʱ�����ٵľ��ο��С��֮����ٵľ��ο���С�����
int minHeight = 0;


//----------------------------------------
POINT NowCursorPos;//��ŵ�ǰ���������
POINT OldCursorPos;
POINT OldBox;		//���پ��ο�
POINT NowBox;

int	iOldSize = 0;//�����һ�����еľ������
int	iNowSize = 0;

int		iframe = 0;//ͳ��֡����ÿ3֡������һ�θ�������ļ��㣬��ȡһ�ε�ǰ���λ�ã�Ȼ�����



//----------------------------------------

//����¼��������ֶ�Ѱ��Ŀ��
void on_mouse( int event, int x, int y, int flags ,void* param)
{
	if( !image )
		return;

	if( image->origin )//�����ԭ��������ж�
		y = image->height - y;

	if( select_object )
	{
		selection.x = MIN(x,origin.x);//������������Ԫ��֮�� �Ľ�Сֵ 
		selection.y = MIN(y,origin.y);
		selection.width = selection.x + CV_IABS(x - origin.x); //a^b��a��b���   ������   ����ֵ
		selection.height = selection.y + CV_IABS(y - origin.y);

		//�����ǿ���ѡȡ��image��С��Χ֮��
		selection.x = MAX( selection.x, 0 );
		selection.y = MAX( selection.y, 0 );
		selection.width = MIN( selection.width, image->width );
		selection.height = MIN( selection.height, image->height );
		selection.width -= selection.x;
		selection.height -= selection.y;

	}

	switch( event )
	{
	case CV_EVENT_LBUTTONDOWN:
		origin = cvPoint(x,y);
		selection = cvRect(x,y,0,0);
		select_object = 1;
		break;
	case CV_EVENT_LBUTTONUP:
		select_object = 0;
		if( selection.width > 0 && selection.height > 0 )
			track_object = -1;
#ifdef _DEBUG
		printf("\n # ����ѡ������"); 
		printf("\n   X = %d, Y = %d, Width = %d, Height = %d",
			selection.x, selection.y, selection.width, selection.height);
#endif
		break;
	}
}

CvScalar hsv2rgb( float hue )
{
	int rgb[3], p, sector;
	static const int sector_data[][3]=
	{{0,2,1}, {1,2,0}, {1,0,2}, {2,0,1}, {2,1,0}, {0,1,2}};
	hue *= 0.033333333333333333333333333333333f;
	sector = cvFloor(hue);
	p = cvRound(255*(hue - sector));
	p ^= sector & 1 ? 255 : 0;

	rgb[sector_data[sector][0]] = 255;
	rgb[sector_data[sector][1]] = 0;
	rgb[sector_data[sector][2]] = p;

#ifdef _DEBUG
	printf("\n # Convert HSV to RGB��"); 
	printf("\n   HUE = %f", hue);
	printf("\n   R = %d, G = %d, B = %d", rgb[0],rgb[1],rgb[2]);
#endif

	return cvScalar(rgb[2], rgb[1], rgb[0],0);
}
//��ȡRed��ʼ��ͼƬ���Ա����tracking
BOOL loadTemplateImage_R()
{
	IplImage *tempimage = cvLoadImage("d:/Read.jpg",1);
	if (!tempimage)
	{
		return	FALSE;
	}
	cvCvtColor( tempimage, hsv, CV_BGR2HSV );
	int _vmin = vmin, _vmax = vmax;

	cvInRangeS( hsv, cvScalar(0,smin,MIN(_vmin,_vmax),0),
		cvScalar(180,256,MAX(_vmin,_vmax),0), mask );

	cvSplit( hsv, hue, 0, 0, 0 );

	selection.x = 1;
	selection.y = 1;
	selection.width = winHeight-1;//640:480
	selection.height= winWidth-1;

	cvSetImageROI( hue, selection );
	cvSetImageROI( mask, selection );
	cvCalcHist( &hue, hist, 0, mask );

	float max_val = 0.f;  

	cvGetMinMaxHistValue( hist, 0, &max_val, 0, 0 );
	cvConvertScale( hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0 );
	cvResetImageROI( hue );
	cvResetImageROI( mask );
	track_window = selection;
	track_object = 1;

	cvReleaseImage(&tempimage);

	return TRUE;
}
//��ȡGreen��ʼ��ͼƬ���Ա����tracking
BOOL loadTemplateImage_G()
{
	IplImage *tempimage = cvLoadImage("d:/Green.jpg",1);
	if (!tempimage)
	{
		return	FALSE;
	}
	cvCvtColor( tempimage, hsv, CV_BGR2HSV );
	int _vmin = vmin, _vmax = vmax;

	cvInRangeS( hsv, cvScalar(0,smin,MIN(_vmin,_vmax),0),
		cvScalar(180,256,MAX(_vmin,_vmax),0), mask );

	cvSplit( hsv, hue, 0, 0, 0 );

	selection.x = 1;
	selection.y = 1;
	selection.width = winHeight-1;//640:480
	selection.height= winWidth-1;

	cvSetImageROI( hue, selection );
	cvSetImageROI( mask, selection );
	cvCalcHist( &hue, hist, 0, mask );

	float max_val = 0.f;  

	cvGetMinMaxHistValue( hist, 0, &max_val, 0, 0 );
	cvConvertScale( hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0 );
	cvResetImageROI( hue );
	cvResetImageROI( mask );
	track_window = selection;
	track_object = 1;

	cvReleaseImage(&tempimage);

	return TRUE;
}
//��ȡBlue��ʼ��ͼƬ���Ա����tracking
BOOL loadTemplateImage_B()
{
	IplImage *tempimage = cvLoadImage("d:/Blue.jpg",1);
	if (!tempimage)
	{
		return	FALSE;
	}
	cvCvtColor( tempimage, hsv, CV_BGR2HSV );
	int _vmin = vmin, _vmax = vmax;

	cvInRangeS( hsv, cvScalar(0,smin,MIN(_vmin,_vmax),0),
		cvScalar(180,256,MAX(_vmin,_vmax),0), mask );

	cvSplit( hsv, hue, 0, 0, 0 );

	selection.x = 1;
	selection.y = 1;
	selection.width = winHeight-1;//640:480
	selection.height= winWidth-1;

	cvSetImageROI( hue, selection );
	cvSetImageROI( mask, selection );
	cvCalcHist( &hue, hist, 0, mask );

	float max_val = 0.f;  

	cvGetMinMaxHistValue( hist, 0, &max_val, 0, 0 );
	cvConvertScale( hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0 );
	cvResetImageROI( hue );
	cvResetImageROI( mask );
	track_window = selection;
	track_object = 1;

	cvReleaseImage(&tempimage);

	return TRUE;
}

//���������ֵ��
int  iAbsolute(int a, int b)
{
	int c = 0;
	if (a > b)
	{
		c = a - b;
	}
	else
	{
		c = b - a;
	}
	return	c;
}