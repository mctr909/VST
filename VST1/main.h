// ============================================================================================
// インクルードファイル
// ============================================================================================
#include "audioeffectx.h"

// ============================================================================================
// 定数
// ============================================================================================
#define SAMPLE_RATE		44100
#define MASTER_PITCH	8.1757989
#define OSC_MAXNUM		32
#define MIDIMSG_MAXNUM	4410

// ============================================================================================
// 構造体
// ============================================================================================
typedef struct MidiMessage
{
	VstInt32 deltaFrames;  //MIDIメッセージを処理するタイミング
	unsigned char message; //MIDIメッセージ番号
	unsigned char channel; //MIDIチャンネル
	unsigned char data1;   //MIDIデータ1
	unsigned char data2;   //MIDIデータ2
};

typedef struct ADSR
{
	float attackTime;
	float decayTime;
	float sustainTime;
	float releaseTime;

	float attackLevel;
	float decayLevel;
	float sustainLevel;
};

typedef struct Channel
{
	unsigned char volume;
	unsigned char expression;
	unsigned char pan;

	unsigned char program;
	unsigned char bankMsb;
	unsigned char bankLsb;

	signed char	  rpnMsb;
	signed char	  rpnLsb;
	unsigned char bendWidth;

	float		  pitch;

	ADSR*         adsrAMP;
	ADSR*         adsrEQ;
};

typedef struct Osc
{
	unsigned char channel;
	signed char   noteNo;
	unsigned char release;

	float counter;
	float tempCounter;
	float time;

	float level;
	float amplitude;
	float frequency;
	float nois;
};

// ============================================================================================
// VSTの基本となるクラス
// ============================================================================================
class VST1 : public AudioEffectX
{
private:
	void clearChannel(Channel *channel);
	void clearOsc(Osc *osc);
	void releaseOsc(Osc *osc);
	void readMidiMsg(MidiMessage *midiMsg, Channel *channel, Osc *osc);

	float sqr50(Osc *osc);
	float sqr25(Osc *osc);
	float sqr12(Osc *osc);
	float tri(Osc *osc);
	float nois(Osc *osc);

protected:
	int          m_MidiMsgNum;					// 受け取ったMIDIメッセージの数
	MidiMessage  m_MidiMsgList[MIDIMSG_MAXNUM];	// 受け取ったMIDIメッセージを保管するバッファ
	Channel      channel[16];					// MIDIチャンネルのバッファ
	Osc			 osc[OSC_MAXNUM];				// 発振器のバッファ

public:
	VST1(audioMasterCallback audioMaster);

	// MIDIメッセージをホストアプリケーションから受け取るためのメンバー関数
	VstInt32 processEvents(VstEvents* events);

	// 音声信号を処理するメンバー関数
	virtual void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
};