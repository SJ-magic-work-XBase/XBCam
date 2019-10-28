/************************************************************
************************************************************/
#include "ofApp.h"

#include <time.h>

/************************************************************
************************************************************/
using namespace ofxCv;
using namespace cv;


/************************************************************
************************************************************/

/******************************
******************************/
ofApp::ofApp(int _Cam_id)
: Cam_id(_Cam_id)
, MouseOffset(ofVec2f(0, 0))
, b_flipCamera(false)
, col_Evil(ofColor(255, 0, 0))
// , col_Evil(ofColor(0, 255, 0))
, col_Calm(ofColor(0, 0, 255))
, State_Led_Evil(5, IMG_S_WIDTH * IMG_S_HEIGHT / 2, 200, 300)
, State_Led_Evil_Raw(5, IMG_S_WIDTH * IMG_S_HEIGHT / 2, 200, 300)
, State_Led_Calm(5, IMG_S_WIDTH * IMG_S_HEIGHT / 2, 200, 300)
, State_Led_Calm_Raw(5, IMG_S_WIDTH * IMG_S_HEIGHT / 2, 200, 300)
, State_Motion(0, IMG_S_WIDTH * IMG_S_HEIGHT, 0, 100)
, Osc_Effect("127.0.0.1", 12345, 12346)
{
	/********************
	********************/
	srand((unsigned) time(NULL));
	
	/********************
	********************/
	font[FONT_S].load("font/RictyDiminished-Regular.ttf", 10, true, true, true);
	font[FONT_M].load("font/RictyDiminished-Regular.ttf", 12, true, true, true);
	font[FONT_L].load("font/RictyDiminished-Regular.ttf", 15, true, true, true);
	font[FONT_LL].load("font/RictyDiminished-Regular.ttf", 30, true, true, true);
	
	/********************
	********************/
	fp_Log = fopen("../../../data/Log.csv", "w");
}

/******************************
******************************/
ofApp::~ofApp()
{
	if(fp_Log) fclose(fp_Log);
	if(Gui_Global) delete Gui_Global;
}

/******************************
******************************/
void ofApp::exit()
{
}

/******************************
******************************/
void ofApp::setup(){
	/********************
	********************/
	ofSetWindowTitle("XBC");
	
	ofSetWindowShape( WINDOW_WIDTH, WINDOW_HEIGHT );
	/*
	ofSetVerticalSync(false);
	ofSetFrameRate(0);
	/*/
	ofSetVerticalSync(true);
	ofSetFrameRate(30);
	//*/
	
	ofSetEscapeQuitsApp(false);
	
	/********************
	********************/
	setup_Gui();
	if(isFile_Exist("../../../data/gui.xml"))	Gui_Global->gui.loadFromFile("gui.xml");
	
	setup_Camera();
	if(!b_CamSearchFailed){
		/********************
		********************/
		img_Frame.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		img_LedDetectedArea_Last.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		img_LedDetectedArea_Current.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		img_LedDetectedArea.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		img_LedDetectedArea_Current_Calm.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_LedDetectedArea_Current_Evil.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_Frame_Gray.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_LastFrame_Gray.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_AbsDiff_BinGray.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_BinGray_Cleaned.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		
		for(int i = 0; i < NUM_CALIB_IMGS; i++){
			CalibImage[i].img.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		}
	}
}

/******************************
******************************/
bool ofApp::isFile_Exist(char* FileName)
{
	FILE* fp = fopen(FileName, "r");
	if(fp == nullptr){
		return false;
	}else{
		fclose(fp);
		
		printf(">>> gui : load from file <<<\n");
		fflush(stdout);
		
		return true;
	}
}

/******************************
description
	memoryを確保は、app start後にしないと、
	segmentation faultになってしまった。
******************************/
void ofApp::setup_Gui()
{
	/********************
	********************/
	Gui_Global = new GUI_GLOBAL;
	Gui_Global->setup("XBC", "gui.xml", 1250, 10);
}

/******************************
******************************/
void ofApp::setup_Camera()
{
	/********************
	********************/
	printf("> setup camera\n");
	fflush(stdout);
	
	ofSetLogLevel(OF_LOG_VERBOSE);
    cam.setVerbose(true);
	
	vector< ofVideoDevice > Devices = cam.listDevices();// 上 2行がないと、List表示されない.
	
	/********************
	search for camera by device name.
	********************/
	if(Cam_id == -2){
		Cam_id = -1;
		
		int i;
		for(i = 0; i < Devices.size(); i++){
			if(Devices[i].deviceName == "HD Pro Webcam C920" ){
				Cam_id = i;
				break;
			}
		}
		
		if(i == Devices.size()){
			b_CamSearchFailed = true;
			t_CamSearchFailed = ofGetElapsedTimef();
			
			return;
		}
	}
	
	/********************
	********************/
	if(Cam_id == -1){
		std::exit(1);
	}else{
		if(Devices.size() <= Cam_id) { ERROR_MSG(); std::exit(1); }
		
		cam.setDeviceID(Cam_id);
		cam.initGrabber(VIDEO_WIDTH, VIDEO_HEIGHT);
		
		printf("> Cam size asked = (%d, %d), actual = (%d, %d)\n", int(VIDEO_WIDTH), int(VIDEO_HEIGHT), int(cam.getWidth()), int(cam.getHeight()));
		fflush(stdout);
	}
	
	return;
}

