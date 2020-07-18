#include <assert.h>
#include <SDL2/SDL.h>

#include "audio.h"
#include "spectrum.h"

using namespace std;

float *PCM;
size_t PCM_len;
size_t audio_len;
AVCodecContext *aCodecCtx = NULL;
size_t buf_pos = 0;

/*
	userdata:	an application-specific parameter saved in the SDL_AudioSpec structure's userdata field
	stream:		a pointer to the audio data buffer filled in by SDL_AudioCallback()
	len:		the length of that buffer in bytes
*/
void audio_callback(void *userdata, Uint8 *stream, int len) {
	assert((int) sizeof(float) < len);

	aCodecCtx = (AVCodecContext *) userdata;
	float *audio_buf = (float *) stream;
	size_t buf_len = len / sizeof(float);
	if (PCM_len >= buf_len) {memcpy(audio_buf, &PCM[buf_pos], len);}
	else {
		// end of the PCM data
		memcpy(audio_buf, &PCM[buf_pos], PCM_len * sizeof(float));
		PCM_len = 0;
		return;
	}
	
	buf_pos += buf_len;
	PCM_len -= buf_len;
}

// time spent since the audio starts playing, in ns
unsigned long get_time(unsigned long &audio_len) {
	return (audio_len - PCM_len) * 1000000000 / 44100;
}

int main(int argc, char const *argv[]) {
	if (argc != 2) { // invalid usage
		cout << "usage: $" << argv[0] << " path_to_audio" << endl;
		exit(1);
	}

	audio new_audio(argv[1]);
	new_audio.decode(aCodecCtx, &PCM, &PCM_len);

	spectrum new_spectrum(2048);
	vector<vector<float>> f_bins = new_spectrum.DFT(&PCM, &PCM_len);
	// analyze the energy distributed in the investigated frequency band
	// to determine the appearance of the beat
	unordered_set<unsigned long> appeared_t = new_spectrum.beat_detector(f_bins, 60, 130);	// kick drum
	unordered_set<unsigned long> midrange = new_spectrum.beat_detector(f_bins, 301, 750);	// snare drum
	appeared_t.insert(midrange.begin(), midrange.end());

	audio_len = PCM_len;

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		cout << "Failed to initialize SDL" << endl;
		exit(1);
	}

	SDL_AudioSpec wanted_spec, spec;
	// set audio
	wanted_spec.freq = 44100;
	wanted_spec.format = AUDIO_F32SYS;
	wanted_spec.channels = 1;
	wanted_spec.silence = 0;
	// a sample frame is a chunk
	// of audio data of the size specified in format multiplied by the number of channels
	wanted_spec.samples = 2048;
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = aCodecCtx;
	if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
		cout << "SDL failed to open audio" << endl;
		exit(1);
	}

	SDL_PauseAudio(0); // audio start playing

	// create window & renderer
	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;
	if (SDL_CreateWindowAndRenderer(1024, 480, 0, &window, &renderer) == 0) {
		SDL_SetWindowTitle(window, argv[1]);
		SDL_Event event;
		// beat indicator
		SDL_Rect indicator = {1004, 460, 10, 10};

		// while audio is playing
		// display its corresponding frequency spectrum based on
		// the current PCM data that SDL is reading
		while (PCM_len > 0) {
			// set the background
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
			SDL_RenderClear(renderer);
			// draw the spectrum
			SDL_SetRenderDrawColor(renderer, 0, 255, 127, SDL_ALPHA_OPAQUE);

			unsigned long c_time = get_time(audio_len);
			unsigned long t_interval = new_spectrum.get_interval();
			size_t frame = c_time / t_interval;
			// end of the animation
			if (frame == f_bins.size()) {break;}

			// only the first 1024 frequency bins are useful
			// display the first 512 frequency bins
			SDL_Rect bar[1024];
			for (size_t i = 0; i < 512; i++) {
				bar[i].x = i * 2;
				bar[i].y = 240;
				bar[i].w = 2;
				bar[i].h = f_bins[frame][i];

				// mirror image
				bar[512 + i].x = i * 2;
				bar[512 + i].y = 240 - f_bins[frame][i];
				bar[512 + i].w = 2;
				bar[512 + i].h = f_bins[frame][i];
			}
			SDL_RenderFillRects(renderer, bar, 1024);

			// if a beat occur
			if (appeared_t.find(c_time / 1000) != appeared_t.end()) {
				SDL_SetRenderDrawColor(renderer, 255, 69, 0, SDL_ALPHA_OPAQUE);
				SDL_RenderFillRect(renderer, &indicator);
			}

			SDL_RenderPresent(renderer);

			// delay for a time interval
			while (get_time(audio_len) - c_time < t_interval and PCM_len != 0) {SDL_PollEvent(&event);}

			// close window action triggered
			if (event.type == SDL_QUIT) {
				break;
			}
		}
	} else {
		cout << "Failed to initialize SDL window..." << endl;
		exit(1);
	}

	// close video output
	if (renderer) {
		SDL_DestroyRenderer(renderer);
	}
	if (window) {
		SDL_DestroyWindow(window);
	}
	// close audio output
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	
	// free spaces
	avcodec_free_context(&aCodecCtx);
	free(PCM);

	return 0;
}
