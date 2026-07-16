#pragma once

#include "comet_server.h"

#include "meteor_push.grpc.pb.h"

#include <grpcpp/grpcpp.h>

namespace meteorpush {

// CometService 的实现：被 job 通过 gRPC 调用，将下行消息推送到本机连接
class CometServiceImpl final : public meteorpush::CometService::Service {
 public:
  explicit CometServiceImpl(CometServer* server);

  ::grpc::Status PushToComet(
      ::grpc::ServerContext* context,
      const ::meteorpush::PushToCometRequest* request,
      ::meteorpush::PushToCometReply* response) override;

  // 客户端流：Job持续推送消息
  ::grpc::Status PushStream(
      ::grpc::ServerContext* context,
      ::grpc::ServerReader<::meteorpush::PushToCometRequest>* reader,
      ::meteorpush::PushStreamReply* response) override;

 private:
  // 处理单条推送请求的内部方法
  void ProcessPushRequest(const ::meteorpush::PushToCometRequest& request);
  CometServer* server_;
};

}  // namespace meteorpush



