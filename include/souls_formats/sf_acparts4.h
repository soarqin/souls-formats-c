/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — AcParts4 public surface.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/AcParts/AC4/AcParts4.cs
 *   SoulsFormats/Formats/AcParts/AC4/Component/ (all .cs files)
 *   SoulsFormats/Formats/AcParts/AC4/Part/ (all .cs files)
 *   SoulsFormats/Formats/AcParts/AC4/Types/DispType.cs
 */

#ifndef SOULS_FORMATS_SF_ACPARTS4_H
#define SOULS_FORMATS_SF_ACPARTS4_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SF_ACPARTS4_NAME_LEN         33u
#define SF_ACPARTS4_EXPLAIN_LEN      257u
#define SF_ACPARTS4_AIM_TYPE_LEN     17u
#define SF_ACPARTS4_DEVICE_NAME_LEN  17u

typedef struct sf_acparts4 sf_acparts4_t;

typedef enum sf_acparts4_version {
    SF_ACPARTS4_VERSION_AC4  = 0,
    SF_ACPARTS4_VERSION_ACFA = 1,
} sf_acparts4_version_t;
_Static_assert(SF_ACPARTS4_VERSION_AC4 == 0, "AcParts4Version drift (AC4)");
_Static_assert(SF_ACPARTS4_VERSION_ACFA == 1, "AcParts4Version drift (ACFA)");

typedef enum sf_acparts4_part_category {
    SF_ACPARTS4_PART_HEAD                       = 0,
    SF_ACPARTS4_PART_CORE                       = 1,
    SF_ACPARTS4_PART_ARMS                       = 2,
    SF_ACPARTS4_PART_LEGS                       = 3,
    SF_ACPARTS4_PART_FCS                        = 4,
    SF_ACPARTS4_PART_GENERATOR                  = 5,
    SF_ACPARTS4_PART_MAIN_BOOSTER               = 6,
    SF_ACPARTS4_PART_BACK_BOOSTER               = 7,
    SF_ACPARTS4_PART_SIDE_BOOSTER               = 8,
    SF_ACPARTS4_PART_OVERED_BOOSTER             = 9,
    SF_ACPARTS4_PART_ARM_UNIT                   = 10,
    SF_ACPARTS4_PART_BACK_UNIT                  = 11,
    SF_ACPARTS4_PART_SHOULDER_UNIT              = 12,
    SF_ACPARTS4_PART_HEAD_TOP_STABILIZER        = 13,
    SF_ACPARTS4_PART_HEAD_SIDE_STABILIZER       = 14,
    SF_ACPARTS4_PART_CORE_UPPER_SIDE_STABILIZER = 15,
    SF_ACPARTS4_PART_CORE_LOWER_SIDE_STABILIZER = 16,
    SF_ACPARTS4_PART_ARM_STABILIZER             = 17,
    SF_ACPARTS4_PART_LEG_BACK_STABILIZER        = 18,
    SF_ACPARTS4_PART_LEG_UPPER_STABILIZER       = 19,
    SF_ACPARTS4_PART_LEG_MIDDLE_STABILIZER      = 20,
    SF_ACPARTS4_PART_LEG_LOWER_STABILIZER       = 21,
} sf_acparts4_part_category_t;
_Static_assert(SF_ACPARTS4_PART_HEAD == 0, "PartCategory drift (Head)");
_Static_assert(SF_ACPARTS4_PART_LEG_LOWER_STABILIZER == 21,
               "PartCategory drift (LegLowerStabilizer)");

typedef enum sf_acparts4_weapon_firing_mode {
    SF_ACPARTS4_WEAPON_FIRING_GUN  = 0,
    SF_ACPARTS4_WEAPON_FIRING_MIS  = 1,
    SF_ACPARTS4_WEAPON_FIRING_CAN  = 2,
    SF_ACPARTS4_WEAPON_FIRING_BLD  = 4,
    SF_ACPARTS4_WEAPON_FIRING_PILE = 5,
} sf_acparts4_weapon_firing_mode_t;
_Static_assert(SF_ACPARTS4_WEAPON_FIRING_PILE == 5, "FiringMode drift");

