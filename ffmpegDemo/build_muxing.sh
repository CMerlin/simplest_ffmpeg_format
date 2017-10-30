#! /bin/sh
#c++录制诚信
#gcc record.cpp -I /usr/local/ffmpeg/include -L /usr/local/ffmpeg/lib -g -o simplest_ffmpeg_demuxer.out \ -I /usr/local/include -L /usr/local/lib -lavformat -lavcodec -lavutil
#删除历史的可执行程序
rm ./muxing.out
#c写的录制诚信
#gcc muxing.c -I /usr/local/ffmpeg/include -L /usr/local/ffmpeg/lib -g -o muxing.out  -L /usr/local/lib -lavformat -lavcodec -lavutil -lm -lswresample -lswscale
gcc muxing.c -I /usr/local/ffmpeg/include -L /usr/local/ffmpeg/lib -g -o muxing.out  -L /usr/local/lib -lavformat -lavcodec -lavutil -lm -lswresample -lswscale
#运行可执行程序
rm ./test.mp4
./muxing.out ./test.mp4

#gcc record.c  -o simplest_ffmpeg_demuxer.out -lavcodec -lavformat -lavutil -lavfliter -lswresample -lswscale  -lavformat