/******************************
******************************/
void ofApp::clear_fbo(ofFbo& fbo)
{
	fbo.begin();
	ofClear(0, 0, 0, 0);
	fbo.end();
}

/******************************
******************************/
void ofApp::update(){
	/********************
	********************/
	now = ofGetElapsedTimeMillis();
	
	// if(State_Overlook == STATEOVERLOOK__RUN) fprintf(fp_Log, "%d,", now);
	
	/********************
	********************/
	if(b_CamSearchFailed){
		if(2.0 < ofGetElapsedTimef() - t_CamSearchFailed)	{ ofExit(1); return; }
		else												{ return; }
	}
	
	/********************
	********************/
    cam.update();
    if(cam.isFrameNew())	{ update_img_OnCam(); }
	
	/********************
	********************/
	StateChart_overlook();
	
	/********************
	********************/
	KeyInputCommand.Reset(); // 持ち越し なし.
	
	// if(State_Overlook == STATEOVERLOOK__RUN) fprintf(fp_Log, "%d,", ofGetElapsedTimeMillis());
}

/******************************
******************************/
void ofApp::update_img_OnCam(){
	if(!b_HaltRGBVal_inCalib){
		ofxCv::copy(cam, img_Frame);
		if(b_flipCamera) img_Frame.mirror(false, true);
		// ofxCv::blur(img_Frame, ForceOdd((int)Gui_Global->BlurRadius_Frame));
		img_Frame.update();
		
		img_LastFrame_Gray = img_Frame_Gray;
		// img_LastFrame_Gray.update(); // drawしないので不要.
		
		convertColor(img_Frame, img_Frame_Gray, CV_RGB2GRAY);
		ofxCv::blur(img_Frame_Gray, ForceOdd((int)Gui_Global->BlurRadius_Frame));
		img_Frame_Gray.update();
	}
}

/******************************
******************************/
int ofApp::ForceOdd(int val){
	if(val <= 0)			return val;
	else if(val % 2 == 0)	return val - 1;
	else					return val;
}

/******************************
******************************/
void ofApp::StateChart_overlook()
{
	switch(State_Overlook){
		case STATEOVERLOOK__CALIB:
			StateChart_Calib();
			break;
			
		case STATEOVERLOOK__RUN:
			if(KeyInputCommand.Is_RetryCalib()){
				State_Overlook = STATEOVERLOOK__CALIB;
				State_Calib = STATECALIB__WAITSHOOT_IMG_0;
				
				State_Led_Evil.Reset(now);
				State_Led_Evil_Raw.Reset(now);
				State_Led_Calm.Reset(now);
				State_Led_Calm_Raw.Reset(now);
				State_Motion.Reset(now);
			}else{
				ImageProcessing();
			}
			break;
	}
}

/******************************
******************************/
void ofApp::StateChart_Calib()
{
	switch(State_Calib){
		case STATECALIB__WAITSHOOT_IMG_0:
			if(KeyInputCommand.Is_ShootImg()){
				CalibImage[0].img = img_Frame;
				CalAndMark_MinDistance(CalibImage[0]);
				
				State_Calib = STATECALIB__WAITSHOOT_IMG_1;
			}
			break;
			
		case STATECALIB__WAITSHOOT_IMG_1:
			if(KeyInputCommand.Is_RetryCalib()){
				State_Calib = STATECALIB__WAITSHOOT_IMG_0;
			}else if(KeyInputCommand.Is_ShootImg()){
				CalibImage[1].img = img_Frame;
				CalAndMark_MinDistance(CalibImage[1]);
				
				/********************
				********************/
				save_NearestColor();
				
				State_Calib = STATECALIB__FIN;
			}
			break;
			
		case STATECALIB__FIN:
			if(KeyInputCommand.Is_RetryCalib()){
				State_Calib = STATECALIB__WAITSHOOT_IMG_0;
			}else if(KeyInputCommand.Is_Enter()){
				State_Overlook = STATEOVERLOOK__RUN;
				State_Calib = STATECALIB__WAITSHOOT_IMG_0; // 一応.
				
				State_Led_Evil.Reset(now);
				State_Led_Evil_Raw.Reset(now);
				State_Led_Calm.Reset(now);
				State_Led_Calm_Raw.Reset(now);
				State_Motion.Reset(now);
			}
			break;
	}
}

