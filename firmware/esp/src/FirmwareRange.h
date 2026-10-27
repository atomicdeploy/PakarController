#pragma once
#include <Arduino.h>
// Imported PU850 parser, isolated so native tests exercise the production code.
namespace FirmwareTransferRange {
struct FirmwareRange {
	uint32_t start;
	uint32_t end;
	bool partial;
};

inline bool parseDecimal(const String& value, uint32_t& result) {
	if (value.isEmpty()) return false;

	uint64_t parsed = 0;
	for (size_t index = 0; index < value.length(); ++index) {
		const char character = value[index];
		if (character < '0' || character > '9') return false;
		parsed = parsed * 10 + static_cast<uint8_t>(character - '0');
		if (parsed > UINT32_MAX) return false;
	}

	result = static_cast<uint32_t>(parsed);
	return true;
}

inline bool parseRange(const String& headerValue, uint32_t totalSize, FirmwareRange& range) {
	if (totalSize == 0) return false;

	String value = headerValue;
	value.trim();
	if (!value.startsWith("bytes=") || value.indexOf(',') >= 0) return false;

	value.remove(0, 6);
	const int separator = value.indexOf('-');
	if (separator < 0 || value.indexOf('-', separator + 1) >= 0) return false;

	const String first = value.substring(0, separator);
	const String last = value.substring(separator + 1);
	if (first.isEmpty() && last.isEmpty()) return false;

	if (first.isEmpty()) {
		uint32_t suffixLength = 0;
		if (!parseDecimal(last, suffixLength) || suffixLength == 0) return false;
		range.start = suffixLength >= totalSize ? 0 : totalSize - suffixLength;
		range.end = totalSize - 1;
		range.partial = true;
		return true;
	}

	if (!parseDecimal(first, range.start) || range.start >= totalSize) return false;

	if (last.isEmpty()) {
		range.end = totalSize - 1;
	} else {
		if (!parseDecimal(last, range.end) || range.start > range.end) return false;
		if (range.end >= totalSize) range.end = totalSize - 1;
	}

	range.partial = true;
	return true;
}
}
