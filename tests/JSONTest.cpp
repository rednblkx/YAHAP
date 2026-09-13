#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/common/JsonValue.hpp"
#include "TestUtil.hpp"

using namespace hap::core;
using JsonValue = hap::common::JsonValue;

void test_characteristic_json() {
    auto c = std::make_shared<Characteristic>(
        0x23, // Name characteristic type
        Format::String,
        Permissions{Permission::PairedRead}
    );
    c->set_value(std::string("Test Device"));

    AttributeDatabase db;
    auto acc = std::make_shared<Accessory>(1);
    auto svc = std::make_shared<Service>(0x3E, "Test");
    svc->add_characteristic(c);
    acc->add_service(svc);
    db.add_accessory(acc);

    std::string json_str = db.to_json_string();
    bool err = false;
    auto j = JsonValue::parse(json_str, &err);
    CHECK(!err);

    CHECK(j.contains("accessories"));
    CHECK(j.find("accessories")->is_array());
    CHECK_EQ(j.find("accessories")->size(), 1);

    const auto& acc_json = j.find("accessories")->items()[0];
    CHECK_EQ(acc_json.find("aid")->as_uint64(), 1);
    CHECK_EQ(acc_json.find("services")->size(), 1);

    const auto& svc_json = acc_json.find("services")->items()[0];
    CHECK_EQ(svc_json.find("iid")->as_uint64(), 1); // Service is IID 1 (AID 1 always starts services at 1)
    CHECK_EQ(svc_json.find("characteristics")->size(), 1);

    const auto& char_json = svc_json.find("characteristics")->items()[0];
    CHECK_EQ(char_json.find("iid")->as_uint64(), 2); // Characteristics start at 2 after the service
    CHECK_EQ(char_json.find("format")->as_string(), "string");
    CHECK_EQ(char_json.find("value")->as_string(), "Test Device");
    CHECK_EQ(char_json.find("perms")->size(), 1);
    CHECK_EQ(char_json.find("perms")->items()[0].as_string(), "pr");
}

void test_multi_accessory_json() {
    AttributeDatabase db;

    // Accessory 1 (Bridge)
    auto acc1 = std::make_shared<Accessory>(1);
    auto svc1 = std::make_shared<Service>(0x3E, "Bridge");

    auto name_char = std::make_shared<Characteristic>(0x23, Format::String, Permissions{Permission::PairedRead});
    name_char->set_value(std::string("My Bridge"));
    svc1->add_characteristic(name_char);

    acc1->add_service(svc1);
    db.add_accessory(acc1);

    // Accessory 2 (Lightbulb)
    auto acc2 = std::make_shared<Accessory>(2);
    auto svc2 = std::make_shared<Service>(0x43, "Lightbulb");

    auto on_char = std::make_shared<Characteristic>(0x25, Format::Bool, Permissions{Permission::PairedRead, Permission::PairedWrite, Permission::Notify});
    on_char->set_value(true);
    svc2->add_characteristic(on_char);

    acc2->add_service(svc2);
    db.add_accessory(acc2);

    std::string json_str = db.to_json_string();
    bool err = false;
    auto j = JsonValue::parse(json_str, &err);
    CHECK(!err);

    CHECK_EQ(j.find("accessories")->size(), 2);
    CHECK_EQ(j.find("accessories")->items()[0].find("aid")->as_uint64(), 1);
    CHECK_EQ(j.find("accessories")->items()[1].find("aid")->as_uint64(), 2);

    const auto& bulb_char = j.find("accessories")->items()[1].find("services")->items()[0].find("characteristics")->items()[0];
    CHECK_EQ(bulb_char.find("value")->as_bool(), true);
    CHECK_EQ(bulb_char.find("format")->as_string(), "bool");
    CHECK_EQ(bulb_char.find("perms")->size(), 3);
}

void test_number_serialization() {
    // Integral values (e.g. minValue/maxValue/minStep metadata set as doubles)
    // must serialize as integers. A previous regression printed them as 0
    // because the integer fast-path read an unpopulated magnitude field —
    // HomeKit rejects the database as non-compliant when minStep/maxValue
    // come back wrong.
    struct Case { double v; const char* want; };
    const Case cases[] = {
        {3.0, "3"}, {1.0, "1"}, {100.0, "100"}, {0.0, "0"}, {-5.0, "-5"},
        {0.1, "0.1"}, {20.5, "20.5"}, {26.123456, "26.123456"}, {-0.5, "-0.5"},
        {123456789.125, "123456789.125"},
    };
    for (const auto& c : cases) {
        CHECK_EQ(JsonValue(c.v).dump(), std::string(c.want));
    }

    // Parsing: integers keep exact magnitude (64-bit range), floats survive.
    bool err = false;
    auto obj = JsonValue::parse("{\"aid\": 65535, \"big\": 4294967296, \"neg\": -70401}", &err);
    CHECK(!err);
    CHECK_EQ(obj.find("aid")->as_uint64(), 65535ull);
    CHECK_EQ(obj.find("big")->as_uint64(), 4294967296ull);
    CHECK_EQ(obj.find("neg")->as_int(), -70401);

    auto arr = JsonValue::parse("[0.1, 20.5, 1e2]", &err);
    CHECK(!err);
    CHECK_EQ(arr.items()[0].as_double(), 0.1);
    CHECK_EQ(arr.items()[1].as_double(), 20.5);
    CHECK_EQ(arr.items()[2].as_double(), 100.0);
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
    RUN_TEST(test_number_serialization);
    RUN_TEST(test_duplicate_aid_rejected);
    return 0;
}