/******************************
******************************/
void ofApp::save_NearestColor(){
	{
		int id = 0;
		int Nearest = CalibImage[0].ret_MinSquareDistance__to_Evil;
		
		for(int i = 1; i < NUM_CALIB_IMGS; i++){
			if(CalibImage[i].ret_MinSquareDistance__to_Evil < Nearest){
				Nearest = CalibImage[i].ret_MinSquareDistance__to_Evil;
				id = i;
			}
		}
		CalibResult_Evil.set(CalibImage[id].col_NearestTo_Evil);
	}
	
	{
		int id = 0;
		int Nearest = CalibImage[0].ret_MinSquareDistance__to_Calm;
		
		for(int i = 1; i < NUM_CALIB_IMGS; i++){
			if(CalibImage[i].ret_MinSquareDistance__to_Calm < Nearest){
				Nearest = CalibImage[i].ret_MinSquareDistance__to_Calm;
				id = i;
			}
		}
		CalibResult_Calm.set(CalibImage[id].col_NearestTo_Calm);
	}
}

/******************************
******************************/
void ofApp::CalAndMark_MinDistance(CALIB_IMG& _CalibImage){
	/********************
	openCv上でmarking : ofImageには直接描画できないので.
	
	cv::circle
		http://opencv.jp/opencv-2svn/cpp/drawing_functions.html#cv-line
		https://cvtech.cc/opencv/3/
	********************/
	cv::Mat Mat_temp = toCv(_CalibImage.img);
	ofPoint ret_pos;
	
	Search_MinSquareDistance_to_TargetCol(_CalibImage.img, col_Evil, _CalibImage.ret_MinSquareDistance__to_Evil, ret_pos, _CalibImage.col_NearestTo_Evil);
	cv::circle(Mat_temp, cv::Point(ret_pos.x, ret_pos.y), 4/* radius */, toCv(col_Evil)/*cv::Scalar(255, 0, 0)*/, 1/* thickness : -1で塗りつぶし */, CV_AA);
	
	Search_MinSquareDistance_to_TargetCol(_CalibImage.img, col_Calm, _CalibImage.ret_MinSquareDistance__to_Calm, ret_pos, _CalibImage.col_NearestTo_Calm);
	cv::circle(Mat_temp, cv::Point(ret_pos.x, ret_pos.y), 4/* radius */, cv::Scalar(0, 255, 255), 1/* thickness : -1で塗りつぶし */, CV_AA);
	
	_CalibImage.img.update(); // apply to texture.
}

/******************************
description
	昇順
******************************/
int ofApp::int_sort( const void * a , const void * b )
{
	if(*(int*)a < *(int*)b){
		return -1;
	}else if(*(int*)a == *(int*)b){
		return 0;
	}else{
		return 1;
	}
}

/******************************
******************************/
void ofApp::ImageProcessing()
{
	if(!b_HaltRGBVal_inCalib) img_LedDetectedArea_Last = img_LedDetectedArea_Current;
	img_LedDetectedArea_Current.setColor(ofColor(0, 0, 0)); // all clear.
	img_LedDetectedArea.setColor(ofColor(0, 0, 0)); // all clear.
	
	update__img_LedDetected_Current(img_LedDetectedArea_Current_Evil, CalibResult_Evil, col_Evil);
	update__img_LedDetected_Current(img_LedDetectedArea_Current_Calm, CalibResult_Calm, col_Calm);
	
	Judge_if_LedExist();
	
	/********************
	********************/
	Judge_if_MotionExist();
}

/******************************
******************************/
void ofApp::update__img_LedDetected_Current(ofImage& _img_LedDetectedArea_Current, const CALIB_RESULT& CalibResult, const ofColor& col_Target)
{
	/********************
	********************/
	ofPixels& pix_Frame = img_Frame.getPixels();
	ofPixels& pix_LedDetectedArea_Current = _img_LedDetectedArea_Current.getPixels();
	
	for(int _x = 0; _x < img_Frame.getWidth(); _x++){
		for(int _y = 0; _y < img_Frame.getHeight(); _y++){
			ofColor col_Frame = pix_Frame.getColor(_x, _y);
			
			if(isThisColor_Judged_as_Led(CalibResult, col_Frame)){
				pix_LedDetectedArea_Current.setColor(_x, _y, ofColor(255, 255, 255));
			}else{
				pix_LedDetectedArea_Current.setColor(_x, _y, ofColor(0, 0, 0));
			}
		}
	}
	
	/********************
	OpenCVの膨張縮小って4近傍？8近傍?
		http://micchysdiary.blogspot.com/2012/10/opencv48.html
		
	画像の平滑化 : median blur
		http://labs.eecs.tottori-u.ac.jp/sd/Member/oyamada/OpenCV/html/py_tutorials/py_imgproc/py_filtering/py_filtering.html
	********************/
	cv::Mat Mat_temp = toCv(_img_LedDetectedArea_Current);
	ofxCv::medianBlur(Mat_temp, ForceOdd((int)Gui_Global->MedianRadius_Color));
	_img_LedDetectedArea_Current.update(); // 一応.
}

