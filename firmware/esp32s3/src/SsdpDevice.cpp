// Adapted from atomicdeploy/pu850-esp LocalLib/SSDPDevice (actual source),
// retaining queued/de-duplicated M-SEARCH, alive/update/byebye and escaped XML.
// ESP8266-only UDP/IGMP lifecycle is replaced by an interface-bound S3 adapter.
#include "SsdpDevice.h"
#include "DiscoveryInterfaces.h"
#include <firmware/esp/src/UpnpDescription.h>
#include <firmware/esp/src/BoardIconAsset.h>

#include <new>
namespace PakarS3 {

SsdpDevice::SsdpDevice() :
	m_server(0),
	m_port(SSDP_HTTP_PORT),
	m_ttl(SSDP_MULTICAST_TTL),
	m_last(0, 0, 0, 0),
	m_stationJoined(0, 0, 0, 0),
	m_accessPointJoined(0, 0, 0, 0)
{
	m_uuid[0] = '\0';
	m_modelNumber[0] = '\0';
	strlcpy(m_deviceType, FW_UPNP_DEVICE_TYPE, sizeof(m_deviceType));
	m_friendlyName[0] = '\0';
	m_presentationURL[0] = '\0';
	m_serialNumber[0] = '\0';
	m_modelName[0] = '\0';
	m_modelURL[0] = '\0';
	m_manufacturer[0] = '\0';
	m_manufacturerURL[0] = '\0';
	strlcpy(m_schemaURL, FW_UPNP_SCHEMA_PATH, sizeof(m_schemaURL));

	uint32_t chipId = static_cast<uint32_t>(ESP.getEfuseMac());

	sprintf_P(m_uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
		(uint16_t) ((chipId >> 16) & 0xff),
		(uint16_t) ((chipId >>  8) & 0xff),
		(uint16_t)   chipId        & 0xff);

	refreshConfigId();

	clearQueue();
}

SsdpDevice::~SsdpDevice() {
	end();
}

bool SsdpDevice::begin() {
	end();
	refreshConfigId();

	m_bootId = ESP.getCycleCount() & 0x7fffffffUL;
	if (m_bootId == 0) m_bootId = 1;
	clearQueue();

	return true;
}

void SsdpDevice::end() {
	if (m_server) {
		sendAllByebye(m_stationJoined);
		if (m_accessPointJoined != m_stationJoined) {
			sendAllByebye(m_accessPointJoined);
		}
		if (m_ethernetJoined != m_stationJoined && m_ethernetJoined != m_accessPointJoined)
			sendAllByebye(m_ethernetJoined);

		m_server->stop();
		leaveJoinedInterfaces();
		delete m_server;
		m_server = nullptr;
	}

	m_last = IPAddress(0, 0, 0, 0);
	m_stationJoined = IPAddress(0, 0, 0, 0);
	m_accessPointJoined = IPAddress(0, 0, 0, 0);
	m_ethernetJoined = IPAddress();
	m_stationSeen = m_accessPointSeen = m_ethernetSeen = IPAddress();
	m_joinRetryAt = 0;

	clearQueue();
}

void SsdpDevice::update() {
	if (!m_server || m_last == IPAddress(0, 0, 0, 0)) return;
	refreshConfigId();

	// Collapse overlapping configuration-change requests into the update that
	// is already queued so BOOTID/NEXTBOOTID remain a contiguous pair.
	for (uint8_t index = 0; index < SSDP_QUEUE_SIZE; ++index) {
		if (m_queue[index].time != 0 && m_queue[index].type == NOTIFY_UPDATE) {
			return;
		}
	}

	const uint32_t nextBootId = m_bootId == 0x7fffffffUL ? 1UL : m_bootId + 1UL;
	postNotifyUpdate(m_stationJoined);
	if (m_accessPointJoined != m_stationJoined) {
		postNotifyUpdate(m_accessPointJoined);
	}
	if (m_ethernetJoined != m_stationJoined && m_ethernetJoined != m_accessPointJoined)
		postNotifyUpdate(m_ethernetJoined);

	// postNotifyUpdate captures the old and next IDs in each queue entry.
	m_bootId = nextBootId;
}

bool SsdpDevice::readLine(String &value) {
	char buffer[65];
	int bufferPos = 0;

	while (1) {
		int c = m_server->read();

		if (c < 0) {
			buffer[bufferPos] = '\0';

			break;
		}
		if (c == '\r' && m_server->peek() == '\n') {
			m_server->read();

			buffer[bufferPos] = '\0';

			break;
		}
		if (bufferPos < 64) {
			buffer[bufferPos++] = c;
		}
	}

	value = String(buffer);

	return bufferPos > 0;
}

bool SsdpDevice::readKeyValue(String &key, String &value) {
	char buffer[65];
	int bufferPos = 0;

	while (1) {
		int c = m_server->read();

		if (c < 0) {
			if (bufferPos == 0) return false;

			buffer[bufferPos] = '\0';

			break;
		}
		if (c == ':') {
			buffer[bufferPos] = '\0';

			while (m_server->peek() == ' ') m_server->read();

			break;
		}
		else if (c == '\r' && m_server->peek() == '\n') {
			m_server->read();

			if (bufferPos == 0) return false;

			buffer[bufferPos] = '\0';

			key = String();
			value = String(buffer);

			return true;
		}
		if (bufferPos < 64) {
			buffer[bufferPos++] = c;
		}
	}

	key = String(buffer);

	readLine(value);

	return true;
}

bool SsdpDevice::serviceEnabled() const {
	return FW_UPNP_SERVICE_TYPE[0] != '\0';
}

void SsdpDevice::postNotifyALive(IPAddress interfaceAddress) {
	if (interfaceAddress == IPAddress(0, 0, 0, 0)) return;

	unsigned long time = millis();

	post(NOTIFY_ALIVE_INIT, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 10);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 30);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 50);
	if (serviceEnabled()) {
		post(NOTIFY_ALIVE_INIT, SERVICE_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 70);
	}

	post(NOTIFY_ALIVE_INIT, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 210);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 230);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 250);
	if (serviceEnabled()) {
		post(NOTIFY_ALIVE_INIT, SERVICE_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 270);
	}

	post(NOTIFY_ALIVE, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 610);
	post(NOTIFY_ALIVE, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 630);
	post(NOTIFY_ALIVE, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 650);
	if (serviceEnabled()) {
		post(NOTIFY_ALIVE, SERVICE_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 670);
	}
}

