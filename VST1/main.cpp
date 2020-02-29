#include "main.h"
#include <math.h>
#include <stdlib.h>

// ============================================================================================
// このVSTのを生成するための関数
// ============================================================================================
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
	// このVSTを生成したポインタを返す
	return new VST1(audioMaster);
}

// ============================================================================================
// コンストラクタ(VSTの初期化)
// ============================================================================================
VST1::VST1(audioMasterCallback audioMaster)
	: AudioEffectX(audioMaster, 1, 0) {
	//以下の関数を呼び出して入力数、出力数等の情報を設定する。
	//必ず呼び出さなければならない。
	setNumInputs(2);		//入力数。モノラル入力=1、ステレオ入力=2
	setNumOutputs(2);		//出力数。モノラル出力=1、ステレオ出力=2
	setUniqueID('SMPL');	//ユニークIDの設定
							//公開する場合は以下URLで発行されたユニークIDを入力する。
							//http://ygrabit.steinberg.de/~ygrabit/public_html/index.html

	//このVSTがSynthかどうかのフラグを設定。Synthの場合…true、Effectorの場合…false
	isSynth(true);

	//このVSTが音声処理可能かどうかのフラグを設定。音声処理を行わないVSTはないので必ずこの関数を呼び出す。
	canProcessReplacing();

	//上記の関数を呼び出した後に初期化を行う
	m_MidiMsgNum = 0;
	memset(m_MidiMsgList, 0, sizeof(MidiMessage) * MIDIMSG_MAXNUM);

	// チャンネルのクリア
	if (NULL == mpChannel) {
		mpChannel = (Channel*)malloc(sizeof(Channel) * CH_COUNT);
	}
	memset(mpChannel, 0, sizeof(Channel) * CH_COUNT);
	for (int ch = 0; ch < CH_COUNT; ch++) {
		clearChannel(&mpChannel[ch]);
	}

	// 発振器のクリア
	if (NULL == mpOsc) {
		mpOsc = (Osc*)malloc(sizeof(Osc) * OSC_COUNT);
	}
	memset(mpOsc, 0, sizeof(Osc) * OSC_COUNT);
	for (int oscCnt = 0; oscCnt < OSC_COUNT; oscCnt++) {
		auto osc = &mpOsc[oscCnt];
		osc->channel = 0;
		osc->noteNo = -1;
		osc->state = E_OSC_STATE::STANDBY;

		osc->counter = 0.0f;
		osc->param = 0.0f;
		osc->time = 0.0f;

		osc->level = 0.0f;
		osc->amp = 0.0f;
		osc->delta = 0.0f;
		osc->nois = 0.0f;
	}
}

// ============================================================================================
// MIDIメッセージを処理するメンバー関数
// processReplacing()の前に必ず1度だけ呼び出される。
// ============================================================================================
VstInt32 VST1::processEvents(VstEvents* events) {
	// MIDIのリストを初期化
	m_MidiMsgNum = 0;
	memset(m_MidiMsgList, 0, sizeof(MidiMessage) * MIDIMSG_MAXNUM);

	// VSTイベントの回数だけループをまわす。
	int loops = (events->numEvents);
	for (int i = 0; i < loops; i++) {
		// 与えられたイベントがMIDIならばmidimsgbufにストックする
		if ((events->events[i])->type == kVstMidiType) {
			VstMidiEvent* midievent = (VstMidiEvent*)(events->events[i]);

			m_MidiMsgList[m_MidiMsgNum].deltaFrames = midievent->deltaFrames;
			m_MidiMsgList[m_MidiMsgNum].message = midievent->midiData[0] & 0xF0;	// MIDIメッセージ
			m_MidiMsgList[m_MidiMsgNum].channel = midievent->midiData[0] & 0x0F;	// MIDIチャンネル
			m_MidiMsgList[m_MidiMsgNum].data1 = midievent->midiData[1];				// MIDIデータ1
			m_MidiMsgList[m_MidiMsgNum].data2 = midievent->midiData[2];				// MIDIデータ2
			m_MidiMsgNum++;

			// MIDIメッセージのバッファがいっぱいの場合はループを打ち切る。
			if (i >= MIDIMSG_MAXNUM) {
				break;
			}
		}
	}

	return 1;
}

