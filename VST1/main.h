// ============================================================================================
// �C���N���[�h�t�@�C��
// ============================================================================================
#include "audioeffectx.h"

// ============================================================================================
// �萔
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
// �\����
// ============================================================================================
#pragma pack(push, 8)
typedef struct {
	VstInt32 deltaFrames;  //MIDI���b�Z�[�W����������^�C�~���O
	unsigned char message; //MIDI���b�Z�[�W�ԍ�
	unsigned char channel; //MIDI�`�����l��
	unsigned char data1;   //MIDI�f�[�^1
	unsigned char data2;   //MIDI�f�[�^2
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
// VST�̊�{�ƂȂ�N���X
// ============================================================================================
class VST1 : public AudioEffectX {
protected:
	int          mMidiMsgNum;					// �󂯎����MIDI���b�Z�[�W�̐�
	MidiMessage  mMidiMsgList[MIDIMSG_MAXNUM];	// �󂯎����MIDI���b�Z�[�W��ۊǂ���o�b�t�@
	CHANNEL      *mpChannel = NULL;				// MIDI�`�����l���̃o�b�t�@
	OSC          *mpOsc = NULL;					// ���U��̃o�b�t�@

public:
	VST1(audioMasterCallback audioMaster);
	// MIDI���b�Z�[�W���z�X�g�A�v���P�[�V��������󂯎�邽�߂̃����o�[�֐�
	VstInt32 processEvents(VstEvents* events);
	// �����M�����������郁���o�[�֐�
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
