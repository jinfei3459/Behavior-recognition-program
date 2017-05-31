
/************************************************************************/
/*			A few more thoughts on codebook models
In general, the codebook method works quite well across a wide number of conditions,
and it is relatively quick to train and to run. It doesn��t deal well with varying patterns of
light �� such as morning, noon, and evening sunshine �� or with someone turning lights
on or off indoors. This type of global variability can be taken into account by using
several different codebook models, one for each condition, and then allowing the condition
to control which model is active.		                                */
/************************************************************************/

//#include "stdafx.h"
#include <cv.h>
#include <highgui.h>
#include <cxcore.h>

#define CHANNELS 3
// ���ô����ͼ��ͨ����,Ҫ��С�ڵ���ͼ�����ͨ����

///////////////////////////////////////////////////////////////////////////
// ����Ϊ�뱾��Ԫ�����ݽṹ
// ����ͼ��ʱÿ�����ض�Ӧһ���뱾,ÿ���뱾�п������ɸ���Ԫ
// ���漰һ��������,ͨ��������һЩ��ֵ�����,��Ҫ����Щ�����Ż�,��ʵ˼·���Ǽ򵥵�
typedef struct ce {
	uchar	learnHigh[CHANNELS];	// High side threshold for learning
	// ����Ԫ��ͨ���ķ�ֵ����(ѧϰ����)
	uchar	learnLow[CHANNELS];		// Low side threshold for learning
	// ����Ԫ��ͨ���ķ�ֵ����
	// ѧϰ���������һ�������ظ�ͨ��ֵx[i],���� learnLow[i]<=x[i]<=learnHigh[i],������ؿɺϲ��ڴ���Ԫ
	uchar	max[CHANNELS];			// High side of box boundary
	// ���ڴ���Ԫ�������и�ͨ�������ֵ
	uchar	min[CHANNELS];			// Low side of box boundary
	// ���ڴ���Ԫ�������и�ͨ������Сֵ
	int		t_last_update;			// This is book keeping to allow us to kill stale entries
	// ����Ԫ���һ�θ��µ�ʱ��,ÿһ֡Ϊһ����λʱ��,���ڼ���stale
	int		stale;					// max negative run (biggest period of inactivity)
	// ����Ԫ�������ʱ��,����ɾ���涨ʱ�䲻���µ���Ԫ,�����뱾
} code_element;						// ��Ԫ�����ݽṹ

typedef struct code_book {
	code_element	**cb;
	// ��Ԫ�Ķ�άָ��,���Ϊָ����Ԫָ�������ָ��,ʹ�������Ԫʱ����Ҫ���ظ�����Ԫ,ֻ��Ҫ�򵥵�ָ�븳ֵ����
	int				numEntries;
	// ���뱾����Ԫ����Ŀ
	int				t;				// count every access
	// ���뱾���ڵ�ʱ��,һ֡Ϊһ��ʱ�䵥λ
} codeBook;							// �뱾�����ݽṹ


