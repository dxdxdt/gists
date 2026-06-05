.PHONY: benchmark showcase

W = 640
H = 360

benchmark: iterative-div
	./iterative-div -t5    < /dev/urandom > /dev/null || echo $$?
	./iterative-div -t5 -c < /dev/urandom > /dev/null || echo $$?
	./iterative-div -t5    < /dev/zero >    /dev/null || echo $$?
	./iterative-div -t5 -c < /dev/zero >    /dev/null || echo $$?

iterative-div: iterative-div.c libdivide.h
	cc -mtune=native -march=native -g -O2 -Wall -Wextra \
		-o iterative-div iterative-div.c

libdivide.h:
	curl -sSLO 'https://raw.githubusercontent.com/ridiculousfish/libdivide/master/libdivide.h'

clean:
	rm -f iterative-div libdivide.h vid_$(W)x$(H).webm *.tmp *.fifo

vid_$(W)x$(H).webm:
	curl -SLR -o vid_$(W)x$(H).webm.tmp \
		"https://www.elecard.com/storage/video/UshaikaRiverEmb_$(W)x$(H).webm"
	mv vid_$(W)x$(H).webm.tmp vid_$(W)x$(H).webm

a.fifo:
	mkfifo a.fifo
b.fifo:
	mkfifo b.fifo

showcase: iterative-div vid_$(W)x$(H).webm a.fifo b.fifo
# source
	ffmpeg -stream_loop -1 -re -i vid_$(W)x$(H).webm -an -f rawvideo -vcodec rawvideo \
		-pix_fmt rgb24 pipe: | \
			tee a.fifo > b.fifo &
# original stream for comparison
	ffplay -vcodec rawvideo -pixel_format rgb24 -video_size ${W}x${H} -f rawvideo \
		-vf "setpts=0" pipe: < a.fifo &
# avg frame stream
	./iterative-div -s $$(($(W) * $(H) * 3)) < b.fifo | \
		ffplay -vcodec rawvideo -pixel_format rgb24 -video_size ${W}x${H} -f rawvideo \
			-vf "setpts=0" pipe: &
	wait; wait; wait; true
