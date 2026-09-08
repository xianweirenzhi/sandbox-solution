#include <Arduino.h>

#include "net_config.h"   // 设备统一名称（NET_DEVICE_NAME）
#include "wifi_net.h"     // 网络模块对外接口
#include "host_link.h"    // 主机通信模块对外接口
#include "oled_ctrl.h"    // OLED 状态可视化层模块对外接口
#include "sht30_sensor.h" // SHT30 温湿度采集模块对外接口
#include "bh1750_sensor.h" // GY-30(BH1750) 光照采集模块对外接口
#include "mq2_sensor.h"   // MQ-2 可燃气体采集模块对外接口
#include "soil_sensor.h"  // 土壤湿度监测模块对外接口
#include "sgp30_sensor.h" // SGP30 空气质量采集模块对外接口
#include "actuator_ctrl.h" // 执行器模块（水泵/风扇/舵机）对外接口

// 全局模块实例（业务模块后续在此按需添加同类实例）
WifiNet net;
HostLink link;        // F8266-1 从机 → esp32-1 主机(NET_HOST_PORT) 的 TCP 链路
OledCtrl oled;        // 智慧农场状态可视化层（信息行由各子系统写入）
Sht30Sensor sht30;    // 子系统①：温湿度采集（I2C 0x44，与 OLED 共线）
Bh1750Sensor gy30;    // 子系统②：光照采集（GY-30/BH1750，I2C 0x23，共线）
Mq2Sensor mq2;        // 子系统③：可燃气体采集（MQ-2，ADC 独立 5V 供电+共地+分压）
SoilSensor soil;      // 子系统④：土壤湿度监测（DO→GPIO14，3V3 供电，1s 环境消抖）
Sgp30Sensor sgp30;    // 子系统⑤：空气质量 eCO₂/TVOC（SGP30，I2C 0x58，共线）
ActuatorCtrl act;     // 执行实体：水泵（GPIO0）/风扇正反转（GPIO13/2）/舵机（GPIO12）

// ===================== 业务命令扩展口 =====================
// 收到主机下发命令【内容】时回调这里（帧头 @ / 帧尾 / 已由 host_link 剥离）。
// 命令词表（主机网页按键 / 控制台下发，见 esp32-1 与 ARCHITECTURE 协议）：
//   PUMP ON / PUMP OFF        水泵开关
//   FAN FWD / FAN REV / FAN STOP  风扇正转/反转/停
//   SERVO <0~180>             舵机角度
void onHostCommand(const String &cmd) {
  if (cmd == F("PUMP ON")) { act.pumpOn(); return; }
  if (cmd == F("PUMP OFF")) { act.pumpOff(); return; }
  if (cmd == F("FAN FWD")) { act.fanForward(); return; }
  if (cmd == F("FAN REV")) { act.fanReverse(); return; }
  if (cmd == F("FAN STOP")) { act.fanStop(); return; }
  if (cmd.startsWith(F("SERVO "))) {
    int deg = cmd.substring(6).toInt();
    if (deg >= 0 && deg <= 180) { act.servoSet(deg); return; }
  }
  Serial.print(F("[main] 未知命令: "));
  Serial.println(cmd);
}

