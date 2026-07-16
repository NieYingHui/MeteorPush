#pragma once

#include "conversation_store.h"
#include "group_dao.h"
#include "user_dao.h"
#include "kafka_producer.h"
#include "redis_store.h"

#include "meteor_push.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace meteorpush {

// LogicService 的具体实现，主要负责鉴权、上行消息处理、路由与调用 JobService
class LogicServiceImpl final : public meteorpush::LogicService::Service {
 public:
  LogicServiceImpl(ConversationStore* store,
                   GroupMemberDao* group_member_dao,
                   UserDao* user_dao,
                   KafkaProducer* producer,
                   KafkaProducer* broadcast_producer,
                   RedisStore* redis_store);

  // token 校验：demo 规则为 "token-<user_id>"
  ::grpc::Status VerifyToken(::grpc::ServerContext* context,
                             const ::meteorpush::VerifyTokenRequest* request,
                             ::meteorpush::VerifyTokenReply* response) override;

  ::grpc::Status SendUpstreamMessage(
      ::grpc::ServerContext* context,
      const ::meteorpush::UpstreamMessageRequest* request,
      ::meteorpush::UpstreamMessageReply* response) override;

  ::grpc::Status UserOffline(::grpc::ServerContext* context,
                             const ::meteorpush::UserOfflineRequest* request,
                             ::meteorpush::SimpleReply* response) override;

  ::grpc::Status ReportRoomJoin(::grpc::ServerContext* context,
                                const ::meteorpush::RoomReportRequest* request,
                                ::meteorpush::SimpleReply* response) override;

  ::grpc::Status ReportRoomLeave(::grpc::ServerContext* context,
                                 const ::meteorpush::RoomReportRequest* request,
                                 ::meteorpush::SimpleReply* response) override;

  ::grpc::Status Broadcast(::grpc::ServerContext* context,
                           const ::meteorpush::BroadcastRequest* request,
                           ::meteorpush::BroadcastReply* response) override;

  // 双向流：高性能消息通道
  ::grpc::Status MessageStream(
      ::grpc::ServerContext* context,
      ::grpc::ServerReaderWriter<::meteorpush::StreamResponse,
                                 ::meteorpush::StreamMessage>* stream) override;

 private:
  // 内部处理方法（复用现有逻辑）
  void HandleUpstreamMessage(const ::meteorpush::UpstreamMessageRequest& request,
                             ::meteorpush::UpstreamMessageReply* response);
  void HandleUserOffline(const ::meteorpush::UserOfflineRequest& request,
                         ::meteorpush::SimpleReply* response);
  void HandleRoomJoin(const ::meteorpush::RoomReportRequest& request,
                      ::meteorpush::SimpleReply* response);
  void HandleRoomLeave(const ::meteorpush::RoomReportRequest& request,
                       ::meteorpush::SimpleReply* response);
  void SetError(ErrorInfo* e, int code, const std::string& msg);

  // 简单路由结构：user_id -> set<comet_id>
  void AddRoute(int64_t user_id, const std::string& comet_id);
  void RemoveRoute(int64_t user_id, const std::string& comet_id);
  std::unordered_set<std::string> GetUserComets(int64_t user_id);

  ConversationStore* store_;
  GroupMemberDao* group_member_dao_;
  UserDao* user_dao_;
  KafkaProducer* producer_;
  KafkaProducer* broadcast_producer_;
  RedisStore* redis_store_;
};

}  // namespace meteorpush