typedef enum sf_acparts4_weapon_type {
    SF_ACPARTS4_WEAPON_TYPE_WEAPON    = 0,
    SF_ACPARTS4_WEAPON_TYPE_DOZAR     = 1,
    SF_ACPARTS4_WEAPON_TYPE_PILE      = 4,
    SF_ACPARTS4_WEAPON_TYPE_BALLISTIC = 5,
    SF_ACPARTS4_WEAPON_TYPE_ENERGY    = 6,
    SF_ACPARTS4_WEAPON_TYPE_MISSILE   = 7,
    SF_ACPARTS4_WEAPON_TYPE_KOJIMA    = 9,
} sf_acparts4_weapon_type_t;
_Static_assert(SF_ACPARTS4_WEAPON_TYPE_KOJIMA == 9, "WeaponType drift");

typedef enum sf_acparts4_damage_type {
    SF_ACPARTS4_DAMAGE_RIGID  = 0,
    SF_ACPARTS4_DAMAGE_ENERGY = 1,
} sf_acparts4_damage_type_t;
_Static_assert(SF_ACPARTS4_DAMAGE_ENERGY == 1, "DamageType drift");

typedef enum sf_acparts4_leg_type {
    SF_ACPARTS4_LEG_BIPEDAL       = 0,
    SF_ACPARTS4_LEG_REVERSE_JOINT = 1,
    SF_ACPARTS4_LEG_QUAD          = 2,
    SF_ACPARTS4_LEG_TANK          = 3,
} sf_acparts4_leg_type_t;
_Static_assert(SF_ACPARTS4_LEG_TANK == 3, "LegType drift");

typedef enum sf_acparts4_fcs_deflect_type {
    SF_ACPARTS4_FCS_DEFLECT_NONE     = 0,
    SF_ACPARTS4_FCS_DEFLECT_ROUGH    = 1,
    SF_ACPARTS4_FCS_DEFLECT_DETAILED = 2,
} sf_acparts4_fcs_deflect_type_t;
_Static_assert(SF_ACPARTS4_FCS_DEFLECT_DETAILED == 2, "DeflectType drift");

typedef enum sf_acparts4_arm_unit_hanger_type {
    SF_ACPARTS4_HANGER_NOT_HANGERABLE = 0,
    SF_ACPARTS4_HANGER_TANK_ONLY      = 1,
    SF_ACPARTS4_HANGER_ENABLE         = 2,
} sf_acparts4_arm_unit_hanger_type_t;
_Static_assert(SF_ACPARTS4_HANGER_ENABLE == 2, "HangerType drift");

typedef enum sf_acparts4_back_unit_type {
    SF_ACPARTS4_BACK_UNIT_WEAPON         = 0,
    SF_ACPARTS4_BACK_UNIT_RADAR          = 1,
    SF_ACPARTS4_BACK_UNIT_PA_MOLDER      = 2,
    SF_ACPARTS4_BACK_UNIT_ADD_BOOSTER    = 3,
    SF_ACPARTS4_BACK_UNIT_ASSAULT_CANNON = 4,
} sf_acparts4_back_unit_type_t;
_Static_assert(SF_ACPARTS4_BACK_UNIT_ASSAULT_CANNON == 4, "BackUnitType drift");

typedef enum sf_acparts4_shoulder_type {
    SF_ACPARTS4_SHOULDER_LINK_WEAPON    = 0,
    SF_ACPARTS4_SHOULDER_MANUAL_WEAPON  = 1,
    SF_ACPARTS4_SHOULDER_SPECIAL_DEVICE = 2,
    SF_ACPARTS4_SHOULDER_PA_MOLDER      = 3,
    SF_ACPARTS4_SHOULDER_ADD_BOOSTER    = 4,
} sf_acparts4_shoulder_type_t;
_Static_assert(SF_ACPARTS4_SHOULDER_ADD_BOOSTER == 4, "ShoulderType drift");

