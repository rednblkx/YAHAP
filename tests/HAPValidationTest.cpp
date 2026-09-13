#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/Accessory.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "hap/core/HAPValidation.hpp"
#include <iostream>
#include "TestUtil.hpp"

using namespace hap::core;

void test_valid_bridge_two_accessories() {
    AttributeDatabase db;
    
    // Bridge accessory (AID=1)
    auto bridge = std::make_unique<Accessory>(1);
    auto bridge_svc = std::make_unique<Service>(0x3E, "Bridge");
    bridge->add_service(std::move(bridge_svc));
    
    auto result1 = db.add_accessory(std::move(bridge));
    CHECK(result1 == ValidationResult::Success);
    
    // Bridged accessory (AID=2)
    auto light = std::make_unique<Accessory>(2);
    auto light_svc = std::make_unique<Service>(0x43, "Lightbulb");
    light->add_service(std::move(light_svc));
    
    auto result2 = db.add_accessory(std::move(light));
    CHECK(result2 == ValidationResult::Success);
    
    CHECK(db.accessories().size() == 2);
    std::cout << "test_valid_bridge_two_accessories passed" << std::endl;
}

void test_duplicate_aid_fails() {
    AttributeDatabase db;
    
    auto acc1 = std::make_unique<Accessory>(1);
    acc1->add_service(std::make_unique<Service>(0x3E, "Test"));
    
    auto result1 = db.add_accessory(std::move(acc1));
    CHECK(result1 == ValidationResult::Success);
    
    // Try adding another accessory with same AID
    auto acc2 = std::make_unique<Accessory>(1);
    acc2->add_service(std::make_unique<Service>(0x43, "Lightbulb"));
    
    auto result2 = db.add_accessory(std::move(acc2));
    CHECK(result2 == ValidationResult::DuplicateAccessoryId);
    
    CHECK(db.accessories().size() == 1);
    std::cout << "test_duplicate_aid_fails passed" << std::endl;
}

void test_too_many_services_fails() {
    AttributeDatabase db;
    
    auto acc = std::make_unique<Accessory>(1);
    
    // Add 101 services (exceeds limit of 100)
    for (int i = 0; i < 101; ++i) {
        acc->add_service(std::make_unique<Service>(0x100 + i, "Service" + std::to_string(i)));
    }
    
    auto result = db.add_accessory(std::move(acc));
    CHECK(result == ValidationResult::TooManyServices);
    
    CHECK(db.accessories().size() == 0);
    std::cout << "test_too_many_services_fails passed" << std::endl;
}

void test_too_many_characteristics_fails() {
    AttributeDatabase db;
    
    auto acc = std::make_unique<Accessory>(1);
    auto svc = std::make_unique<Service>(0x3E, "Test");
    
    // Add 101 characteristics (exceeds limit of 100)
    for (int i = 0; i < 101; ++i) {
        auto ch = std::make_unique<Characteristic>(
            0x100 + i, Format::Bool, Permissions{Permission::PairedRead});
        svc->add_characteristic(std::move(ch));
    }
    acc->add_service(std::move(svc));
    
    auto result = db.add_accessory(std::move(acc));
    CHECK(result == ValidationResult::TooManyCharacteristics);
    
    CHECK(db.accessories().size() == 0);
    std::cout << "test_too_many_characteristics_fails passed" << std::endl;
}

void test_validation_result_strings() {
    CHECK(validation_result_str(ValidationResult::Success) != nullptr);
    CHECK(validation_result_str(ValidationResult::TooManyAccessories) != nullptr);
    CHECK(validation_result_str(ValidationResult::DuplicateAccessoryId) != nullptr);
    CHECK(validation_result_str(ValidationResult::TooManyServices) != nullptr);
    CHECK(validation_result_str(ValidationResult::TooManyCharacteristics) != nullptr);
    std::cout << "test_validation_result_strings passed" << std::endl;
}

void test_linked_services() {
    AttributeDatabase db;
    
    auto acc = std::make_unique<Accessory>(1);
    
    auto irrigation = std::make_unique<Service>(0xCF, "Irrigation System", true);
    auto valve1 = std::make_unique<Service>(0xD0, "Valve 1");
    auto valve2 = std::make_unique<Service>(0xD0, "Valve 2");
    
    acc->add_service(std::move(irrigation));
    acc->add_service(std::move(valve1));
    acc->add_service(std::move(valve2));
    
    db.add_accessory(std::move(acc));
    
    // Link valves to irrigation after IIDs are assigned. Ownership moved
    // into the database, so reach the services through it.
    Service* irrigation_p = db.accessories()[0]->services()[0].get();
    Service* valve1_p = db.accessories()[0]->services()[1].get();
    Service* valve2_p = db.accessories()[0]->services()[2].get();
    irrigation_p->add_linked_service(valve1_p->iid());
    irrigation_p->add_linked_service(valve2_p->iid());

    CHECK(irrigation_p->linked_services().size() == 2);
    std::cout << "test_linked_services passed" << std::endl;
}

int main() {
    test_valid_bridge_two_accessories();
    test_duplicate_aid_fails();
    test_too_many_services_fails();
    test_too_many_characteristics_fails();
    test_validation_result_strings();
    test_linked_services();
    
    std::cout << "\n=== All bridge validation tests passed ===" << std::endl;
    return 0;
}
