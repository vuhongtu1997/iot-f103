#include "Sntp.h"

extern "C" void initialize_sntp(bool wait_network);
extern "C" bool have_ntp_time(void);

void Sntp::init(bool waitNetwork)
{
	initialize_sntp(waitNetwork);
}

bool Sntp::haveNtpTime(void)
{
	return have_ntp_time();
}

bool Sntp::haveTime(void)
{
	return have_ntp_time();
}
