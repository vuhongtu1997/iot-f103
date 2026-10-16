#include "TelemetryV2.h"
#include <cstring>

namespace
{
std::string Timestamp(time_t value)
{
  struct tm utc{};
  char text[32]{};
  if (!gmtime_r(&value, &utc) ||
      !strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", &utc))
    return "";
  return text;
}
}

void TelemetryV2::Add(const ModbusNodeInfo &node, const uint16_t *r, time_t at)
{
  if (!r || node.sensorUid.empty() || at < 1704067200 ||
      memcmp(node.uid, "C3NODE", 6) != 0 || r[4] != 0 ||
      r[DIGITAL_SCHEMA_REGISTER] != DIGITAL_SCHEMA_V1)
    return;
  const uint16_t valid = r[DIGITAL_VALID_REGISTER];
  if (valid & (SENSOR_PROTOCOL_UNAVAILABLE | SENSOR_WARMING_UP))
    return;
  Json::Value metrics(Json::objectValue);
  if (node.sensorType == MODBUS_SENSOR_GAS)
  {
    if (valid & SENSOR_VALID_NH3) metrics["nh3"] = r[DIGITAL_NH3_REGISTER] / 100.0;
    if (valid & SENSOR_VALID_H2S) metrics["h2s"] = r[DIGITAL_H2S_REGISTER] / 100.0;
  }
  else if (node.sensorType == MODBUS_SENSOR_TEMPERATURE_HUMIDITY)
  {
    if (valid & SENSOR_VALID_TEMPERATURE)
      metrics["temp_c"] = static_cast<int16_t>(r[DIGITAL_TEMPERATURE_REGISTER]) / 10.0;
    if (valid & SENSOR_VALID_HUMIDITY)
      metrics["humidity_pct"] = r[DIGITAL_HUMIDITY_REGISTER] / 10.0;
  }
  else if (node.sensorType == MODBUS_SENSOR_PRESSURE && (valid & SENSOR_VALID_PRESSURE))
  {
    // Keep the node's Pa unit; the cloud contract does not define a pressure unit.
    metrics["water_pressure"] = static_cast<double>(
        (static_cast<uint32_t>(r[DIGITAL_PRESSURE_REGISTER]) << 16) |
        r[DIGITAL_PRESSURE_REGISTER + 1]);
  }
  if (metrics.empty()) return;
  if (!oldest || at < oldest) oldest = at;
  if (at > newest) newest = at;
  Json::Value sample(Json::objectValue);
  sample["sensor_uid"] = node.sensorUid;
  sample["sampled_at"] = Timestamp(at);
  sample["metrics"] = metrics;
  if (node.sensorType == MODBUS_SENSOR_GAS)
  {
    for (const auto &previous : gas)
      if (previous["sensor_uid"].asString() == node.sensorUid) invalid = true;
    if (gas.size() == 32) invalid = true;
    else gas.append(sample);
  }
  else
  {
    if (!env.empty() && env["sensor_uid"].asString() != node.sensorUid)
    {
      invalid = true; // A batch describes exactly one environment sensor/area.
      return;
    }
    if (env.empty()) env = sample;
    else
    {
      for (const auto &key : metrics.getMemberNames())
      {
        if (env["metrics"].isMember(key)) invalid = true;
        env["metrics"][key] = metrics[key];
      }
      // Conservative timestamp for env assembled from two physical nodes.
      if (sample["sampled_at"].asString() < env["sampled_at"].asString())
        env["sampled_at"] = sample["sampled_at"];
    }
  }
}

bool TelemetryV2::Serialize(const std::string &uid, const std::string &token,
                            time_t now, std::string &payload) const
{
  if (invalid || uid.empty() || uid.size() > 128 || token.empty() ||
      uid.find_first_of("/+#") != std::string::npos ||
      uid.find('\0') != std::string::npos || token.find('\0') != std::string::npos ||
      env.empty() || gas.empty() || now < 1704067200 ||
      oldest < now - 86400 || newest > now + 300)
    return false;
  Json::Value body(Json::objectValue);
  body["schema_version"] = 2;
  body["device_uid"] = uid;
  body["sampled_at"] = Timestamp(now);
  body["ingest_token"] = token;
  body["env"] = env;
  body["gas"] = gas;
  const std::string result = body.toString();
  if (result.size() > 32 * 1024) return false;
  payload = result;
  return true;
}
