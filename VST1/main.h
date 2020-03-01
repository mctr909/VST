// ============================================================================================
// インクルードファイル
// ============================================================================================
#include "audioeffectx.h"

// ============================================================================================
// 定数
// ============================================================================================
#define MASTER_PITCH	8.1757989
#define OSC_COUNT		128
#define CH_COUNT		16
#define MIDIMSG_MAXNUM	1024

enum struct E_OSC_STATE : unsigned char {
	STANDBY,
	ACTIVE,
	RELEASE,
	PURGE
};

enum struct E_OSC_TYPE : unsigned char {
	TRI,
	SQR50,
	SQR25,
	SQR12,
	SINE,
	NOIS
};

enum struct E_CTRL_TYPE : unsigned char {
	BANK_MSB = 0,
	MODULATION = 1,
	PORTA_TIME = 5,
	DATA_MSB = 6,
	VOLUME = 7,
	PAN = 10,
	EXPRESSION = 11,
	BANK_LSB = 32,
	HOLD = 64,
	PORTAMENTO = 65,
	RESONANCE = 71,
	RELEACE = 72,
	ATTACK = 73,
	CUTOFF = 74,
	VIB_RATE = 76,
	VIB_DEPTH = 77,
	VIB_DELAY = 78,
	REVERB = 91,
	CHORUS = 93,
	DELAY = 94,
	NRPN_LSB = 98,
	NRPN_MSB = 99,
	RPN_LSB = 100,
	RPN_MSB = 101,
	ALL_RESET = 121
};

// ============================================================================================
// 構造体
// ============================================================================================
#pragma pack(push, 8)
typedef struct {
	VstInt32 deltaFrames;  //MIDIメッセージを処理するタイミング
	unsigned char message; //MIDIメッセージ番号
	unsigned char channel; //MIDIチャンネル
	unsigned char data1;   //MIDIデータ1
	unsigned char data2;   //MIDIデータ2
} MidiMessage;
#pragma pack(pop)

#pragma pack(push, 8)
typedef struct {
	double rise;
	double top;
	double sustain;
	double fall;
	double attack;
	double holdTime;
	double decay;
	double release;
} ENVELOPE;
#pragma pack(pop)

#pragma pack(push, 4)
typedef struct {
	unsigned char progNum;
	unsigned char bankMsb;
	unsigned char bankLsb;

	unsigned char vol;
	unsigned char exp;
	unsigned char pan;

	unsigned char del;
	unsigned char cho;
	unsigned char rev;

	signed char	  rpnMsb;
	signed char	  rpnLsb;
	unsigned char bendWidth;
	
	double pitch;

	double chorusSend;
	double chorusRate;
	double chorusDepth;
	double chorusLfoU;
	double chorusLfoV;
	double chorusLfoW;

	int writeIndex;
	int readIndex;
	double delaySend;
	double delayTime;
	double* pDelayTapL = NULL;
	double* pDelayTapR = NULL;

	double outputL;
	double outputR;

	ENVELOPE adsrAMP;
	ENVELOPE adsrEQ;
} CHANNEL;
#pragma pack(pop)

typedef struct {
	unsigned char channel;
	unsigned char noteNo;
	E_OSC_STATE   state;
	E_OSC_TYPE    type;

	double time;
	double counter;
	double level;
	double amp;
	double pitch;
	double param;
	double value;
} OSC;

// ============================================================================================
// VSTの基本となるクラス
// ============================================================================================
class VST1 : public AudioEffectX {
protected:
	int          mMidiMsgNum;					// 受け取ったMIDIメッセージの数
	MidiMessage  mMidiMsgList[MIDIMSG_MAXNUM];	// 受け取ったMIDIメッセージを保管するバッファ
	CHANNEL      *mpChannel = NULL;				// MIDIチャンネルのバッファ
	OSC          *mpOsc = NULL;					// 発振器のバッファ

public:
	VST1(audioMasterCallback audioMaster);
	// MIDIメッセージをホストアプリケーションから受け取るためのメンバー関数
	VstInt32 processEvents(VstEvents* events);
	// 音声信号を処理するメンバー関数
	virtual void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);

private:
	void clearChannel(CHANNEL* channel);
	void readMidiMsg(MidiMessage* midiMsg, CHANNEL* channel, OSC* osc);

private:
	inline double sqr50(OSC* osc);
	inline double sqr25(OSC* osc);
	inline double sqr12(OSC* osc);
	inline double tri(OSC* osc);
	inline double tri4bit(OSC* osc);
	inline double sine(OSC* osc, CHANNEL* ch);
	inline double nois(OSC* osc);
};