///////////////////////////////////////////////////////////////////////////////////
// int updateCodeBook(uchar *p, codeBook &c, unsigned cbBounds)
// Updates the codebook entry with a new data point
//
// p			Pointer to a YUV pixel
// c			Codebook for this pixel
// cbBounds		Learning bounds for codebook (Rule of thumb: 10)
// numChannels	Number of color channels we're learning
//
// NOTES:
//		cvBounds must be of size cvBounds[numChannels]
//
// RETURN
//	codebook index
int cvupdateCodeBook(uchar *p, codeBook &c, unsigned *cbBounds, int numChannels)
{
	if(c.numEntries == 0) c.t = 0;
	// �뱾����ԪΪ��ʱ��ʼ��ʱ��Ϊ0
	c.t += 1;	// Record learning event
	// ÿ����һ�μ�һ,��ÿһ֡ͼ���һ

	//SET HIGH AND LOW BOUNDS
	int n;
	unsigned int high[3],low[3];
	for (n=0; n<numChannels; n++)
	{
		high[n] = *(p+n) + *(cbBounds+n);
		// *(p+n) �� p[n] ����ȼ�,������*(p+n) �ٶȸ���
		if(high[n] > 255) high[n] = 255;
		low[n] = *(p+n)-*(cbBounds+n);
		if(low[n] < 0) low[n] = 0;
		// ��p ��ָ����ͨ������,�Ӽ�cbBonds����ֵ,��Ϊ�����ط�ֵ��������
	}

	//SEE IF THIS FITS AN EXISTING CODEWORD
	int matchChannel;
	int i;
	for (i=0; i<c.numEntries; i++)
	{
		// �������뱾ÿ����Ԫ,����p�����Ƿ���������֮һ
		matchChannel = 0;
		for (n=0; n<numChannels; n++)
			//����ÿ��ͨ��
		{
			if((c.cb[i]->learnLow[n] <= *(p+n)) && (*(p+n) <= c.cb[i]->learnHigh[n])) //Found an entry for this channel
			// ���p ����ͨ�������ڸ���Ԫ��ֵ������֮��
			{
				matchChannel++;
			}
		}
		if (matchChannel == numChannels)		// If an entry was found over all channels
			// ���p ���ظ�ͨ����������������
		{
			c.cb[i]->t_last_update = c.t;
			// ���¸���Ԫʱ��Ϊ��ǰʱ��
			// adjust this codeword for the first channel
			for (n=0; n<numChannels; n++)
				//��������Ԫ��ͨ�������Сֵ
			{
				if (c.cb[i]->max[n] < *(p+n))
					c.cb[i]->max[n] = *(p+n);
				else if (c.cb[i]->min[n] > *(p+n))
					c.cb[i]->min[n] = *(p+n);
			}
			break;
		}
	}

	// ENTER A NEW CODE WORD IF NEEDED
	if(i == c.numEntries)  // No existing code word found, make a new one
	// p ���ز�������뱾���κ�һ����Ԫ,���洴��һ������Ԫ
	{
		code_element **foo = new code_element* [c.numEntries+1];
		// ����c.numEntries+1 ��ָ����Ԫ��ָ��
		for(int ii=0; ii<c.numEntries; ii++)
			// ��ǰc.numEntries ��ָ��ָ���Ѵ��ڵ�ÿ����Ԫ
			foo[ii] = c.cb[ii];

		foo[c.numEntries] = new code_element;
		// ����һ���µ���Ԫ
		if(c.numEntries) delete [] c.cb;
		// ɾ��c.cb ָ������
		c.cb = foo;
		// ��foo ͷָ�븳��c.cb
		for(n=0; n<numChannels; n++)
			// ��������Ԫ��ͨ������
		{
			c.cb[c.numEntries]->learnHigh[n] = high[n];
			c.cb[c.numEntries]->learnLow[n] = low[n];
			c.cb[c.numEntries]->max[n] = *(p+n);
			c.cb[c.numEntries]->min[n] = *(p+n);
		}
		c.cb[c.numEntries]->t_last_update = c.t;
		c.cb[c.numEntries]->stale = 0;
		c.numEntries += 1;
	}

	// OVERHEAD TO TRACK POTENTIAL STALE ENTRIES
	for(int s=0; s<c.numEntries; s++)
	{
		// This garbage is to track which codebook entries are going stale
		int negRun = c.t - c.cb[s]->t_last_update;
		// �������Ԫ�Ĳ�����ʱ��
		if(c.cb[s]->stale < negRun)
			c.cb[s]->stale = negRun;
	}

	// SLOWLY ADJUST LEARNING BOUNDS
	for(n=0; n<numChannels; n++)
		// �������ͨ�������ڸߵͷ�ֵ��Χ��,������Ԫ��ֵ֮��,������������Ԫѧϰ����
	{
		if(c.cb[i]->learnHigh[n] < high[n])
			c.cb[i]->learnHigh[n] += 1;
		if(c.cb[i]->learnLow[n] > low[n])
			c.cb[i]->learnLow[n] -= 1;
	}

	return(i);
}

