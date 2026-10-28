#pragma once
// Protocol types are included globally by ShellBuild, not inside the sketch's
// anonymous owner namespace (which would shadow the shared PakarS3 namespace).
ManagedTransferFiles sshTransferFiles(transferFileRequest);
uint32_t sshTransferToken=0,sshTransferRevision=0;
#if PAKAR_SCP_ENABLED
PakarS3::ScpUpload sshUpload;
#endif
#if PAKAR_SFTP_ENABLED
std::unique_ptr<PakarS3::SftpSession<ManagedTransferFiles>> sshSftp;
#endif
void resetSshTransfer(){
  sshTransferFiles.cancel();sshTransferToken=sshTransferRevision=0;
#if PAKAR_SCP_ENABLED
  sshUpload=PakarS3::ScpUpload{};
#endif
#if PAKAR_SFTP_ENABLED
  sshSftp.reset();
#endif
}
bool startSshTransfer(const PakarS3::SshTransport::Packet& packet){
#if PAKAR_SFTP_ENABLED
  if(packet.kind==PakarS3::SshTransport::Sftp){
    sshSftp.reset(new(std::nothrow) PakarS3::SftpSession<ManagedTransferFiles>(sshTransferFiles));return sshSftp&&sshSftp->ready();
  }
#endif
#if PAKAR_SCP_ENABLED
  if(packet.kind==PakarS3::SshTransport::FileWrite&&packet.size&&packet.size<=64){char name[65]{};memcpy(name,packet.bytes,packet.size);sshUpload.start(name);return true;}
#endif
  return false;
}
bool inputSshTransfer(uint8_t byte){
#if PAKAR_SFTP_ENABLED
  if(sshSftp)return sshSftp->input(byte);
#endif
#if PAKAR_SCP_ENABLED
  return sshUpload.input(byte,sshTransferFiles);
#else
  return false;
#endif
}
void serviceSshTransfer(){
  if(!sshTransferToken)return;
  auto emit=[](const uint8_t* bytes,size_t count){return deviceSsh.output(sshTransferToken,sshTransferRevision,bytes,count);};
#if PAKAR_SFTP_ENABLED
  if(sshSftp){sshSftp->service(emit);if(sshSftp->failed())deviceSsh.close(sshTransferToken);return;}
#endif
#if PAKAR_SCP_ENABLED
  sshUpload.service(sshTransferFiles,emit);
  if(sshUpload.phase()==PakarS3::ScpUpload::Done||sshUpload.reported())deviceSsh.finish(sshTransferToken,sshUpload.reported()?1:0);
#endif
}
