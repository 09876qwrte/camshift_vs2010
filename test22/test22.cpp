// test22.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "Global.h"
//-----------------------------------------------------------------



int _tmain(int argc, _TCHAR* argv[])
{
// 	CvCapture* capture = 0;
//  	IplImage* frame = 0;
// 	CCameraDS camera;
// //	�򿪵�һ������ͷ
// 	if(! camera.OpenCamera(0, true)) //��������ѡ�񴰿�
// 	if(! camera.OpenCamera(0, false, 320,240)) //����������ѡ�񴰿ڣ��ô����ƶ�ͼ���͸�
// 	{
// 		fprintf(stderr, "Can not open camera.\n");
// 		return -1;
// 	}

	int width=640;//640:480
	int height=480;

	CvFont font;
	cvInitFont(&font,CV_FONT_HERSHEY_SCRIPT_COMPLEX,1,1);
	
 	IplImage *frame=cvCreateImage(cvSize(width,height), IPL_DEPTH_8U, 3);
 
 	videoInput video;//������Ƶ�������
	video.setupDevice(0, width, height);//�����豸
	video.showSettingsWindow(0);//����������ʾ��Ƶ���ô��ڣ�����ȥ��

	//----------------------------------------------------------------------
	//�õ� kalman �˲�����Ԥ�⣬���������ܵ�����Χ���� camshift ����
	//�ȳ�ʼ��
	//1.kalman filter setup
	const int stateNum=4;
	const int measureNum=2;
	CvKalman* kalman = cvCreateKalman( stateNum, measureNum, 0 );//state(x,y,detaX,detaY)
	CvMat* process_noise = cvCreateMat( stateNum, 1, CV_32FC1 );
	CvMat* measurement = cvCreateMat( measureNum, 1, CV_32FC1 );//measurement(x,y)
	CvRNG rng = cvRNG(-1);
	float A[stateNum][stateNum] ={//transition matrix
		1,0,25,0,
		0,1,0,25,
		0,0,1,0,
		0,0,0,1
	};

	memcpy( kalman->transition_matrix->data.fl,A,sizeof(A));
	cvSetIdentity(kalman->measurement_matrix,cvRealScalar(1) );
	cvSetIdentity(kalman->process_noise_cov,cvRealScalar(1e-5));
	cvSetIdentity(kalman->measurement_noise_cov,cvRealScalar(1e-1));
	cvSetIdentity(kalman->error_cov_post,cvRealScalar(1));
	//initialize post state of kalman filter at random
	cvRandArr(&rng,kalman->state_post,CV_RAND_UNI,cvRealScalar(0),cvRealScalar(winHeight>winWidth?winWidth:winHeight));
	//----------------------------------------------------------------------

//	capture = cvCreateCameraCapture( 0 );

	//-----------------------------------------------------------------------
	printf( "Hot keys: \n"
		"\tESC - quit the program\n"
		"\tc - stop the tracking\n"
		"\tb - switch to/from backprojection view\n"
		"\th - show/hide object histogram\n"
		"\tg - tracking Green\n"
		"To initialize tracking, select the object with mouse\n" );
	//-----------------------------------------------------------------------
	cvNamedWindow( "CamShiftDemo", 1 );
	cvNamedWindow("out",1);
	cvSetMouseCallback( "CamShiftDemo", on_mouse, NULL ); // on_mouse �Զ����¼�
	cvCreateTrackbar( "Vmin", "CamShiftDemo", &vmin, 256, 0 );
	cvCreateTrackbar( "Vmax", "CamShiftDemo", &vmax, 256, 0 );
	cvCreateTrackbar( "Smin", "CamShiftDemo", &smin, 256, 0 );

	for(;;)
	{
		int i, bin_w, c;

//		frame = camera.QueryFrame();
		video.getPixels(0, (unsigned char *)frame->imageData, false, true);//��ȡһ֡
//		frame = cvQueryFrame( capture );
		if (!frame){
			break;
		}

		if( !image )
		{
			/* allocate all the buffers */
			image = cvCreateImage( cvGetSize(frame), 8, 3 );

			image->origin = frame->origin;
			hsv = cvCreateImage( cvGetSize(frame), 8, 3 );
			hue = cvCreateImage( cvGetSize(frame), 8, 1 );
			mask = cvCreateImage( cvGetSize(frame), 8, 1 );
			backproject = cvCreateImage( cvGetSize(frame), 8, 1 );

			hist = cvCreateHist( 1, &hdims, CV_HIST_ARRAY, &hranges, 1 );  // ����ֱ��ͼ
			hist_0 = cvCreateHist( 1, &hdims, CV_HIST_ARRAY, &hranges, 1 );
			hist_1 = cvCreateHist( 1, &hdims, CV_HIST_ARRAY, &hranges, 1 ); 
			hist_2 = cvCreateHist( 1, &hdims, CV_HIST_ARRAY, &hranges, 1 );

			histimg = cvCreateImage( cvSize(320,240), 8, 3 );
			cvZero( histimg );
		//	loadTemplateImage();
		}

		cvCopy( frame, image, 0 );
		cvCvtColor( image, hsv, CV_BGR2HSV );  // ��ɫ�ռ�ת�� BGR to HSV 

		if( track_object )
		{
			int _vmin = vmin, _vmax = vmax;

			cvInRangeS( hsv, cvScalar(0,smin,MIN(_vmin,_vmax),0),
				cvScalar(180,256,MAX(_vmin,_vmax),0), mask );  // �õ���ֵ��MASK
			cvSplit( hsv, hue, 0, 0, 0 );  // ֻ��ȡ HUE ����

			if( track_object < 0 )
			{
				float max_val = 0.f;
				cvSetImageROI( hue, selection );  // �õ�ѡ������ for ROI
				cvSetImageROI( mask, selection ); // �õ�ѡ������ for mask
				cvCalcHist( &hue, hist, 0, mask ); // ����ֱ��ͼ
				cvGetMinMaxHistValue( hist, 0, &max_val, 0, 0 );  // ֻ�����ֵ
				cvConvertScale( hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0 ); // ���� bin ������ [0,255] 
				/*
				bins : ���ڴ��ֱ��ͼÿ���Ҷȼ���Ŀ������ָ�룬������cvCreateHist ��ʱ�򴴽���
				��ά����cvCreateHist ȷ����һ����һά�Ƚϳ�����*/
				cvResetImageROI( hue );  // remove ROI
				cvResetImageROI( mask );
				track_window = selection;
				track_object = 1;

				cvZero( histimg );
				bin_w = histimg->width / hdims;  // hdims: ���ĸ������� bin_w Ϊ���Ŀ��

				// ��ֱ��ͼ
				for( i = 0; i < hdims; i++ )
				{
					//cvRound ��һ��double�͵��������������룬������һ����������
					int val = cvRound( cvGetReal1D(hist->bins,i)*histimg->height/255 );
					//cvGetReal1D���ص�ͨ�������ָ��Ԫ�� 
					CvScalar color = hsv2rgb(i*180.f/hdims);
					cvRectangle( histimg, cvPoint(i*bin_w,histimg->height),
						cvPoint((i+1)*bin_w,histimg->height - val),
						color, -1, 8, 0 );
				}
			}

			cvCalcBackProject( &hue, backproject, hist );  // ʹ�� back project ����
			cvAnd( backproject, mask, backproject, 0 );

			// calling CAMSHIFT �㷨ģ��
			cvCamShift( backproject, track_window,
				cvTermCriteria( CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1 ),
				&track_comp, &track_box );
			track_window = track_comp.rect;//���︳ֵ����һ�β��ҵ����򣬿�����΢����һ����������
		//	track_window_temp = track_comp.rect;//

			//������СҪ��ľ��ο��С
			if (bOnceSave)
			{
				minWidth = track_box.size.width;
				minHeight = track_box.size.height;
				iOldSize = minHeight * minWidth;
				bOnceSave = FALSE;
			}


			///////////////////////////////////////////////////////////////////////////////////
			mousePosition.x = track_box.center.x;
			mousePosition.y = track_box.center.y;
			//���� kalman Ԥ�⣬���Եõ� predict_pt Ԥ������
			//2.kalman prediction
			const CvMat* prediction=cvKalmanPredict(kalman,0);
			CvPoint predict_pt=cvPoint((int)prediction->data.fl[0],(int)prediction->data.fl[1]);
			//3.update measurement
			measurement->data.fl[0]=(float)mousePosition.x;
			measurement->data.fl[1]=(float)mousePosition.y;
			//4.update
			cvKalmanCorrect( kalman, measurement );	
			///////////////////////////////////////////////////////////////////////////////////
			//��Ϊ��Ƶ���õĲɼ���С��640 * 480����ô��������������Ҳ�����ⷶΧ��
			//-------------------------------------------------------------------------------------
			//����������ʵ����һ�����Ե�Ԥ�� track_window �ķ�Χ
			//��Ϊ�����ѡȡ��ʱ����ʱ����ֻ�ǵ���˴��壬����û�� width  �� height ��Ϊ0
			int iBetween = 0;
			//ȷ��Ԥ��� �� ʵ�ʵ�֮�� ���߾��� �� ���� track_box ��size ֮��
			iBetween = sqrt(	powf((kalman->PosterState[0] - track_box.center.x),2) 
				+  
				powf((kalman->PosterState[1] - track_box.center.y),2) );

			CvPoint prePoint;//Ԥ��ĵ� ����� ʵ�ʵ� �ĶԳƵ�

			if ( iBetween > 5)
			{
				//��ʵ�ʵ� �� Ԥ��� �ұ�
				if (track_box.center.x > kalman->PosterState[0])
				{
					//�ң�ʵ�ʵ��� Ԥ��� ����
					if (track_box.center.y > kalman->PosterState[1])
					{
						prePoint.x = track_box.center.x + iAbsolute(track_box.center.x,kalman->PosterState[0]);
						prePoint.y = track_box.center.y + iAbsolute(track_box.center.y,kalman->PosterState[1]);
					}
					//�ң�ʵ�ʵ��� Ԥ��� ����
					else
					{
						prePoint.x = track_box.center.x + iAbsolute(track_box.center.x,kalman->PosterState[0]);
						prePoint.y = track_box.center.y - iAbsolute(track_box.center.y,kalman->PosterState[1]);
					}
					//���
					if (track_window.width != 0)
					{
						track_window.width += iBetween + iAbsolute(track_box.center.x,kalman->PosterState[0]);
					}

					if (track_window.height != 0)
					{
						track_window.height += iBetween + iAbsolute(track_box.center.x,kalman->PosterState[0]);
					}
				}
				//��ʵ�ʵ� �� Ԥ��� ���
				else
				{
					//�ң�ʵ�ʵ��� Ԥ��� ����
					if (track_box.center.y > kalman->PosterState[1])
					{
						prePoint.x = track_box.center.x - iAbsolute(track_box.center.x,kalman->PosterState[0]);
						prePoint.y = track_box.center.y + iAbsolute(track_box.center.y,kalman->PosterState[1]);
					}
					//�ң�ʵ�ʵ��� Ԥ��� ����
					else
					{
						prePoint.x = track_box.center.x - iAbsolute(track_box.center.x,kalman->PosterState[0]);
						prePoint.y = track_box.center.y - iAbsolute(track_box.center.y,kalman->PosterState[1]);
					}
					//���
					if (track_window.width != 0)
					{
						track_window.width += iBetween + iAbsolute(track_box.center.x,kalman->PosterState[0]);
					}

					if (track_window.height != 0)
					{
						track_window.height += iBetween +iAbsolute(track_box.center.x,kalman->PosterState[0]);
					}
				}

				track_window.x = prePoint.x - iBetween;	
				track_window.y = prePoint.y - iBetween;
			}
			else
			{
				track_window.x -= iBetween;
				track_window.y -= iBetween;
				//���
				if (track_window.width != 0)
				{
					track_window.width += iBetween;
				}

				if (track_window.height != 0)
				{
					track_window.height += iBetween;
				}
			}

			//���ٵľ��ο���С�ڳ�ʼ����⵽�Ĵ�С������������ʱ��X �� Y�����ʵ�������С
			if (track_window.width < minWidth)
			{
				track_window.width = minWidth;
				track_window.x -= iBetween;
			}
			if (track_window.height < minHeight)
			{
				track_window.height = minHeight;
				track_window.y -= iBetween;
			}

			//ȷ��������ľ��δ�С��640 * 480֮��
			if (track_window.x <= 0)
			{
				track_window.x = 0;
			}
			if (track_window.y <= 0)
			{
				track_window.y = 0;
			}
			if (track_window.x >= 600)
			{
				track_window.x = 600;
			}
			if (track_window.y >= 440)
			{
				track_window.y = 440;
			}

			if (track_window.width + track_window.x >= 640)
			{
				track_window.width = 640 - track_window.x;
			}
			if (track_window.height + track_window.y >= 640)
			{
				track_window.height = 640 - track_window.y;
			}
			//-------------------------------------------------------------------------------------
			img_out=cvCreateImage(cvSize(winWidth,winHeight),8,3);
			cvSet(img_out,cvScalar(255,255,255,0));
			char buf[256];
			sprintf(buf,"%d",iBetween);
			cvPutText(img_out,buf,cvPoint(10,30),&font,CV_RGB(0,0,0));
			sprintf(buf,"%d : %d",track_window.x,track_window.y);
			cvPutText(img_out,buf,cvPoint(10,50),&font,CV_RGB(0,0,0));
			sprintf(buf,"%d : %d",track_window.width,track_window.height);
			cvPutText(img_out,buf,cvPoint(10,70),&font,CV_RGB(0,0,0));

			sprintf(buf,"size: %0.2f",track_box.size.width * track_box.size.height);
			cvPutText(img_out,buf,cvPoint(10,90),&font,CV_RGB(0,0,0));
			//-------------------------------------------------------------------------------------
			if( backproject_mode )
				cvCvtColor( backproject, image, CV_GRAY2BGR ); // ʹ��backproject�Ҷ�ͼ��
			if( image->origin )
				track_box.angle = -track_box.angle;

			//--------------------------
			cvRectangle(image,cvPoint(track_window.x,track_window.y),
						cvPoint(track_window.x + track_window.width,track_window.y + track_window.height),
						CV_RGB(0,255,255)
						);
			//--------------------------
			cvCircle(image,predict_pt,5,CV_RGB(0,0,255),3);//predicted point with green
			cvEllipseBox( image, track_box, CV_RGB(0,255,0), 3, CV_AA, 0 );

			//--------------------------------------------------------------------------
			if (iframe == 0)
			{
				GetCursorPos(&OldCursorPos);//��ȡ��ǰ�������
				OldBox.x = track_box.center.x;
				OldBox.y = track_box.center.y;
			}
			if (iframe < 3)//ÿ3֡����һ���ж�
			{
				iframe++;
			}
			else
			{
				iframe = 0;
				iNowSize = track_box.size.width * track_box.size.height;

				if ((iNowSize / iOldSize) > (3/5))
				{
					NowBox.x = track_box.center.x;
					NowBox.y = track_box.center.y;

					SetCursorPos(OldCursorPos.x - (NowBox.x - OldBox.x)*1366/640,
						OldCursorPos.y - (NowBox.y - OldBox.y)*768/480);
				}
			}
			//--------------------------------------------------------------------------
		}

		if( select_object && selection.width > 0 && selection.height > 0 )
		{
			cvSetImageROI( image, selection );
			cvXorS( image, cvScalarAll(255), image, 0 );
			cvResetImageROI( image );
		}

		//----------------------------------------------------------------------
		int x=(cvGetSize(frame).width-cvGetSize(frame).height)/2;
		int y=cvGetSize(frame).height;
		cvLine(image,cvPoint(x,0),cvPoint(x,y),CV_RGB(255,255,0));
		cvLine(image,cvPoint(cvGetSize(frame).width-x,0),cvPoint(cvGetSize(frame).width-x,y),CV_RGB(255,255,0));
		x=cvGetSize(frame).width/2;
		y=cvGetSize(frame).height/2;
		cvLine(image,cvPoint(x+10,y),cvPoint(x-10,y),CV_RGB(255,0,0));
		cvLine(image,cvPoint(x,y+10),cvPoint(x,y-10),CV_RGB(255,0,0));
		//----------------------------------------------------------------------
		cvShowImage( "out", img_out );
		cvShowImage( "CamShiftDemo", image );
		//----------------------------------------------------------------------
		if (!show_hist)
		{
			cvShowImage( "Histogram", histimg );
		}
		else
		{
			cvDestroyWindow("Histogram");
		}
		//----------------------------------------------------------------------
		cvReleaseImage(&img_out);
		//----------------------------------------------------------------------
		c = cvWaitKey(10);
		if( c == 27 )
			break;  // exit from for-loop
		switch( c )
		{
		case 'b':
			backproject_mode ^= 1;
			break;
		case 'c':
			track_object = 0;
			bOnceSave = TRUE;
			cvZero( histimg );
			break;
		case 'h':
			show_hist ^= 1;
			if( !show_hist )
				cvDestroyWindow( "Histogram" );
			else
				cvNamedWindow( "Histogram", 1 );
			break;
		case 'g':
			loadTemplateImage_G();
			break;
		default:
			;
		}
		//---------------------------------------------	
	}
	video.stopDevice(0);
//	cvReleaseCapture( &capture );
	cvDestroyWindow("CamShiftDemo");
	cvDestroyWindow("out");

	return 0;
}