///////////////////////////////////////////////////////////////////////////////////
// uchar cvbackgroundDiff(uchar *p, codeBook &c, int minMod, int maxMod)
// Given a pixel and a code book, determine if the pixel is covered by the codebook
//
// p		pixel pointer (YUV interleaved)
// c		codebook reference
// numChannels  Number of channels we are testing
// maxMod	Add this (possibly negative) number onto max level when code_element determining if new pixel is foreground
// minMod	Subract this (possible negative) number from min level code_element when determining if pixel is foreground
//
// NOTES:
// minMod and maxMod must have length numChannels, e.g. 3 channels => minMod[3], maxMod[3].
//
// Return
// 0 => background, 255 => foreground
uchar cvbackgroundDiff(uchar *p, codeBook &c, int numChannels, int *minMod, int *maxMod)
{
	// ���沽��ͱ���ѧϰ�в�����Ԫ���һ��
	int matchChannel;
	//SEE IF THIS FITS AN EXISTING CODEWORD
	int i;
	for (i=0; i<c.numEntries; i++)
	{
		matchChannel = 0;
		for (int n=0; n<numChannels; n++)
		{
			if ((c.cb[i]->min[n] - minMod[n] <= *(p+n)) && (*(p+n) <= c.cb[i]->max[n] + maxMod[n]))
				matchChannel++; //Found an entry for this channel
			else
				break;
		}
		if (matchChannel == numChannels)
			break; //Found an entry that matched all channels
	}
	if(i == c.numEntries)
		// p���ظ�ͨ��ֵ�����뱾������һ����Ԫ,�򷵻ذ�ɫ
		return(255);

	return(0);
}


//UTILITES/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
//int clearStaleEntries(codeBook &c)
// After you've learned for some period of time, periodically call this to clear out stale codebook entries
//
//c		Codebook to clean up
//
// Return
// number of entries cleared
int cvclearStaleEntries(codeBook &c)
{
	int staleThresh = c.t >> 1;			// �趨ˢ��ʱ��
	int *keep = new int [c.numEntries];	// ����һ���������
	int keepCnt = 0;					// ��¼��ɾ����Ԫ��Ŀ
	//SEE WHICH CODEBOOK ENTRIES ARE TOO STALE
	for (int i=0; i<c.numEntries; i++)
		// �����뱾��ÿ����Ԫ
	{
		if (c.cb[i]->stale > staleThresh)
			// ����Ԫ�еĲ�����ʱ������趨��ˢ��ʱ��,����Ϊɾ��
			keep[i] = 0; //Mark for destruction
		else
		{
			keep[i] = 1; //Mark to keep
			keepCnt += 1;
		}
	}

	// KEEP ONLY THE GOOD
	c.t = 0;						//Full reset on stale tracking
	// �뱾ʱ������
	code_element **foo = new code_element* [keepCnt];
	// �����СΪkeepCnt ����Ԫָ������
	int k=0;
	for(int ii=0; ii<c.numEntries; ii++)
	{
		if(keep[ii])
		{
			foo[k] = c.cb[ii];
			foo[k]->stale = 0;		//We have to refresh these entries for next clearStale
			foo[k]->t_last_update = 0;
			k++;
		}
	}
	//CLEAN UP
	delete [] keep;
	delete [] c.cb;
	c.cb = foo;
	// ��foo ͷָ���ַ����c.cb
	int numCleared = c.numEntries - keepCnt;
	// ���������Ԫ����
	c.numEntries = keepCnt;
	// ʣ�����Ԫ��ַ
	return(numCleared);
}



