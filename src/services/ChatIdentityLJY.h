#pragma once
#include "domain/ChatMessageLJY.h"

// 在主线程调用：要求当前已登录服务，目标不是本人且已是当前服务好友。
// 返回身份快照；会话注销、切换或好友删除时，上层应停止旧连接。
ChatIdentityLJY makeChatIdentityLJY(const SessionManagerLJY& session, const std::string& peerAccount);