/******************************
******************************/
void ofApp::Judge_if_LedExist()
{
	/********************
	********************/
	ofPixels& pix_Evil = img_LedDetectedArea_Current_Evil.getPixels();
	ofPixels& pix_Calm = img_LedDetectedArea_Current_Calm.getPixels();
	ofPixels& pix_Current = img_LedDetectedArea_Current.getPixels();
	ofPixels& pix_Last = img_LedDetectedArea_Last.getPixels();
	ofPixels& pix_Detected = img_LedDetectedArea.getPixels();
	
	int counter_Evil_Raw = 0;
	int counter_Calm_Raw = 0;
	int counter_Evil = 0;
	int counter_Calm = 0;
	for(int _x = 0; _x < IMG_S_WIDTH; _x++){
		for(int _y = 0; _y < IMG_S_HEIGHT; _y++){
			/* Evil */
			if( (pix_Evil.getColor(_x, _y).r == 255) ){ // white
				pix_Current.setColor(_x, _y, col_Evil);
				counter_Evil_Raw++;
				
				if(pix_Last.getColor(_x, _y) == col_Evil){ // both Current and Last are color of Evil.
					pix_Detected.setColor(_x, _y, col_Evil);
					counter_Evil++;
				}
			}
			
			/* Calm */
			if( (pix_Calm.getColor(_x, _y).r == 255) ){
				pix_Current.setColor(_x, _y, col_Calm);
				counter_Calm_Raw++;
				
				if(pix_Last.getColor(_x, _y) == col_Calm){ // both Current and Last are color of Evil.
					pix_Detected.setColor(_x, _y, col_Calm);
					counter_Calm++;
				}
			}
		}
	}
	State_Led_Evil_Raw.update(counter_Evil_Raw, now);
	State_Led_Calm_Raw.update(counter_Calm_Raw, now);
	State_Led_Evil.update(counter_Evil, now);
	State_Led_Calm.update(counter_Calm, now);

	/********************
	********************/
	img_LedDetectedArea_Current_Evil.update();
	img_LedDetectedArea_Current_Calm.update();
	img_LedDetectedArea_Current.update();
	img_LedDetectedArea_Last.update();
	img_LedDetectedArea.update();	
}

/******************************
******************************/
bool ofApp::isThisColor_Judged_as_Led(const CALIB_RESULT& CalibResult, const ofColor& col)
{
	if	(	(CalibResult.get_deltaHue(col.getHueAngle()) < Gui_Global->thresh_Diff_Hue ) &&
			(CalibResult.get_Saturation() * Gui_Global->k_Saturation < col.getSaturation()) &&
			(CalibResult.get_Brightness() * Gui_Global->k_Brightness < col.getBrightness())
		)		
	{
		return true;
	}else{
		return false;
	}
}

/******************************
******************************/
void ofApp::Judge_if_MotionExist()
{
	/********************
	********************/
	absdiff(img_Frame_Gray, img_LastFrame_Gray, img_AbsDiff_BinGray);
	threshold(img_AbsDiff_BinGray, Gui_Global->thresh_Diff_to_Bin);
	img_AbsDiff_BinGray.update();
	
	/********************
	opencv:resize
		https://cvtech.cc/resize/
		
	OpenCVの膨張縮小って4近傍？8近傍?	
		http://micchysdiary.blogspot.com/2012/10/opencv48.html
	********************/
	cv::Mat MatSrc		= toCv(img_AbsDiff_BinGray);
	cv::Mat MatCleaned	= toCv(img_BinGray_Cleaned);
	
	cv::erode(MatSrc, MatCleaned, Mat(), cv::Point(-1, -1), (int)Gui_Global->ErodeSize_Motion); // 白に黒が侵食 : Clean
	img_BinGray_Cleaned.update();
	
	/********************
	********************/
	int counter = 0;
	ofPixels& pix = img_BinGray_Cleaned.getPixels();
	for(int _x = 0; _x < pix.getWidth(); _x++){
		for(int _y = 0; _y < pix.getHeight(); _y++){
			/********************
			ofImage, ofPixelsがGrayScaleの場合でも、
				ofColor col = pix.getColor(_x, _y);
			はRGB値がそれぞれ、きちんと取れる(全て同じ値)。
			********************/
			ofColor col = pix.getColor(_x, _y);
			if( 0 < col.r ){
				counter++;
			}
		}
	}	
	
	State_Motion.update(counter, now);
}

/******************************
******************************/
void ofApp::Search_MinSquareDistance_to_TargetCol(ofImage& img,  const ofColor& TargetCol, int& ret_MinSquareDistance, ofPoint& ret_pos, ofColor& ret_Col_Nearest)
{
	/********************
	meas result : max = 2ms : 320 x 180
	********************/
	// fprintf(fp_Log, "%d,", ofGetElapsedTimeMillis());
	
	ofPixels& pix = img.getPixels();
	
	ofColor col = pix.getColor(0, 0);
	ret_MinSquareDistance = cal_SquareDistance(TargetCol, col);
	ret_pos = ofPoint(0, 0);
	ret_Col_Nearest = col;
	
	for(int _x = 0; _x < img.getWidth(); _x++){
		for(int _y = 0; _y < img.getHeight(); _y++){
			ofColor col = pix.getColor(_x, _y);
			int temp = cal_SquareDistance(TargetCol, col);
			
			if(temp < ret_MinSquareDistance){
				ret_MinSquareDistance = temp;
				ret_pos.x = _x;
				ret_pos.y = _y;
				ret_Col_Nearest = col;
			}
		}
	}
	
	// fprintf(fp_Log, "%d\n", ofGetElapsedTimeMillis());
}

