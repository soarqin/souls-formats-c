/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_acparts4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void fill_part(sf_acparts4_part_component_t *p, uint8_t category, uint16_t id,
                      const char *name) {
    p->part_id = id;
    p->model_id = (uint16_t)(id + 100u);
    p->price = 12345 + id;
    p->weight = 200;
    p->en_cost = 30;
    p->category = category;
    p->init_status = 1;
    p->cap_id = 7;
    strcpy(p->name, name);
    strcpy(p->maker_name, "maker");
    strcpy(p->sub_category, "subcat");
    p->sub_category_id = 9;
    p->set_id = 10;
    strcpy(p->explain, "synthetic part");
}

static void test_acparts4_acfa_round_trip(void) {
    sf_acparts4_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_create(&a, SF_ACPARTS4_VERSION_ACFA, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_head_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_core_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_arm_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_leg_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_fcs_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_generator_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_main_booster_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_back_booster_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_side_booster_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_overed_booster_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_arm_unit_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_back_unit_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_shoulder_unit_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_head_top_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_head_side_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_core_upper_side_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_core_lower_side_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_arm_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_leg_back_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_leg_upper_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_leg_middle_stabilizer_count(a, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_set_leg_lower_stabilizer_count(a, 1));

    sf_acparts4_head_t *head = sf_acparts4_head_mutable_data(a);
    fill_part(&head[0].part, SF_ACPARTS4_PART_HEAD, 100, "head");
    head[0].defense.ballistic_defense = 111;
    head[0].pa.pa_rectification = 12;
    head[0].frame.ap = 3000;
    head[0].frame.drag_coefficient = 1.25f;
    head[0].stability = 400;
    head[0].camera_functionality = 55;
    head[0].stabilizer_side_y = -7;

    sf_acparts4_core_t *core = sf_acparts4_core_mutable_data(a);
    fill_part(&core[0].part, SF_ACPARTS4_PART_CORE, 101, "core");
    core[0].stability = 500;

    sf_acparts4_arm_t *arm = sf_acparts4_arm_mutable_data(a);
    fill_part(&arm[0].part, SF_ACPARTS4_PART_ARMS, 102, "arm");
    arm[0].is_weapon_arm = true;
    strcpy(arm[0].aim_type, "aim");
    arm[0].weapon.weapon_firing_mode = SF_ACPARTS4_WEAPON_FIRING_GUN;
    arm[0].weapon.can_lock_on = true;
    arm[0].weapon.attack_power = 123.0f;

    sf_acparts4_leg_t *leg = sf_acparts4_leg_mutable_data(a);
    fill_part(&leg[0].part, SF_ACPARTS4_PART_LEGS, 103, "leg");
    leg[0].type = SF_ACPARTS4_LEG_QUAD;
    leg[0].movement_ability = 777;
    leg[0].horizontal_boost.thrust = 1000;
    leg[0].unk_a4 = 36;

    fill_part(&sf_acparts4_fcs_mutable_data(a)[0].part, SF_ACPARTS4_PART_FCS, 104, "fcs");
    sf_acparts4_fcs_mutable_data(a)[0].radar.radar_range = 600;
    fill_part(&sf_acparts4_generator_mutable_data(a)[0].part, SF_ACPARTS4_PART_GENERATOR, 105, "gen");
    sf_acparts4_generator_mutable_data(a)[0].energy_capacity = 90000;
    fill_part(&sf_acparts4_main_booster_mutable_data(a)[0].part, SF_ACPARTS4_PART_MAIN_BOOSTER, 106, "main");
    sf_acparts4_main_booster_mutable_data(a)[0].quick_reload_time = 8;
    fill_part(&sf_acparts4_back_booster_mutable_data(a)[0].part, SF_ACPARTS4_PART_BACK_BOOSTER, 107, "backboost");
    fill_part(&sf_acparts4_side_booster_mutable_data(a)[0].part, SF_ACPARTS4_PART_SIDE_BOOSTER, 108, "sideboost");
    fill_part(&sf_acparts4_overed_booster_mutable_data(a)[0].part, SF_ACPARTS4_PART_OVERED_BOOSTER, 109, "ob");
    sf_acparts4_overed_booster_mutable_data(a)[0].assault_armor_attack_power = 44;
    fill_part(&sf_acparts4_arm_unit_mutable_data(a)[0].part, SF_ACPARTS4_PART_ARM_UNIT, 110, "armunit");
    sf_acparts4_arm_unit_mutable_data(a)[0].weapon.attack_power = 222.0f;
    fill_part(&sf_acparts4_back_unit_mutable_data(a)[0].part, SF_ACPARTS4_PART_BACK_UNIT, 111, "backunit");
    sf_acparts4_back_unit_mutable_data(a)[0].takes_both_slots = true;
    fill_part(&sf_acparts4_shoulder_unit_mutable_data(a)[0].part, SF_ACPARTS4_PART_SHOULDER_UNIT, 112, "shoulder");
    strcpy(sf_acparts4_shoulder_unit_mutable_data(a)[0].device_name, "device");
    sf_acparts4_shoulder_unit_mutable_data(a)[0].aa_attack_power = 3.5f;

    sf_acparts4_stabilizer_part_t *stabs[] = {
        sf_acparts4_head_top_stabilizer_mutable_data(a),
        sf_acparts4_head_side_stabilizer_mutable_data(a),
        sf_acparts4_core_upper_side_stabilizer_mutable_data(a),
        sf_acparts4_core_lower_side_stabilizer_mutable_data(a),
        sf_acparts4_arm_stabilizer_mutable_data(a),
        sf_acparts4_leg_back_stabilizer_mutable_data(a),
        sf_acparts4_leg_upper_stabilizer_mutable_data(a),
        sf_acparts4_leg_middle_stabilizer_mutable_data(a),
        sf_acparts4_leg_lower_stabilizer_mutable_data(a),
    };
    for (uint8_t i = 0; i < 9; i++) {
        fill_part(&stabs[i][0].part, (uint8_t)(SF_ACPARTS4_PART_HEAD_TOP_STABILIZER + i),
                  (uint16_t)(120 + i), "stab");
        stabs[i][0].stabilizer.category = stabs[i][0].part.category;
        stabs[i][0].stabilizer.control_calibration = (float)i;
    }

    void *bytes_a = NULL;
    size_t size_a = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_write_to_memory(a, &bytes_a, &size_a, NULL));
    TEST_ASSERT_NOT_NULL(bytes_a);
    TEST_ASSERT_TRUE(size_a > 48u);
    TEST_ASSERT_EQUAL_UINT8(0, ((uint8_t *)bytes_a)[0]);
    TEST_ASSERT_EQUAL_UINT8(1, ((uint8_t *)bytes_a)[5]);

    sf_acparts4_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_read_from_memory(&b, bytes_a, size_a,
                                                             SF_ACPARTS4_VERSION_ACFA, NULL));
    TEST_ASSERT_EQUAL_size_t(22, sf_acparts4_count(b));
    TEST_ASSERT_EQUAL_STRING("head", sf_acparts4_head_data(b)[0].part.name);
    TEST_ASSERT_EQUAL_UINT16(111, sf_acparts4_head_data(b)[0].defense.ballistic_defense);
    TEST_ASSERT_EQUAL_INT16(-7, sf_acparts4_head_data(b)[0].stabilizer_side_y);
    TEST_ASSERT_EQUAL_INT32(777, sf_acparts4_leg_data(b)[0].movement_ability);
    TEST_ASSERT_TRUE(sf_acparts4_back_unit_data(b)[0].takes_both_slots);
    TEST_ASSERT_EQUAL_STRING("device", sf_acparts4_shoulder_unit_data(b)[0].device_name);

    void *bytes_b = NULL;
    size_t size_b = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acparts4_write_to_memory(b, &bytes_b, &size_b, NULL));
    TEST_ASSERT_EQUAL_size_t(size_a, size_b);
    TEST_ASSERT_EQUAL_MEMORY(bytes_a, bytes_b, size_a);

    sf_free(NULL, bytes_b);
    sf_acparts4_destroy(b);
    sf_free(NULL, bytes_a);
    sf_acparts4_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_acparts4_acfa_round_trip);
    return UNITY_END();
}