// ============================================================================================
// 音声信号を処理するメンバー関数
// ============================================================================================
void VST1::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
	//入力、出力は2次元配列で渡される。
	//入力は-1.0f～1.0fの間で渡される。
	//出力は-1.0f～1.0fの間で書き込む必要がある。
	//sampleFramesが処理するバッファのサイズ
	auto outL = outputs[0];	//出力 左用
	auto outR = outputs[1];	//出力 右用

	Channel* pCh;
	Osc* pOsc;

	int midiMsgCur = 0; // midieventlistの読み込み位置
	int idx = 0;
	int overSampling;
	float wave = 0.0f;

	for (int s = 0; s < sampleFrames; s++) {
		// MIDIメッセージがあるか確認
		if (m_MidiMsgNum > 0) {
			// MIDIメッセージを処理するタイミングかどうかを確認する。
			if (m_MidiMsgList[midiMsgCur].deltaFrames <= s) {
				// MIDIメッセージの読み取り
				readMidiMsg(&m_MidiMsgList[midiMsgCur], mpChannel, mpOsc);
				// midimsgbufからMIDIメッセージを読み出したので
				// 読み込み位置を進め、MIDIメッセージの数を減らす
				--m_MidiMsgNum;
				++midiMsgCur;
			}
		}

		//********************************
		// ここで音声処理を行う
		//********************************
		for (idx = 0; idx < OSC_COUNT; ++idx) {
			pOsc = &mpOsc[idx];
			if (pOsc->state == E_OSC_STATE::STANDBY) {
				continue;
			}

			pCh = &mpChannel[pOsc->channel];

			wave = 0.0;
			for (overSampling = 0; overSampling < 16; overSampling++) {
				if (pOsc->type == E_OSC_TYPE::SQR12) {
					wave += sqr12(pOsc);
				} else if (pOsc->type == E_OSC_TYPE::SQR25) {
					wave += sqr25(pOsc);
				} else if (pOsc->type == E_OSC_TYPE::SQR50) {
					wave += sqr50(pOsc);
				} else if (pOsc->type == E_OSC_TYPE::NOIS) {
					wave += nois(pOsc);
				} else {
					if (pOsc->channel == 9) {
						wave += nois(pOsc);
					} else {
						wave += tri4bit(pOsc);
					}
				}
				pOsc->counter += pOsc->delta * pCh->pitch * 0.0625f;
				if (1.0f <= pOsc->counter) {
					pOsc->counter -= 1.0f;
				}
			}

			// 出力
			wave *= 0.25f * pCh->vol * pCh->exp * pOsc->level * pOsc->amp / (127 * 127 * 16);
			pCh->outputL += wave * (1 - pCh->pan / 128.0f);
			pCh->outputR += wave * (pCh->pan / 128.0f);

			// エンベロープ
			switch (pOsc->state) {
			case E_OSC_STATE::ACTIVE:
				if (pOsc->time < pCh->adsrAMP.holdTime) {
					pOsc->amp += (1.0f - pOsc->amp) * pCh->adsrAMP.attack;
				} else {
					pOsc->amp += (pCh->adsrAMP.sustain - pOsc->amp) * pCh->adsrAMP.decay;
				}
				break;
			case E_OSC_STATE::RELEASE:
				pOsc->amp -= pOsc->amp * pCh->adsrAMP.release;
				break;
			case E_OSC_STATE::PURGE:
				pOsc->amp -= pOsc->amp * 500.0f / SAMPLE_RATE;
				break;
			}
			pOsc->time += 1.0f;

			// クリア
			if (pCh->adsrAMP.holdTime < pOsc->time && pOsc->amp < 1 / 256.0f) {
				pOsc->state = E_OSC_STATE::STANDBY;
				pOsc->channel = 0;
				pOsc->noteNo = -1;
				pOsc->counter = 0.0f;
				pOsc->param = 0.0f;
				pOsc->time = 0.0f;
				pOsc->level = 0.0f;
				pOsc->amp = 0.0;
				pOsc->delta = 0.0;
				pOsc->nois = 0.0f;
			}
		}

		outL[s] = 0.0f;
		outR[s] = 0.0f;
		for (idx = 0; idx < CH_COUNT; ++idx) {
			pCh = &mpChannel[idx];

			// ディレイ
			auto delayL = pCh->pDelayTapL[pCh->readIndex];
			auto delayR = pCh->pDelayTapR[pCh->readIndex];
			pCh->outputL += (delayL * 0.75f + delayR * 0.25f) * pCh->delaySend;
			pCh->outputR += (delayR * 0.75f + delayL * 0.25f) * pCh->delaySend;
			pCh->pDelayTapL[pCh->writeIndex] = pCh->outputL;
			pCh->pDelayTapR[pCh->writeIndex] = pCh->outputR;

			// 出力
			outL[s] += pCh->outputL;
			outR[s] += pCh->outputR;
			pCh->outputL = 0.0f;
			pCh->outputR = 0.0f;

			//
			pCh->writeIndex++;
			if (sampleRate * 2 <= pCh->writeIndex) {
				pCh->writeIndex -= sampleRate * 2;
			}
			pCh->readIndex = pCh->writeIndex - (int)(pCh->delayTime * sampleRate);
			if (pCh->readIndex < 0) {
				pCh->readIndex += sampleRate * 2;
			}
			if (sampleRate * 2 <= pCh->readIndex) {
				pCh->readIndex -= sampleRate * 2;
			}
		}
	}
}