/******************************
******************************/
int ofApp::cal_SquareDistance(const ofColor& col_0, const ofColor& col_1)
{
	int SquareDistance = (int)pow((col_0.r - col_1.r), 2) + (int)pow((col_0.g - col_1.g), 2) + (int)pow((col_0.b - col_1.b), 2);
	return SquareDistance;
}

/******************************
warning
	ofDrawLine()などの描画は、fboなど介さず直接drawしているので、offset関連に注意.
******************************/
void ofApp::draw_RGB_ofThePoint_Calib()
{
	/********************
	********************/
	/* pos in img_Frame */
	int imgCoord_x = mouseX + MouseOffset.x - pos_Draw[0].x;
	int imgCoord_y = mouseY + MouseOffset.y - pos_Draw[0].y;
	
	if( (0 <= imgCoord_x) && (imgCoord_x < img_Frame.getWidth()) && (0 <= imgCoord_y) && (imgCoord_y < img_Frame.getHeight()) ){
		ofColor col = img_Frame.getPixels().getColor(imgCoord_x, imgCoord_y);
		if(b_HaltRGBVal_inCalib)	ofSetColor(255, 255, 0, 200);
		else						ofSetColor(255, 255, 255, 200);
		
		const int TextHeight = font[FONT_S].stringHeight("(A,yq]") * 1;
		char buf[BUF_SIZE_S];
		sprintf(buf, "(%3d, %3d, %3d)->(%3.0f, %3.0f, %3.0f)", col.r, col.g, col.b, col.getHueAngle(), col.getSaturation(), col.getBrightness());
		// font[FONT_S].drawString(buf, pos_Draw[0].x + img_Frame.getWidth() + 10,  pos_Draw[0].y + img_Frame.getHeight() - 10);
		font[FONT_S].drawString(buf, pos_Draw[0].x,  pos_Draw[0].y + img_Frame.getHeight() + TextHeight);
		
		ofSetColor(255, 255, 0, 255);
		ofSetLineWidth(1);
		ofDrawLine(pos_Draw[0].x, imgCoord_y + pos_Draw[0].y, pos_Draw[0].x + img_Frame.getWidth(), imgCoord_y + pos_Draw[0].y); // draw時のoffsetに注意.
		ofDrawLine(imgCoord_x + pos_Draw[0].x,  pos_Draw[0].y, imgCoord_x + pos_Draw[0].x, pos_Draw[0].y + img_Frame.getHeight());
	}
}

