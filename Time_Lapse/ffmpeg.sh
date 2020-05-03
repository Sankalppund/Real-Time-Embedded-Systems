# !/bin/bash
ffmpeg -start_number 0 -i Image_No_%4d.ppm -vframes 2000 -vcodec mpeg4 video.mp4


