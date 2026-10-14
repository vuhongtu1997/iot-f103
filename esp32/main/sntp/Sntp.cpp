#include "Sntp.h"

extern "C" void initialize_sntp(bool wait_network);
extern "C" bool have_ntp_time(void);
extern "C" bool ds1307_have_time(void);

void Sntp::init(bool waitNetwork)
{
	initialize_sntp(waitNetwork);
}

bool Sntp::haveNtpTime(void)
{
	return have_ntp_time();
}

bool Sntp::haveRtcTime(void)
{
	return ds1307_have_time();
}

bool Sntp::haveTime(void)
{
	return have_ntp_time() || ds1307_have_time();
}
