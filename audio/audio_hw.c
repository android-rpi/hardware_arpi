/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <log/log.h>

#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>

#define PCM_CARD 0
#define PCM_DEVICE 0

#define PCM_IN_CARD_NAME "sndrpii2scard"
#define PCM_IN_CARD_DEFAULT 1
#define PCM_IN_DEVICE 0

#define DEFAULT_PERIOD_SIZE  1024
#define DEFAULT_PERIOD_COUNT 4

#define STUB_DEFAULT_SAMPLE_RATE   48000
#define STUB_DEFAULT_AUDIO_FORMAT  AUDIO_FORMAT_PCM_16_BIT

#define STUB_INPUT_BUFFER_MILLISECONDS  20
#define STUB_INPUT_DEFAULT_CHANNEL_MASK AUDIO_CHANNEL_IN_STEREO

#define STUB_OUTPUT_BUFFER_MILLISECONDS  10
#define STUB_OUTPUT_DEFAULT_CHANNEL_MASK AUDIO_CHANNEL_OUT_STEREO

struct pcm_config pcm_config_out = {
    .channels = 2,
    .rate = STUB_DEFAULT_SAMPLE_RATE,
    .period_size = DEFAULT_PERIOD_SIZE,
    .period_count = DEFAULT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_in = {
    .channels = 2,
    .rate = STUB_DEFAULT_SAMPLE_RATE,
    .period_size = DEFAULT_PERIOD_SIZE,
    .period_count = DEFAULT_PERIOD_COUNT,
    .format = PCM_FORMAT_S32_LE,
};

struct stub_audio_device {
    struct audio_hw_device device;
    pthread_mutex_t lock;
};

struct stub_stream_out {
    struct audio_stream_out stream;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    size_t frame_count;

    pthread_mutex_t lock;
    struct pcm_config *config;
    struct pcm *pcm;
    bool standby;
    uint64_t written;
    struct stub_audio_device *dev;
};

struct stub_stream_in {
    struct audio_stream_in stream;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    size_t frame_count;

    pthread_mutex_t lock;
    struct pcm_config *config;
    struct pcm *pcm;
    bool standby;
    struct stub_audio_device *dev;
    uint32_t pcm_card;