/******************************
warning
	ofDrawLine()などの描画は、fboなど介さず直接drawしているので、offset関連に注意.
******************************/
void ofApp::draw_RGB_ofThePoint_Run()
{
	/********************
	********************/
	/* pos in img_Frame */
	int imgCoord_x = mouseX + MouseOffset.x - pos_Draw[0].x;
	int imgCoord_y = mouseY + MouseOffset.y - pos_Draw[0].y;
	
	if( (0 <= imgCoord_x) && (imgCoord_x < img_Frame.getWidth()) && (0 <= imgCoord_y) && (imgCoord_y < img_Frame.getHeight()) ){
	
		/********************
		********************/
		const ofColor col_NotDetected(120);
		const ofColor col_Detected(255);
		
		/********************
		img_Frame
		********************/
		ofColor col = img_Frame.getPixels().getColor(imgCoord_x, imgCoord_y);
		
		const int _x = 10;
		// const int _x = pos_Draw[0].x + img_Frame.getWidth() + 10;
		const int TextHeight = font[FONT_S].stringHeight("(A,yq]") * 1;
		char buf[BUF_SIZE_S];
		ofSetColor(col_NotDetected);
		sprintf(buf, "(%3d, %3d, %3d)->(%3.0f, %3.0f, %3.0f)", col.r, col.g, col.b, col.getHueAngle(), col.getSaturation(), col.getBrightness());
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 1);
		
		/* */
		if(isThisColor_Judged_as_Led(CalibResult_Evil, col))	ofSetColor(col_Detected);
		else													ofSetColor(col_NotDetected);
		sprintf(buf, "Evil");
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 3);
		
		if(CalibResult_Evil.get_deltaHue(col.getHueAngle()) < Gui_Global->thresh_Diff_Hue)	ofSetColor(col_Detected);
		else																				ofSetColor(col_NotDetected);
		sprintf(buf, "H : %3.0f", CalibResult_Evil.get_Hue());
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 4);
		
		if(CalibResult_Evil.get_Saturation() * Gui_Global->k_Saturation < col.getSaturation())	ofSetColor(col_Detected);
		else																					ofSetColor(col_NotDetected);
		sprintf(buf, "%3.0f < S", CalibResult_Evil.get_Saturation() * Gui_Global->k_Saturation);
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 5);

		if(CalibResult_Evil.get_Brightness() * Gui_Global->k_Brightness < col.getBrightness())	ofSetColor(col_Detected);
		else																					ofSetColor(col_NotDetected);
		sprintf(buf, "%3.0f < V", CalibResult_Evil.get_Brightness() * Gui_Global->k_Brightness);
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 6);
		
		/* */
		if(isThisColor_Judged_as_Led(CalibResult_Calm, col))	ofSetColor(col_Detected);
		else													ofSetColor(col_NotDetected);
		sprintf(buf, "Calm");
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 8);
		
		if(CalibResult_Calm.get_deltaHue(col.getHueAngle()) < Gui_Global->thresh_Diff_Hue)	ofSetColor(col_Detected);
		else																				ofSetColor(col_NotDetected);
		sprintf(buf, "H : %3.0f", CalibResult_Calm.get_Hue());
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 9);
		
		if(CalibResult_Calm.get_Saturation() * Gui_Global->k_Saturation < col.getSaturation())	ofSetColor(col_Detected);
		else																					ofSetColor(col_NotDetected);
		sprintf(buf, "%3.0f < S ?", CalibResult_Calm.get_Saturation() * Gui_Global->k_Saturation);
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 10);

		if(CalibResult_Calm.get_Brightness() * Gui_Global->k_Brightness < col.getBrightness())	ofSetColor(col_Detected);
		else																					ofSetColor(col_NotDetected);
		sprintf(buf, "%3.0f < V ?", CalibResult_Calm.get_Brightness() * Gui_Global->k_Brightness);
		font[FONT_S].drawString(buf, _x,  pos_Draw[0].y + TextHeight * 11);
		
		/* Line */
		ofSetColor(255, 255, 0, 255);
		ofSetLineWidth(1);
		ofDrawLine(pos_Draw[0].x, imgCoord_y + pos_Draw[0].y, pos_Draw[0].x + img_Frame.getWidth(), imgCoord_y + pos_Draw[0].y); // draw時のoffsetに注意.
		ofDrawLine(imgCoord_x + pos_Draw[0].x,  pos_Draw[0].y, imgCoord_x + pos_Draw[0].x, pos_Draw[0].y + img_Frame.getHeight());
		
		/********************
		img_LedDetectedArea_Current
		********************/
		col = img_LedDetectedArea_Current.getPixels().getColor(imgCoord_x, imgCoord_y);;
		
		/* */
		ofSetColor(col_NotDetected);
		sprintf(buf, "(%3d, %3d, %3d)", col.r, col.g, col.b);
		font[FONT_S].drawString(buf, pos_Draw[1].x + img_LedDetectedArea_Current.getWidth() + 10,  pos_Draw[1].y + TextHeight * 1);
		
		/* Line */
		ofSetColor(255, 255, 0, 255);
		ofSetLineWidth(1);
		ofDrawLine(pos_Draw[1].x, imgCoord_y + pos_Draw[1].y, pos_Draw[1].x + img_LedDetectedArea_Current.getWidth(), imgCoord_y + pos_Draw[1].y); // draw時のoffsetに注意.
		ofDrawLine(imgCoord_x + pos_Draw[1].x,  pos_Draw[1].y, imgCoord_x + pos_Draw[1].x, pos_Draw[1].y + img_LedDetectedArea_Current.getHeight());
		
		/********************
		img_LedDetectedArea = Last * Current
		********************/
		col = img_LedDetectedArea.getPixels().getColor(imgCoord_x, imgCoord_y);;
		
		/* */
		ofSetColor(col_NotDetected);
		sprintf(buf, "(%3d, %3d, %3d)", col.r, col.g, col.b);
		font[FONT_S].drawString(buf, pos_Draw[2].x + img_LedDetectedArea.getWidth() + 10,  pos_Draw[2].y + TextHeight * 1);
		
		/* Line */
		ofSetColor(255, 255, 0, 255);
		ofSetLineWidth(1);
		ofDrawLine(pos_Draw[2].x, imgCoord_y + pos_Draw[2].y, pos_Draw[2].x + img_LedDetectedArea.getWidth(), imgCoord_y + pos_Draw[2].y); // draw時のoffsetに注意.
		ofDrawLine(imgCoord_x + pos_Draw[2].x,  pos_Draw[2].y, imgCoord_x + pos_Draw[2].x, pos_Draw[2].y + img_LedDetectedArea.getHeight());
	}
}

/******************************
******************************/
void ofApp::draw_HaltMark(){
	ofSetColor(255, 255, 0, 255);
	ofSetLineWidth(1);
	const int OverSize = 5;
	ofNoFill();
	ofDrawRectangle(pos_Draw[0].x - OverSize, pos_Draw[0].y - OverSize, IMG_S_WIDTH + OverSize * 2, IMG_S_HEIGHT + OverSize * 2);
}

/******************************
******************************/
void ofApp::drawMessage_CamSearchFailed(){
	ofBackground(0);
	ofSetColor(255, 0, 0, 255);
	
	char buf[BUF_SIZE_S];
	sprintf(buf, "USB Camera not Exsist");
	font[FONT_L].drawString(buf, ofGetWidth()/2 - font[FONT_L].stringWidth(buf)/2, ofGetHeight()/2);
}