typedef enum sf_acparts4_disp_type {
    SF_ACPARTS4_DISP_SHELL_GUN         = 0,
    SF_ACPARTS4_DISP_MAGAZINE_GUN      = 1,
    SF_ACPARTS4_DISP_EN_GUN            = 2,
    SF_ACPARTS4_DISP_SHOTGUN           = 3,
    SF_ACPARTS4_DISP_RAILGUN           = 4,
    SF_ACPARTS4_DISP_MISSILE           = 5,
    SF_ACPARTS4_DISP_VT_MISSILE        = 6,
    SF_ACPARTS4_DISP_SPREAD_MISSILE    = 7,
    SF_ACPARTS4_DISP_ROCKET            = 8,
    SF_ACPARTS4_DISP_BLADE             = 9,
    SF_ACPARTS4_DISP_PILE              = 10,
    SF_ACPARTS4_DISP_KOJIMA            = 11,
    SF_ACPARTS4_DISP_LINK_MISSILE      = 12,
    SF_ACPARTS4_DISP_NOLOCK_MISSILE    = 13,
    SF_ACPARTS4_DISP_ECM               = 14,
    SF_ACPARTS4_DISP_FLARE             = 15,
    SF_ACPARTS4_DISP_PA_CHARGER        = 16,
    SF_ACPARTS4_DISP_BACK_PA_MOLDER    = 17,
    SF_ACPARTS4_DISP_SHLDR_PA_MOLDER   = 18,
    SF_ACPARTS4_DISP_FLASH             = 19,
    SF_ACPARTS4_DISP_SHOT_MINE         = 21,
    SF_ACPARTS4_DISP_RADAR             = 22,
    SF_ACPARTS4_DISP_MULTI_LASER       = 23,
    SF_ACPARTS4_DISP_BUCKSHOT          = 24,
    SF_ACPARTS4_DISP_GRENADE           = 25,
    SF_ACPARTS4_DISP_ADD_BOOSTER       = 26,
    SF_ACPARTS4_DISP_ASSAULT_AMPLIFIER = 27,
    SF_ACPARTS4_DISP_ECM_STATIC        = 28,
    SF_ACPARTS4_DISP_ASSAULT_CANNON    = 29,
    SF_ACPARTS4_DISP_DOZAR_PILE        = 30,
    SF_ACPARTS4_DISP_KOJIMA_PILE       = 31,
} sf_acparts4_disp_type_t;
_Static_assert(SF_ACPARTS4_DISP_KOJIMA_PILE == 31, "DispType drift");

typedef struct sf_acparts4_part_component {
    uint16_t part_id;
    uint16_t model_id;
    int32_t  price;
    uint16_t weight;
    uint16_t en_cost;
    uint8_t  category;
    uint8_t  init_status;
    uint16_t cap_id;
    char     name[SF_ACPARTS4_NAME_LEN];
    char     maker_name[SF_ACPARTS4_NAME_LEN];
    char     sub_category[SF_ACPARTS4_NAME_LEN];
    uint16_t sub_category_id;
    uint16_t set_id;
    char     explain[SF_ACPARTS4_EXPLAIN_LEN];
} sf_acparts4_part_component_t;

typedef struct sf_acparts4_defense_component { uint16_t ballistic_defense, energy_defense; } sf_acparts4_defense_component_t;
typedef struct sf_acparts4_pa_component { uint16_t pa_rectification, pa_durability; } sf_acparts4_pa_component_t;
typedef struct sf_acparts4_frame_component {
    uint16_t tune_max_rectification, tune_efficiency_rectification, ap, unk06;
    float drag_coefficient;
    uint16_t weight_balance_front, weight_balance_back, weight_balance_right, weight_balance_left;
} sf_acparts4_frame_component_t;
typedef struct sf_acparts4_booster_component {
    uint32_t thrust, tune_max_thrust;
    uint16_t tune_efficiency_thrust, quick_boost_duration;
    uint32_t thrust_en_cost;
} sf_acparts4_booster_component_t;
typedef struct sf_acparts4_radar_component {
    uint16_t radar_range, ecm_resistance, tune_max_ecm_resistance;
    uint16_t tune_efficiency_ecm_resistance, radar_refresh_rate;
    uint16_t tune_max_radar_refresh_rate, tune_efficiency_radar_refresh_rate;
} sf_acparts4_radar_component_t;
typedef struct sf_acparts4_stabilizer_component {
    uint8_t category;
    float control_calibration;
} sf_acparts4_stabilizer_component_t;
typedef struct sf_acparts4_weapon_component {
    uint8_t weapon_firing_mode;
    bool can_lock_on;
    uint8_t missile_lock_time;
    bool unk03;
    uint16_t firing_range, melee_ability;
    uint32_t bullet_id, sfx_id, hit_effect_id;
    float ballistics_velocity, en_cost;
    bool multi_proc;
    uint8_t projectile_count, continuous_fire_count, unk1f;
    uint16_t unk20, auto_interval, fire_rate, recoil, cost_per_round, shot_precision;
    uint16_t number_of_magazines, magazine_capacity, magazine_reload_time, weapon_type;
    uint16_t charge_time, kp_charge_cost;
    float kojima_max_damage_rate;
    uint16_t attack_latency, unk3e;
    uint8_t damage_type;
    bool damage_pierce, unk41, damage_radial;
    float attack_power, impact_force, pa_attentuation, pa_penetration;
} sf_acparts4_weapon_component_t;
typedef struct sf_acparts4_weapon_booster_component {
    uint32_t horizontal_thrust, vertical_thrust, quick_boost, unk0c_thrust;
    uint32_t horizontal_en_cost, vertical_en_cost, quick_boost_en_cost, unk0c_en_cost;
} sf_acparts4_weapon_booster_component_t;

