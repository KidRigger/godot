/*************************************************************************/
/*  videostream_ffmpeg.cpp                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "videostream_ffmpeg.h"
#include <project_settings.h>
#include <servers/audio_server.h>

static const godot_videodecoder_interface_gdnative *stat_interface = nullptr;

// NOTE: Callbacks for the GDNative libraries.
extern "C" {
godot_int GDAPI godot_videodecoder_file_read(void *ptr, uint8_t *buf, int buf_size) {
	// ptr is a FileAccess
	FileAccess *file = reinterpret_cast<FileAccess *>(ptr);

	// if file exists
	if (file) {
		long bytes_read = file->get_buffer(buf, buf_size);
		// No bytes to read => EOF
		if (bytes_read == 0) {
			return 0;
		}
		return bytes_read;
	}
	return -1;
}

int64_t GDAPI godot_videodecoder_file_seek(void *ptr, int64_t pos, int whence) {
	// file
	FileAccess *file = reinterpret_cast<FileAccess *>(ptr);

	size_t len = file->get_len();
	if (file) {
		switch (whence) {
			case SEEK_SET: {
				// Just for explicitness
				size_t new_pos = static_cast<size_t>(pos);
				if (new_pos > len) {
					return -1;
				}
				file->seek(new_pos);
				pos = static_cast<int64_t>(file->get_position());
				return pos;
			} break;
			case SEEK_CUR: {
				// Just in case it doesn't exist
				if (pos < 0 && -pos > file->get_position()) {
					return -1;
				}
				pos = pos + static_cast<int>(file->get_position());
				file->seek(pos);
				pos = static_cast<int64_t>(file->get_position());
				return pos;
			} break;
			case SEEK_END: {
				// Just in case something goes wrong
				if (-pos > len) {
					return -1;
				}
				file->seek_end(pos);
				pos = static_cast<int64_t>(file->get_position());
				return pos;
			} break;
			default: {
				// Only 4 possible options, hence default = AVSEEK_SIZE
				// Asks to return the length of file
				return static_cast<int64_t>(len);
			} break;
		}
	}
	// In case nothing works out.
	return -1;
}

void GDAPI godot_videodecoder_register_decoder(const godot_videodecoder_interface_gdnative *p_interface) {
	print_line("Interface registered");
	print_line(p_interface->get_plugin_name());
	stat_interface = p_interface;
}
}

// VideoStreamPlaybackFFMPEG starts here.

bool VideoStreamPlaybackFFMPEG::open_file(const String &p_file) {
	ERR_FAIL_COND_V(interface == nullptr, false);
	file = FileAccess::open(p_file, FileAccess::READ);
	bool file_opened = interface->open_file(data_struct, file);

	num_channels = interface->get_channels(data_struct);
	mix_rate = interface->get_mix_rate(data_struct);

	godot_vector2 vec = interface->get_texture_size(data_struct);
	texture_size = *(Vector2 *)&vec;

	pcm = (float *)memalloc(num_channels * AUX_BUFFER_SIZE * sizeof(float));
	memset(pcm, 0, num_channels * AUX_BUFFER_SIZE * sizeof(float));
	pcm_write_idx = -1;
	samples_decoded = 0;

	texture->create((int)texture_size.width, (int)texture_size.height, Image::FORMAT_RGBA8, Texture::FLAG_FILTER | Texture::FLAG_VIDEO_SURFACE);

	return file_opened;
}

void VideoStreamPlaybackFFMPEG::update(float p_delta) {
	if (!playing || paused) {
		return;
	}
	if (!file) {
		return;
	}
	time += p_delta;
	ERR_FAIL_COND(interface == nullptr);
	interface->update(data_struct, p_delta);

	if (pcm_write_idx >= 0) {
		// Previous remains
		int mixed = mix_callback(mix_udata, pcm, samples_decoded);
		if (mixed == samples_decoded) {
			pcm_write_idx = -1;
		} else {
			samples_decoded -= mixed;
			pcm_write_idx += mixed;
		}
	}
	if (pcm_write_idx < 0) {
		samples_decoded = interface->get_audioframe(data_struct, pcm, AUX_BUFFER_SIZE);
		pcm_write_idx = mix_callback(mix_udata, pcm, samples_decoded);
		if (pcm_write_idx == samples_decoded) {
			pcm_write_idx = -1;
		} else {
			samples_decoded -= pcm_write_idx;
		}
	}

	printf("R: %i\tM %i\n", samples_decoded, pcm_write_idx);

	while (interface->get_playback_position(data_struct) < time) {

		update_texture();
	}
}

void VideoStreamPlaybackFFMPEG::update_texture() {
	PoolByteArray *pba = (PoolByteArray *)interface->get_videoframe(data_struct);

	if (pba == NULL) {
		playing = false;
		return;
	}

	Ref<Image> img = memnew(Image(texture_size.width, texture_size.height, 0, Image::FORMAT_RGBA8, *pba));

	texture->set_data(img);
}

// ctor and dtor

VideoStreamPlaybackFFMPEG::VideoStreamPlaybackFFMPEG() :
		texture(Ref<ImageTexture>(memnew(ImageTexture))),
		time(0),
		mix_udata(nullptr),
		mix_callback(nullptr),
		num_channels(-1),
		mix_rate(0) {}

VideoStreamPlaybackFFMPEG::~VideoStreamPlaybackFFMPEG() {
	cleanup();
}

void VideoStreamPlaybackFFMPEG::cleanup() {
	interface->destructor(data_struct);
	memfree(pcm);
	pcm = nullptr;
	time = 0;
	num_channels = -1;
	interface = nullptr;
	data_struct = nullptr;
}

void VideoStreamPlaybackFFMPEG::set_interface(const godot_videodecoder_interface_gdnative *p_interface) {
	if (interface != nullptr) {
		cleanup();
	}
	interface = p_interface;
	data_struct = interface->constructor((godot_object *)this);
}

// controls

bool VideoStreamPlaybackFFMPEG::is_playing() const {
	return playing;
}

bool VideoStreamPlaybackFFMPEG::is_paused() const {
	return paused;
}

void VideoStreamPlaybackFFMPEG::play() {

	stop();

	playing = true;

	delay_compensation = ProjectSettings::get_singleton()->get("audio/video_delay_compensation_ms");
	delay_compensation /= 1000.0;
}

void VideoStreamPlaybackFFMPEG::stop() {
	if (playing) {
		seek(0);
	}
	playing = false;
}

void VideoStreamPlaybackFFMPEG::seek(float p_time) {
	ERR_FAIL_COND(interface == nullptr);
	interface->seek(data_struct, p_time);
}

void VideoStreamPlaybackFFMPEG::set_paused(bool p_paused) {
	paused = p_paused;
}

Ref<Texture> VideoStreamPlaybackFFMPEG::get_texture() {
	return texture;
}

float VideoStreamPlaybackFFMPEG::get_length() const {
	ERR_FAIL_COND_V(interface == nullptr, 0);
	return interface->get_length(data_struct);
}

float VideoStreamPlaybackFFMPEG::get_playback_position() const {

	ERR_FAIL_COND_V(interface == nullptr, 0);
	return interface->get_playback_position(data_struct);
}

bool VideoStreamPlaybackFFMPEG::has_loop() const {
	// TODO: Implement looping?
	return false;
}

void VideoStreamPlaybackFFMPEG::set_loop(bool p_enable) {
	// Do nothing
}

void VideoStreamPlaybackFFMPEG::set_audio_track(int p_idx) {
	ERR_FAIL_COND(interface == nullptr);
	interface->set_audio_track(data_struct, p_idx);
}

void VideoStreamPlaybackFFMPEG::set_mix_callback(AudioMixCallback p_callback, void *p_userdata) {

	mix_udata = p_userdata;
	mix_callback = p_callback;
}

int VideoStreamPlaybackFFMPEG::get_channels() const {
	ERR_FAIL_COND_V(interface == nullptr, 0);

	return (num_channels > 0) ? num_channels : 0;
}

int VideoStreamPlaybackFFMPEG::get_mix_rate() const {
	ERR_FAIL_COND_V(interface == nullptr, 0);

	return mix_rate;
}

/* --- NOTE VideoStreamFFMPEG starts here. ----- */

Ref<VideoStreamPlayback> VideoStreamFFMPEG::instance_playback() {
	Ref<VideoStreamPlaybackFFMPEG> pb = memnew(VideoStreamPlaybackFFMPEG);
	pb->set_interface(stat_interface);
	pb->set_audio_track(audio_track);
	if (pb->open_file(file))
		return pb;
	return NULL;
}

void VideoStreamFFMPEG::set_file(const String &p_file) {

	file = p_file;
}

String VideoStreamFFMPEG::get_file() {

	return file;
}

void VideoStreamFFMPEG::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_file", "file"), &VideoStreamFFMPEG::set_file);
	ClassDB::bind_method(D_METHOD("get_file"), &VideoStreamFFMPEG::get_file);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "file", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "set_file", "get_file");
}

void VideoStreamFFMPEG::set_audio_track(int p_track) {

	audio_track = p_track;
}