/******************************
******************************/
void ofApp::draw_Calib()
{
	/********************
	********************/
	ofSetColor(255);
	img_Frame.draw(pos_Draw[0]);
	
	draw_RGB_ofThePoint_Calib();
	
	/********************
	********************/
	int NumImagesToDraw = 0;
	switch(State_Calib){
		case STATECALIB__WAITSHOOT_IMG_0:
			NumImagesToDraw = 0;
			break;
			
		case STATECALIB__WAITSHOOT_IMG_1:
			NumImagesToDraw = 1;
			break;
			
		case STATECALIB__FIN:
		{
			NumImagesToDraw = 2;
			
			/* */
			int TextHeight = font[FONT_M].stringHeight("(A,yq]") * 1.2;
			int _x = pos_Draw[1].x + IMG_S_WIDTH/2;
			
			char buf[BUF_SIZE_S];
			
			/* */
			ofSetColor(col_Evil);
			ofColor col = CalibResult_Evil.get_Color();
			sprintf(buf, "(%3d, %3d, %3d)->(%3.0f, %3.0f, %3.0f)", col.r, col.g, col.b, CalibResult_Evil.get_Hue(), CalibResult_Evil.get_Saturation(), CalibResult_Evil.get_Brightness() );
			font[FONT_M].drawString(buf, _x,  pos_Draw[1].y + TextHeight * 1);
			
			/* */
			ofSetColor(0, 255, 255); //ofSetColor(col_Calm);
			col = CalibResult_Calm.get_Color();
			sprintf(buf, "(%3d, %3d, %3d)->(%3.0f, %3.0f, %3.0f)", col.r, col.g, col.b, CalibResult_Calm.get_Hue(), CalibResult_Calm.get_Saturation(), CalibResult_Calm.get_Brightness() );
			font[FONT_M].drawString(buf, _x,  pos_Draw[1].y + TextHeight * 3);
		}
			break;
	}
	
	/********************
	********************/
	for(int i = 0; i < NumImagesToDraw; i++){
		ofSetColor(255);
		CalibImage[i].img.draw(pos_Draw[3 + i]);
		
		/* */
		int TextHeight = font[FONT_S].stringHeight("(A,yq]") * 1;
		
		char buf[BUF_SIZE_S];
		ofSetColor(col_Evil);
		sprintf(buf, "min to Evil = %d (%3d, %3d, %3d)", CalibImage[i].ret_MinSquareDistance__to_Evil, CalibImage[i].col_NearestTo_Evil.r, CalibImage[i].col_NearestTo_Evil.g, CalibImage[i].col_NearestTo_Evil.b);
		font[FONT_S].drawString(buf, pos_Draw[3 + i].x,  pos_Draw[3 + i].y + IMG_S_HEIGHT + TextHeight * 1);
		
		ofSetColor(0, 255, 255); // ofSetColor(col_Calm);
		sprintf(buf, "min to Calm = %d (%3d, %3d, %3d)", CalibImage[i].ret_MinSquareDistance__to_Calm, CalibImage[i].col_NearestTo_Calm.r, CalibImage[i].col_NearestTo_Calm.g, CalibImage[i].col_NearestTo_Calm.b);
		font[FONT_S].drawString(buf, pos_Draw[3 + i].x,  pos_Draw[3 + i].y + IMG_S_HEIGHT + TextHeight * 2);
	}
}

