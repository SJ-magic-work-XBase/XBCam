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
, col_Target(ofColor(0, 0, 255))
, State_Led(0, IMG_S_WIDTH * IMG_S_HEIGHT, 300)
, State_Motion(0, IMG_S_WIDTH * IMG_S_HEIGHT, 100)
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
	
	setup_Camera();
	if(!b_CamSearchFailed){
		/********************
		********************/
		img_Frame.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		img_LedDetectedArea.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		img_Frame_Gray.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_LastFrame_Gray.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_AbsDiff_BinGray.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		img_BinGray_Cleaned.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_GRAYSCALE);
		
		img_Calib_Back.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
		img_Calib_Led.allocate(IMG_S_WIDTH, IMG_S_HEIGHT, OF_IMAGE_COLOR);
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
	Gui_Global->setup("XBC", "gui.xml", 1000, 10);
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
}

/******************************
******************************/
void ofApp::update_img_OnCam(){
	ofxCv::copy(cam, img_Frame);
	if(b_flipCamera) img_Frame.mirror(false, true);
	img_Frame.update();
	
	img_LastFrame_Gray = img_Frame_Gray;
	// img_LastFrame_Gray.update(); // drawしないので不要.
	
	convertColor(img_Frame, img_Frame_Gray, CV_RGB2GRAY);
	ofxCv::blur(img_Frame_Gray, ForceOdd((int)Gui_Global->BlurRadius_Frame));
	img_Frame_Gray.update();
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
				State_Calib = STATECALIB__WAITSHOOT_BACK_IMG;
				
				State_Led.Reset();
				State_Motion.Reset();
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
		case STATECALIB__WAITSHOOT_BACK_IMG:
			if(KeyInputCommand.Is_ShootImg()){
				img_Calib_Back = img_Frame;
				State_Calib = STATECALIB__WAITSHOOT_LED_IMG;
			}
			break;
			
		case STATECALIB__WAITSHOOT_LED_IMG:
			if(KeyInputCommand.Is_RetryCalib()){
				State_Calib = STATECALIB__WAITSHOOT_BACK_IMG;
			}else if(KeyInputCommand.Is_ShootImg()){
				img_Calib_Led = img_Frame;
				
				if(CalThresh__ifTargetColorExist())	State_Calib = STATECALIB__FIN;
				else								State_Calib = STATECALIB__WAITSHOOT_BACK_IMG;
			}
			break;
			
		case STATECALIB__FIN:
			if(KeyInputCommand.Is_RetryCalib()){
				State_Calib = STATECALIB__WAITSHOOT_BACK_IMG;
			}else if(KeyInputCommand.Is_Enter()){
				State_Overlook = STATEOVERLOOK__RUN;
				State_Calib = STATECALIB__WAITSHOOT_BACK_IMG; // 一応.
				
				Judge_if_LedExist(true); // set pix Range of State_Led.
				State_Led.Reset();
				State_Motion.Reset();
			}
			break;
	}
}

/******************************
******************************/
bool ofApp::CalThresh__ifTargetColorExist(){
	/********************
	openCv上でmarking : ofImageには直接描画できないので.
	
	cv::circle
		http://opencv.jp/opencv-2svn/cpp/drawing_functions.html#cv-line
		https://cvtech.cc/opencv/3/
	********************/
	ofPoint ret_pos;
	
	Search_MinSquareDistance_to_TargetCol(img_Calib_Back, ret_MinSquareDistance__Back, ret_pos);
	{
		cv::Mat Mat_temp = toCv(img_Calib_Back);
		cv::circle(Mat_temp, cv::Point(ret_pos.x, ret_pos.y), 4/* radius */, cv::Scalar(255, 0, 0), 1/* thickness : -1で塗りつぶし */, CV_AA);
		img_Calib_Back.update(); // apply to texture.
	}
	
	/********************
	openCv上でmarking : ofImageには直接描画できないので.
	
	cv::circle
		http://opencv.jp/opencv-2svn/cpp/drawing_functions.html#cv-line
		https://cvtech.cc/opencv/3/
	********************/
	Search_MinSquareDistance_to_TargetCol(img_Calib_Led, ret_MinSquareDistance__Led, ret_pos);
	{
		cv::Mat Mat_temp = toCv(img_Calib_Led);
		cv::circle(Mat_temp, cv::Point(ret_pos.x, ret_pos.y), 4/* radius */, cv::Scalar(255, 0, 0), 1/* thickness : -1で塗りつぶし */, CV_AA);
		img_Calib_Led.update(); // apply to texture.
	}
	
	/********************
	********************/
	const int margin = 5000;
	const double k = 0.3;
	if(ret_MinSquareDistance__Back < ret_MinSquareDistance__Led + margin){
		printf("(Back, Led) = (%d, %d)\n", ret_MinSquareDistance__Back, ret_MinSquareDistance__Led);
		fflush(stdout);
		
		return false;
	}else{
		thresh_ifTargetColorExist = (int)( (1 - k) * ret_MinSquareDistance__Led + k * ret_MinSquareDistance__Back );
		
		printf("\n(Back, Led) = (%d, %d) : diff = %d : thresh = %d\n", ret_MinSquareDistance__Back, ret_MinSquareDistance__Led, ret_MinSquareDistance__Back - ret_MinSquareDistance__Led, thresh_ifTargetColorExist);
		fflush(stdout);
		
		return true;
	}
}

