-- P3 表结构：由存储类和独立验证程序共用；正式菜单在后续批次接入。
-- 每次打开连接都应在事务外启用外键。
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS persons (
    person_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    birth_date TEXT NOT NULL,
    location TEXT NOT NULL,
    avatar_color INTEGER NOT NULL DEFAULT -1 CHECK (avatar_color BETWEEN -1 AND 5)
);

CREATE TABLE IF NOT EXISTS service_accounts (
    account_id INTEGER PRIMARY KEY,
    person_id INTEGER NOT NULL REFERENCES persons(person_id),
    service INTEGER NOT NULL CHECK (service IN (0, 1, 2)),
    external_id TEXT NOT NULL,
    nickname TEXT NOT NULL,
    applied_on TEXT NOT NULL,
    UNIQUE (person_id, service),
    UNIQUE (service, external_id)
);

-- 每个人员一份凭据，沿用虚构课堂账号的简单字符串密码方案。
CREATE TABLE IF NOT EXISTS credentials (
    person_id INTEGER PRIMARY KEY REFERENCES persons(person_id),
    password TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS account_bindings (
    wechat_account_id INTEGER PRIMARY KEY REFERENCES service_accounts(account_id),
    qq_account_id INTEGER NOT NULL UNIQUE REFERENCES service_accounts(account_id)
);

-- 较小账号编号在前，两个备注分别属于两端账号。
CREATE TABLE IF NOT EXISTS friendships (
    first_account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    second_account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    first_remark TEXT NOT NULL DEFAULT '',
    second_remark TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (first_account_id, second_account_id),
    CHECK (first_account_id < second_account_id)
);

CREATE TABLE IF NOT EXISTS chat_groups (
    group_id INTEGER PRIMARY KEY,
    service INTEGER NOT NULL CHECK (service IN (0, 1, 2)),
    number INTEGER NOT NULL,
    name TEXT NOT NULL,
    owner_account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    style INTEGER NOT NULL CHECK (style IN (0, 1)),
    -- 顶级群用 NULL；加载到 C++ 时转换为原有的 0。
    parent_group_id INTEGER REFERENCES chat_groups(group_id),
    UNIQUE (service, number)
);

CREATE TABLE IF NOT EXISTS group_members (
    group_id INTEGER NOT NULL REFERENCES chat_groups(group_id),
    account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    is_administrator INTEGER NOT NULL CHECK (is_administrator IN (0, 1)),
    -- 保留原成员 vector 的展示顺序，不把账号编号误当成成员顺序。
    member_order INTEGER NOT NULL,
    PRIMARY KEY (group_id, account_id),
    UNIQUE (group_id, member_order)
);

-- 请求沿用现有实体的“服务＋对外号码”表示，不额外复制内部编号。
CREATE TABLE IF NOT EXISTS group_requests (
    request_id INTEGER PRIMARY KEY,
    service INTEGER NOT NULL CHECK (service IN (0, 1, 2)),
    group_number INTEGER NOT NULL,
    type INTEGER NOT NULL CHECK (type IN (0, 1)),
    sender_number TEXT NOT NULL,
    target_number TEXT NOT NULL,
    state INTEGER NOT NULL CHECK (state IN (0, 1, 2, 3)),
    FOREIGN KEY (service, group_number) REFERENCES chat_groups(service, number),
    FOREIGN KEY (service, sender_number) REFERENCES service_accounts(service, external_id),
    FOREIGN KEY (service, target_number) REFERENCES service_accounts(service, external_id)
);

-- 单行保存发号状态。删除最高编号后也不能倒退为现存记录的最大值。
CREATE TABLE IF NOT EXISTS id_state (
    singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
    last_person_id INTEGER NOT NULL,
    last_account_id INTEGER NOT NULL,
    last_shared_number INTEGER NOT NULL,
    last_wechat_number INTEGER NOT NULL,
    last_group_id INTEGER NOT NULL,
    last_group_request_id INTEGER NOT NULL
);
INSERT OR IGNORE INTO id_state VALUES (1, 0, 0, 10000, 20000, 0, 0);

-- 好友申请每对账号保留最近一条；同意前不写 friendships。
CREATE TABLE IF NOT EXISTS friend_requests (
    first_account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    second_account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    sender_account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    recipient_account_id INTEGER NOT NULL REFERENCES service_accounts(account_id),
    state INTEGER NOT NULL CHECK (state IN (0,1,2)),
    PRIMARY KEY(first_account_id,second_account_id),
    CHECK(first_account_id < second_account_id),
    CHECK((sender_account_id=first_account_id AND recipient_account_id=second_account_id)
       OR (sender_account_id=second_account_id AND recipient_account_id=first_account_id))
);
