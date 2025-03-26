#pragma once
#include <string>
#include <unordered_map>

#include "json.hpp"

using json = nlohmann::json;

const std::string DEVICE_ID = "WATER_BOTTLE_01";

const static std::unordered_map<std::string, std::string> TRANS_DICT = {
    // �������
      {"KKBS",    "F01F"}, // ���ڰ���
      {"NJBS",    "F02F"}, // Ť�ذ���
      {"YBKC",    "F03F"}, //�α꿨��
      {"TTBS",    "F04F"}, // ��Ͳ����
      {"GZC",     "F05F"}, //��ֱ��
      {"SPC",     "F06F"}, // ˮƽ��
      {"XDSC",    "F07F"}, // б������
      {"MB",      "F08F"}, // ���
      {"LLJ",     "F09F"}, // ������
      {"ZOB",     "F10F"}, // ��ŷ����֮ǰģ����С���Ե��������ǡ�(JYDZCSY)��
      {"YQB",     "F11F"}, // ����ʣ�������Щʱ��г��˼Ǻűʣ�
      {"DDG",     "F12F"}, // �����
      {"AQM",     "F13F"}, // ��ȫñ
      {"AQY",     "F14F"}, // ��ȫ��
      {"FGBX",    "F15F"}, // ���ⱳ��
      {"ST",      "F16F"}, // ����
      {"SGTHBS",  "F17F"}, // �������ɰ���
      {"M6TT",    "F18F"}, // M6TT��M8TT(M6��Ͳ��M8��Ͳ)������û�ɼ����ݣ�ûȥ��ʶ��
      {"M8TT",    "F19F"}, // ���������ֻ������ȥ����Ť�ذ���Ť��������ȷʱȥ������M6����M8��Ͳ
      {"SDT",     "F20F"}, // �ֵ�Ͳ
      {"SC",      "F21F"}, // ����
      {"JKBS",    "F22F"}, // ��������
      {"XKQ",     "F23F"}, // б��ǯ

      // �������
        {"ZWTHK",   "D01D"}, // ���������һֱ��̼���飬��ǵı�ǩ�ǡ���λ̼���顱�������������̼����
        {"THKSIDE", "D01D"}, // ���������������̼�����һ�ˣ�Ҳ��Ϊ��̼����
        {"GTZJ",    "D02D"}, // ��ͷ֧��
        {"TH",      "D03D"}, // ����
        {"THH",     "D04D"}, // ���ɺ�
        {"HXTH",    "D05D"}, // ���򵯻�
        {"ST",      "D06D"}, // ��ͨ��ע�ⲻ�����ף����������׵�key��һ���ģ�
        {"PUFG",    "D07D"}, // PU���
        {"BZX",     "D08D"}, // ��֯�ߣ�֮ǰҲ�з������ߣ�
};

// ---------------------------------------------------------------------------------------
// ���ĵ�header
const static json SDG_HEADER = {
    {"Accept",       "application/json"},
    {"Content-Type", "application/json"}
};

// ---------------------------------------------------------------------------------------
// �����ı���
static json HEART_JSON = {
    {"accessPointCode", "aiserver"},
    {"eventType",       33001     },
    {"token",           ""        },
    {"content",         ""        }
};
static json HEART_CONTENT = {
    {"equipmentCode",   ""               },
    {"equipmentStatus", 0                }, // ��������þ���0�����þ���1
    {"restoreStatus",   0                }, // ���λ�ò���ȷ����0����ȷ����1
    {"restoreLog",      "�Ƕ���ƫ��"}  // ��ʱ��û������ֶ�
};

// ---------------------------------------------------------------------------------------
// �������ı��ĸ�ʽ
static json REQUEST_JSON = {
    {"accessPointCode", "aiserver"},
    {"eventType",       33000     },
    {"token",           ""        },
    {"content",         ""        }
};

// 1.����ʶ�����
static json ASR_CONTENT = {
    {"deviceID",         DEVICE_ID    },
    {"gatherDeviceCode", "V02"        },
    {"timeStamp",        1692686752388},
    {"result",         ""           },
};
