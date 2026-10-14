#include "Util.h"
#include "Base64.h"
#include "Log.h"
#include "Sntp.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <locale>
#include <netinet/in.h>
#include <sstream>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef ESP_PLATFORM
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#else
// #include "Sntp.h" // SNTP feature disabled
#endif

using namespace std;

string Util::GenRandRQI(int size)
{
  string rqi = "";
  srand(time(0));
  for (int i = 0; i < size; i++)
  {
    rqi += 'a' + rand() % 26;
  }
  return rqi;
}

string getTimeStrFromTime(time_t t)
{
  struct tm start;
  start = *localtime(&t);
  char timeBuffer[100];
  sprintf(timeBuffer, "%04d%02d%02dT%02d%02d%02d", start.tm_year + 1900,
          start.tm_mon + 1, start.tm_mday, start.tm_hour, start.tm_min,
          start.tm_sec);
  return string(timeBuffer);
}

string Util::GetCurrentTimeStr() { return getTimeStrFromTime(time(NULL)); }

int Util::GetCurrentTimeInDay()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
}

int Util::GetYearsCurrent()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_year + 1900;
}

int Util::GetMonthsCurrent()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_mon + 1;
}

int Util::GetDateCurrent()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_mday;
}

int Util::GetDaysCurrent()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_wday + 1;
}

int Util::GetHoursCurrent()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_hour;
}

int Util::GetMinutesCurrent()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_min;
}

int Util::GetSecondsCurrent()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_sec;
}

uint64_t Util::GetTimeMillis()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec) * (uint64_t)1000 + (tv.tv_usec) / 1000;
}

int Util::GetCurrentWeekDay()
{
  time_t t = time(NULL);
  struct tm lt = *localtime(&t);
  return lt.tm_wday;
}

int Util::ConvertStrTimeToInt(string time)
{
  int hour;
  int minute;
  int second;
  if (sscanf(time.c_str(), "%d:%d:%d", &hour, &minute, &second) == 3)
    return hour * 3600 + minute * 60 + second;
  else if (sscanf(time.c_str(), "%d:%d", &hour, &minute) == 2)
    return hour * 3600 + minute * 60;
  return CODE_ERROR;
}

bool Util::HaveRTC()
{
#if 1 // SNTP feature disabled
#ifdef ESP_PLATFORM
  if (!Sntp::haveTime())
  {
    return false;
  }
#endif
#endif
  return true;
}

string Util::setString(const char *value) { return value ? value : ""; }

string Util::ConvertU32ToHexString(uint8_t *data, int len)
{
  char buff[100];
  if (len > 50)
    len = 50;
  for (int i = 0; i < len; i++)
  {
    sprintf(buff + i * 2, "%02x", data[i]);
  }
  return string(buff);
}

int Util::CheckDayInWeek(int day, int repeater)
{
  int byte;
  switch (day)
  {
  case 0: // sun
    byte = 6;
    break;
  case 1: // mon
  case 2: // tue
  case 3: // wed
  case 4: // thu
  case 5: // fri
  case 6: // sat
    byte = day - 1;
    break;
  default:
    return false;
  }
  if (repeater & (1 << byte))
    return true;
  return false;
}

std::vector<std::string> Util::splitString(const std::string &str, char splitter)
{
    std::vector<std::string> result;
    result.reserve(6); // optimize cho format của bạn

    size_t start = 0;
    size_t end;

    while ((end = str.find(splitter, start)) != std::string::npos)
    {
        result.emplace_back(str.data() + start, end - start);
        start = end + 1;
    }

    result.emplace_back(str.data() + start, str.size() - start);

    return result;
}


bool Util::CompareNumber(string op, int a, int b, int c)
{
  LOGD("CompareNumber, op: %s, a: %d, b: %d, c: %d", op.c_str(), a, b, c);
  if (op == "==")
    return a == b;
  else if (op == "!=")
    return a != b;
  else if (op == ">")
    return a > b;
  else if (op == ">=")
    return a >= b;
  else if (op == "<")
    return a < b;
  else if (op == "<=")
    return a <= b;
  else if (op == "<>")
    return ((a >= b) && (a <= c));
  else if (op == "><")
    return ((a <= b) || (a >= c));
  return false;
}

string Util::ExecuteCMD(string command) { return ExecuteCMD(command.c_str()); }

string Util::ExecuteCMD(char const *command)
{
  string msg_rsp = "";
#ifndef ESP_PLATFORM
  FILE *file;
  char msg_line[100] = {0};
  file = popen(command, "r");
  if (file == NULL)
  {
    LOGE("ExecuteCMD");
    exit(1);
  }
  while (1)
  {
    fgets(msg_line, 100, file);
    msg_rsp += msg_line;
    if (feof(file))
    {
      break;
    }
  }
  pclose(file);
#endif
  return msg_rsp;
}