void SsdpDevice::postNotifyUpdate(IPAddress interfaceAddress) {
	if (interfaceAddress == IPAddress(0, 0, 0, 0)) return;

	unsigned long time = millis();

	post(NOTIFY_UPDATE, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 10);
	post(NOTIFY_UPDATE, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 30);
	post(NOTIFY_UPDATE, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 50);
	if (serviceEnabled()) {
		post(NOTIFY_UPDATE, SERVICE_BY_TYPE, SSDP_MULTICAST_ADDR, interfaceAddress, SSDP_PORT, time + 70);
	}
}

void SsdpDevice::postResponse(long mx) {
	unsigned long time = millis();
	const unsigned long maxDelay = static_cast<unsigned long>(max(1L, mx)) * 1000UL;

	IPAddress address = m_server->remoteIP();
	IPAddress interfaceAddress = interfaceForRemote(address);
	uint16_t port = m_server->remotePort();

	post(RESPONSE, ROOT_FOR_ALL, address, interfaceAddress, port, time + random(static_cast<long>(maxDelay)));
	post(RESPONSE, ROOT_BY_UUID, address, interfaceAddress, port, time + random(static_cast<long>(maxDelay)));
	post(RESPONSE, ROOT_BY_TYPE, address, interfaceAddress, port, time + random(static_cast<long>(maxDelay)));
	if (serviceEnabled()) {
		post(RESPONSE, SERVICE_BY_TYPE, address, interfaceAddress, port, time + random(static_cast<long>(maxDelay)));
	}
}

void SsdpDevice::postResponse(ssdp_udn_t udn, long mx) {
	const unsigned long maxDelay = static_cast<unsigned long>(max(1L, mx)) * 1000UL;
	IPAddress address = m_server->remoteIP();
	post(
		RESPONSE,
		udn,
		address,
		interfaceForRemote(address),
		m_server->remotePort(),
		millis() + random(static_cast<long>(maxDelay))
	);
}

