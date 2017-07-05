#! /bin/sh
gcc simplest_ffmpeg_demuxer.cpp -I /usr/local/ffmpeg/include -L /usr/local/ffmpeg/lib -g -o simplest_ffmpeg_demuxer.out \
-I /usr/local/include -L /usr/local/lib -lavformat -lavcodec -lavutil
