/************************************************************
************************************************************/
#include "sj_common.h"

/************************************************************
************************************************************/
/********************
********************/
int GPIO_0 = 0;
int GPIO_1 = 0;

/********************
********************/
GUI_GLOBAL* Gui_Global = NULL;

FILE* fp_Log = NULL;


/************************************************************
func
************************************************************/
/******************************
******************************/
double LPF(double LastVal, double CurrentVal, double Alpha_dt, double dt)
{
	double Alpha;
	if((Alpha_dt <= 0) || (Alpha_dt < dt))	Alpha = 1;
	else									Alpha = 1/Alpha_dt * dt;
	
	return CurrentVal * Alpha + LastVal * (1 - Alpha);
}

/******************************
******************************/
double LPF(double LastVal, double CurrentVal, double Alpha)
{
	if(Alpha < 0)		Alpha = 0;
	else if(1 < Alpha)	Alpha = 1;
	
	return CurrentVal * Alpha + LastVal * (1 - Alpha);
}

/******************************
******************************/
double sj_max(double a, double b)
{
	if(a < b)	return b;
	else		return a;
}


/************************************************************
class
************************************************************/

/******************************
******************************/
void GUI_GLOBAL::setup(string GuiName, string FileName, float x, float y)
{
	/********************
	********************/
	gui.setup(GuiName.c_str(), FileName.c_str(), x, y);
	
	/********************
	********************/
	Group_ImageProcess.setup("ImageProcess");
		Group_ImageProcess.add(BlurRadius_Frame.setup("Blur_Frame", 5, 0, 100));
		Group_ImageProcess.add(thresh_Diff_to_Bin.setup("thresh_Diff", 15, 1, 200));
		Group_ImageProcess.add(ErodeSize.setup("ErodeSize", 1, 0, 15));
	gui.add(&Group_ImageProcess);
	
	Group_DetectColor.setup("DetectColor");
		Group_DetectColor.add(thresh_Diff_Hue.setup("thresh_Diff_Hue", 7, 0, 180));
		Group_DetectColor.add(k_Saturation.setup("k_Saturation", 0.63, 0, 1.0));
		Group_DetectColor.add(k_Brightness.setup("k_Brightness", 0.75, 0, 1.0));
	gui.add(&Group_DetectColor);
	
	/********************
	********************/
	gui.minimizeAll();
}

