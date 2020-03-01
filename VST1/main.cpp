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
	mMidiMsgNum = 0;
	memset(mMidiMsgList, 0, sizeof(MidiMessage) * MIDIMSG_MAXNUM);

	// チャンネルのクリア
	if (NULL == mpChannel) {
		mpChannel = (CHANNEL*)malloc(sizeof(CHANNEL) * CH_COUNT);
	}
	memset(mpChannel, 0, sizeof(CHANNEL) * CH_COUNT);
	for (int ch = 0; ch < CH_COUNT; ch++) {
		clearChannel(&mpChannel[ch]);
	}

	// 発振器のクリア
	if (NULL != mpOsc) {
		free(mpOsc);
	}
	mpOsc = (OSC*)malloc(sizeof(OSC) * OSC_COUNT);
	memset(mpOsc, 0, sizeof(OSC) * OSC_COUNT);
	for (int oscCnt = 0; oscCnt < OSC_COUNT; oscCnt++) {
		auto osc = mpOsc + oscCnt;
		osc->channel = 0;
		osc->noteNo = 0;
		osc->state = E_OSC_STATE::STANDBY;
		osc->counter = 0.0;
		osc->param = 0.0;
		osc->time = 0.0;
		osc->level = 0.0;
		osc->amp = 0.0;
		osc->pitch = 0.0;
		osc->value = 1.0;
	}
}