void SsdpDevice::post(
	ssdp_message_t type,
	ssdp_udn_t udn,
	IPAddress address,
	IPAddress interfaceAddress,
	uint16_t port,
	unsigned long time
) {
	int8_t freeIndex = -1;
	int8_t disposableResponse = -1;

	for (uint8_t i = 0; i < SSDP_QUEUE_SIZE; i++) {
		if (
			type != NOTIFY_ALIVE_INIT &&
			m_queue[i].time != 0 &&
			m_queue[i].type == type &&
			m_queue[i].udn == udn &&
			m_queue[i].address == address &&
			m_queue[i].interfaceAddress == interfaceAddress &&
			m_queue[i].port == port
		) {
			// De-duplicate bursts from repeated M-SEARCH packets and repeated
			// lifecycle requests. Preserve the earliest legal send time.
			if (static_cast<int32_t>(m_queue[i].time - time) > 0) {
				m_queue[i].time = time;
			}
			m_queue[i].bootId = m_bootId;
			m_queue[i].nextBootId =
				type == NOTIFY_UPDATE
					? (m_bootId == 0x7fffffffUL ? 1UL : m_bootId + 1UL)
					: 0;
			return;
		}
		if (m_queue[i].time == 0 && freeIndex < 0) freeIndex = i;
		if (m_queue[i].time != 0 && m_queue[i].type == RESPONSE && disposableResponse < 0) {
			disposableResponse = i;
		}
	}

	int8_t index = freeIndex;
	if (index < 0 && type != RESPONSE) index = disposableResponse;
	if (index < 0) return;

	m_queue[index].type = type;
	m_queue[index].udn = udn;
	m_queue[index].address = address;
	m_queue[index].interfaceAddress = interfaceAddress;
	m_queue[index].port = port;
	m_queue[index].time = time;
	m_queue[index].bootId = m_bootId;
	m_queue[index].nextBootId =
		type == NOTIFY_UPDATE
			? (m_bootId == 0x7fffffffUL ? 1UL : m_bootId + 1UL)
			: 0;
}

void SsdpDevice::send(ssdp_send_parameters_t *parameters) {
	char buffer[768];

	const char *typeTemplate;
	const char *uri, *usn1, *usn2, *usn3;

	switch (parameters->type) {
		case NOTIFY_ALIVE_INIT:
		case NOTIFY_ALIVE:
			typeTemplate = SSDP_NOTIFY_ALIVE_TEMPLATE;
			break;
		case NOTIFY_UPDATE:
			typeTemplate = SSDP_NOTIFY_UPDATE_TEMPLATE;
			break;
		case NOTIFY_BYEBYE:
			typeTemplate = nullptr;
			break;
		default: // RESPONSE
			typeTemplate = SSDP_RESPONSE_TEMPLATE;
			break;
	}

	String uuid = "uuid:" + String(m_uuid);

	switch (parameters->udn) {
		case ROOT_FOR_ALL:
			uri = "upnp:rootdevice";
			usn1 = uuid.c_str();
			usn2 = "::";
			usn3 = "upnp:rootdevice";
			break;
		case ROOT_BY_UUID:
			uri = uuid.c_str();
			usn1 = uuid.c_str();
			usn2 = "";
			usn3 = "";
			break;
		case ROOT_BY_TYPE:
			uri = m_deviceType;
			usn1 = uuid.c_str();
			usn2 = "::";
			usn3 = m_deviceType;
			break;
		case SERVICE_BY_TYPE:
			if (!serviceEnabled()) return;
			uri = FW_UPNP_SERVICE_TYPE;
			usn1 = uuid.c_str();
			usn2 = "::";
			usn3 = FW_UPNP_SERVICE_TYPE;
			break;
		default: return;
	}

	if (parameters->type == NOTIFY_BYEBYE) {
		int len = snprintf_P(
			buffer,
			sizeof(buffer),
			SSDP_NOTIFY_BYEBYE_PACKET_TEMPLATE,
			uri,
			usn1,
			usn2,
			usn3,
			parameters->bootId,
			m_configId
		);
		if (
			len > 0 &&
			static_cast<size_t>(len) < sizeof(buffer) &&
			m_server->beginPacketMulticast(
				SSDP_MULTICAST_ADDR,
				SSDP_PORT,
				parameters->interfaceAddress,
				m_ttl
			)
		) {
			m_server->write(reinterpret_cast<const uint8_t *>(buffer), static_cast<size_t>(len));
			m_server->endPacket();
		}

		parameters->time = 0;
		return;
	}

	IPAddress ip = parameters->interfaceAddress;
	if (ip == IPAddress(0, 0, 0, 0)) {
		parameters->time = 0;
		return;
	}

	if (parameters->type != NOTIFY_UPDATE) {
		parameters->bootId = m_bootId;
		parameters->nextBootId = 0;
	}

	char nextBootHeader[48] = { 0 };
	if (parameters->type == NOTIFY_UPDATE && parameters->nextBootId != 0) {
		snprintf(
			nextBootHeader,
			sizeof(nextBootHeader),
			"NEXTBOOTID.UPNP.ORG: %lu\r\n",
			static_cast<unsigned long>(parameters->nextBootId)
		);
	}

	const char *serverVersion =
		m_modelNumber[0] == '\0' ? FW_UPNP_SERVER_VERSION : m_modelNumber;
	char ipText[16];
	snprintf(
		ipText,
		sizeof(ipText),
		"%u.%u.%u.%u",
		ip[0],
		ip[1],
		ip[2],
		ip[3]
	);

	int len = snprintf_P(
		buffer,
		sizeof(buffer),
		SSDP_PACKET_TEMPLATE,
		typeTemplate,
		m_interval,
		ipText,
		m_port,
		m_schemaURL,
		serverVersion,
		usn1,
		usn2,
		usn3,
		parameters->type == RESPONSE ? "ST" : "NT",
		uri,
		parameters->bootId,
		m_configId,
		nextBootHeader
	);
	if (len <= 0 || static_cast<size_t>(len) >= sizeof(buffer)) {
		parameters->time = 0;
		return;
	}

	if (parameters->address == SSDP_MULTICAST_ADDR) {
		if (
			!m_server->beginPacketMulticast(
				parameters->address,
				parameters->port,
				parameters->interfaceAddress,
				m_ttl
			)
		) {
			parameters->time = 0;
			return;
		}
	}
	else {
		if (!m_server->beginPacket(parameters->address, parameters->port)) {
			parameters->time = 0;
			return;
		}
	}

	m_server->write(reinterpret_cast<const uint8_t *>(buffer), static_cast<size_t>(len));
	m_server->endPacket();

	// Refresh the complete advertisement set before half of max-age.
	parameters->time =
		parameters->type == NOTIFY_ALIVE
			? millis() + m_interval * 450UL
			: 0;
}