// ============================================================================================
// チャンネルのクリア
// ============================================================================================
void VST1::clearChannel(Channel* channel) {
	channel->vol = 100;
	channel->exp = 100;
	channel->pan = 64;
	channel->progNum = 0;
	channel->pitch = 1.0F;
	channel->bendWidth = 2;
	channel->del = 0;
	channel->rev = 0;
	channel->cho = 0;
	channel->rpnMsb = -1;
	channel->rpnLsb = -1;
	channel->bankMsb = 0;
	channel->bankLsb = 0;

	channel->adsrAMP.rise = 0.0f;
	channel->adsrAMP.top = 1.0f;
	channel->adsrAMP.sustain = 1.0f;
	channel->adsrAMP.fall = 0.0f;
	channel->adsrAMP.attack = 1000.0f / SAMPLE_RATE / 1.0f;
	channel->adsrAMP.holdTime = 1000.0f / SAMPLE_RATE / 1.0f;
	channel->adsrAMP.decay = 1000.0f / SAMPLE_RATE / 1.0f;
	channel->adsrAMP.release = 1000.0f / SAMPLE_RATE / 1.0f;

	channel->adsrEQ.rise = 1.0f;
	channel->adsrEQ.top = 1.0f;
	channel->adsrEQ.sustain = 1.0f;
	channel->adsrEQ.fall = 1.0f;
	channel->adsrEQ.attack = 1000.0f / SAMPLE_RATE / 1.0f;
	channel->adsrEQ.holdTime = 1000.0f / SAMPLE_RATE / 1.0f;
	channel->adsrEQ.decay = 1000.0f / SAMPLE_RATE / 1.0f;
	channel->adsrEQ.release = 1000.0f / SAMPLE_RATE / 1.0f;

	channel->delaySend = 0.0f;
	channel->delayTime = 0.17f;
	channel->writeIndex = 0.0f;
	if (NULL == channel->pDelayTapL) {
		channel->pDelayTapL = (float*)malloc(sizeof(float) * sampleRate * 2);
	}
	if (NULL == channel->pDelayTapR) {
		channel->pDelayTapR = (float*)malloc(sizeof(float) * sampleRate * 2);
	}
	memset(channel->pDelayTapL, 0, sizeof(float) * sampleRate * 2);
	memset(channel->pDelayTapR, 0, sizeof(float) * sampleRate * 2);
}

