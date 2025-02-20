#include "stdafx.h"
#include "AudioBackend.h"
#include "Emu/system_config.h"
#include "Emu/IdManager.h"
#include "Emu//Cell/Modules/cellAudioOut.h"

AudioBackend::AudioBackend() {}

void AudioBackend::SetErrorCallback(std::function<void()> cb)
{
	std::lock_guard lock(m_error_cb_mutex);
	m_error_callback = cb;
}

/*
 * Helper methods
 */
u32 AudioBackend::get_sampling_rate() const
{
	return static_cast<std::underlying_type_t<decltype(m_sampling_rate)>>(m_sampling_rate);
}

u32 AudioBackend::get_sample_size() const
{
	return static_cast<std::underlying_type_t<decltype(m_sample_size)>>(m_sample_size);
}

u32 AudioBackend::get_channels() const
{
	return static_cast<std::underlying_type_t<decltype(m_channels)>>(m_channels);
}

bool AudioBackend::get_convert_to_s16() const
{
	return m_sample_size == AudioSampleSize::S16;
}

void AudioBackend::convert_to_s16(u32 cnt, const f32* src, void* dst)
{
	for (u32 i = 0; i < cnt; i++)
	{
		static_cast<s16*>(dst)[i] = static_cast<s16>(std::clamp(src[i] * 32768.5f, -32768.0f, 32767.0f));
	}
}

f32 AudioBackend::apply_volume(const VolumeParam& param, u32 sample_cnt, const f32* src, f32* dst)
{
	ensure(param.ch_cnt > 1 && param.ch_cnt % 2 == 0); // Tends to produce faster code

	const f32 vol_incr = (param.target_volume - param.initial_volume) / (VOLUME_CHANGE_DURATION * param.freq);
	f32 crnt_vol = param.current_volume;
	u32 sample_idx = 0;

	if (vol_incr >= 0)
	{
		for (sample_idx = 0; sample_idx < sample_cnt && crnt_vol != param.target_volume; sample_idx += param.ch_cnt)
		{
			crnt_vol = std::min(param.current_volume + (sample_idx + 1) / param.ch_cnt * vol_incr, param.target_volume);

			for (u32 i = 0; i < param.ch_cnt; i++)
			{
				dst[sample_idx + i] = src[sample_idx + i] * crnt_vol;
			}
		}
	}
	else
	{
		for (sample_idx = 0; sample_idx < sample_cnt && crnt_vol != param.target_volume; sample_idx += param.ch_cnt)
		{
			crnt_vol = std::max(param.current_volume + (sample_idx + 1) / param.ch_cnt * vol_incr, param.target_volume);

			for (u32 i = 0; i < param.ch_cnt; i++)
			{
				dst[sample_idx + i] = src[sample_idx + i] * crnt_vol;
			}
		}
	}

	if (sample_cnt > sample_idx)
	{
		apply_volume_static(param.target_volume, sample_cnt - sample_idx, &src[sample_idx], &dst[sample_idx]);
	}

	return crnt_vol;
}

void AudioBackend::apply_volume_static(f32 vol, u32 sample_cnt, const f32* src, f32* dst)
{
	for (u32 i = 0; i < sample_cnt; i++)
	{
		dst[i] = src[i] * vol;
	}
}

void AudioBackend::normalize(u32 sample_cnt, const f32* src, f32* dst)
{
	for (u32 i = 0; i < sample_cnt; i++)
	{
		dst[i] = std::clamp<f32>(src[i], -1.0f, 1.0f);
	}
}

AudioChannelCnt AudioBackend::get_channel_count(u32 device_index)
{
	audio_out_configuration& audio_out_cfg = g_fxo->get<audio_out_configuration>();
	std::lock_guard lock(audio_out_cfg.mtx);
	ensure(device_index < audio_out_cfg.out.size());
	const audio_out_configuration::audio_out& out = audio_out_cfg.out.at(device_index);

	switch (out.downmixer)
	{
	case CELL_AUDIO_OUT_DOWNMIXER_NONE:
	{
		switch (out.channels)
		{
		case 2: return AudioChannelCnt::STEREO;
		case 6: return AudioChannelCnt::SURROUND_5_1;
		case 8: return AudioChannelCnt::SURROUND_7_1;
		default:
			fmt::throw_exception("Unsupported channel count in cellAudioOut config: %d", out.channels);
		}
	}
	case CELL_AUDIO_OUT_DOWNMIXER_TYPE_A:
	{
		switch (out.channels)
		{
		case 2:
			return AudioChannelCnt::STEREO;
		default:
			fmt::throw_exception("Unsupported channel count for CELL_AUDIO_OUT_DOWNMIXER_TYPE_A in cellAudioOut config: %d", out.channels);
		}
	}
	case CELL_AUDIO_OUT_DOWNMIXER_TYPE_B:
	{
		switch (out.channels)
		{
		case 6:
		case 8:
			return AudioChannelCnt::SURROUND_5_1;
		default:
			fmt::throw_exception("Unsupported channel count for CELL_AUDIO_OUT_DOWNMIXER_TYPE_B in cellAudioOut config: %d", out.channels);
		}
	}
	default:
		fmt::throw_exception("Unknown downmixer in cellAudioOut config: %d", out.downmixer);
	}
}