void SsdpDevice::sendNotifyByebye(
	ssdp_udn_t udn,
	IPAddress interfaceAddress
) {
	if (!m_server || interfaceAddress == IPAddress(0, 0, 0, 0)) return;

	ssdp_send_parameters_t parameters = {};
	parameters.time = millis();
	parameters.type = NOTIFY_BYEBYE;
	parameters.udn = udn;
	parameters.address = SSDP_MULTICAST_ADDR;
	parameters.interfaceAddress = interfaceAddress;
	parameters.port = SSDP_PORT;
	parameters.bootId = m_bootId;
	send(&parameters);
}

void SsdpDevice::sendAllByebye(IPAddress interfaceAddress) {
	if (interfaceAddress == IPAddress(0, 0, 0, 0)) return;

	sendNotifyByebye(ROOT_FOR_ALL, interfaceAddress);
	sendNotifyByebye(ROOT_BY_UUID, interfaceAddress);
	sendNotifyByebye(ROOT_BY_TYPE, interfaceAddress);
	if (serviceEnabled()) {
		sendNotifyByebye(SERVICE_BY_TYPE, interfaceAddress);
	}
}

void SsdpDevice::clearQueue() {
	for (uint8_t index = 0; index < SSDP_QUEUE_SIZE; ++index) {
		m_queue[index].time = 0;
	}
}

void SsdpDevice::leaveJoinedInterfaces() {
	if (m_stationJoined != IPAddress(0, 0, 0, 0)) {
		// Socket-scoped multicast membership is dropped by SsdpUdp::stop().
	}
	if (
		m_accessPointJoined != IPAddress(0, 0, 0, 0) &&
		m_accessPointJoined != m_stationJoined
	) {
		// Socket-scoped multicast membership is dropped by SsdpUdp::stop().
	}
}

IPAddress SsdpDevice::interfaceForRemote(IPAddress remote) const {
	const auto active = activeDiscoveryInterfaces();
	DiscoveryInterfaces joined;
	for (uint8_t i=0;i<active.count;++i) {
		const auto &entry=active.entries[i]; const IPAddress ip=discoveryAddress(entry.address);
		if (ip==m_ethernetJoined || ip==m_stationJoined || ip==m_accessPointJoined) joined.add(entry);
	}
	const auto *selected=joined.forRemote(discoveryAddress(remote));
	if (selected) return discoveryAddress(selected->address);
	return m_last;
}