string Util::uuidToStr(uint8_t *uuid)
{
  char buf[100];
  sprintf(
      buf,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
      uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14],
      uuid[15]);
  buf[36] = '\0';
  return string(buf);
}

static bool ledInternet = false;
static bool ledService = false;
static bool ledBle = false;

void Util::LedInternet(bool value)
{
  ledInternet = value;
#ifdef __OPENWRT__
  if (value)
  {
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:internet/brightness");
  }
  else
  {
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:internet/brightness");
  }
#endif
}

void Util::LedService(bool value)
{
  ledService = value;
#ifdef __OPENWRT__
  if (value)
  {
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:service/brightness");
  }
  else
  {
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:service/brightness");
  }
#endif
}

void Util::LedBle(bool value)
{
  ledBle = value;
#ifdef __OPENWRT__
  if (value)
  {
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:ble/brightness");
  }
  else
  {
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:ble/brightness");
  }
#endif
}

void Util::LedAll(bool value)
{
#ifdef __OPENWRT__
  if (value)
  {
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:internet/brightness");
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:service/brightness");
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:ble/brightness");
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:zigbee/brightness");
    ExecuteCMD("/bin/echo \"1\" > "
               "/sys/class/leds/linkit-smart-7688:orange:thread/brightness");
  }
  else
  {
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:internet/brightness");
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:service/brightness");
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:ble/brightness");
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:zigbee/brightness");
    ExecuteCMD("/bin/echo \"0\" > "
               "/sys/class/leds/linkit-smart-7688:orange:thread/brightness");
  }
#endif
}

void Util::LedRestoreLastValue()
{
  LedInternet(ledInternet);
  LedService(ledService);
  LedBle(ledBle);
}

static int ledServiceCount = 0;
void Util::LedServiceLock()
{
  ledServiceCount++;
  if (ledServiceCount)
    LedService(false);
}

void Util::LedServiceUnlock()
{
  ledServiceCount--;
  if (!ledServiceCount)
    LedService(true);
}

bool Util::GetStatusLedBle() { return ledBle; }

bool Util::GetStatusLedService() { return ledService; }

bool Util::GetStatusLedInternet() { return ledInternet; }

bool Util::compareByID(const Json::Value &obj1, const Json::Value &obj2)
{
  return obj1["ID"].asInt() > obj2["ID"].asInt();
}

Json::Value Util::arrangeJson(Json::Value &property)
{
  if (property.isArray())
  {
    vector<Json::Value> listProperties;
    for (int numProperty = 0; numProperty < property.size(); numProperty++)
    {
      if (property[numProperty].isObject())
      {
        listProperties.push_back(property[numProperty]);
      }
    }
    std::sort(listProperties.begin(), listProperties.end(), Util::compareByID);
    Json::Value result;
    for (int i = 0; i < listProperties.size(); i++)
    {
      result.append(listProperties[i]);
    }
    return result;
  }
  return Json::Value::null;
}

#ifndef ESP_PLATFORM
static void handleErrors(void)
{
  ERR_print_errors_fp(stderr);
  abort();
}

string Util::encryptAes128(string key, string plaintext)
{
  unsigned char *key_c = new unsigned char[key.length() + 1];
  memcpy((char *)key_c, key.c_str(), key.length());
  key_c[key.length()] = '\0';

  unsigned char *plaintext_c = new unsigned char[plaintext.length() + 1];
  memcpy((char *)plaintext_c, plaintext.c_str(), plaintext.length());
  plaintext_c[plaintext.length()] = '\0';

  int plaintext_len = plaintext.length();

  EVP_CIPHER_CTX *ctx;
  unsigned char ciphertext[128] = {0};
  int len;
  int ciphertext_len;

  if (!(ctx = EVP_CIPHER_CTX_new()))
    handleErrors();

  if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key_c, NULL))
  {
    handleErrors();
  }

  if (1 !=
      EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext_c, plaintext_len))
  {
    handleErrors();
  }
  ciphertext_len = len;

  if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
    handleErrors();
  ciphertext_len += len;

  EVP_CIPHER_CTX_free(ctx);

  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (int i = 0; i < ciphertext_len; ++i)
  {
    ss << std::setw(2) << static_cast<int>(ciphertext[i]);
  }

  delete[] key_c;
  delete[] plaintext_c;

  return ss.str();
}

string Util::calculateSHA256Checksum(string &filePath)
{
  std::ifstream file(filePath, std::ios::binary);
  if (!file)
  {
    throw std::runtime_error("Failed to open file.");
  }

  SHA256_CTX sha256Context;
  SHA256_Init(&sha256Context);

  char buffer[1024];
  while (!file.eof())
  {
    file.read(buffer, sizeof(buffer));
    SHA256_Update(&sha256Context, buffer, file.gcount());
  }

  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_Final(hash, &sha256Context);

  std::stringstream checksum;
  checksum << std::hex << std::setfill('0');
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
  {
    checksum << std::setw(2) << static_cast<int>(hash[i]);
  }
  return checksum.str();
}
#endif
