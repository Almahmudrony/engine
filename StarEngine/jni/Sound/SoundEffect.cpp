#include "SoundEffect.h"
#include "AudioManager.h"
#include "../Logger.h"
#include "../Assets/Resource.h"
#include "../Helpers/Helpers.h"
#include "../Helpers/Filepath.h"
#include "../Helpers/HelpersMath.h"

#ifdef ANDROID
#include "../StarEngine.h"
#endif

namespace star
{
	#ifdef DESKTOP
	int SoundEffect::PLAY_CHANNELS = 0;
	#endif

	SoundEffect::SoundEffect(const tstring& path, uint8 channel)
		: BaseSound(channel)
	#ifdef DESKTOP
		, mPlayChannel(PLAY_CHANNELS++)
		, mpSound(nullptr)
	#else
		, mPlayerObjs(MAX_SAMPLES)
		, mPlayers(MAX_SAMPLES)
	#endif
	{
	#ifdef ANDROID
		SLEngineItf engine = AudioManager::GetInstance()->GetEngine();
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			CreateSound(mPlayerObjs[i], engine, mPlayers[i], path);
			mLoopTimes.push_back(0);
		}
	#else
		Filepath real_path(path);
		sstring sound_path = string_cast<sstring>(real_path.GetAssetsPath());
		mpSound = Mix_LoadWAV(sound_path.c_str());
		if(!mpSound)
		{
			star::Logger::GetInstance()->Log(star::LogLevel::Error,
				_T("SoundEffect: Could not load sound, reason : ")
				+ string_cast<tstring>(Mix_GetError()));
		}
	#endif
		SetChannel(channel);
	}

	SoundEffect::~SoundEffect()
	{
	#ifdef DESKTOP
		Mix_HaltChannel(mPlayChannel);
	#else
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			DestroySound(mPlayerObjs[i], mPlayers[i]);
		}
	#endif
		if(!mNoChannelAssigned)
		{
			UnsetChannel();
		}
	}

	void SoundEffect::Play(int loopTime)
	{
		BaseSound::Play(loopTime);
	#ifdef DESKTOP
		Mix_PlayChannel(mPlayChannel, mpSound, loopTime);
	#else
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			SLresult lRes;
			(*mPlayers[i])->GetPlayState(mPlayers[i],&lRes);
			if( lRes == SL_PLAYSTATE_STOPPED)
			{
				mLoopTimes[i] = loopTime;
				lRes = (*mPlayers[i])->SetPlayState(mPlayers[i],SL_PLAYSTATE_PLAYING);
				if (lRes != SL_RESULT_SUCCESS)
				{
					star::Logger::GetInstance()->Log(star::LogLevel::Error,
						_T("SoundEffect: Can't play audio!"));
					Stop();
					return;
				};
				return;
			}
		}
#endif
	}

	void SoundEffect::Stop()
	{
		BaseSound::Stop();
#ifdef DESKTOP
		Mix_HaltChannel(mPlayChannel);
#else	
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			(*mPlayers[i])->SetPlayState(mPlayers[i], SL_PLAYSTATE_STOPPED);
		}
#endif
	}
	
	void SoundEffect::Pause()
	{
		BaseSound::Pause();
#ifdef DESKTOP
		Mix_Pause(mPlayChannel);
#else
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			SLresult lres = (*mPlayers[i])->GetPlayState(
				mPlayers[i], &lres
				);
			if(lres == SL_PLAYSTATE_PLAYING)
			{
				(*mPlayers[i])->SetPlayState(
					mPlayers[i],SL_PLAYSTATE_PAUSED
					);
			}
		}
#endif
	}

	void SoundEffect::Resume()
	{
		BaseSound::Resume();
#ifdef DESKTOP
		Mix_Resume(mPlayChannel);
#else
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			ResumeSound(mPlayers[i]);
		}
#endif
	}

	void SoundEffect::SetChannel(uint8 channel)
	{
		BaseSound::SetChannel(channel);
		AudioManager::GetInstance()->AddSoundToChannel(channel, this);
	}

	void SoundEffect::UnsetChannel()
	{
		BaseSound::UnsetChannel();
		AudioManager::GetInstance()->RemoveSoundFromChannel(mChannel, this);
	}

#ifdef ANDROID
	void SoundEffect::SetVolume(float volume)
	{
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			SetSoundVolume(
				mPlayerObjs[i],
				mPlayers[i],
				volume
				);
		}
	}
#endif

	float SoundEffect::GetVolume() const
	{
#ifdef ANDROID
		return GetSoundVolume(mPlayerObjs[0], mPlayers[0]);
#else
		if(mIsMuted)
		{
			return mVolume;
		}
		else
		{
			float volume = float(Mix_Volume(mPlayChannel, -1));
			return volume / float(MIX_MAX_VOLUME);
		}
#endif
	}

	void SoundEffect::SetMuted(bool muted)
	{
#ifdef ANDROID
		for(int i = 0 ; i < MAX_SAMPLES ; ++i)
		{
			SetSoundMuted(mPlayerObjs[i], mPlayers[i], muted);
		}
#else
		SetSoundMuted(muted);
#endif
	}

	bool SoundEffect::IsMuted() const
	{
#ifdef ANDROID
		return GetSoundMuted(mPlayerObjs[0], mPlayers[0]);
#else
		return mIsMuted;
#endif
	}

#ifdef DESKTOP
	void SoundEffect::SetSoundVolume(int volume)
	{
		Mix_Volume(mPlayChannel, volume);
	}
#else
	void SoundEffect::RegisterCallback(SLPlayItf & player)
	{
		if((*player)->RegisterCallback(
			player, MusicStoppedCallback,
			this) != SL_RESULT_SUCCESS)
		{
			star::Logger::GetInstance()->Log(star::LogLevel::Error,
				_T("SoundEffect: Can't set callback"));
		}
	}

	void SoundEffect::MusicStoppedCallback(SLPlayItf caller,void *pContext,SLuint32 event)
	{
		(*caller)->SetPlayState(caller, SL_PLAYSTATE_STOPPED);
		SoundEffect* eff = reinterpret_cast<SoundEffect*>(pContext);
		//Search for the caller in the list
		for(int i = 0; i < eff->mPlayers.size() ; ++i)
		{
			if(eff->mPlayers[i] == caller)
			{
				//If infinite loop keep playing
				if(eff->mLoopTimes[i] == -1)
				{
					(*caller)->SetPlayState(caller, SL_PLAYSTATE_PLAYING);
				}
				else
				{
					//check if only play once otherwise keep playing
					if(eff->mLoopTimes[i] != 0)
					{
						(*caller)->SetPlayState(caller, SL_PLAYSTATE_PLAYING);
					}
					--eff->mLoopTimes[i];
				}
				break;
			}
		}
	}
#endif
}