/******************************
******************************/
void ofApp::ImageProcessing()
{
	Judge_if_LedExist();
	
	Judge_if_MotionExist();
}

/******************************
******************************/
void ofApp::Judge_if_LedExist(bool b_Update_pix_Range)
{
	/********************
	meas result : max = 5ms : 320 x 180
	********************/
	// fprintf(fp_Log, "%d,", ofGetElapsedTimeMillis());
	
	ofPixels& pix_Frame = img_Frame.getPixels();
	ofPixels& pix_LedDetectedArea = img_LedDetectedArea.getPixels();
	
	int counter = 0;
	for(int _x = 0; _x < img_Frame.getWidth(); _x++){
		for(int _y = 0; _y < img_Frame.getHeight(); _y++){
			ofColor col_Frame = pix_Frame.getColor(_x, _y);
			int SquareDistance = cal_SquareDistance_to_TargetCol(col_Frame);
			if(SquareDistance < thresh_ifTargetColorExist){
				counter++;
				pix_LedDetectedArea.setColor(_x, _y, col_Target);
			}else{
				pix_LedDetectedArea.setColor(_x, _y, ofColor(0, 0, 0));
			}
		}
	}
	
	State_Led.update(counter, now);
	if(b_Update_pix_Range) State_Led.set_Range(0, counter * 5);
	
	img_LedDetectedArea.update();
	
	// fprintf(fp_Log, "%d\n", ofGetElapsedTimeMillis());
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
	
	cv::erode(MatSrc, MatCleaned, Mat(), cv::Point(-1, -1), (int)Gui_Global->ErodeSize); // 白に黒が侵食 : Clean
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
void ofApp::Search_MinSquareDistance_to_TargetCol(ofImage& img, int& ret_MinSquareDistance, ofPoint& ret_pos)
{
	/********************
	meas result : max = 2ms : 320 x 180
	********************/
	// fprintf(fp_Log, "%d,", ofGetElapsedTimeMillis());
	
	ofPixels& pix = img.getPixels();
	
	ofColor col = pix.getColor(0, 0);
	ret_MinSquareDistance = cal_SquareDistance_to_TargetCol(col);
	ret_pos = ofPoint(0, 0);
	
	for(int _x = 0; _x < img.getWidth(); _x++){
		for(int _y = 0; _y < img.getHeight(); _y++){
			ofColor col = pix.getColor(_x, _y);
			int temp = cal_SquareDistance_to_TargetCol(col);
			
			if(temp < ret_MinSquareDistance){
				ret_MinSquareDistance = temp;
				ret_pos.x = _x;
				ret_pos.y = _y;
			}
		}
	}
	
	// fprintf(fp_Log, "%d\n", ofGetElapsedTimeMillis());
}

/******************************
******************************/
int ofApp::cal_SquareDistance_to_TargetCol(ofColor& col)
{
	int SquareDistance = (int)pow((col_Target.r - col.r), 2) + (int)pow((col_Target.g - col.g), 2) + (int)pow((col_Target.b - col.b), 2);
	return SquareDistance;
}

/******************************
warning
	ofDrawLine()などの描画は、fboなど介さず直接drawしているので、offset関連に注意.
******************************/
void ofApp::draw_RGB_ofThePoint()
{
	/********************
	********************/
	static ofImage LastImg;
	
	/********************
	********************/
	/* pos in img_Frame */
	int imgCoord_x = mouseX + MouseOffset.x - pos_Draw[0].x;
	int imgCoord_y = mouseY + MouseOffset.y - pos_Draw[0].y;
	
	if( (0 <= imgCoord_x) && (imgCoord_x < img_Frame.getWidth()) && (0 <= imgCoord_y) && (imgCoord_y < img_Frame.getHeight()) ){
		ofColor col;
		if(b_HaltRGBVal_inCalib)	col = LastImg.getPixels().getColor(imgCoord_x, imgCoord_y);
		else						col = img_Frame.getPixels().getColor(imgCoord_x, imgCoord_y);
		
		char buf[BUF_SIZE_S];
		if(b_HaltRGBVal_inCalib){
			int ret_MinSquareDistance;
			ofPoint ret_pos;
			Search_MinSquareDistance_to_TargetCol(LastImg, ret_MinSquareDistance, ret_pos);	// 本methodは時間costが高い(2msだが)ので、もっと上手く対応することもできるが、
																							// haltはdebug用途であり、あまり通らないので、このままでok.
			
			sprintf(buf, "(%3d, %3d, %3d) : [halted] : SquareDistance = %d : minDitance = %d", col.r, col.g, col.b, cal_SquareDistance_to_TargetCol(col), ret_MinSquareDistance);
			
			ofSetColor(255, 0, 0, 200);
			ofSetLineWidth(1);
			ofDrawCircle(ret_pos + pos_Draw[0], 2); // ret_posは、imgの座標なので、draw時にはは、offsetさせる点に注意
			
		}else{
			sprintf(buf, "(%3d, %3d, %3d)", col.r, col.g, col.b);
			LastImg = img_Frame;
		}
		ofSetColor(200);
		font[FONT_M].drawString(buf, pos_Draw[0].x + img_Frame.getWidth() + 10,  pos_Draw[0].y + img_Frame.getHeight() - 10);
		
		ofSetColor(255, 255, 0, 255);
		ofSetLineWidth(1);
		ofDrawLine(pos_Draw[0].x, imgCoord_y + pos_Draw[0].y, pos_Draw[0].x + img_Frame.getWidth(), imgCoord_y + pos_Draw[0].y); // draw時のoffsetに注意.
		ofDrawLine(imgCoord_x + pos_Draw[0].x,  pos_Draw[0].y, imgCoord_x + pos_Draw[0].x, pos_Draw[0].y + img_Frame.getHeight());
	}
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
	
	draw_RGB_ofThePoint();
	
	/********************
	********************/
	ofSetColor(255);
	switch(State_Calib){
		case STATECALIB__WAITSHOOT_BACK_IMG:
			break;
			
		case STATECALIB__WAITSHOOT_LED_IMG:
			img_Calib_Back.draw(pos_Draw[3]);
			break;
			
		case STATECALIB__FIN:
		{
			img_Calib_Back.draw(pos_Draw[3]);
			img_Calib_Led.draw(pos_Draw[4]);
			
			/* */
			char buf[BUF_SIZE_S];
			sprintf(buf, "(Back, Led) = (%d, %d) : Diff = %d : thresh = %d\n", ret_MinSquareDistance__Back, ret_MinSquareDistance__Led, ret_MinSquareDistance__Back - ret_MinSquareDistance__Led, thresh_ifTargetColorExist);
			ofSetColor(200);
			font[FONT_M].drawString(buf, pos_Draw[4].x,  pos_Draw[4].y + IMG_S_HEIGHT + font[FONT_M].stringHeight("ABC") * 1.5);
		}
			break;
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
	
	img_LedDetectedArea.draw(pos_Draw[1]);
	
	img_Frame_Gray.draw(pos_Draw[3]);
	img_AbsDiff_BinGray.draw(pos_Draw[4]);
	img_BinGray_Cleaned.draw(pos_Draw[5]);	
	
	/********************
	********************/
	const ofColor col_detected(0, 150, 255);
	
	char buf[BUF_SIZE_S];
	sprintf(buf, "%5d (%5d - %5d)", State_Led.get__NumPix_Detected(), State_Led.get__pixRange_from(), State_Led.get__pixRange_to());
	if(State_Led.get_State() == STATE_ONOFF::STATE__DETECTED)	ofSetColor(col_detected);
	else														ofSetColor(150);
	font[FONT_M].drawString(buf, pos_Draw[1].x + IMG_S_WIDTH + 10,  pos_Draw[1].y + IMG_S_HEIGHT);
	
	sprintf(buf, "%5d", State_Motion.get__NumPix_Detected());
	if(State_Motion.get_State() == STATE_ONOFF::STATE__DETECTED)	ofSetColor(col_detected);
	else															ofSetColor(150);
	font[FONT_M].drawString(buf, pos_Draw[5].x + IMG_S_WIDTH + 10,  pos_Draw[5].y + IMG_S_HEIGHT);
}

/******************************
******************************/
void ofApp::draw(){
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
	
	printf("%5d, %5d : %3d, %5d : %3d, %5d : %5.0f\r", State_Overlook, State_Calib, State_Led.get_State(), State_Led.get__NumPix_Detected(), State_Motion.get_State(), State_Motion.get__NumPix_Detected(), ofGetFrameRate());
	fflush(stdout);
}

/******************************
******************************/
void ofApp::SendOSC_to_Effect(){
	ofxOscMessage m;
	m.setAddress("/XBCam");
	
	m.addFloatArg((float)State_Led.get_State());
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
