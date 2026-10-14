#pragma once

#include "Device.h"
#include "module/Module.h"

#define BLE_TIMES_CHECK_ISOLATE 180
#define BLE_PERIOD_ISOLATE 180
using namespace std;

class DeviceBle : public Device {
private:
  string deviceKey;
  uint16_t timesUpdateInPeriod;
  time_t lastTimeCheckIsolate;

protected:
  int countElement;
  vector<Module *> modules;

public:
  using Device::InputData;
  DeviceBle(string id, string name, string mac, string type, uint16_t addr,
            uint16_t version, Json::Value *dataJson);
  ~DeviceBle();

  string GetDeviceKey(Json::Value *dataJson);
  string GetDeviceKey();

  virtual bool CheckAddr(uint16_t addr);

  void Isolate();

  virtual int BuildTelemetryValue(Json::Value &pushDataValue);

  // virtual void Getstatus(Json::Value &jsonValue);

  virtual void InputData(Json::Value &dataValue, bool isPushTelemety = true);
  virtual void InputData(uint8_t *data, int len, uint16_t addr = 0);
  virtual bool HandleBleBroadcastStatus(uint8_t *data, int len,
                                        uint16_t resolvedAddr);
  virtual bool HandleBleStatus(uint16_t opcode, uint8_t *data, int len,
                               uint16_t resolvedAddr);
  virtual bool CheckData(Json::Value &dataValue, bool &rs);
  int GetNumElement();

  // send RF
  int AddToGroup(uint16_t groupAddr, uint16_t epId);
  int RemoveFromGroup(uint16_t groupAddr, uint16_t epId);
  int AddToScene(uint16_t sceneAddr, Json::Value &dataValue, uint16_t groupAddr = 0);
  int RemoveFromScene(uint16_t sceneAddr, Json::Value &dataValue, uint16_t groupAddr = 0);
  int EditInScene(uint16_t sceneAddr, Json::Value &dataValue);
  int AddToRoom(uint16_t roomAddr);
  int RemoveFromRoom(uint16_t roomAddr);
  int AddToGroupInRoom(uint16_t groupAddr, uint32_t groupType);
  int AddToSceneInRoom(uint16_t sceneAddr, Json::Value &dataValue, uint16_t roomAddr = 0);
  int RemoveFromGroupInRoom(uint16_t groupAddr);
  int RemoveFromSceneInRoom(uint16_t sceneAddr, Json::Value &dataValue, uint16_t roomAddr = 0);
  virtual int Do(Json::Value &dataValue);
  virtual int InitAttribute(string attribute, double value);
};
