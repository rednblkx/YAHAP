#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cassert>

using namespace hap::core;
using json = nlohmann::json;

void test_characteristic_json() {
    auto c = std::make_shared<Characteristic>(
        0x23, // Name characteristic type
        Format::String,
        std::vector{Permission::PairedRead}
    );
    c->set_iid(1);
    c->set_value(std::string("Test Device"));
    
    AttributeDatabase db;
    auto acc = std::make_shared<Accessory>(1);
    auto svc = std::make_shared<Service>(0x3E, "Test");
    svc->set_iid(1);
    svc->add_characteristic(c);
    acc->add_service(svc);
    db.add_accessory(acc);
    
    std::string json_str = db.to_json_string();
    auto j = json::parse(json_str);
    
    assert(j.contains("accessories"));
    assert(j["accessories"].is_array());
    assert(j["accessories"].size() == 1);
    
    auto& acc_json = j["accessories"][0];
    assert(acc_json["aid"] == 1);
    assert(acc_json["services"].size() == 1);
    
    auto& svc_json = acc_json["services"][0];
    assert(svc_json["iid"] == 1);
    assert(svc_json["characteristics"].size() == 1);
    
    auto& char_json = svc_json["characteristics"][0];
    assert(char_json["iid"] == 1);
    assert(char_json["format"] == "string");
    assert(char_json["value"] == "Test Device");
    assert(char_json["perms"].size() == 1);
    assert(char_json["perms"][0] == "pr");
    
    std::cout << "test_characteristic_json passed" << std::endl;
}

void test_multi_accessory_json() {
    AttributeDatabase db;
    
    // Accessory 1 (Bridge)
    auto acc1 = std::make_shared<Accessory>(1);
    auto svc1 = std::make_shared<Service>(0x3E, "Bridge");
    svc1->set_iid(1);
    
    auto name_char = std::make_shared<Characteristic>(0x23, Format::String, std::vector{Permission::PairedRead});
    name_char->set_iid(2);
    name_char->set_value(std::string("My Bridge"));
    svc1->add_characteristic(name_char);
    
    acc1->add_service(svc1);
    db.add_accessory(acc1);
    
    // Accessory 2 (Lightbulb)
    auto acc2 = std::make_shared<Accessory>(2);
    auto svc2 = std::make_shared<Service>(0x43, "Lightbulb");
    svc2->set_iid(1);
    
    auto on_char = std::make_shared<Characteristic>(0x25, Format::Bool, std::vector{Permission::PairedRead, Permission::PairedWrite, Permission::Notify});
    on_char->set_iid(2);
    on_char->set_value(true);
    svc2->add_characteristic(on_char);
    
    acc2->add_service(svc2);
    db.add_accessory(acc2);
    
    std::string json_str = db.to_json_string();
    auto j = json::parse(json_str);
    
    assert(j["accessories"].size() == 2);
    assert(j["accessories"][0]["aid"] == 1);
    assert(j["accessories"][1]["aid"] == 2);
    
    auto& bulb_char = j["accessories"][1]["services"][0]["characteristics"][0];
    assert(bulb_char["value"] == true);
    assert(bulb_char["format"] == "bool");
    assert(bulb_char["perms"].size() == 3);
    
    std::cout << "test_multi_accessory_json passed" << std::endl;
}

int main() {
    test_characteristic_json();
    test_multi_accessory_json();
    return 0;
}