typedef struct sf_acparts4_head {
    sf_acparts4_part_component_t part;
    sf_acparts4_defense_component_t defense;
    sf_acparts4_pa_component_t pa;
    sf_acparts4_frame_component_t frame;
    uint16_t stability, sfx_monoeye, tune_max_stability, tune_efficiency_stability;
    uint16_t camera_functionality, system_recovery, unk28, unk2a;
    int16_t stabilizer_top_x, stabilizer_top_y, stabilizer_side_x, stabilizer_side_y;
} sf_acparts4_head_t;

typedef struct sf_acparts4_core {
    sf_acparts4_part_component_t part;
    sf_acparts4_defense_component_t defense;
    sf_acparts4_pa_component_t pa;
    sf_acparts4_frame_component_t frame;
    uint32_t hunger_unit, unk20;
    uint16_t unk1c, tune_max_unk1c, tune_efficiency_unk1c, stability;
    uint16_t tune_max_stability, tune_efficiency_stability;
    int16_t stabilizer_up_x, stabilizer_up_y, stabilizer_low_x, stabilizer_low_y;
} sf_acparts4_core_t;

typedef struct sf_acparts4_arm {
    sf_acparts4_part_component_t part;
    sf_acparts4_defense_component_t defense;
    sf_acparts4_pa_component_t pa;
    sf_acparts4_frame_component_t frame;
    bool is_weapon_arm;
    uint8_t unk1d;
    uint16_t firing_stability, energy_weapon_skill, weapon_maneuverability, aim_precision;
    uint8_t unk26, unk27;
    uint16_t unk28, unk2a, tune_max_firing_stability, tune_efficiency_firing_stability;
    uint16_t tune_max_energy_weapon_skill, tune_efficiency_energy_weapon_skill;
    uint16_t tune_max_weapon_maneuverability, tune_efficiency_weapon_maneuverability;
    uint16_t tune_max_aim_precision, tune_efficiency_aim_precision;
    uint8_t display_type, unk3d, unk3e, unk3f;
    char aim_type[SF_ACPARTS4_AIM_TYPE_LEN];
    sf_acparts4_weapon_component_t weapon;
    int16_t stabilizer_x, stabilizer_y;
} sf_acparts4_arm_t;

typedef struct sf_acparts4_leg {
    sf_acparts4_part_component_t part;
    sf_acparts4_defense_component_t defense;
    sf_acparts4_pa_component_t pa;
    sf_acparts4_frame_component_t frame;
    uint8_t type, motion;
    uint16_t unk1e, max_load, load, tune_max_load, tune_efficiency_load;
    int32_t back_unit_angle, movement_ability, turning_ability, braking_ability;
    int32_t jumping_ability, landing_stability, hit_stability, shoot_stability;
    int32_t tune_max_movement_ability, tune_max_turning_ability, tune_max_braking_ability;
    int32_t tune_max_jumping_ability, tune_max_landing_stability, tune_max_hit_stability;
    int32_t tune_max_shoot_stability;
    uint16_t tune_efficiency_movement_ability, tune_efficiency_turning_ability;
    uint16_t tune_efficiency_braking_ability, tune_efficiency_jumping_ability;
    uint16_t tune_efficiency_landing_stability, tune_efficiency_hit_stability;
    uint16_t tune_efficiency_shoot_stability, unk70;
    sf_acparts4_booster_component_t horizontal_boost, vertical_boost, quick_boost;
    uint8_t unk_a4, unk_a5;
    int16_t unk_a6;
    int16_t stabilizer_back_x, stabilizer_back_y, stabilizer_up_x, stabilizer_up_y;
    int16_t stabilizer_mid_x, stabilizer_mid_y, stabilizer_low_x, stabilizer_low_y;
    int16_t stabilizer_up_right_x, stabilizer_up_right_y, stabilizer_mid_right_x;
    int16_t stabilizer_mid_right_y, stabilizer_low_right_x, stabilizer_low_right_y;
} sf_acparts4_leg_t;

