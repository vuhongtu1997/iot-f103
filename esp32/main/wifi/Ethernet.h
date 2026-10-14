#pragma once

#include <string>

namespace Ethernet
{
bool Start();
void Stop();
bool IsStarted();
bool WaitForIp(unsigned int timeoutMs);
bool IsConnected();
std::string GetIP();
}
