#include <rtc_writer.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

static int rtc_write_stdout(rtc_handle* h, void const* buf, size_t len, int flags) {
	(void)h;
	(void)flags;
	fwrite(buf, len, 1, stdout);
	return 0;
}

#ifndef STRINGIFY
#  define STRINGIFY_(x) #x
#  define STRINGIFY(x) STRINGIFY_(x)
#endif

#define check_res(call) { errno = (call); if(errno) { perror(STRINGIFY(call)); exit(1); } }

int main() {
	rtc_param param;
	rtc_handle h;
	rtc_stream_param sp = {"\"bla\"", RTC_STREAM_VARIABLE_LENGTH, "\"bla\": true", false};
	rtc_stream_param sp2 = {"\"bla2\"", RTC_STREAM_VARIABLE_LENGTH, "\"bla\": true", false};
	rtc_stream s;
	rtc_stream s2;

	rtc_param_default(&param);
	param.unit = 64;
	param.write = rtc_write_stdout;

	check_res(rtc_start(&h, &param));
#if 0
	check_res(rtc_meta(&h, rtc_write_stdout, true));
#endif

	check_res(rtc_create(&h, &s, &sp));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 2, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));
	check_res(rtc_write(&s, "asdf", 4, false));

	check_res(rtc_create(&h, &s2, &sp2));
	check_res(rtc_write(&s2, "zzzz", 4, false));

	check_res(rtc_write(&s, "asdf", 4, false));

	check_res(rtc_stop(&h));

	return 0;
}