typedef struct sf_acparts4_fcs {
    sf_acparts4_part_component_t part;
    uint8_t deflect, lock_target_max;
    uint16_t blade_lock_distance, parallel_processing, visibility, lock_distance;
    uint16_t lock_box_height, lock_box_width, unk_lock_range4, second_lock_time;
    uint16_t missile_lock_speed;
    bool multi_lock;
    uint8_t unk15;
    uint16_t unk16, zoom_range;
    sf_acparts4_radar_component_t radar;
    uint16_t tune_max_second_lock_time, tune_efficiency_second_lock_time;
    uint16_t tune_max_missile_lock_speed, tune_efficiency_missile_lock_speed;
} sf_acparts4_fcs_t;

typedef struct sf_acparts4_generator {
    sf_acparts4_part_component_t part;
    int32_t energy_capacity, tune_max_energy_capacity;
    uint16_t tune_efficiency_energy_capacity, unk0a;
    int32_t energy_output_soft_limit, energy_output, tune_max_energy_output;
    uint16_t tune_efficiency_energy_output, kp_output, tune_max_kp_output;
    uint16_t tune_efficiency_kp_output, active_se, unk22;
} sf_acparts4_generator_t;

typedef struct sf_acparts4_main_booster {
    sf_acparts4_part_component_t part;
    sf_acparts4_booster_component_t horizontal_boost, vertical_boost, quick_boost;
    uint8_t quick_reload_time, unk31;
    uint16_t unk32;
} sf_acparts4_main_booster_t;
typedef struct sf_acparts4_back_booster {
    sf_acparts4_part_component_t part;
    sf_acparts4_booster_component_t horizontal_boost, quick_boost;
    uint8_t quick_reload_time, unk31;
    uint16_t unk32;
} sf_acparts4_back_booster_t;
typedef sf_acparts4_back_booster_t sf_acparts4_side_booster_t;

typedef struct sf_acparts4_overed_booster {
    sf_acparts4_part_component_t part;
    sf_acparts4_booster_component_t horizontal_boost;
    uint16_t overed_boost_kp_cost, unk12;
    uint32_t prepare_en_cost, prepare_kp_cost, ob_activation_thrust, ob_activation_en_cost;
    uint32_t ob_activation_kp_cost, ob_activation_limit;
    uint16_t sfx_overboost_charge, sfx_overboost_launch;
    uint32_t unk2c;
    uint16_t assault_armor_attack_power, assault_armor_range;
    uint32_t unk34, unk38, unk3c;
} sf_acparts4_overed_booster_t;

typedef struct sf_acparts4_arm_unit {
    sf_acparts4_part_component_t part;
    sf_acparts4_weapon_component_t weapon;
    uint8_t hanger_requirement, display_type;
    uint16_t unk56;
} sf_acparts4_arm_unit_t;

typedef struct sf_acparts4_back_unit {
    sf_acparts4_part_component_t part;
    sf_acparts4_weapon_component_t weapon;
    uint8_t unk54, unk55;
    uint16_t unk56, unk58;
    sf_acparts4_radar_component_t radar;
    sf_acparts4_weapon_booster_component_t weapon_booster;
    uint16_t assault_cannon_attack_power, unk8a, assault_cannon_impact;
    uint16_t assault_cannon_attentuation, assault_cannon_penetration, unk92;
    uint8_t type, display_type, unk6a, unk6b;
    bool takes_both_slots;
    uint8_t unk97;
    sf_acparts4_pa_component_t pa;
} sf_acparts4_back_unit_t;

typedef struct sf_acparts4_shoulder_unit {
    sf_acparts4_part_component_t part;
    uint8_t type;
    bool is_weapon;
    uint8_t display_type, unk03;
    sf_acparts4_pa_component_t pa;
    char device_name[SF_ACPARTS4_DEVICE_NAME_LEN];
    uint16_t use_count, effect_duration, reload_frame, unk1e;
    float effect_param_0, effect_param_1, aa_attack_power, aa_range_boost;
    sf_acparts4_weapon_component_t weapon;
    sf_acparts4_weapon_booster_component_t weapon_booster;
} sf_acparts4_shoulder_unit_t;

