#include "FirmwareTransfer.h"
#include "FirmwareRange.h"
#include "FirmwarePlatform.h"
#include "FirmwareReadResponse.h"

#include <BuildInfo.h>
#include <functional>

// Imported from atomicdeploy/pu850-esp LocalLib/FirmwareTransfer.cpp.
// Identity/auth adapted; full-flash dumps stay disabled to protect stored secrets.
#define FW_PRODUCT_ID "PakarController"
#if defined(ARDUINO_ARCH_ESP32)
#define FW_PRODUCT_MODEL_NAME "ESP32-S3 + ATmega328P"
#else
#define FW_PRODUCT_MODEL_NAME "ESP8266 + ATmega328P"
#endif
#define FW_ENABLE_FIRMWARE_DOWNLOAD 1
#define FW_ENABLE_FULL_FLASH_DOWNLOAD 0
static std::function<bool(AsyncWebServerRequest*)> transferAuthorization;

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <mbedtls/base64.h>
#else
#include <ESP8266WiFi.h>
#include <libb64/cencode.h>
#endif

namespace {

using namespace FirmwareTransferRange;

FirmwareExport::ReadResult readFlashRange(uint32_t address, uint8_t* output, size_t length, uint32_t totalSize) {
	using FirmwareExport::ReadResult;
	if (output == nullptr || length == 0) return ReadResult::Fatal;
	if (static_cast<uint64_t>(address)+length>totalSize) return ReadResult::Fatal;
#if defined(ARDUINO_ARCH_ESP32)
	// This route permits app-image downloads only. Offset zero is relative to
	// the running OTA partition, NOT physical flash and its boot/settings area.
	// totalSize was verified once when constructing this response. On S3 the
	// SDK getSketchSize() hashes/verifies the full image, so never call it for
	// every TCP chunk from the AsyncTCP task.
	const auto *running=esp_ota_get_running_partition();
	if(!running || static_cast<uint64_t>(address)+length>running->size) return ReadResult::Fatal;
	// Preserve the guard's transient ESP_ERR_TIMEOUT rather than collapsing it
	// into the AsyncWebServer end-of-file value. Invalid offsets never retry.
	const esp_err_t error=esp_partition_read(running,address,output,length);
	if(error==ESP_OK) return ReadResult::Complete;
	return error==ESP_ERR_TIMEOUT ? ReadResult::Retryable : ReadResult::Fatal;
#else
	const uint32_t flashSize = ESP.getFlashChipSize();
	const uint64_t requestedEnd = static_cast<uint64_t>(address) + length;
	if (requestedEnd > flashSize) return ReadResult::Fatal;

	const uint32_t alignedAddress = address & ~static_cast<uint32_t>(0x03);
	const size_t prefix = address - alignedAddress;
	const size_t alignedLength = (prefix + length + 3) & ~static_cast<size_t>(0x03);
	if (static_cast<uint64_t>(alignedAddress) + alignedLength > flashSize) return ReadResult::Fatal;

	if (prefix == 0 &&
		(length & 0x03) == 0 &&
		(reinterpret_cast<uintptr_t>(output) & 0x03) == 0) {
		return ESP.flashRead(alignedAddress, output, length) ? ReadResult::Complete : ReadResult::Retryable;
	}

	uint8_t* alignedBuffer = static_cast<uint8_t*>(malloc(alignedLength));
	if (alignedBuffer == nullptr) return ReadResult::Retryable;

	const bool read = ESP.flashRead(alignedAddress, alignedBuffer, alignedLength);
	if (read) memcpy(output, alignedBuffer + prefix, length);
	free(alignedBuffer);
	return read ? ReadResult::Complete : ReadResult::Retryable;
#endif
}

int hexNibble(char value) {
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

String sketchMd5Base64(const String& hexadecimal) {
	if (hexadecimal.length() != 32) return String();

	char bytes[16];
	for (size_t index = 0; index < sizeof(bytes); ++index) {
		const int high = hexNibble(hexadecimal[index * 2]);
		const int low = hexNibble(hexadecimal[index * 2 + 1]);
		if (high < 0 || low < 0) return String();
		bytes[index] = static_cast<char>((high << 4) | low);
	}

	char encoded[32] = {};
#if defined(ARDUINO_ARCH_ESP32)
	size_t length=0;
	if(mbedtls_base64_encode(reinterpret_cast<unsigned char *>(encoded),sizeof(encoded),&length,
	    reinterpret_cast<const unsigned char *>(bytes),sizeof(bytes))!=0) return String();
#else
	base64_encodestate state;
	base64_init_encodestate(&state);
	int length = base64_encode_block(bytes, sizeof(bytes), encoded, &state);
	length += base64_encode_blockend(encoded + length, &state);
#endif
	while (length > 0 && (encoded[length - 1] == '\r' || encoded[length - 1] == '\n')) {
		--length;
	}
	encoded[length] = '\0';
	return String(encoded);
}

void sendTransferError(AsyncHttpService* server, AsyncWebServerRequest* request, int status, const char* message) {
	server->send(
		status,
		"text/plain",
		request->method() == HTTP_HEAD ? "" : message);
}

bool wantsFullFlash(AsyncWebServerRequest* request) {
	if (!request->hasArg("full")) return false;
	String value = request->arg("full");
	value.toLowerCase();
	return value == "1" || value == "true" || value == "yes";
}

void addIdentityHeaders(
	AsyncWebServerResponse* response,
	const String& sketchMd5,
	uint32_t totalSize,
	bool fullFlash,
	bool partial) {
	response->addHeader("Accept-Ranges", "bytes");
	response->addHeader("Cache-Control", "no-store");
	response->addHeader("X-Firmware-Mode", fullFlash ? "full" : "sketch");
	response->addHeader("X-Firmware-Size", String(totalSize));
	response->addHeader("X-Product-ID", FW_PRODUCT_ID);
	response->addHeader("X-Product-Model", FW_PRODUCT_MODEL_NAME);
	response->addHeader("X-Firmware-Build", BoardCore::BuildInfo::kVersion);

	if (fullFlash) return;

	response->addHeader("X-Firmware-MD5", sketchMd5);
	response->addHeader("ETag", String("\"") + sketchMd5 + "\"");

	if (!partial) {
		const String encodedMd5 = sketchMd5Base64(sketchMd5);
		if (!encodedMd5.isEmpty()) {
			response->addHeader("Content-MD5", encodedMd5);
			response->addHeader("Digest", "md5=" + encodedMd5);
		}
	}
}

} // namespace

bool FirmwareTransferHasAuthorization(AsyncWebServerRequest* request) {
	return transferAuthorization && transferAuthorization(request);
}

bool FirmwareTransferAuthorize(AsyncWebServerRequest* request) {
	if (FirmwareTransferHasAuthorization(request)) return true;

	AsyncWebServerResponse* response = request->beginResponse(
		401,
		"text/plain",
		request->method() == HTTP_HEAD ? "" : "Unauthorized");
	response->addHeader("X-Login-Path", "/login");
	response->addHeader("Cache-Control", "no-store");
	request->send(response);
	return false;
}

void RegisterFirmwareTransferRoutes(AsyncHttpService* server,
    std::function<bool(AsyncWebServerRequest*)> authorize,
    std::function<FirmwareTransferIdentity()> identityProvider,
    std::function<std::shared_ptr<void>()> acquireReader) {
	transferAuthorization = authorize;
#if FW_ENABLE_FIRMWARE_DOWNLOAD
	server->on(
		"/firmware/download",
		HTTP_HEAD | HTTP_GET,
		[server,identityProvider,acquireReader]() {
			auto *request=server->request();
			if (!request) return;
			if (!FirmwareTransferHasAuthorization(request)) {
				auto *response=request->beginResponse(401,"text/plain",request->method()==HTTP_HEAD ? "" : "Unauthorized");
				response->addHeader("X-Login-Path","/login");
				response->addHeader("Cache-Control","no-store");
				server->send(response);return;
			}

			const bool fullFlash = wantsFullFlash(request);
#if !FW_ENABLE_FULL_FLASH_DOWNLOAD
			if (fullFlash) {
				sendTransferError(server, request, 403, "Full-flash download is disabled by this build");
				return;
			}
#endif

			const FirmwareTransferIdentity identity=identityProvider ? identityProvider() :
				FirmwareTransferIdentity{FirmwarePlatform::sketchSize(),FirmwarePlatform::sketchMd5(),FirmwarePlatform::runningIntegrity()};
			const uint32_t totalSize=fullFlash ? ESP.getFlashChipSize() : identity.size;
			if (totalSize == 0 || !identity.integrityVerified || sketchMd5Base64(identity.md5).isEmpty()) {
				sendTransferError(server, request, 503, "Verified firmware identity is unavailable");
				return;
			}

			FirmwareRange range = {0, totalSize - 1, false};
			if (request->hasHeader("Range")) {
				const AsyncWebHeader* header = request->getHeader("Range");
				if (header == nullptr || !parseRange(header->value(), totalSize, range)) {
					AsyncWebServerResponse* response = request->beginResponse(
						416,
						"text/plain",
						request->method() == HTTP_HEAD ? "" : "Invalid byte range");
					response->addHeader("Content-Range", "bytes */" + String(totalSize));
					response->addHeader("Cache-Control", "no-store");
					server->send(response);
					return;
				}
			}

			const uint32_t contentLength = range.end - range.start + 1;
			const String sketchMd5 = identity.md5;
			const String etag = String("\"") + sketchMd5 + "\"";
			if (!fullFlash &&
				!range.partial &&
				request->hasHeader("If-None-Match") &&
				request->getHeader("If-None-Match")->value() == etag) {
				AsyncWebServerResponse* response = request->beginResponse(304);
				addIdentityHeaders(response, sketchMd5, totalSize, false, false);
				server->send(response);
				return;
			}

			AsyncWebServerResponse* response = nullptr;
			if (request->method() == HTTP_HEAD) {
				response = request->beginResponse(
					range.partial ? 206 : 200,
					"application/octet-stream",
					"");
				response->setContentLength(contentLength);
			} else {
				auto lease=acquireReader ? acquireReader() : std::shared_ptr<void>();
				if(!lease) { sendTransferError(server,request,503,"Firmware reader is unavailable during maintenance");return; }
				response = new FirmwareExport::ReadResponse(range.start,contentLength,
					[totalSize](uint32_t address,uint8_t *output,size_t length) {
						return readFlashRange(address,output,length,totalSize);
					},std::move(lease));
				if (range.partial) response->setCode(206);
			}

			const String filename =
				fullFlash
					? String(FW_PRODUCT_ID) + "-flash.bin"
					: String(FW_PRODUCT_ID) + "-firmware.bin";
			response->addHeader(
				"Content-Disposition",
				"attachment; filename=\"" + filename + "\"");
			if (range.partial) {
				response->addHeader(
					"Content-Range",
					"bytes " + String(range.start) + "-" + String(range.end) +
						"/" + String(totalSize));
			}
			addIdentityHeaders(
				response,
				sketchMd5,
				totalSize,
				fullFlash,
				range.partial);
			server->send(response);
		});
#else
	(void)server;
#endif
}
