/*************************************************************************/
/*  resource_importer_ffmpeg.cpp                                         */
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

#include "resource_importer_ffmpeg.h"

#include "io/resource_saver.h"
#include "os/file_access.h"
#include "scene/resources/texture.h"

String ResourceImporterFFMPEG::get_importer_name() const {

	return "FFMPEG";
}

String ResourceImporterFFMPEG::get_visible_name() const {

	return "FFMPEG";
}
void ResourceImporterFFMPEG::get_recognized_extensions(List<String> *p_extensions) const {

	p_extensions->push_back("mp4");
}

String ResourceImporterFFMPEG::get_save_extension() const {
	return "ffmpegstr";
}

String ResourceImporterFFMPEG::get_resource_type() const {

	return "VideoStreamFFMPEG";
}

bool ResourceImporterFFMPEG::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {

	return true;
}

int ResourceImporterFFMPEG::get_preset_count() const {
	return 0;
}
String ResourceImporterFFMPEG::get_preset_name(int p_idx) const {

	return String();
}

void ResourceImporterFFMPEG::get_import_options(List<ImportOption> *r_options, int p_preset) const {

	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "loop"), true));
}

Error ResourceImporterFFMPEG::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files) {

	VideoStreamFFMPEG *stream = memnew(VideoStreamFFMPEG);
	stream->set_file(p_source_file);

	Ref<VideoStreamFFMPEG> ogv_stream = Ref<VideoStreamFFMPEG>(stream);

	return ResourceSaver::save(p_save_path + ".ffmpegstr", ogv_stream);
}

ResourceImporterFFMPEG::ResourceImporterFFMPEG() {
}
