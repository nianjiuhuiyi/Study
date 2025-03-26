#pragma once
#include <string>
#include <unordered_map>

#include "json.hpp"

using json = nlohmann::json;

const std::string DEVICE_ID = "WATER_BOTTLE_01";

const static std::unordered_map<std::string, std::string> TRANS_DICT = {
    // 工具相关
      {"KKBS",    "F01F"}, // 开口扳手
      {"NJBS",    "F02F"}, // 扭矩扳手
      {"YBKC",    "F03F"}, //游标卡尺
      {"TTBS",    "F04F"}, // 套筒扳手
      {"GZC",     "F05F"}, //钢直尺
      {"SPC",     "F06F"}, // 水平尺
      {"XDSC",    "F07F"}, // 斜度塞尺
      {"MB",      "F08F"}, // 秒表
      {"LLJ",     "F09F"}, // 拉力计
      {"ZOB",     "F10F"}, // 兆欧表，（之前模型里叫“绝缘电阻测试仪”(JYDZCSY)）
      {"YQB",     "F11F"}, // 油漆笔（可能有些时候叫成了记号笔）
      {"DDG",     "F12F"}, // 导电膏
      {"AQM",     "F13F"}, // 安全帽
      {"AQY",     "F14F"}, // 安全衣
      {"FGBX",    "F15F"}, // 反光背心
      {"ST",      "F16F"}, // 手套
      {"SGTHBS",  "F17F"}, // 升弓弹簧扳手
      {"M6TT",    "F18F"}, // M6TT、M8TT(M6套筒、M8套筒)这两个没采集数据，没去做识别
      {"M8TT",    "F19F"}, // 这两个类别只是用于去区分扭矩扳手扭矩设置正确时去区分是M6还是M8套筒
      {"SDT",     "F20F"}, // 手电筒
      {"SC",      "F21F"}, // 塞尺
      {"JKBS",    "F22F"}, // 棘开扳手
      {"XKQ",     "F23F"}, // 斜口钳

      // 部件相关
        {"ZWTHK",   "D01D"}, // 后视相机，一直把碳滑块，标记的标签是“在位碳滑块”，对外输出都是碳滑块
        {"THKSIDE", "D01D"}, // 侧面相机，看到的碳滑块的一端，也认为是碳滑块
        {"GTZJ",    "D02D"}, // 弓头支架
        {"TH",      "D03D"}, // 弹簧
        {"THH",     "D04D"}, // 弹簧盒
        {"HXTH",    "D05D"}, // 横向弹簧
        {"ST",      "D06D"}, // 三通（注意不是手套，跟上面手套的key是一样的）
        {"PUFG",    "D07D"}, // PU风管
        {"BZX",     "D08D"}, // 编织线（之前也叫分流导线）
};

// ---------------------------------------------------------------------------------------
// 报文的header
const static json SDG_HEADER = {
    {"Accept",       "application/json"},
    {"Content-Type", "application/json"}
};

// ---------------------------------------------------------------------------------------
// 心跳的报文
static json HEART_JSON = {
    {"accessPointCode", "aiserver"},
    {"eventType",       33001     },
    {"token",           ""        },
    {"content",         ""        }
};
static json HEART_CONTENT = {
    {"equipmentCode",   ""               },
    {"equipmentStatus", 0                }, // 相机不可用就是0，可用就是1
    {"restoreStatus",   0                }, // 相机位置不正确就是0，正确就是1
    {"restoreLog",      "角度无偏差"}  // 暂时还没用这个字段
};

// ---------------------------------------------------------------------------------------
// 各个项点的报文格式
static json REQUEST_JSON = {
    {"accessPointCode", "aiserver"},
    {"eventType",       33000     },
    {"token",           ""        },
    {"content",         ""        }
};

// 1.语音识别项点
static json ASR_CONTENT = {
    {"deviceID",         DEVICE_ID    },
    {"gatherDeviceCode", "V02"        },
    {"timeStamp",        1692686752388},
    {"result",         ""           },
};
