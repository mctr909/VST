// ============================================================================================
// インクルードファイル
// ============================================================================================
#include "audioeffectx.h"

// ============================================================================================
// 定数
// ============================================================================================
#define SAMPLE_RATE		44100
#define MASTER_PITCH	8.1757989
#define OSC_COUNT		128
#define CH_COUNT		16
#define MIDIMSG_MAXNUM	4410

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
	NOIS
};

// ============================================================================================
// 構造体
// ============================================================================================
#pragma pack(push, 8)
typedef struct MidiMessage {
	VstInt32 deltaFrames;  //MIDIメッセージを処理するタイミング
	unsigned char message; //MIDIメッセージ番号
	unsigned char channel; //MIDIチャンネル
	unsigned char data1;   //MIDIデータ1
	unsigned char data2;   //MIDIデータ2
};
#pragma pack(pop)

#pragma pack(push, 8)
typedef struct ADSR {
	float rise;
	float top;
	float sustain;
	float fall;
	float attack;
	float holdTime;
	float decay;
	float release;
};
#pragma pack(pop)

#pragma pack(push, 4)
typedef struct Channel {
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

	float		  pitch;

	float delaySend;
	float delayTime;
	int writeIndex;
	int readIndex;

	float outputL;
	float outputR;
	float* pDelayTapL = NULL;
	float* pDelayTapR = NULL;

	ADSR adsrAMP;
	ADSR adsrEQ;
};
#pragma pack(pop)

typedef struct Osc {
	unsigned char channel;
	signed char   noteNo;
	E_OSC_STATE   state;
	E_OSC_TYPE    type;
	
	float counter;
	float param;
	float time;

	float level;
	float amp;
	float delta;
	float nois;
};

// ============================================================================================
// VSTの基本となるクラス
// ============================================================================================
class VST1 : public AudioEffectX {
protected:
	int          m_MidiMsgNum;					// 受け取ったMIDIメッセージの数
	MidiMessage  m_MidiMsgList[MIDIMSG_MAXNUM];	// 受け取ったMIDIメッセージを保管するバッファ
	Channel      *mpChannel = NULL;				// MIDIチャンネルのバッファ
	Osc          *mpOsc = NULL;					// 発振器のバッファ

public:
	VST1(audioMasterCallback audioMaster);
	// MIDIメッセージをホストアプリケーションから受け取るためのメンバー関数
	VstInt32 processEvents(VstEvents* events);
	// 音声信号を処理するメンバー関数
	virtual void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);

private:
	void clearChannel(Channel* channel);
	void readMidiMsg(MidiMessage* midiMsg, Channel* channel, Osc* osc);

private:
	inline float sqr50(Osc* osc);
	inline float sqr25(Osc* osc);
	inline float sqr12(Osc* osc);
	inline float tri(Osc* osc);
	inline float tri4bit(Osc* osc);
	inline float nois(Osc* osc);
};
