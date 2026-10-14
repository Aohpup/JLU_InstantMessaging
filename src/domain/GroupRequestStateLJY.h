#pragma once
// 失效用于邀请人离群，或对象已通过其他请求入群的情况。
enum class GroupRequestStateLJY { Pending, Accepted, Rejected, Cancelled };
