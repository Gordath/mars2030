#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <vorbis/vorbisfile.h>
#include "openal.h"
#include "sample.h"


#define CHECK_ERROR \
    do { \
        unsigned int err = alGetError(); \
        if(err != 0) { \
            fprintf(stderr, "%s:%d: AL error: %#x\n", __FILE__, __LINE__, err); \
            abort(); \
        } \
    } while(0)


AudioSample::AudioSample()
{
	albuffer = 0;
}

AudioSample::~AudioSample()
{
	destroy();
}

void AudioSample::destroy()
{
	if(albuffer) {
		alDeleteBuffers(1, &albuffer);
		albuffer = 0;
	}
}

bool AudioSample::load(const char *fname)
{
	OggVorbis_File vf;
	if(ov_fopen(fname, &vf) != 0) {
		fprintf(stderr, "failed to open ogg/vorbis file: %s\n", fname);
		return false;
	}
	vorbis_info *vinfo = ov_info(&vf, -1);
	ALenum alfmt = vinfo->channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

	printf("loading sample: %s: %ld samples/s, %s (%d chan)\n", fname, vinfo->rate,
			vinfo->channels == 1 ? "mono" : "stereo", vinfo->channels);

	long num_samples = ov_pcm_total(&vf, -1) * vinfo->channels;
	int16_t *samples = new int16_t[num_samples];

	long bufsz = num_samples * sizeof *samples;
	long total_read = 0;
	while(total_read < bufsz) {
		int bitstream;
		long rd = ov_read(&vf, (char*)samples + total_read, bufsz, 0, 2, 1, &bitstream);
		if(!rd) {
			bufsz = total_read;
			printf("%s: unexpected eof while reading: %s\n", __FUNCTION__, fname);
		} else {
			total_read += rd;
		}
	}

	assert(alGetError() == AL_NO_ERROR);

	unsigned int bufobj = 0;
	alGenBuffers(1, &bufobj);
	if(alGetError()) {
		fprintf(stderr, "failed to create OpenAL buffer\n");
		goto err;
	}

	alBufferData(bufobj, alfmt, samples, bufsz, vinfo->rate);
	CHECK_ERROR;
	if(alGetError()) {
		fprintf(stderr, "failed to load sample data into OpenAL buffer: %u\n", bufobj);
		goto err;
	}

	ov_clear(&vf);
	delete [] samples;

	destroy();	// cleanup previous buffer if any
	albuffer = bufobj;
	return true;

err:
	delete [] samples;
	ov_clear(&vf);
	if(bufobj) {
		alDeleteBuffers(1, &bufobj);
	}
	return false;
}

AudioSample *load_audio_sample(const char *fname)
{
	AudioSample *s = new AudioSample;

	if(!s->load(fname)) {
		delete s;
		s = 0;
	}
	return s;
}

void destroy_audio_sample(AudioSample *s)
{
	delete s;
}
