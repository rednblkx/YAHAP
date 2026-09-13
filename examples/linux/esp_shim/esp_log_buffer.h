// Host shim for esp_log_buffer.h — hex-dump logging is dropped on host.
#pragma once

#define ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, len, level) ((void)0)
