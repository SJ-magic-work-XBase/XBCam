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


/************************************************************
■精度検証 結果
-	Rと肌の色が近いので注意が必要
	-	Gを使った方がいいかも
	-	Camの見える空間を暗くすれば、肌のR成分が減り、精度upするかもしれない
	
-	そもそもRのLedがRとして認識し辛い
	-	thresh_B < thresh_R としてRは認識し易くする必要あり
	-	これは、肌の色をより検出し易い方向なので、その点は注意してthreshを決める必要あり
	-	本番では、Rの検出が上手く行かなくなったら、Bのみで運用で一時的な回避はできる
	
-	Bの検出はGood
-	Cam位置は、割と近めでないと精度に影響が大きいだろう
-	Ledの数は、多めでないと上手く検出できなそう。Sheet(10個) 1枚そのまま使うくらい
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
	const int THRESH_TIME_OFFtoON;
	const int THRESH_TIME_ONtoOFF;
	int NumPix_Detected = 0;
	
	bool IsInRange(){
		if( (RANGE_NUM_PIX_FROM < NumPix_Detected) && (NumPix_Detected < RANGE_NUM_PIX_TO) )	return true;
		else																					return false;
	}
	
public:
	enum STATE{
		STATE__NOT_DETECTED,
		STATE__DETECTED,
	};
	
	STATE State = STATE__NOT_DETECTED;
	
	STATE_ONOFF(int _RANGE_NUM_PIX_FROM, int _RANGE_NUM_PIX_TO, int _THRESH_TIME_OFFtoON, int _THRESH_TIME_ONtoOFF)
	: RANGE_NUM_PIX_FROM(_RANGE_NUM_PIX_FROM)
	, RANGE_NUM_PIX_TO(_RANGE_NUM_PIX_TO)
	, THRESH_TIME_OFFtoON(_THRESH_TIME_OFFtoON)
	, THRESH_TIME_ONtoOFF(_THRESH_TIME_ONtoOFF)
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
				if(IsInRange()){
					if(THRESH_TIME_OFFtoON < now - t_from_ms){
						State = STATE__DETECTED;
						t_from_ms = now;
					}
				}else{
					t_from_ms = now;
				}
				break;
				
			case STATE__DETECTED:
				if(IsInRange()){
					t_from_ms = now;
				}else{
					if(THRESH_TIME_ONtoOFF < now - t_from_ms){
						State = STATE__NOT_DETECTED;
						t_from_ms = now;
					}
				}
				break;			
		}
	}
	
	int get_State()				{ return (int)State; }
	int get__NumPix_Detected()	{ return NumPix_Detected; }
	int get__pixRange_from()	{ return RANGE_NUM_PIX_FROM; }
	int get__pixRange_to()		{ return RANGE_NUM_PIX_TO; }
	
	void Reset(int now) { State = STATE__NOT_DETECTED; NumPix_Detected = 0; t_from_ms = now;  }
};

/**************************************************
**************************************************/
struct CALIB_IMG{
	ofImage img;
	
	int ret_MinSquareDistance__to_Evil;
	ofColor col_NearestTo_Evil;
	
	int ret_MinSquareDistance__to_Calm;
	ofColor col_NearestTo_Calm;
};

/**************************************************
description
	thresh, Diff, Margin は、都度算出する(kで値が変わる)。
**************************************************/
class CALIB_RESULT{
private:
	ofColor col;
	
	float Hue;
	float Saturation;
	float Brightness;
	
public:
	void set(const ofColor& _col){
		col = _col;
		
		Hue = col.getHueAngle();
		Saturation = col.getSaturation();
		Brightness = col.getBrightness();
	}
	
	ofColor get_Color() const	{ return col; }
	
	float get_deltaHue(float _H) const
	{
		float delta = abs(_H - Hue);
		if(180 <= delta) delta = 360 - delta;
		return delta;
	}
	
	float get_Hue() const	{ return Hue; }
	float get_Saturation() const	{ return Saturation; }
	float get_Brightness() const	{ return Brightness; }
};

/**************************************************
**************************************************/
class ofApp : public ofBaseApp{
private:
	/****************************************
	****************************************/
	enum{ FONT_S, FONT_M, FONT_L, FONT_LL, NUM_FONTSIZE, };
	enum{ NUM_DRAW_POS = 6, };
	enum{ NUM_CALIB_IMGS = 2, };
	
	enum STATE_OVERLOOK{
		STATEOVERLOOK__CALIB,
		STATEOVERLOOK__RUN,
	};
	enum STATE_CALIB{
		STATECALIB__WAITSHOOT_IMG_0,
		STATECALIB__WAITSHOOT_IMG_1,
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
	
	/* for Auto Search of  Cam device */
	bool b_CamSearchFailed = false;
	float t_CamSearchFailed;
	
	/********************
	********************/
	bool b_HaltRGBVal_inCalib = false;
	ofVec2f MouseOffset;
	
	/********************
	********************/
	const ofColor col_Evil;
	const ofColor col_Calm;
	
	CALIB_IMG CalibImage[NUM_CALIB_IMGS];
	
	CALIB_RESULT CalibResult_Evil;
	CALIB_RESULT CalibResult_Calm;
	
	/********************
	********************/
	KEYINPUT_COMMAND KeyInputCommand;
	
	/********************
	********************/
    ofImage img_Frame;
    ofImage img_LedDetectedArea_Last;
    ofImage img_LedDetectedArea_Current;
    ofImage img_LedDetectedArea_Current_Calm;
    ofImage img_LedDetectedArea_Current_Evil;
    ofImage img_LedDetectedArea;
    ofImage img_Frame_Gray;
    ofImage img_LastFrame_Gray;
    ofImage img_AbsDiff_BinGray;
    ofImage img_BinGray_Cleaned;
	
	/********************
	********************/
	STATE_OVERLOOK State_Overlook = STATEOVERLOOK__CALIB;
	STATE_CALIB State_Calib = STATECALIB__WAITSHOOT_IMG_0;
	
	STATE_ONOFF State_Led_Evil;
	STATE_ONOFF State_Led_Evil_Raw;
	STATE_ONOFF State_Led_Calm;
	STATE_ONOFF State_Led_Calm_Raw;
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
	void draw_RGB_ofThePoint_Calib();
	void draw_RGB_ofThePoint_Run();	
	int cal_SquareDistance(const ofColor& col_0, const ofColor& col_1);
	void Search_MinSquareDistance_to_TargetCol(ofImage& img,  const ofColor& TargetCol, int& ret_MinSquareDistance, ofPoint& ret_pos, ofColor& ret_Col_Nearest);
	void draw_Calib();
	void draw_Run();
	void update__img_LedDetected_Current(ofImage& _img_LedDetectedArea_Current, const CALIB_RESULT& CalibResult, const ofColor& col_Target);
	void Judge_if_LedExist();
	void Judge_if_MotionExist();
	void SendOSC_to_Effect();
	void CalAndMark_MinDistance(CALIB_IMG& _CalibImage);
	static int int_sort( const void * a , const void * b );
	void draw_HaltMark();
	void save_NearestColor();
	bool isThisColor_Judged_as_Led(const CALIB_RESULT& CalibResult, const ofColor& col);
	bool isFile_Exist(char* FileName);

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
