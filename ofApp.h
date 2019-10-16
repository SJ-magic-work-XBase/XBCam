/************************************************************
■ofxCv
	https://github.com/kylemcdonald/ofxCv
	
	note
		Your linker will also need to know where the OpenCv headers are. In XCode this means modifying one line in Project.xconfig:
			HEADER_SEARCH_PATHS = $(OF_CORE_HEADERS) "../../../addons/ofxOpenCv/libs/opencv/include/" "../../../addons/ofxCv/libs/ofxCv/include/"
			
■openFrameworksのAddon、ofxCvの導入メモ
	https://qiita.com/nenjiru/items/50325fabe4c3032da270
	
	contents
		導入時に、一手間 必要.
		
■Kill camera process
	> sudo killall VDCAssistant
************************************************************/
#pragma once

/************************************************************
************************************************************/
#include "ofMain.h"
#include "ofxCv.h"

#include "ofxOsc_BiDirection.h"

#include "sj_common.h"


/************************************************************
************************************************************/

/**************************************************
**************************************************/
class KEYINPUT_COMMAND : private Noncopyable{
private:
	enum{
		COMMAND__SHOOT_IMG,
		COMMAND__RETRY_CALIB,
		COMMAND__ENTER,
		
		NUM_COMMANDS,
	};
	
	bool b_Command[NUM_COMMANDS];
	
public:
	KEYINPUT_COMMAND()
	{
		Reset();
	}
	
	void Reset(){
		for(int i = 0; i < NUM_COMMANDS; i++){
			b_Command[i] = false;
		}
	}
	
	void keyPressed(int key){
		switch(key){
			case OF_KEY_RETURN:
				b_Command[COMMAND__ENTER] = true;
				break;
				
			case 'r':
				b_Command[COMMAND__RETRY_CALIB] = true;				
				break;
				
			case ' ':
				b_Command[COMMAND__SHOOT_IMG] = true;				
				break;
		}
	}
	
	bool Is_ShootImg()		{ return b_Command[COMMAND__SHOOT_IMG]; }
	bool Is_Enter()			{ return b_Command[COMMAND__ENTER]; }
	bool Is_RetryCalib()	{ return b_Command[COMMAND__RETRY_CALIB]; }
};

/**************************************************
**************************************************/
class STATE_ONOFF : private Noncopyable{
private:
	int RANGE_NUM_PIX_FROM;
	int RANGE_NUM_PIX_TO;
	
	int t_from_ms = 0;
	const int THRESH_TIME;
	int NumPix_Detected = 0;
	
public:
	enum STATE{
		STATE__NOT_DETECTED,
		STATE__DETECTED,
	};
	
	STATE State = STATE__NOT_DETECTED;
	
	STATE_ONOFF(int _RANGE_NUM_PIX_FROM, int _RANGE_NUM_PIX_TO, int _THRESH_TIME)
	: RANGE_NUM_PIX_FROM(_RANGE_NUM_PIX_FROM)
	, RANGE_NUM_PIX_TO(_RANGE_NUM_PIX_TO)
	, THRESH_TIME(_THRESH_TIME)
	{
	}
	
	void set_Range(int _RANGE_NUM_PIX_FROM, int _RANGE_NUM_PIX_TO){
		RANGE_NUM_PIX_FROM = _RANGE_NUM_PIX_FROM;
		RANGE_NUM_PIX_TO = _RANGE_NUM_PIX_TO;
	}
	
	void update(int _NumPix_Detected, int now){
		NumPix_Detected = _NumPix_Detected;
		switch(State){
			case STATE__NOT_DETECTED:
				if( (RANGE_NUM_PIX_FROM < NumPix_Detected) && (NumPix_Detected < RANGE_NUM_PIX_TO) ){
					State = STATE__DETECTED;
					t_from_ms = now;
				}
				break;
				
			case STATE__DETECTED:
				if( (RANGE_NUM_PIX_FROM < NumPix_Detected) && (NumPix_Detected < RANGE_NUM_PIX_TO) ){
					t_from_ms = now;
				}else{
					if(THRESH_TIME < now - t_from_ms){
						State = STATE__NOT_DETECTED;
					}
				}
				break;			
		}
	}
	