void SsdpDevice::refreshConfigId() {
	const char* fields[]={"relative-icon-description",m_deviceType,m_friendlyName,m_serialNumber,
		m_modelName,m_modelNumber,m_modelURL,m_manufacturer,m_manufacturerURL,m_uuid,m_presentationURL,kBoardIconEtag};
	m_configId=PakarDiscovery::configId(fields,sizeof(fields)/sizeof(*fields));
}

void SsdpDevice::schema(Print &client) const {
	client.print(F(
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/xml; charset=utf-8\r\n"
		"Connection: close\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n"
	));
	PakarDiscovery::description(client,{m_deviceType,m_friendlyName,m_manufacturer,m_manufacturerURL,
		m_modelName,m_modelNumber,m_modelURL,m_serialNumber,m_uuid,m_presentationURL,
		kBoardIconWidth,kBoardIconHeight,kBoardIconDepth,m_configId});
}

void SsdpDevice::handleClient() {
	const IPAddress zero(0, 0, 0, 0);

	const auto interfaces=activeDiscoveryInterfaces();
	IPAddress station=zero, accessPoint=zero, ethernet=zero;
	for (uint8_t i=0;i<interfaces.count;++i) {
		const auto &entry=interfaces.entries[i];
		if (!strcmp(entry.kind,"ethernet")) ethernet=discoveryAddress(entry.address);
		else if (!strcmp(entry.kind,"station")) station=discoveryAddress(entry.address);
		else accessPoint=discoveryAddress(entry.address);
	}
	// Join all active local netifs, including Ethernet without an associated STA.
	// Preferred address is a discovery/default fallback, not Internet proof.
	const IPAddress preferred=interfaces.count ? discoveryAddress(interfaces.entries[0].address) : zero;

	const bool topologyChanged =
		(m_last != zero && m_last != preferred) ||
		m_stationSeen != station || m_accessPointSeen != accessPoint || m_ethernetSeen != ethernet;
	m_stationSeen=station; m_accessPointSeen=accessPoint; m_ethernetSeen=ethernet;

	if (topologyChanged) {
		clearQueue();
		if (m_server) {
			sendAllByebye(m_stationJoined);
			if (m_accessPointJoined != m_stationJoined) {
				sendAllByebye(m_accessPointJoined);
			}
			if (m_ethernetJoined != m_stationJoined && m_ethernetJoined != m_accessPointJoined)
				sendAllByebye(m_ethernetJoined);
			m_server->stop();
			leaveJoinedInterfaces();
		}

		m_last = zero;
		m_stationJoined = zero;
		m_accessPointJoined = zero;
		m_ethernetJoined = zero;
		m_joinRetryAt = 0;
	}

	const uint32_t now = millis();
	if (
		preferred != zero &&
		m_last == zero &&
		(m_joinRetryAt == 0 || static_cast<int32_t>(now - m_joinRetryAt) >= 0)
	) {
		if (!m_server) m_server = new (std::nothrow) SsdpUdp();
		if (m_server) {
			m_server->stop();
			if (m_server->beginMulticast(preferred, SSDP_MULTICAST_ADDR, SSDP_PORT)) {
				m_last = preferred;
				if (preferred == station) m_stationJoined = station;
				if (preferred == accessPoint) m_accessPointJoined = accessPoint;
				if (preferred == ethernet) m_ethernetJoined = ethernet;
				m_joinRetryAt = 0;
				postNotifyALive(preferred);
			}
			else {
				// Socket-scoped membership and the failed socket are released
				// together; retry does not retain a partial membership owner.
				m_server->stop();
				m_joinRetryAt = now + 1000UL;
			}
		}
		else {
			m_joinRetryAt = now + 1000UL;
		}
	}
	// Retry failed secondary joins at a bounded cadence; a new STA/AP/ETH netif
	// is not silently ignored just because another membership already exists.
	if (m_server && m_last != zero &&
		(m_joinRetryAt==0 || static_cast<int32_t>(now-m_joinRetryAt)>=0)) {
		bool retry=false;
		for (uint8_t i=0;i<interfaces.count;++i) {
			const IPAddress ip=discoveryAddress(interfaces.entries[i].address);
			if (ip==m_stationJoined || ip==m_accessPointJoined || ip==m_ethernetJoined) continue;
			if (!m_server->joinMulticast(ip,SSDP_MULTICAST_ADDR)) { retry=true; continue; }
			if (ip==station) m_stationJoined=ip;
			if (ip==accessPoint) m_accessPointJoined=ip;
			if (ip==ethernet) m_ethernetJoined=ip;
			postNotifyALive(ip);
		}
		m_joinRetryAt=retry ? now+1000UL : 0;
	}

	if (m_server && m_last != zero && m_server->parsePacket()) {
		String value;

		if (readLine(value) && value.equalsIgnoreCase("M-SEARCH * HTTP/1.1")) {
			String key, st;
			bool host = false, man = false;
			long mx = 0;

			while (readKeyValue(key, value)) {
				key.trim();
				value.trim();

				if (
					key.equalsIgnoreCase("HOST") &&
					(
						value.equals("239.255.255.250:1900") ||
						value.equals("239.255.255.250")
					)
				) {
					host = true;
				}
				else if (
					key.equalsIgnoreCase("MAN") &&
					value.equalsIgnoreCase("\"ssdp:discover\"")
				) {
					man = true;
				}
				else if (key.equalsIgnoreCase("ST")) {
					st = value;
				}
				else if (key.equalsIgnoreCase("MX")) {
					mx = value.toInt();
				}
			}

			if (host && man && mx > 0) {
				mx = constrain(mx, 1L, 5L);

				if (st.equalsIgnoreCase("ssdp:all")) {
					postResponse(mx);
				}
				else if (st.equalsIgnoreCase("upnp:rootdevice")) {
					postResponse(ROOT_FOR_ALL, mx);
				}
				else if (st.equalsIgnoreCase("uuid:" + String(m_uuid))) {
					postResponse(ROOT_BY_UUID, mx);
				}
				else if (st.equalsIgnoreCase(m_deviceType)) {
					postResponse(ROOT_BY_TYPE, mx);
				}
				else if (
					serviceEnabled() &&
					st.equalsIgnoreCase(FW_UPNP_SERVICE_TYPE)
				) {
					postResponse(SERVICE_BY_TYPE, mx);
				}
			}
		}

		while (m_server->available() > 0) {
			m_server->read();
		}
	}

	if (m_server && m_last != zero) {
		const unsigned long time = millis();
		for (int i = 0; i < SSDP_QUEUE_SIZE; i++) {
			if (
				m_queue[i].time > 0 &&
				static_cast<int32_t>(time - m_queue[i].time) >= 0
			) {
				send(&m_queue[i]);
			}
		}
	}
}

