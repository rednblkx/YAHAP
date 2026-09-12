#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "TestUtil.hpp"
#include <nlohmann/json.hpp>

using namespace hap::core;
using json = nlohmann::json;

void test_characteristic_json() {
    auto c = std::make_shared<Characteristic>(
        0x23, // Name characteristic type
        Format::String,
        std::vector{Permission::PairedRead}
    );
    c->set_value(std::string("Test Device"));

    AttributeDatabase db;
    auto acc = std::make_shared<Accessory>(1);
    auto svc = std::make_shared<Service>(0x3E, "Test");
    svc->add_characteristic(c);
    acc->add_service(svc);
    db.add_accessory(acc);

    std::string json_str = db.to_json_string();
    auto j = json::parse(json_str);

    CHECK(j.contains("accessories"));
    CHECK(j["accessories"].is_array());
    CHECK_EQ(j["accessories"].size(), 1);

    auto& acc_json = j["accessories"][0];
    CHECK_EQ(acc_json["aid"], 1);
    CHECK_EQ(acc_json["services"].size(), 1);

    auto& svc_json = acc_json["services"][0];
    CHECK_EQ(svc_json["iid"], 1); // Service is IID 1 (AID 1 always starts services at 1)
    CHECK_EQ(svc_json["characteristics"].size(), 1);

    auto& char_json = svc_json["characteristics"][0];
    CHECK_EQ(char_json["iid"], 2); // Characteristics start at 2 after the service
    CHECK_EQ(char_json["format"], "string");
    CHECK_EQ(char_json["value"], "Test Device");
    CHECK_EQ(char_json["perms"].size(), 1);
    CHECK_EQ(char_json["perms"][0], "pr");
}

void test_multi_accessory_json() {
    AttributeDatabase db;

    // Accessory 1 (Bridge)
    auto acc1 = std::make_shared<Accessory>(1);
    auto svc1 = std::make_shared<Service>(0x3E, "Bridge");

    auto name_char = std::make_shared<Characteristic>(0x23, Format::String, std::vector{Permission::PairedRead});
    name_char->set_value(std::string("My Bridge"));
    svc1->add_characteristic(name_char);

    acc1->add_service(svc1);
    db.add_accessory(acc1);

    // Accessory 2 (Lightbulb)
    auto acc2 = std::make_shared<Accessory>(2);
    auto svc2 = std::make_shared<Service>(0x43, "Lightbulb");

    auto on_char = std::make_shared<Characteristic>(0x25, Format::Bool, std::vector{Permission::PairedRead, Permission::PairedWrite, Permission::Notify});
    on_char->set_value(true);
    svc2->add_characteristic(on_char);

    acc2->add_service(svc2);
    db.add_accessory(acc2);

    std::string json_str = db.to_json_string();
    auto j = json::parse(json_str);

    CHECK_EQ(j["accessories"].size(), 2);
    CHECK_EQ(j["accessories"][0]["aid"], 1);
    CHECK_EQ(j["accessories"][1]["aid"], 2);

    auto& bulb_char = j["accessories"][1]["services"][0]["characteristics"][0];
    CHECK_EQ(bulb_char["value"], true);
    CHECK_EQ(bulb_char["format"], "bool");
    CHECK_EQ(bulb_char["perms"].size(), 3);
}

void test_duplicate_aid_rejected() {
    AttributeDatabase db;
    auto acc = std::make_shared<Accessory>(1);
    acc->add_service(std::make_shared<Service>(0x3E, "A"));
    CHECK_EQ(db.add_accessory(acc), ValidationResult::Success);
    // Same AID again must be rejected, not silently accepted.
    auto dup = std::make_shared<Accessory>(1);
    dup->add_service(std::make_shared<Service>(0x3E, "B"));
    CHECK_EQ(db.add_accessory(dup), ValidationResult::DuplicateAccessoryId);
    CHECK_EQ(db.accessories().size(), 1);
}

int main() {
    RUN_TEST(test_characteristic_json);
    RUN_TEST(test_multi_accessory_json);
    RUN_TEST(test_duplicate_aid_rejected);
    return 0;
}
