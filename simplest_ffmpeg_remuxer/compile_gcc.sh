#! /bin/sh
gcc simplest_ffmpeg_remuxer.cpp -I /usr/local/ffmpeg/include -L /usr/local/ffmpeg/lib -g -o simplest_ffmpeg_remuxer.out -I /usr/local/include -L /usr/local/lib \
-lavformat -lavcodec -lavutil