    void *raw_buf;
};

static int check_output_config(struct audio_config *audio_config) {
    uint32_t sample_rate = audio_config->sample_rate;
    audio_format_t format = audio_config->format;
    audio_channel_mask_t channel_mask = audio_config->channel_mask;

    if ((sample_rate == STUB_DEFAULT_SAMPLE_RATE) && (format == STUB_DEFAULT_AUDIO_FORMAT) &&
            (channel_mask == STUB_OUTPUT_DEFAULT_CHANNEL_MASK)) {
        return 0;
    } else {   
        ALOGD("check_output_config(sample_rate=%d, format=%d, channel_mask=%d)",
                sample_rate, format, channel_mask);
        return -EINVAL;
    }
}

static int check_input_parameters(uint32_t sample_rate, audio_format_t format,
                                  audio_channel_mask_t channel_mask) {
    if ((sample_rate == STUB_DEFAULT_SAMPLE_RATE) && (format == STUB_DEFAULT_AUDIO_FORMAT) &&
            (channel_mask == STUB_INPUT_DEFAULT_CHANNEL_MASK)) {
        return 0;
    } else {
        ALOGD("check_input_parameters(sample_rate=%d, format=%d, channel_mask=%d)",
                sample_rate, format, channel_mask);
        return -EINVAL;
    }
}

static int start_output_stream(struct stub_stream_out *out)
{
    ALOGV("start_output_stream");
    out->pcm = pcm_open(PCM_CARD, PCM_DEVICE, PCM_OUT, out->config);
    if (out->pcm == NULL) {
        return -ENOMEM;
    }
    if (out->pcm && !pcm_is_ready(out->pcm)) {
        ALOGE("pcm_open(out) failed: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm);
	out->pcm = NULL;
        return -ENOMEM;
    }
    ALOGV("%s exit",__func__);
    return 0;
}

static int start_input_stream(struct stub_stream_in *in)
{
    ALOGV("start_input_stream");
    in->pcm = pcm_open(in->pcm_card, PCM_IN_DEVICE, PCM_IN, in->config);
    if (in->pcm == NULL) {
        return -ENOMEM;
    }
    if (!pcm_is_ready(in->pcm)) {
        ALOGE("pcm_open(in) failed: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        in->pcm = NULL;
        return -ENOMEM;
    }
    return 0;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_sample_rate: %u", out->sample_rate);
    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;

    ALOGV("out_set_sample_rate: %d", rate);
    out->sample_rate = rate;
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;
    size_t buffer_size = out->frame_count *
                         audio_stream_out_frame_size(&out->stream);

    ALOGV("out_get_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_channels: %x", out->channel_mask);
    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_format: %d", out->format);
    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;

    ALOGV("out_set_format: %d", format);
    out->format = format;
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;
    ALOGV("out_standby");
    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);
    if (!out->standby) {
        pcm_close(out->pcm);
        out->pcm = NULL;
        out->standby = true;
    }
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    ALOGV("out_dump");
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("out_set_parameters");
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ALOGV("out_get_parameters");
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    ALOGV("out_get_latency");
    return STUB_OUTPUT_BUFFER_MILLISECONDS;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    ALOGV("out_set_volume: Left:%f Right:%f", left, right);
    return 0;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;
    struct stub_audio_device *adev = out->dev;
    size_t frame_size = audio_stream_out_frame_size(stream);
    int16_t *in_buffer = (int16_t *)buffer;
    size_t in_frames = bytes / frame_size;
    int ret = 0;

    ALOGV("out_write: bytes: %zu", bytes);

    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the output stream mutex - e.g.
     * executing out_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream(out);
        if (ret != 0) {
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }
        out->standby = false;
    }
    pthread_mutex_unlock(&adev->lock);

    ret = pcm_write(out->pcm, in_buffer, in_frames * frame_size);
    if (ret == -EPIPE) {
        /* In case of underrun, don't sleep since we want to catch up asap */
        pthread_mutex_unlock(&out->lock);
        return ret;
    }
    if (ret == 0) {
        out->written += in_frames;
    }

exit:
    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        usleep(bytes * 1000000 / audio_stream_out_frame_size(stream) /
               out_get_sample_rate(&stream->common));
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    *dsp_frames = 0;
    ALOGV("out_get_render_position: dsp_frames: %p", dsp_frames);
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_add_audio_effect: %p", effect);
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_remove_audio_effect: %p", effect);
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    *timestamp = 0;
    ALOGV("out_get_next_write_timestamp: %ld", (long int)(*timestamp));
    return -EINVAL;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_sample_rate: %u", in->sample_rate);
    return in->sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;

    ALOGV("in_set_sample_rate: %u", rate);
    in->sample_rate = rate;
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;
    size_t buffer_size = in->frame_count *
                         audio_stream_in_frame_size(&in->stream);

    ALOGV("in_get_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_channels: %x", in->channel_mask);
    return in->channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_format: %d", in->format);
    return in->format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;

    ALOGV("in_set_format: %d", format);
    in->format = format;
    return 0;
}

static int do_input_standby(struct stub_stream_in *in)
{
    if (!in->standby) {
        pcm_close(in->pcm);
        in->pcm = NULL;
        in->standby = true;
    }
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;
    int status;
    ALOGV("in_standby");
    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    status = do_input_standby(in);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static void copy32to16(void *dest, void *src, size_t bytes) {
    uint16_t *dest16 = (uint16_t *)dest;
    uint16_t *src32 = (uint16_t *)src;
    for(size_t i=0; i < (bytes/2); i++) {
        dest16[i] = src32[2*i + 1];
    }
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;
    struct stub_audio_device *adev = in->dev;
    int ret = 0;

    ALOGV("in_read: bytes %zu", bytes);

    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        pthread_mutex_unlock(&adev->lock);
        if (ret != 0) {
            goto exit;
        }
        in->standby = false;
    }

    ret = pcm_read(in->pcm, in->raw_buf, bytes * 2);
    if (ret != 0) {
        ALOGE("pcm_read() returned error %d : %s", ret, pcm_get_error(in->pcm));
        memset(buffer, 0 , bytes);
    } else {
        copy32to16(buffer, in->raw_buf, bytes);
    }

exit:
    pthread_mutex_unlock(&in->lock);
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static size_t samples_per_milliseconds(size_t milliseconds,
                                       uint32_t sample_rate,
                                       size_t channel_count)
{
    return milliseconds * sample_rate * channel_count / 1000;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    ALOGV("adev_open_output_stream...");

    *stream_out = NULL;

    int ret = check_output_config(config);
    if (ret != 0) return ret;

    struct stub_stream_out *out =
            (struct stub_stream_out *)calloc(1, sizeof(struct stub_stream_out));
    if (!out)
        return -ENOMEM;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->sample_rate = config->sample_rate;
    if (out->sample_rate == 0)
        out->sample_rate = STUB_DEFAULT_SAMPLE_RATE;
    out->channel_mask = config->channel_mask;
    if (out->channel_mask == AUDIO_CHANNEL_NONE)
        out->channel_mask = STUB_OUTPUT_DEFAULT_CHANNEL_MASK;
    out->format = config->format;
    if (out->format == AUDIO_FORMAT_DEFAULT)
        out->format = STUB_DEFAULT_AUDIO_FORMAT;
    out->frame_count = samples_per_milliseconds(
                           STUB_OUTPUT_BUFFER_MILLISECONDS,
                           out->sample_rate, 1);

    out->config = &pcm_config_out;
    out->dev = (struct stub_audio_device *)dev;
    out->standby = true;
    out->written = 0;

    ALOGV("adev_open_output_stream: sample_rate: %u, channels: %x, format: %d,"
          " frames: %zu", out->sample_rate, out->channel_mask, out->format,
          out->frame_count);
    *stream_out = &out->stream;
    return 0;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    ALOGV("adev_close_output_stream...");
    out_standby(&stream->common);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    ALOGV("adev_set_parameters");
    return -ENOSYS;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    ALOGV("adev_get_parameters");
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    ALOGV("adev_init_check");
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_voice_volume: %f", volume);
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_master_volume: %f", volume);
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume)
{
    ALOGV("adev_get_master_volume: %f", *volume);
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    ALOGV("adev_set_master_mute: %d", muted);
    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    ALOGV("adev_get_master_mute: %d", *muted);
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    ALOGV("adev_set_mode: %d", mode);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    ALOGV("adev_set_mic_mute: %d",state);
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    ALOGV("adev_get_mic_mute");
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    size_t buffer_size = samples_per_milliseconds(
                             STUB_INPUT_BUFFER_MILLISECONDS,
                             config->sample_rate,
                             audio_channel_count_from_in_mask(
                                 config->channel_mask));

    if (!audio_has_proportional_frames(config->format)) {
        // Since the audio data is not proportional choose an arbitrary size for
        // the buffer.
        buffer_size *= 4;
    } else {
        buffer_size *= audio_bytes_per_sample(config->format);
    }
    ALOGV("adev_get_input_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static uint32_t probe_pcm_in_card() {
    FILE *fp;
    char card_info[128];
    if((fp = fopen("/proc/asound/cards","r")) == NULL) {
        ALOGE("Cannot open /proc/asound/cards file to get sound card info");
    } else {
        while((fgets(card_info, sizeof(card_info), fp) != NULL)) {
            ALOGV("/proc/asound/cards readout : %s", card_info);
            if (strstr(card_info, PCM_IN_CARD_NAME)) {
                fclose(fp);
                return card_info[1]-'0';
            }
        }
        fclose(fp);
    }
    return PCM_IN_CARD_DEFAULT;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags __unused,
                                  const char *address __unused,
                                  audio_source_t source __unused)
{
    ALOGV("adev_open_input_stream...");

    int ret = check_input_parameters(config->sample_rate, config->format,
           config->channel_mask);
    if (ret != 0) return ret;

    *stream_in = NULL;
    struct stub_stream_in *in = (struct stub_stream_in *)calloc(1, sizeof(struct stub_stream_in));
    if (!in)
        return -ENOMEM;

    in->raw_buf = calloc(2, adev_get_input_buffer_size(NULL, config));
    if (!in->raw_buf) {
        free(in);
        return -ENOMEM;
    }

    in->pcm_card = probe_pcm_in_card();

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->sample_rate = config->sample_rate;
    if (in->sample_rate == 0)
        in->sample_rate = STUB_DEFAULT_SAMPLE_RATE;
    in->channel_mask = config->channel_mask;
    if (in->channel_mask == AUDIO_CHANNEL_NONE)
        in->channel_mask = STUB_INPUT_DEFAULT_CHANNEL_MASK;
    in->format = config->format;
    if (in->format == AUDIO_FORMAT_DEFAULT)
        in->format = STUB_DEFAULT_AUDIO_FORMAT;
    in->frame_count = samples_per_milliseconds(
                          STUB_INPUT_BUFFER_MILLISECONDS, in->sample_rate, 1);

    in->config = &pcm_config_in;
    in->dev = (struct stub_audio_device *)dev;
    in->standby = true;

    ALOGV("adev_open_input_stream: sample_rate: %u, channels: %x, format: %d,"
          "frames: %zu", in->sample_rate, in->channel_mask, in->format,
          in->frame_count);
    *stream_in = &in->stream;
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *in)
{
    ALOGV("adev_close_input_stream...");
    in_standby(&in->common);

    struct stub_stream_in *sin = (struct stub_stream_in *)in;
    free(sin->raw_buf);
    free(in);
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    ALOGV("adev_dump");
    return 0;
}

static int adev_close(hw_device_t *device)
{
    ALOGV("adev_close");
    free(device);
    return 0;
}

static void set_mixer() {
    // Set default mixer ctls
    // Enable channels and set volume
    struct mixer* mixer = mixer_open(PCM_CARD);
    struct mixer_ctl *ctl;
    for (int i = 0; i < (int)mixer_get_num_ctls(mixer); i++) {
        ctl = mixer_get_ctl(mixer, i);
        ALOGD("mixer %d name %s", i, mixer_ctl_get_name(ctl));
        if (!strcmp(mixer_ctl_get_name(ctl), "Headphone Playback Volume")) {
            for (int z = 0; z < (int)mixer_ctl_get_num_values(ctl); z++) {
                ALOGD("set ctl %d to %d", z, 1200);
                mixer_ctl_set_value(ctl, z, 1200);
            }
            continue;
        }
        if (!strcmp(mixer_ctl_get_name(ctl), "Headphone Playback Switch")) {
            for (int z = 0; z < (int)mixer_ctl_get_num_values(ctl); z++) {
                ALOGD("set ctl %d to %d", z, 1);
                mixer_ctl_set_value(ctl, z, 1);
            }
            continue;
        }
    }
    mixer_close(mixer);
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    ALOGV("adev_open: %s", name);

    struct stub_audio_device *adev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct stub_audio_device));
    if (!adev)
        return -ENOMEM;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    *device = &adev->device.common;

    set_mixer();

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Raspberry Pi Audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