IPAddress SsdpDevice::activeInterfaceIP() const {
	const auto interfaces=activeDiscoveryInterfaces();
	return interfaces.count ? discoveryAddress(interfaces.entries[0].address) : IPAddress();
}

void SsdpDevice::setSchemaURL(const char *url) {
	strlcpy(m_schemaURL, url, sizeof(m_schemaURL));
}

void SsdpDevice::setHTTPPort(uint16_t port) {
	m_port = port;
}

void SsdpDevice::setDeviceType(const char *deviceType) {
	strlcpy(m_deviceType, deviceType, sizeof(m_deviceType));
}

void SsdpDevice::setUUID(const char *uuid) {
	snprintf_P(m_uuid, sizeof(m_uuid), PSTR("%s"), uuid);
}

void SsdpDevice::setName(const char *name) {
	strlcpy(m_friendlyName, name, sizeof(m_friendlyName));
}

void SsdpDevice::setURL(const char *url) {
	strlcpy(m_presentationURL, url, sizeof(m_presentationURL));
}

void SsdpDevice::setSerialNumber(const char *serialNumber) {
	strlcpy(m_serialNumber, serialNumber, sizeof(m_serialNumber));
}

void SsdpDevice::setSerialNumber(const uint32_t serialNumber) {
	snprintf(m_serialNumber, sizeof(uint32_t) * 2 + 1, "%08X", serialNumber);
}

void SsdpDevice::setModelName(const char *name) {
	strlcpy(m_modelName, name, sizeof(m_modelName));
}

void SsdpDevice::setModelNumber(const char *num) {
	strlcpy(m_modelNumber, num, sizeof(m_modelNumber));
}

void SsdpDevice::setModelURL(const char *url) {
	strlcpy(m_modelURL, url, sizeof(m_modelURL));
}

void SsdpDevice::setManufacturer(const char *name) {
	strlcpy(m_manufacturer, name, sizeof(m_manufacturer));
}

void SsdpDevice::setManufacturerURL(const char *url) {
	strlcpy(m_manufacturerURL, url, sizeof(m_manufacturerURL));
}

void SsdpDevice::setTTL(const uint8_t ttl) {
	m_ttl = ttl;
}

void SsdpDevice::setInterval(uint32_t interval) {
	m_interval = interval;
}

} // namespace PakarS3
