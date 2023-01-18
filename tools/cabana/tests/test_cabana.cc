
#include "opendbc/can/common.h"
#undef INFO
#include "catch2/catch.hpp"
#include "tools/cabana/dbcmanager.h"
#include "tools/replay/logreader.h"

// demo route, first segment
const std::string TEST_RLOG_URL = "https://commadata2.blob.core.windows.net/commadata2/4cf7a6ad03080c90/2021-09-29--13-46-36/0/rlog.bz2";

TEST_CASE("DBCManager::generateDBC") {
  DBCManager dbc_origin(nullptr);
  dbc_origin.open("toyota_new_mc_pt_generated");
  DBCManager dbc_from_generated(nullptr);
  dbc_from_generated.open("", dbc_origin.generateDBC());

  auto &msgs = dbc_origin.messages();
  auto &new_msgs = dbc_from_generated.messages();
  REQUIRE(msgs.size() == new_msgs.size());
  for (auto &[address, m] : msgs) {
    auto new_m = new_msgs.at(address);
    REQUIRE(m.name == new_m.name);
    REQUIRE(m.size == new_m.size);
    REQUIRE(m.sigs.size() == new_m.sigs.size());
    for (auto &[name, sig] : m.sigs)
      REQUIRE(sig == new_m.sigs[name]);
  }
}

TEST_CASE("Parse can messages") {
  DBCManager dbc(nullptr);
  dbc.open("toyota_new_mc_pt_generated");
  CANParser can_parser(0, "toyota_new_mc_pt_generated", {}, {});

  LogReader log;
  REQUIRE(log.load(TEST_RLOG_URL, nullptr, {}, true));
  REQUIRE(log.events.size() > 0);
  for (auto e : log.events) {
    if (e->which == cereal::Event::Which::CAN) {
      std::map<std::pair<uint32_t, std::string>, std::vector<double>> values_1;
      for (const auto &c : e->event.getCan()) {
        const auto msg = dbc.msg(c.getAddress());
        if (c.getSrc() == 0 && msg) {
          for (auto &[name, sig] : msg->sigs) {
            double val = get_raw_value((uint8_t *)c.getDat().begin(), c.getDat().size(), sig);
            values_1[{c.getAddress(), name.toStdString()}].push_back(val);
          }
        }
      }

      can_parser.UpdateCans(e->mono_time, e->event.getCan());
      auto values_2 = can_parser.query_latest();
      for (auto &[key, v1] : values_1) {
        bool found = false;
        for (auto &v2 : values_2) {
          if (v2.address == key.first && v2.name == key.second) {
            REQUIRE(v2.all_values.size() == v1.size());
            REQUIRE(v2.all_values == v1);
            found = true;
            break;
          }
        }
        REQUIRE(found);
      }
    }
  }
}