// ============================================================================================
// MIDIメッセージの読み取り
// ============================================================================================
void VST1::readMidiMsg(MidiMessage* midiMsg, Channel* channel, Osc* osc) {
	int oscIdx = 0;

	//*** NoteOff ***//
	if (midiMsg->message == 0x80) {
		for (oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
			if (osc[oscIdx].channel == midiMsg->channel && osc[oscIdx].noteNo == midiMsg->data1) {
				osc[oscIdx].state = E_OSC_STATE::RELEASE;
			}
		}
	}

	//*** NoteOn ***//
	if (midiMsg->message == 0x90) {
		for (oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
			if (osc[oscIdx].channel == midiMsg->channel && osc[oscIdx].noteNo == midiMsg->data1) {
				osc[oscIdx].state = E_OSC_STATE::PURGE;
			}
		}
		for (oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
			if (osc[oscIdx].state == E_OSC_STATE::STANDBY) {
				osc[oscIdx].channel = midiMsg->channel;
				osc[oscIdx].noteNo = midiMsg->data1;
				osc[oscIdx].level = midiMsg->data2 / 127.0f;
				osc[oscIdx].delta = (float)(MASTER_PITCH * pow(2.0f, osc[oscIdx].noteNo / 12.0)) / sampleRate;
				osc[oscIdx].state = E_OSC_STATE::ACTIVE;
				break;
			}
		}
	}

	//*** C.C. ***//
	if (midiMsg->message == 0xB0) {
		switch (midiMsg->data1) {
		case 0x00: // Bank MSB
			channel[midiMsg->channel].bankMsb = midiMsg->data2;
			break;
		case 0x20: // Bank LSB
			channel[midiMsg->channel].bankLsb = midiMsg->data2;
			break;
		case 0x06: // DataEntry
			// RPN BendWidth
			if (channel[midiMsg->channel].rpnMsb == 0 && channel[midiMsg->channel].rpnLsb == 0) {
				channel[midiMsg->channel].bendWidth = midiMsg->data2;
			}
			// RPN Reset
			if (channel[midiMsg->channel].rpnMsb >= 0 || channel[midiMsg->channel].rpnLsb >= 0) {
				channel[midiMsg->channel].rpnMsb = -1;
				channel[midiMsg->channel].rpnLsb = -1;
			}
			break;
		case 0x07: // Volume
			channel[midiMsg->channel].vol = midiMsg->data2;
			break;
		case 0x0A: // Pan
			channel[midiMsg->channel].pan = midiMsg->data2;
			break;
		case 0x0B: // Expression
			channel[midiMsg->channel].exp = midiMsg->data2;
			break;
		case 0x48: // Release
			channel[midiMsg->channel].adsrAMP.release = midiMsg->data2 < 64 ? 1.0f : 1000.0f / SAMPLE_RATE / ((midiMsg->data2 * 200.0f / 127.0f) + 0.001f);
			break;
		case 0x49: // Attack
			channel[midiMsg->channel].adsrAMP.attack = 1000.0f / (SAMPLE_RATE * (((int)midiMsg->data2 / 64.0f) + 0.01f));
			break;
		case 0x5B: // Reverb send
			channel[midiMsg->channel].rev = midiMsg->data2;
			break;
		case 0x5D: // Chorus send
			channel[midiMsg->channel].cho = midiMsg->data2;
			break;
		case 0x5E: // Delay send
			channel[midiMsg->channel].del = midiMsg->data2;
			if (0 == midiMsg->data2) {
				channel[midiMsg->channel].delaySend = 0.0f;
			} else {
				channel[midiMsg->channel].delaySend = 0.9f * midiMsg->data2 / 127.0f;
			}
			break;
		case 0x64: // RPN LSB
			channel[midiMsg->channel].rpnLsb = midiMsg->data2;
			break;
		case 0x65: // RPN MSB
			channel[midiMsg->channel].rpnMsb = midiMsg->data2;
			break;
		}
	}

	//*** Program ***//
	if (midiMsg->message == 0xC0) {
		channel[midiMsg->channel].progNum = midiMsg->data1;
	}

	//*** Pitch ***//
	if (midiMsg->message == 0xE0) {
		channel[midiMsg->channel].pitch = midiMsg->data1 | midiMsg->data2 << 7;
		channel[midiMsg->channel].pitch -= 8192;
		channel[midiMsg->channel].pitch = (float)pow(2.0, channel[midiMsg->channel].bendWidth * channel[midiMsg->channel].pitch / 98304.0);
	}
}

// ============================================================================================
// 発振器
// ============================================================================================
inline float VST1::sqr50(Osc* osc) {
	return osc->counter < 0.5 ? 1 : -1;
}
inline float VST1::sqr25(Osc* osc) {
	return osc->counter < 0.25 ? 1 : -1;
}
inline float VST1::sqr12(Osc* osc) {
	return osc->counter < 0.125 ? 1 : -1;
}
inline float VST1::tri(Osc* osc) {
	if (osc->counter < 0.25) {
		return osc->counter * 4.0f;
	} else if (osc->counter < 0.75) {
		return 2.0f - osc->counter * 4.0f;
	} else {
		return osc->counter * 4.0f - 4.0f;
	}
}
inline float VST1::tri4bit(Osc* osc) {
	if (osc->counter < 0.25) {
		return (int)(osc->counter * 32.0f) * 0.125f;
	} else if (osc->counter < 0.75) {
		return (int)(16.0f - osc->counter * 32.0f) * 0.125f;
	} else {
		return (int)(osc->counter * 32.0f - 32.0f) * 0.125f;
	}
}
inline float VST1::nois(Osc* osc) {
	osc->param += 4 * osc->delta;
	if (1.0f <= osc->param) {
		osc->param -= 1.0f;
		osc->nois = 2048 * (1 / 1024.0f - 128 * rand() / 2147483647.0f);
	}
	return osc->nois;
}