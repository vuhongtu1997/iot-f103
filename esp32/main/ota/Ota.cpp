#include "Ota.h"
#include "Log.h"
#include "ErrorCode.h"
#include <string.h>

#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUFFSIZE 1024

static esp_ota_handle_t update_handle = 0;
static const esp_partition_t *update_partition = NULL;

static int fwSize = 0;
static string fwChecksumAlgorithm;
static string fwChecksum;

bool Ota::CheckNeedUpdate(string fwVersion)
{
	LOGD("ota_need_update");
	esp_app_desc_t running_app_info;
	const esp_partition_t *running = esp_ota_get_running_partition();
	if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
	{
		LOGI("update version: %s, running version: %s", fwVersion.c_str(), running_app_info.version);
		if (fwVersion != string(running_app_info.version))
		{
			return true;
		}
	}
	return false;
}

int Ota::Update(string fwUrl, string fwChecksumAlgorithm, string fwChecksum)
{
    if (fwUrl.empty())
    {
        LOGE("OTA URL is empty");
        return CODE_ERROR;
    }

    esp_http_client_config_t config = {};
    config.url = fwUrl.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 30000;
    config.keep_alive_enable = true;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    esp_https_ota_handle_t https_ota_handle = NULL;

    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK)
    {
        LOGE("esp_https_ota_begin failed (%s)", esp_err_to_name(err));
        return CODE_ERROR;
    }

    int last_percent = -1;

    while (true)
    {
        err = esp_https_ota_perform(https_ota_handle);

        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            break;
        }

        int image_len = esp_https_ota_get_image_size(https_ota_handle);
        int read_len  = esp_https_ota_get_image_len_read(https_ota_handle);

        if (image_len > 0)
        {
            int percent =
                (int)(((int64_t)read_len * 100) / image_len);

            if (percent != last_percent)
            {
                last_percent = percent;
                LOGI("OTA Progress: %d%% (%d/%d)", percent, read_len, image_len);
            }
        }
    }

    if (err != ESP_OK)
    {
        LOGE("esp_https_ota_perform failed (%s)", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
        return CODE_OTA_ERROR;
    }

    if (!esp_https_ota_is_complete_data_received(https_ota_handle))
    {
        LOGE("Complete firmware not received");
        esp_https_ota_abort(https_ota_handle);
        return CODE_OTA_DATA_ERROR;
    }

    err = esp_https_ota_finish(https_ota_handle);

    if (err != ESP_OK)
    {
        LOGE("esp_https_ota_finish failed (%s)", esp_err_to_name(err));
        return CODE_OTA_ERROR;
    }

    LOGI("OTA Success");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return CODE_OK;
}

int Ota::Update(int _fwSize, string _fwChecksumAlgorithm, string _fwChecksum)
{
	fwSize = _fwSize;
	fwChecksumAlgorithm = _fwChecksumAlgorithm;
	fwChecksum = _fwChecksum;
	return CODE_OK;
}

int Ota::UpdateChunk(int chunkId, uint8_t *data, int dataLen)
{
	// LOGW("UpdateChunk size: %d", sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t));
	esp_err_t err;
	if (chunkId == 0)
	{
		esp_app_desc_t new_app_info;
		if (dataLen > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
		{
			// check current version with downloading
			memcpy(&new_app_info, &data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
			LOGI("New firmware version: %s", new_app_info.version);

			esp_app_desc_t running_app_info;
			const esp_partition_t *running = esp_ota_get_running_partition();
			if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
			{
				LOGI("Running firmware version: %s", running_app_info.version);
			}

			update_partition = esp_ota_get_next_update_partition(NULL);
			assert(update_partition != NULL);
			LOGI("Writing to partition subtype %d at offset 0x%" PRIx32,
					 update_partition->subtype, update_partition->address);

			// image_header_was_checked = true;

			err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
			if (err != ESP_OK)
			{
				LOGE("esp_ota_begin failed (%s)", esp_err_to_name(err));
				esp_ota_abort(update_handle);
				return CODE_OTA_ERROR;
			}
			LOGI("esp_ota_begin succeeded");
		}
		else
		{
			LOGE("received package is not fit len");
			return CODE_OTA_DATA_ERROR;
		}
	}

	err = esp_ota_write(update_handle, (const void *)data, dataLen);
	if (err != ESP_OK)
	{
		esp_ota_abort(update_handle);
		return CODE_OTA_ERROR;
	}
	return CODE_OK;
}

int Ota::UpdateFinish()
{
	LOGI("UpdateFinish");
	esp_err_t err = esp_ota_end(update_handle);
	if (err != ESP_OK)
	{
		if (err == ESP_ERR_OTA_VALIDATE_FAILED)
		{
			LOGE("Image validation failed, image is corrupted");
			return CODE_OTA_DATA_ERROR;
		}
		else
		{
			LOGE("esp_ota_end failed (%s)!", esp_err_to_name(err));
		}
		return CODE_OTA_ERROR;
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK)
	{
		LOGE("esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
		return CODE_OTA_ERROR;
	}
	return CODE_OK;
}