	int get_State()				{ return (int)State; }
	int get__NumPix_Detected()	{ return NumPix_Detected; }
	int get__pixRange_from()	{ return RANGE_NUM_PIX_FROM; }
	int get__pixRange_to()		{ return RANGE_NUM_PIX_TO; }
	
	void Reset() { State = STATE__NOT_DETECTED; NumPix_Detected = 0; }
};

/**************************************************
**************************************************/
class ofApp : public ofBaseApp{
private:
	/****************************************
	****************************************/
	enum{ FONT_S, FONT_M, FONT_L, FONT_LL, NUM_FONTSIZE, };
	enum{ NUM_DRAW_POS = 6, };
	
	enum STATE_OVERLOOK{
		STATEOVERLOOK__CALIB,
		STATEOVERLOOK__RUN,
	};
	enum STATE_CALIB{
		STATECALIB__WAITSHOOT_BACK_IMG,
		STATECALIB__WAITSHOOT_LED_IMG,
		STATECALIB__FIN,
	};
	
	/****************************************
	****************************************/
	/********************
	********************/
	ofTrueTypeFont font[NUM_FONTSIZE];
	
	int now;
	OSC_TARGET Osc_Effect;
	
	/********************
	********************/
	ofVideoGrabber cam;
	int Cam_id;
	
	bool b_flipCamera;
	
	bool b_CamSearchFailed = false;
	float t_CamSearchFailed;
	
	/********************
	********************/
	bool b_HaltRGBVal_inCalib = false;
	const ofColor col_Target;
	ofVec2f MouseOffset;
	
	/********************
	********************/
	int thresh_ifTargetColorExist;
	int ret_MinSquareDistance__Back;
	int ret_MinSquareDistance__Led;
	
	KEYINPUT_COMMAND KeyInputCommand;
	
	/********************
	********************/
    ofImage img_Frame;
    ofImage img_LedDetectedArea;
    ofImage img_Frame_Gray;
    ofImage img_LastFrame_Gray;
    ofImage img_AbsDiff_BinGray;
    ofImage img_BinGray_Cleaned;
	
    ofImage img_Calib_Back;
    ofImage img_Calib_Led;
	
	
	/********************
	********************/
	STATE_OVERLOOK State_Overlook = STATEOVERLOOK__CALIB;
	STATE_CALIB State_Calib = STATECALIB__WAITSHOOT_BACK_IMG;
	
	STATE_ONOFF State_Led;
	STATE_ONOFF State_Motion;
	
	/********************
	********************/
	ofPoint pos_Draw[NUM_DRAW_POS] = {
		ofPoint(80, 40),
		ofPoint(480, 40),
		ofPoint(880, 40),
		ofPoint(80, 260),
		ofPoint(480, 260),
		ofPoint(880, 260),
	};
	
	/****************************************
	****************************************/
	void setup_Gui();
	void setup_Camera();
	void clear_fbo(ofFbo& fbo);
	void update_img_OnCam();
	void ImageProcessing();
	void StateChart_overlook();
	void StateChart_Calib();
	int ForceOdd(int val);
	void drawMessage_CamSearchFailed();
	void draw_RGB_ofThePoint();
	int cal_SquareDistance_to_TargetCol(ofColor& col);
	void Search_MinSquareDistance_to_TargetCol(ofImage& img, int& ret_Min_SquareDistance, ofPoint& ret_pos);
	bool CalThresh__ifTargetColorExist();
	void draw_Calib();
	void draw_Run();
	void Judge_if_LedExist(bool b_Update_pix_Range = false);
	void Judge_if_MotionExist();
	void SendOSC_to_Effect();

public:
	/****************************************
	****************************************/
	ofApp(int _Cam_id);
	~ofApp();

	void setup();
	void update();
	void draw();
	void exit();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);
	
};