// ============================================================================================
// MIDIメッセージを処理するメンバー関数
// processReplacing()の前に必ず1度だけ呼び出される。
// ============================================================================================
VstInt32 VST1::processEvents(VstEvents* events) {
	// MIDIのリストを初期化
	mMidiMsgNum = 0;
	memset(mMidiMsgList, 0, sizeof(MidiMessage) * MIDIMSG_MAXNUM);

	// VSTイベントの回数だけループをまわす。
	for (int i = 0; i < events->numEvents; i++) {
		// 与えられたイベントがMIDIならばmidimsgbufにストックする
		if ((events->events[i])->type == kVstMidiType) {
			auto midievent = (VstMidiEvent*)(events->events[i]);
			mMidiMsgList[mMidiMsgNum].deltaFrames = midievent->deltaFrames;
			mMidiMsgList[mMidiMsgNum].message = midievent->midiData[0] & 0xF0;	// MIDIメッセージ
			mMidiMsgList[mMidiMsgNum].channel = midievent->midiData[0] & 0x0F;	// MIDIチャンネル
			mMidiMsgList[mMidiMsgNum].data1 = midievent->midiData[1];			// MIDIデータ1
			mMidiMsgList[mMidiMsgNum].data2 = midievent->midiData[2];			// MIDIデータ2
			mMidiMsgNum++;
			// MIDIメッセージのバッファがいっぱいの場合はループを打ち切る。
			if (mMidiMsgNum >= MIDIMSG_MAXNUM) {
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

	int midiMsgCur = 0; // midieventlistの読み込み位置

	for (auto s = 0; s < sampleFrames; ++s) {
		//********************************
		// MIDIメッセージがあるか確認
		//********************************
		if (0 < mMidiMsgNum) {
			// MIDIメッセージを処理するタイミングかどうかを確認する。
			if (mMidiMsgList[midiMsgCur].deltaFrames <= s) {
				// MIDIメッセージの読み取り
				readMidiMsg(&mMidiMsgList[midiMsgCur], mpChannel, mpOsc);
				// midimsgbufからMIDIメッセージを読み出したので
				// 読み込み位置を進め、MIDIメッセージの数を減らす
				--mMidiMsgNum;
				++midiMsgCur;
			}
		}

		//********************************
		// ここで音声処理を行う
		//********************************
		for (auto oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
			auto pOsc = &mpOsc[oscIdx];
			if (pOsc->state == E_OSC_STATE::STANDBY) {
				continue;
			}

			auto pCh = &mpChannel[pOsc->channel];

			auto sumWave = 0.0;
			for (auto overSampling = 0; overSampling < 16; overSampling++) {
				if (E_OSC_TYPE::SQR12 == pOsc->type) {
					sumWave += sqr12(pOsc);
				} else if (E_OSC_TYPE::SQR25 == pOsc->type) {
					sumWave += sqr25(pOsc);
				} else if (E_OSC_TYPE::SQR50 == pOsc->type) {
					sumWave += sqr50(pOsc);
				} else if (E_OSC_TYPE::SINE == pOsc->type) {
					sumWave += sine(pOsc, pCh);
				} else if (E_OSC_TYPE::NOIS == pOsc->type) {
					sumWave += nois(pOsc);
				} else {
					if (pOsc->channel == 9) {
						sumWave += nois(pOsc);
					} else {
						sumWave += tri4bit(pOsc);
					}
				}
				pOsc->counter += pOsc->pitch * pCh->pitch * 0.0625;
				if (1.0 <= pOsc->counter) {
					pOsc->counter -= 1.0;
				}
			}

			// 出力
			sumWave *= 0.25 * pCh->vol * pCh->exp * pOsc->level * pOsc->amp / (127.0 * 127.0 * 16.0);
			pCh->outputL += sumWave * (1 - pCh->pan / 128.0);
			pCh->outputR += sumWave * (pCh->pan / 128.0);

			// エンベロープ
			switch (pOsc->state) {
			case E_OSC_STATE::ACTIVE:
				if (pOsc->time < pCh->adsrAMP.holdTime) {
					pOsc->amp += (1.0 - pOsc->amp) * pCh->adsrAMP.attack;
				} else {
					pOsc->amp += (pCh->adsrAMP.sustain - pOsc->amp) * pCh->adsrAMP.decay;
				}
				break;
			case E_OSC_STATE::RELEASE:
				pOsc->amp -= pOsc->amp * pCh->adsrAMP.release;
				break;
			case E_OSC_STATE::PURGE:
				pOsc->amp -= pOsc->amp * 500.0 / sampleRate;
				break;
			}
			pOsc->time += 1.0 / sampleRate;

			// 待機条件
			if (pCh->adsrAMP.holdTime < pOsc->time && pOsc->amp < 0.0001) {
				pOsc->state = E_OSC_STATE::STANDBY;
			}
		}

		outL[s] = 0.0f;
		outR[s] = 0.0f;
		for (auto chIdx = 0; chIdx < CH_COUNT; ++chIdx) {
			auto pCh = &mpChannel[chIdx];
			// ディレイ
			auto delayL = pCh->pDelayTapL[pCh->readIndex];
			auto delayR = pCh->pDelayTapR[pCh->readIndex];
			pCh->outputL += (delayL * 0.75 + delayR * 0.25) * pCh->delaySend;
			pCh->outputR += (delayR * 0.75 + delayL * 0.25) * pCh->delaySend;
			pCh->pDelayTapL[pCh->writeIndex] = pCh->outputL;
			pCh->pDelayTapR[pCh->writeIndex] = pCh->outputR;
			//
			pCh->chorusLfoU += (pCh->chorusLfoV - pCh->chorusLfoW) * pCh->chorusRate;
			pCh->chorusLfoV += (pCh->chorusLfoW - pCh->chorusLfoU) * pCh->chorusRate;
			pCh->chorusLfoW += (pCh->chorusLfoU - pCh->chorusLfoV) * pCh->chorusRate;
			// 出力
			outL[s] += (float)pCh->outputL;
			outR[s] += (float)pCh->outputR;
			pCh->outputL = 0.0;
			pCh->outputR = 0.0;
			//
			pCh->writeIndex++;
			if (sampleRate * 2 <= pCh->writeIndex) {
				pCh->writeIndex -= (int)(sampleRate * 2);
			}
			pCh->readIndex = pCh->writeIndex - (int)(pCh->delayTime * sampleRate);
			if (pCh->readIndex < 0) {
				pCh->readIndex += (int)(sampleRate * 2);
			}
			if (sampleRate * 2 <= pCh->readIndex) {
				pCh->readIndex -= (int)(sampleRate * 2);
			}
		}
	}
}

// ============================================================================================
// チャンネルのクリア
// ============================================================================================
void VST1::clearChannel(CHANNEL* channel) {
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

	channel->adsrAMP.rise = 0.0;
	channel->adsrAMP.top = 1.0;
	channel->adsrAMP.sustain = 1.0;
	channel->adsrAMP.fall = 0.0;
	channel->adsrAMP.attack = 500.0 / sampleRate / 1.0;
	channel->adsrAMP.holdTime = 500.0 / sampleRate / 1.0;
	channel->adsrAMP.decay = 500.0 / sampleRate / 1.0;
	channel->adsrAMP.release = 500.0 / sampleRate / 1.0;

	channel->adsrEQ.rise = 1.0;
	channel->adsrEQ.top = 1.0;
	channel->adsrEQ.sustain = 1.0;
	channel->adsrEQ.fall = 1.0;
	channel->adsrEQ.attack = 500.0 / sampleRate / 1.0;
	channel->adsrEQ.holdTime = 500.0 / sampleRate / 1.0;
	channel->adsrEQ.decay = 500.0 / sampleRate / 1.0;
	channel->adsrEQ.release = 500.0 / sampleRate / 1.0;

	channel->chorusSend = 0.0;
	channel->chorusRate = 1.0 / (sqrt(3.0) * sampleRate);
	channel->chorusDepth = 0.01;
	channel->chorusLfoU = 1.0;
	channel->chorusLfoV = -0.5;
	channel->chorusLfoW = -0.5;

	channel->writeIndex = 0;
	channel->readIndex = 0;
	channel->delaySend = 0.0;
	channel->delayTime = 0.17;
	if (NULL != channel->pDelayTapL) {
		free(channel->pDelayTapL);
	}
	if (NULL != channel->pDelayTapR) {
		free(channel->pDelayTapR);
	}
	channel->pDelayTapL = (double*)malloc(sizeof(double) * (int)(sampleRate * 2));
	channel->pDelayTapR = (double*)malloc(sizeof(double) * (int)(sampleRate * 2));
	memset(channel->pDelayTapL, 0, sizeof(double) * (int)(sampleRate * 2));
	memset(channel->pDelayTapR, 0, sizeof(double) * (int)(sampleRate * 2));
}

// ============================================================================================
// MIDIメッセージの読み取り
// ============================================================================================
void VST1::readMidiMsg(MidiMessage* midiMsg, CHANNEL* channel, OSC* osc) {
	//**** NoteOff ****//
	if (0x80 == midiMsg->message) {
		for (auto oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
			if (osc[oscIdx].channel == midiMsg->channel && osc[oscIdx].noteNo == midiMsg->data1) {
				osc[oscIdx].state = E_OSC_STATE::RELEASE;
			}
		}
		return;
	}
	//**** NoteOn ****//
	if (0x90 == midiMsg->message) {
		if (0 == midiMsg->data2) {
			for (auto oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
				if (osc[oscIdx].channel == midiMsg->channel && osc[oscIdx].noteNo == midiMsg->data1) {
					osc[oscIdx].state = E_OSC_STATE::RELEASE;
				}
			}
			return;
		}
		for (auto oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
			if (osc[oscIdx].channel == midiMsg->channel && osc[oscIdx].noteNo == midiMsg->data1) {
				osc[oscIdx].state = E_OSC_STATE::PURGE;
			}
		}
		for (auto oscIdx = 0; oscIdx < OSC_COUNT; ++oscIdx) {
			if (osc[oscIdx].state == E_OSC_STATE::STANDBY) {
				auto pOsc = osc + oscIdx;
				pOsc->channel = midiMsg->channel;
				pOsc->noteNo = midiMsg->data1;
				pOsc->level = midiMsg->data2 / 127.0;
				pOsc->pitch = MASTER_PITCH * pow(2.0, osc[oscIdx].noteNo / 12.0) / sampleRate;
				pOsc->counter = 0.0;
				pOsc->param = 0.0;
				pOsc->time = 0.0;
				pOsc->amp = 0.0;
				pOsc->value = 1.0;
				pOsc->state = E_OSC_STATE::ACTIVE;
				break;
			}
		}
		return;
	}
	//**** C.C. ****//
	if (0xB0 == midiMsg->message) {
		switch ((E_CTRL_TYPE)midiMsg->data1) {
		case E_CTRL_TYPE::BANK_MSB:
			channel[midiMsg->channel].bankMsb = midiMsg->data2;
			break;
		case E_CTRL_TYPE::BANK_LSB:
			channel[midiMsg->channel].bankLsb = midiMsg->data2;
			break;
		case E_CTRL_TYPE::DATA_MSB:
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
		case E_CTRL_TYPE::VOLUME:
			channel[midiMsg->channel].vol = midiMsg->data2;
			break;
		case E_CTRL_TYPE::PAN:
			channel[midiMsg->channel].pan = midiMsg->data2;
			break;
		case E_CTRL_TYPE::EXPRESSION:
			channel[midiMsg->channel].exp = midiMsg->data2;
			break;
		case E_CTRL_TYPE::RELEACE:
			channel[midiMsg->channel].adsrAMP.release = midiMsg->data2 < 64 ? 1.0 : 500.0 / sampleRate / ((midiMsg->data2 / 127.0) * 100 + 1);
			break;
		case E_CTRL_TYPE::ATTACK:
			channel[midiMsg->channel].adsrAMP.attack = 500.0 / (sampleRate * (((int)midiMsg->data2 / 64.0) + 0.01));
			break;
		case E_CTRL_TYPE::REVERB:
			channel[midiMsg->channel].rev = midiMsg->data2;
			break;
		case E_CTRL_TYPE::CHORUS:
			channel[midiMsg->channel].cho = midiMsg->data2;
			break;
		case E_CTRL_TYPE::DELAY:
			channel[midiMsg->channel].del = midiMsg->data2;
			if (0 == midiMsg->data2) {
				channel[midiMsg->channel].delaySend = 0.0;
			} else {
				channel[midiMsg->channel].delaySend = 0.9 * midiMsg->data2 / 127.0;
			}
			break;
		case E_CTRL_TYPE::RPN_LSB:
			channel[midiMsg->channel].rpnLsb = midiMsg->data2;
			break;
		case E_CTRL_TYPE::RPN_MSB:
			channel[midiMsg->channel].rpnMsb = midiMsg->data2;
			break;
		}
		return;
	}
	//**** Program ****//
	if (0xC0 == midiMsg->message) {
		channel[midiMsg->channel].progNum = midiMsg->data1;
		return;
	}
	//**** Pitch ****//
	if (0xE0 == midiMsg->message) {
		channel[midiMsg->channel].pitch = midiMsg->data1 | midiMsg->data2 << 7;
		channel[midiMsg->channel].pitch -= 8192;
		channel[midiMsg->channel].pitch = (float)pow(2.0, channel[midiMsg->channel].bendWidth * channel[midiMsg->channel].pitch / 98304.0);
		return;
	}
}

// ============================================================================================
// 発振器
// ============================================================================================
inline double VST1::sqr50(OSC* osc) {
	return osc->counter < 0.5 ? 1 : -1;
}
inline double VST1::sqr25(OSC* osc) {
	return osc->counter < 0.25 ? 1 : -1;
}
inline double VST1::sqr12(OSC* osc) {
	return osc->counter < 0.125 ? 1 : -1;
}
inline double VST1::tri(OSC* osc) {
	if (osc->counter < 0.25) {
		return osc->counter * 4.0;
	} else if (osc->counter < 0.75) {
		return 2.0 - osc->counter * 4.0;
	} else {
		return osc->counter * 4.0 - 4.0;
	}
}
inline double VST1::tri4bit(OSC* osc) {
	if (osc->counter < 0.25) {
		return (int)(osc->counter * 32.0) * 0.125;
	} else if (osc->counter < 0.75) {
		return (int)(16.0 - osc->counter * 32.0) * 0.125;
	} else {
		return (int)(osc->counter * 32.0 - 32.0) * 0.125;
	}
}
inline double VST1::sine(OSC* osc, CHANNEL *ch) {
	osc->value -= osc->param * 0.39269908 * osc->pitch * ch->pitch;
	osc->param += osc->value * 0.39269908 * osc->pitch * ch->pitch;
	return osc->value;
}
inline double VST1::nois(OSC* osc) {
	osc->param += 4 * osc->pitch;
	if (1.0 <= osc->param) {
		osc->param -= 1.0;
		osc->value = 2048 * (1 / 1024.0 - 128.0 * rand() / 2147483647.0);
	}
	return osc->value;
}