typedef struct sf_acparts4_stabilizer_part {
    sf_acparts4_part_component_t part;
    sf_acparts4_stabilizer_component_t stabilizer;
} sf_acparts4_stabilizer_part_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_head_top_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_head_side_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_core_upper_side_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_core_lower_side_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_arm_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_leg_back_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_leg_upper_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_leg_middle_stabilizer_t;
typedef sf_acparts4_stabilizer_part_t sf_acparts4_leg_lower_stabilizer_t;

SF_API sf_result_t sf_acparts4_create(sf_acparts4_t **out, sf_acparts4_version_t version,
                                      const sf_allocator_t *alloc);
SF_API void sf_acparts4_destroy(sf_acparts4_t *acparts);
SF_API sf_result_t sf_acparts4_read_from_memory(sf_acparts4_t **out, const void *bytes,
                                                size_t size, sf_acparts4_version_t version,
                                                const sf_allocator_t *alloc);
SF_API sf_result_t sf_acparts4_write_to_memory(const sf_acparts4_t *acparts, void **out_bytes,
                                               size_t *out_size,
                                               const sf_allocator_t *alloc);
SF_API sf_acparts4_version_t sf_acparts4_version(const sf_acparts4_t *acparts);
SF_API size_t sf_acparts4_count(const sf_acparts4_t *acparts);

#define SF_ACPARTS4_DECLARE_LIST_API(name, ctype)                                         \
    SF_API size_t sf_acparts4_##name##_count(const sf_acparts4_t *acparts);                \
    SF_API const ctype *sf_acparts4_##name##_data(const sf_acparts4_t *acparts);           \
    SF_API ctype *sf_acparts4_##name##_mutable_data(sf_acparts4_t *acparts);               \
    SF_API sf_result_t sf_acparts4_set_##name##_count(sf_acparts4_t *acparts, size_t count)

SF_ACPARTS4_DECLARE_LIST_API(head, sf_acparts4_head_t);
SF_ACPARTS4_DECLARE_LIST_API(core, sf_acparts4_core_t);
SF_ACPARTS4_DECLARE_LIST_API(arm, sf_acparts4_arm_t);
SF_ACPARTS4_DECLARE_LIST_API(leg, sf_acparts4_leg_t);
SF_ACPARTS4_DECLARE_LIST_API(fcs, sf_acparts4_fcs_t);
SF_ACPARTS4_DECLARE_LIST_API(generator, sf_acparts4_generator_t);
SF_ACPARTS4_DECLARE_LIST_API(main_booster, sf_acparts4_main_booster_t);
SF_ACPARTS4_DECLARE_LIST_API(back_booster, sf_acparts4_back_booster_t);
SF_ACPARTS4_DECLARE_LIST_API(side_booster, sf_acparts4_side_booster_t);
SF_ACPARTS4_DECLARE_LIST_API(overed_booster, sf_acparts4_overed_booster_t);
SF_ACPARTS4_DECLARE_LIST_API(arm_unit, sf_acparts4_arm_unit_t);
SF_ACPARTS4_DECLARE_LIST_API(back_unit, sf_acparts4_back_unit_t);
SF_ACPARTS4_DECLARE_LIST_API(shoulder_unit, sf_acparts4_shoulder_unit_t);
SF_ACPARTS4_DECLARE_LIST_API(head_top_stabilizer, sf_acparts4_head_top_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(head_side_stabilizer, sf_acparts4_head_side_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(core_upper_side_stabilizer, sf_acparts4_core_upper_side_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(core_lower_side_stabilizer, sf_acparts4_core_lower_side_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(arm_stabilizer, sf_acparts4_arm_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(leg_back_stabilizer, sf_acparts4_leg_back_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(leg_upper_stabilizer, sf_acparts4_leg_upper_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(leg_middle_stabilizer, sf_acparts4_leg_middle_stabilizer_t);
SF_ACPARTS4_DECLARE_LIST_API(leg_lower_stabilizer, sf_acparts4_leg_lower_stabilizer_t);

#undef SF_ACPARTS4_DECLARE_LIST_API

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_ACPARTS4_H */