int main()
{
	///////////////////////////////////////
	// ��Ҫʹ�õı���
	CvCapture*	capture;
	IplImage*	rawImage;
	IplImage*	yuvImage;
	IplImage*	ImaskCodeBook;
	codeBook*	cB;
	unsigned	cbBounds[CHANNELS];
	uchar*		pColor; //YUV pointer
	int			imageLen;
	int			nChannels = CHANNELS;
	int			minMod[CHANNELS];
	int			maxMod[CHANNELS];

	//////////////////////////////////////////////////////////////////////////
	// ��ʼ��������
	cvNamedWindow("Raw");
	cvNamedWindow("CodeBook");

	capture = cvCreateFileCapture("person01_walking_d1_uncomp.avi");
	if (!capture)
	{
		printf("Couldn't open the capture!");
		return -1;
	}

	rawImage = cvQueryFrame(capture);
	yuvImage = cvCreateImage(cvGetSize(rawImage), 8, 3);
	// ��yuvImage ����һ����rawImage �ߴ���ͬ,8λ3ͨ��ͼ��
	ImaskCodeBook = cvCreateImage(cvGetSize(rawImage), IPL_DEPTH_8U, 1);
	// ΪImaskCodeBook ����һ����rawImage �ߴ���ͬ,8λ��ͨ��ͼ��
	cvSet(ImaskCodeBook, cvScalar(255));
	// ���õ�ͨ����������Ԫ��Ϊ255,����ʼ��Ϊ��ɫͼ��

	imageLen = rawImage->width * rawImage->height;
	cB = new codeBook[imageLen];
	// �õ���ͼ��������Ŀ����һ����һ���뱾,�Ա��ÿ�����ؽ��д���

	for (int i=0; i<imageLen; i++)
		// ��ʼ��ÿ����Ԫ��ĿΪ0
		cB[i].numEntries = 0;
	for (int i=0; i<nChannels; i++)
	{
		cbBounds[i] = 10;	// ����ȷ����Ԫ��ͨ���ķ�ֵ

		minMod[i]	= 20;	// ���ڱ�����ֺ�����
		maxMod[i]	= 20;	// ������ֵ�Դﵽ��õķָ�
	}


	//////////////////////////////////////////////////////////////////////////
	// ��ʼ������Ƶÿһ֡ͼ��
	for (int i=0;;i++)
	{
		cvCvtColor(rawImage, yuvImage, CV_BGR2YCrCb);
		// ɫ�ʿռ�ת��,��rawImage ת����YUVɫ�ʿռ�,�����yuvImage
		// ��ʹ��ת��Ч����Ȼ�ܺ�
		// yuvImage = cvCloneImage(rawImage);

		if (i <= 30)
			// 30֡�ڽ��б���ѧϰ
		{
			pColor = (uchar *)(yuvImage->imageData);
			// ָ��yuvImage ͼ���ͨ������
			for (int c=0; c<imageLen; c++)
			{
				cvupdateCodeBook(pColor, cB[c], cbBounds, nChannels);
				// ��ÿ������,���ô˺���,��׽��������ر仯ͼ��
				pColor += 3;
				// 3 ͨ��ͼ��, ָ����һ������ͨ������
			}
			if (i == 30)
				// ��30 ֡ʱ�������溯��,ɾ���뱾�г¾ɵ���Ԫ
			{
				for (int c=0; c<imageLen; c++)
					cvclearStaleEntries(cB[c]);
			}
		}
		else
		{
			uchar maskPixelCodeBook;
			pColor = (uchar *)((yuvImage)->imageData); //3 channel yuv image
			uchar *pMask = (uchar *)((ImaskCodeBook)->imageData); //1 channel image
			// ָ��ImaskCodeBook ͨ���������е���Ԫ��
			for(int c=0; c<imageLen; c++)
			{
				maskPixelCodeBook = cvbackgroundDiff(pColor, cB[c], nChannels, minMod, maxMod);
				// �ҿ������ʱ��Ȼ����,��ʼ�����codeBook �Ǻ�
				*pMask++ = maskPixelCodeBook;
				pColor += 3;
				// pColor ָ�����3ͨ��ͼ��
			}
		}
		if (!(rawImage = cvQueryFrame(capture)))
			break;
		cvShowImage("Raw", rawImage);
		cvShowImage("CodeBook", ImaskCodeBook);

		if (cvWaitKey(30) == 27)
			break;
		if (i == 56 || i == 63)
			cvWaitKey();
	}

	cvReleaseCapture(&capture);
	if (yuvImage)
		cvReleaseImage(&yuvImage);
	if(ImaskCodeBook)
		cvReleaseImage(&ImaskCodeBook);
	cvDestroyAllWindows();
	delete [] cB;

	return 0;
}