// ===================== 传感数据上报 =====================
// 打包传感数据为 JSON（不含 MQ-2：该传感器仅本机使用，不上报主机），
// 未就绪/故障的传感器字段填 null，主机端据此显示 "--"。
static void reportSensors() {
  if (!link.isOnline()) return;   // 未连上主机不上报

  String j = F("{\"t\":");
  j += sht30.hasData() ? String(sht30.getTempC(), 1) : F("null");
  j += F(",\"h\":");
  j += sht30.hasData() ? String(sht30.getHumRH(), 1) : F("null");
  j += F(",\"lux\":");
  j += gy30.hasData() ? String((int)gy30.getLux()) : F("null");
  j += F(",\"soil\":");
  j += soil.isMoist() ? '1' : '0';           // 土壤数字量始终有值（1=湿润 0=过干）
  j += F(",\"co2\":");
  j += sgp30.hasData() ? String((unsigned)sgp30.getEco2Ppm()) : F("null");
  j += F(",\"tvoc\":");
  j += sgp30.hasData() ? String((unsigned)sgp30.getTvocPpb()) : F("null");
  // 执行器实时状态:主机据此在网页显示水泵/风扇/舵机当前态
  j += F(",\"pump\":");
  j += act.pumpIsOn() ? '1' : '0';          // 0=关 1=开
  j += F(",\"fan\":");
  j += String(act.fanState());              // 0=OFF 1=FWD 2=REV
  j += F(",\"servo\":");
  j += String(act.servoGet());              // 0~180 度
  j += '}';

  if (link.send(j)) {
    Serial.print(F("[main] 上报: "));
    Serial.println(j);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.print(F("[main] "));
  Serial.print(NET_DEVICE_NAME);
  Serial.println(F(" 启动"));

  // OLED 激活（开机即点亮；后续作为硬件/子系统状态可视化入口）
  if (oled.begin()) {
    Serial.println(F("[main] OLED 已激活"));
  } else {
    Serial.println(F("[main] OLED 初始化失败：检查 I2C 接线/地址"));
  }

  // 子系统①：SHT30 温湿度传感器（同一 I2C 总线；未检测到不阻塞启动，handle 内自愈重扫）
  sht30.begin();

  // 子系统②：GY-30(BH1750) 光照传感器（同一 I2C 总线；同上自愈策略）
  gy30.begin();

  // 子系统③：MQ-2 可燃气体传感器（ADC 模拟量；独立 5V 供电须共地，AO 经分压接 ADC）
  mq2.begin();

  // 子系统④：土壤湿度传感器（DO 数字量→GPIO14；高=湿润适宜，低=过干，1s 环境消抖）
  soil.begin();

  // 子系统⑤：SGP30 空气质量传感器（同一 I2C 总线；eCO₂ 为 TVOC 推算等效值，15s 暖机）
  sgp30.begin();

  // 执行实体：水泵/风扇/舵机（进入安全态：泵关/风扇停/舵机 90°；控制入口后续接主机命令/自动逻辑）
  act.begin();

  // 上电自检：逐个激活执行器 0.5s 供人工检查接线/动作（阻塞式，联网前完成，约 2.4s）
  act.selfTest();

  if (!net.begin()) {
    // begin() 内部失败会自重启，正常情况下不会走到这里（防御性处理）
    Serial.println(F("[main] 联网失败，重启"));
    ESP.restart();
  }

  link.begin();                    // 复位主机链路状态
  link.onCommand(onHostCommand);   // 注册业务命令回调（命令扩展口）
  Serial.println(F("[main] 网络就绪，进入主循环"));
}

void loop() {
  net.handle();    // 网络周期维护（断线自愈）
  link.handle();   // 主机链路周期维护（连接/收发/断线重连）
  oled.handle();   // OLED 周期维护（预留整页状态渲染）
  sht30.handle();  // 子系统①：温湿度周期采集（健康判定 + 热插拔重扫）
  gy30.handle();   // 子系统②：光照周期采集（健康判定 + 热插拔重扫）
  mq2.handle();    // 子系统③：可燃气体周期采集（预热/报警/合理性监测）
  soil.handle();   // 子系统④：土壤湿度监测（1s 环境消抖）
  sgp30.handle();  // 子系统⑤：空气质量周期采集（暖机/健康/热插拔重扫）
  act.handle();    // 执行实体周期维护（预留：缓动/超时保护）

  // 传感数据周期上报主机（每 NET_REPORT_MS 一次；不含 MQ-2）
  static uint32_t lastReportMs = 0;
  if (millis() - lastReportMs >= NET_REPORT_MS) {
    lastReportMs = millis();
    reportSensors();
  }

  // 子系统状态上屏（信息区每子系统一行；后续子系统在此追加各行）
  static uint32_t lastUiMs = 0;
  if (millis() - lastUiMs >= 1000) {
    lastUiMs = millis();

    // 行 0：SHT30 温湿度
    if (!sht30.isOk()) {
      oled.setInfoLine(0, F("SHT30 ERR"));
    } else if (!sht30.hasData()) {
      oled.setInfoLine(0, F("SHT30 ..."));    // 已检测到，等待首次读数
    } else {
      char line[24];
      snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%",
               sht30.getTempC(), sht30.getHumRH());
      oled.setInfoLine(0, line);
    }

    // 行 1：GY-30 光照
    if (!gy30.isOk()) {
      oled.setInfoLine(1, F("GY30 ERR"));
    } else if (!gy30.hasData()) {
      oled.setInfoLine(1, F("GY30 ..."));     // 已检测到，等待首次读数
    } else {
      char line[24];
      snprintf(line, sizeof(line), "L:%.0flux", gy30.getLux());
      oled.setInfoLine(1, line);
    }

    // 行 2：MQ-2 可燃气体（预热 → 报警优先 → 读数异常 → 正常读数）
    if (mq2.isAlarm()) {
      oled.setInfoLine(2, F("MQ2 GAS!"));
    } else if (mq2.isPreheat()) {
      oled.setInfoLine(2, F("MQ2 HEAT"));
    } else if (!mq2.isOk()) {
      oled.setInfoLine(2, F("MQ2 ERR"));      // 长期贴 0/贴满量程，疑似断线或分压失效
    } else {
      char line[24];
      snprintf(line, sizeof(line), "G:%d", mq2.getRaw());
      oled.setInfoLine(2, line);
    }

    // 行 3：土壤湿度（已消抖判定）
    oled.setInfoLine(3, soil.isMoist() ? F("SOIL:WET") : F("SOIL:DRY"));

    // 行 4：SGP30 空气质量（eCO₂ 等效值 + TVOC）
    if (!sgp30.isOk()) {
      oled.setInfoLine(4, F("SGP30 ERR"));
    } else if (sgp30.isWarmup()) {
      oled.setInfoLine(4, F("SGP30 INIT"));   // 上电 15s 暖机，读数尚未可靠
    } else if (!sgp30.hasData()) {
      oled.setInfoLine(4, F("SGP30 ..."));    // 已检测到，等待首次读数
    } else {
      char line[24];
      snprintf(line, sizeof(line), "C:%uppm V:%uppb",
               (unsigned)sgp30.getEco2Ppm(), (unsigned)sgp30.getTvocPpb());
      oled.setInfoLine(4, line);
    }

    // 行 5：执行器状态（水泵/风扇/舵机；控制入口接入后此行实时反映）
    {
      char line[24];
      snprintf(line, sizeof(line), "P:%s F:%s S:%u",
               act.pumpIsOn() ? "ON" : "OFF",
               act.fanState() == 1 ? "FWD" : (act.fanState() == 2 ? "REV" : "OFF"),
               (unsigned)act.servoGet());
      oled.setInfoLine(5, line);
    }
  }

  // ===== 业务模块扩展位 =====
  // 新增功能时请单独建模块文件（如 led_ctrl.h/.cpp），并在上方实例化、
  // 在本循环内调用其 handle()。避免把逻辑堆进 main.cpp，便于长期维护。
  // 命令详细内容请在 onHostCommand() 中扩展。

  delay(10);
}
