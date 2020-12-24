#include <rtc_writer.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#ifndef STRINGIFY
#  define STRINGIFY_(x) #x
#  define STRINGIFY(x) STRINGIFY_(x)
#endif

#define check_res(call) { errno = (call); if(errno) { perror(STRINGIFY(call)); exit(1); } }

static int write_file(rtc_handle* h, void const* buf, size_t len, int flags) {
	(void)flags;
	FILE* f = (FILE*)rtc_arg(h);
	return !f || fwrite(buf, len, 1, f) == 0 ? EIO : 0;
}

static int write_timestamp(rtc_stream* s) {
	struct timespec ts;
	if(!timespec_get(&ts, TIME_UTC))
		return EIO;

	int res = 0;
	uint64_t sec = (uint64_t)ts.tv_sec;
	if((res = rtc_write(s, &sec, sizeof(sec), true)))
		return res;

	uint32_t ns = (uint32_t)ts.tv_nsec;
	if((res = rtc_write(s, &ns, sizeof(ns))))
		return res;

	return 0;
}

int main() {
	FILE* f = fopen("inputs.rtc", "ab");
	if(!f) {
		perror("Cannot open rtc file.");
		exit(1);
	}

	rtc_param p;
	rtc_param_default(&p);
	p.write = &write_file;
	p.arg = (void*)f;

	rtc_handle h;
	check_res(rtc_start(&h, &p));

	rtc_stream_param param_clk = {"clk", RTC_STREAM_VARIABLE_LENGTH, "name:\"clk\",clock:true,content:\"timespec\""};
	rtc_stream stream_clk;
	check_res(rtc_create(&h, &stream_clk, &param_clk));
	check_res(write_timestamp(&stream_clk));

	rtc_stream_param param_stdin = {"stdin", RTC_STREAM_VARIABLE_LENGTH, "name:\"stdin\",cont:true,content:\"utf-8\""};
	rtc_stream stream_stdin;
	check_res(rtc_create(&h, &stream_stdin, &param_stdin));

	printf("Any input via stdin is passed to the RTC file.\n");
	printf("Press Ctrl+D to terminate.\n");

	char buffer[0x1000];
	size_t len;
	while((len = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		check_res(write_timestamp(&stream_clk));
		check_res(rtc_write(&stream_stdin, buffer, len));
	}

	check_res(rtc_stop(&h));
	fclose(f);
	return 0;
}

