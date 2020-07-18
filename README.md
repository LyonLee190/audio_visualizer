# Audio Visualizer
1. Decodes audio files into PCM.
2. Applies DFT to the PCM data to get the frequency spectrum.
3. Plays audio while displaying its corresponding frequency spectrum.
4. Detects the occurrence of the beat (not precise), the orange dot indicator flashes at the bottom right of the window.

### Prerequisite
- libavcodec

```sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev```

- SDL 2.0

```sudo apt-get install libsdl2-2.0```

```sudo apt-get install libsdl2-dev```

- FFTW

  Follow the installation instruction on http://www.fftw.org/, add ``--enable-float``  to enable the float type support.

### Instruction
In terminal:

```make```

```/main PTAH_TO_AUDIO```

### Demo
[!DEMO](./DEMO/demo.gif)

### Notes
You may need to use ```-lavcodec-ffmpeg -lavformat-ffmpeg -lavutil -lswresample``` flag in the Makefile to compile the program.

### Reference
https://rodic.fr/blog/libavcodec-tutorial-decode-audio-file/

http://dranger.com/ffmpeg/ffmpegtutorial_all.htm
