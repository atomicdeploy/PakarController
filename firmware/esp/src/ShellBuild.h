#pragma once
#if defined(ARDUINO_ARCH_ESP32) && __has_include(<build/config/esp32s3/LedBackend.h>)
#include <build/config/esp32s3/LedBackend.h>
#endif
#ifndef PAKAR_TELNET_SESSIONS
#define PAKAR_TELNET_SESSIONS 2
#endif
#ifndef PAKAR_SERIAL_SHELL_ENABLED
#define PAKAR_SERIAL_SHELL_ENABLED 1
#endif
#ifndef PAKAR_TELNET_ENABLED
#define PAKAR_TELNET_ENABLED 1
#endif
#ifndef PAKAR_SSH_ENABLED
#define PAKAR_SSH_ENABLED 0
#endif
#ifndef PAKAR_SCP_ENABLED
#define PAKAR_SCP_ENABLED 0
#endif
#ifndef PAKAR_SFTP_ENABLED
#define PAKAR_SFTP_ENABLED 0
#endif
#ifndef PAKAR_FTP_ENABLED
#define PAKAR_FTP_ENABLED 0
#endif
#if PAKAR_SFTP_ENABLED && !PAKAR_SSH_ENABLED
#error Native SFTP requires native SSH
#endif
#if PAKAR_FTP_ENABLED && !defined(ARDUINO_ARCH_ESP32)
#error Native FTP requires ESP32-S3
#endif
#if PAKAR_SCP_ENABLED && !PAKAR_SSH_ENABLED
#error Native SCP requires native SSH
#endif
#if PAKAR_SSH_ENABLED
#if !defined(ARDUINO_ARCH_ESP32)
#error Native SSH requires ESP32-S3
#endif
#include "../../esp32s3/src/SshTransport.h"
#endif
#if PAKAR_SFTP_ENABLED
#include "../../esp32s3/src/SftpSession.h"
#endif
#if PAKAR_FTP_ENABLED
#include <Preferences.h>
#include <sys/socket.h>
#include <errno.h>
#include "../../esp32s3/src/FtpSession.h"
#endif
