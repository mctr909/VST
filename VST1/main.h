// ============================================================================================
// �C���N���[�h�t�@�C��
// ============================================================================================
#include "audioeffectx.h"

// ============================================================================================
// �萔
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
// �\����
// ============================================================================================
#pragma pack(push, 8)
typedef struct MidiMessage {
	VstInt32 deltaFrames;  //MIDI���b�Z�[�W����������^�C�~���O
	unsigned char message; //MIDI���b�Z�[�W�ԍ�
	unsigned char channel; //MIDI�`�����l��
	unsigned char data1;   //MIDI�f�[�^1
	unsigned char data2;   //MIDI�f�[�^2
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
// VST�̊�{�ƂȂ�N���X
// ============================================================================================
class VST1 : public AudioEffectX {
protected:
	int          m_MidiMsgNum;					// �󂯎����MIDI���b�Z�[�W�̐�
	MidiMessage  m_MidiMsgList[MIDIMSG_MAXNUM];	// �󂯎����MIDI���b�Z�[�W��ۊǂ���o�b�t�@
	Channel      *mpChannel = NULL;				// MIDI�`�����l���̃o�b�t�@
	Osc          *mpOsc = NULL;					// ���U��̃o�b�t�@

public:
	VST1(audioMasterCallback audioMaster);
	// MIDI���b�Z�[�W���z�X�g�A�v���P�[�V��������󂯎�邽�߂̃����o�[�֐�
	VstInt32 processEvents(VstEvents* events);
	// �����M�����������郁���o�[�֐�
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