/******************************
******************************/
void ofApp::draw_Run()
{
	/********************
	********************/
	ofSetColor(255);
	img_Frame.draw(pos_Draw[0]);
	img_LedDetectedArea_Current.draw(pos_Draw[1]);
	img_LedDetectedArea.draw(pos_Draw[2]);
	draw_RGB_ofThePoint_Run();
	
	ofSetColor(255);
	img_Frame_Gray.draw(pos_Draw[3]);
	img_AbsDiff_BinGray.draw(pos_Draw[4]);
	img_BinGray_Cleaned.draw(pos_Draw[5]);	
	
	/********************
	********************/
	int TextHeight = font[FONT_S].stringHeight("(A,yq]") * 1.5;
	
	const ofColor col_NotDetected(120);
	const ofColor col_Detected(255);
	
	char buf[BUF_SIZE_S];
	
	/* */
	sprintf(buf, "Evil: %5d", State_Led_Evil_Raw.get__NumPix_Detected());
	if(State_Led_Evil_Raw.get_State() == STATE_ONOFF::STATE__DETECTED)	ofSetColor(col_Detected);
	else																ofSetColor(col_NotDetected);
	font[FONT_S].drawString(buf, pos_Draw[1].x + IMG_S_WIDTH + 10,  pos_Draw[1].y + IMG_S_HEIGHT - TextHeight);
	
	sprintf(buf, "Calm: %5d", State_Led_Calm_Raw.get__NumPix_Detected());
	if(State_Led_Calm_Raw.get_State() == STATE_ONOFF::STATE__DETECTED)	ofSetColor(col_Detected);
	else															ofSetColor(col_NotDetected);
	font[FONT_S].drawString(buf, pos_Draw[1].x + IMG_S_WIDTH + 10,  pos_Draw[1].y + IMG_S_HEIGHT);
	
	/* */
	sprintf(buf, "Evil: %5d (%5d - %5d)", State_Led_Evil.get__NumPix_Detected(), State_Led_Evil.get__pixRange_from(), State_Led_Evil.get__pixRange_to());
	if(State_Led_Evil.get_State() == STATE_ONOFF::STATE__DETECTED)	ofSetColor(col_Detected);
	else															ofSetColor(col_NotDetected);
	font[FONT_S].drawString(buf, pos_Draw[2].x + IMG_S_WIDTH + 10,  pos_Draw[2].y + IMG_S_HEIGHT - TextHeight);
	
	sprintf(buf, "Calm: %5d (%5d - %5d)", State_Led_Calm.get__NumPix_Detected(), State_Led_Calm.get__pixRange_from(), State_Led_Calm.get__pixRange_to());
	if(State_Led_Calm.get_State() == STATE_ONOFF::STATE__DETECTED)	ofSetColor(col_Detected);
	else															ofSetColor(col_NotDetected);
	font[FONT_S].drawString(buf, pos_Draw[2].x + IMG_S_WIDTH + 10,  pos_Draw[2].y + IMG_S_HEIGHT);
	
	/* */
	sprintf(buf, "%5d", State_Motion.get__NumPix_Detected());
	if(State_Motion.get_State() == STATE_ONOFF::STATE__DETECTED)	ofSetColor(col_Detected);
	else															ofSetColor(col_NotDetected);
	font[FONT_S].drawString(buf, pos_Draw[5].x + IMG_S_WIDTH + 10,  pos_Draw[5].y + IMG_S_HEIGHT);
}

/******************************
******************************/
void ofApp::draw(){
	// if(State_Overlook == STATEOVERLOOK__RUN) fprintf(fp_Log, "%d,", ofGetElapsedTimeMillis());
	
	/********************
	********************/
	if(b_CamSearchFailed){ drawMessage_CamSearchFailed(); return; }
	
	/********************
	加算合成とsmoothingは同時に効かない
	********************/
	/*
	ofEnableAlphaBlending();
	// ofEnableBlendMode(OF_BLENDMODE_ADD);
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	*/
	ofDisableAlphaBlending();
	ofEnableSmoothing();
	
	ofBackground(50);
	
	/* */
	if(b_HaltRGBVal_inCalib) { draw_HaltMark(); }
	
	/* */
	switch(State_Overlook){
		case STATEOVERLOOK__CALIB:
			draw_Calib();
			break;
			
		case STATEOVERLOOK__RUN:
			draw_Run();
			break;
	}
	
	/********************
	********************/
	Gui_Global->gui.draw();
	
	/********************
	********************/
	SendOSC_to_Effect();
	
	printf("%5.0f\r", ofGetFrameRate());
	fflush(stdout);
	
	// if(State_Overlook == STATEOVERLOOK__RUN) fprintf(fp_Log, "%d\n", ofGetElapsedTimeMillis());
}

/******************************
******************************/
void ofApp::SendOSC_to_Effect(){
	ofxOscMessage m;
	m.setAddress("/XBCam");
	
	m.addFloatArg((float)State_Led_Evil.get_State());
	m.addFloatArg((float)State_Led_Calm.get_State());
	m.addFloatArg((float)State_Motion.get_State());
	
	Osc_Effect.OscSend.sendMessage(m);
}

/******************************
******************************/
void ofApp::keyPressed(int key){
	/********************
	********************/
	KeyInputCommand.keyPressed(key);
	
	/********************
	********************/
	switch(key){
		case 'h':
			b_HaltRGBVal_inCalib = !b_HaltRGBVal_inCalib;
			break;
			
		case OF_KEY_RIGHT:
			MouseOffset.x++;
			break;
			
		case OF_KEY_LEFT:
			MouseOffset.x--;
			break;
			
		case OF_KEY_DOWN:
			MouseOffset.y++;
			break;
			
		case OF_KEY_UP:
			MouseOffset.y--;
			break;
	}
}

/******************************
******************************/
void ofApp::keyReleased(int key){

}

/******************************
******************************/
void ofApp::mouseMoved(int x, int y ){
	MouseOffset.x = 0;
	MouseOffset.y = 0;
}

/******************************
******************************/
void ofApp::mouseDragged(int x, int y, int button){

}

/******************************
******************************/
void ofApp::mousePressed(int x, int y, int button){

}

/******************************
******************************/
void ofApp::mouseReleased(int x, int y, int button){

}

/******************************
******************************/
void ofApp::mouseEntered(int x, int y){

}

/******************************
******************************/
void ofApp::mouseExited(int x, int y){

}

/******************************
******************************/
void ofApp::windowResized(int w, int h){

}

/******************************
******************************/
void ofApp::gotMessage(ofMessage msg){

}

/******************************
******************************/
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
