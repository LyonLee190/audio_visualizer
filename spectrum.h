#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <algorithm>
#include <cmath>
#include <fftw3.h>
#include <iostream>
#include <unordered_set>
#include <vector>

using namespace std;

class spectrum{
public:
	/*
		num_samples: # of samples used to perform a DFT
	*/
	spectrum(size_t num_samples) {
		this->num_samples = num_samples;
		this->num_bins = num_samples / 2 + 1;
		this->t_interval = 1000000000 * num_samples / 44100;
	}

	/*
		PCM:	pulse-code modulation
		len:	size of the PCM data

		Apply Hanning window to prevent spectral leakage
	*/
	void Hanning(float **PCM, size_t &len);

	/*
		PCM:		pulse-code modulation
		PCM_len:	size of the PCM data
	*/
	vector<vector<float>> DFT(float **PCM, size_t *PCM_len);

	/*
		f_bins:		magnitudes of frequency bins for each time interval
		band_min:	lower bound of the investigated frequency band
		band_max:	upper bound of the investigated frequency band

		Analyze the energy distributed in the investigated frequency band
		to determine the appearance of the beat
		https://www.parallelcube.com/2018/03/30/beat-detection-algorithm/
	*/
	unordered_set<unsigned long> beat_detector(vector<vector<float>> &f_bins, size_t band_min, size_t band_max);

	unsigned int get_interval() {
		return this->t_interval;
	}

private:
	size_t num_samples;
	// # of frequency bins
	size_t num_bins;
	// time interval for the frequency spectrum set, in ns
	unsigned long t_interval;
};

void spectrum::Hanning(float **PCM, size_t &len) {
	for (size_t i = 0; i < len; i++) {
		// exponential term
		float W_n = 0.5 * (1 - cos(2 * M_PI * i / (len - 1)));

		(*PCM)[i] = (*PCM)[i] * W_n;
	}
}

vector<vector<float>> spectrum::DFT(float **PCM, size_t *PCM_len) {
	cout << "Performing DFT..." << endl;
	size_t duration = *PCM_len / num_samples;

	// magnitudes of frequency bins for each time interval
	vector<vector<float>> f_bins(duration, vector<float> (num_bins, 0));

	// Discrete Fourier transform
	for (size_t i = 0; i < duration; i++) {
		float *buffer = new float[num_samples];
		copy_n(*PCM + i * num_samples, num_samples, buffer);

		Hanning(&buffer, num_samples);

		fftwf_complex *out;
		out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * num_bins);
		fftwf_plan p = fftwf_plan_dft_r2c_1d(num_samples, buffer, out, FFTW_ESTIMATE);
		fftwf_execute(p);

		for (size_t j = 0; j < num_bins; j++) {
			// magnitude
			f_bins[i][j] = sqrt(pow(out[j][0], 2) + pow(out[j][1], 2));
		}

		fftwf_destroy_plan(p);
		fftwf_free(out);
		delete [] buffer;
	}

	cout << "Done!" << endl;
	return f_bins;
}

unordered_set<unsigned long> spectrum::beat_detector(vector<vector<float>> &f_bins,
	size_t band_min, size_t band_max) {
	cout << "Detecting beats..." << endl;

	// energy distributed in the investigated frequency band for each time interval
	vector<float> energy;
	// band_max * num_samples / sample_rate: index of the maximum frequency in the frequency bins
	// band_min * num_samples / sample_rate: index of the minimum frequency in the frequency bins
	size_t index_num = (band_max * num_samples / 44100) - (band_min * num_samples / 44100);
	// total energy distributed in the investigated frequency band
	float totalE = 0;
	for (size_t i = 0; i < f_bins.size(); i++) {
		float f_magnitude_sum = 0;
		// get the energy distributed for certain frequency
		for (size_t j = band_min * num_samples / 44100; j < band_max * num_samples / 44100; j++) {
			f_magnitude_sum += f_bins[i][j];
		}

		energy.push_back(f_magnitude_sum / index_num);
		totalE += (f_magnitude_sum / index_num);
	}

	// average energy distributed in the investigated frequency band for each time interval
	totalE = totalE / f_bins.size();

	// compute the variance & threshold
	float variance = 0;
	for (size_t i = 0; i < f_bins.size(); i++) {
		variance += pow(energy[i] - totalE, 2);
	}
	variance = variance / f_bins.size();

	// http://mziccard.me/2015/05/28/beats-detection-algorithms-1/
	float threshold = -0.0000015 * variance / f_bins.size() + 1.5142857;
	// float threshold = -15 * variance + 1.55;

	// appeared time
	unordered_set<unsigned long> appeared_t;
	// qualified appearances of beats
	for (size_t i = 0; i < energy.size(); i++) {
		if (energy[i] > threshold * totalE) {
			appeared_t.insert(i * t_interval / 1000); // round to us
		}
	}

	cout << "Done!" << endl;
	return appeared_t;
}

#endif
