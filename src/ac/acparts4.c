/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_acparts4.h"
#include "souls_formats/sf_io.h"

#include "internal/sf_internal.h"

#include <string.h>

struct sf_acparts4 {
    const sf_allocator_t *alloc;
    sf_acparts4_version_t version;
#define SF_ACPARTS4_FIELD(name, ctype) ctype *name; size_t name##_count
    SF_ACPARTS4_FIELD(head, sf_acparts4_head_t);
    SF_ACPARTS4_FIELD(core, sf_acparts4_core_t);
    SF_ACPARTS4_FIELD(arm, sf_acparts4_arm_t);
    SF_ACPARTS4_FIELD(leg, sf_acparts4_leg_t);
    SF_ACPARTS4_FIELD(fcs, sf_acparts4_fcs_t);
    SF_ACPARTS4_FIELD(generator, sf_acparts4_generator_t);
    SF_ACPARTS4_FIELD(main_booster, sf_acparts4_main_booster_t);
    SF_ACPARTS4_FIELD(back_booster, sf_acparts4_back_booster_t);
    SF_ACPARTS4_FIELD(side_booster, sf_acparts4_side_booster_t);
    SF_ACPARTS4_FIELD(overed_booster, sf_acparts4_overed_booster_t);
    SF_ACPARTS4_FIELD(arm_unit, sf_acparts4_arm_unit_t);
    SF_ACPARTS4_FIELD(back_unit, sf_acparts4_back_unit_t);
    SF_ACPARTS4_FIELD(shoulder_unit, sf_acparts4_shoulder_unit_t);
    SF_ACPARTS4_FIELD(head_top_stabilizer, sf_acparts4_head_top_stabilizer_t);
    SF_ACPARTS4_FIELD(head_side_stabilizer, sf_acparts4_head_side_stabilizer_t);
    SF_ACPARTS4_FIELD(core_upper_side_stabilizer, sf_acparts4_core_upper_side_stabilizer_t);
    SF_ACPARTS4_FIELD(core_lower_side_stabilizer, sf_acparts4_core_lower_side_stabilizer_t);
    SF_ACPARTS4_FIELD(arm_stabilizer, sf_acparts4_arm_stabilizer_t);
    SF_ACPARTS4_FIELD(leg_back_stabilizer, sf_acparts4_leg_back_stabilizer_t);
    SF_ACPARTS4_FIELD(leg_upper_stabilizer, sf_acparts4_leg_upper_stabilizer_t);
    SF_ACPARTS4_FIELD(leg_middle_stabilizer, sf_acparts4_leg_middle_stabilizer_t);
    SF_ACPARTS4_FIELD(leg_lower_stabilizer, sf_acparts4_leg_lower_stabilizer_t);
#undef SF_ACPARTS4_FIELD
};

static void copy_fixed(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) return;
    memset(dst, 0, dst_size);
    if (!src) return;
    size_t n = strlen(src);
    if (n >= dst_size) n = dst_size - 1u;
    memcpy(dst, src, n);
}

static sf_result_t read_fix(sf_binary_reader_t *r, size_t n, char *dst, size_t dst_size) {
    char *s = NULL;
    sf_result_t e = sf_binary_reader_read_fix_str(r, n, &s, NULL);
    if (e != SF_OK) return e;
    copy_fixed(dst, dst_size, s);
    sf_free(NULL, s);
    return SF_OK;
}

static sf_result_t write_fix(sf_binary_writer_t *w, const char *s, size_t n) {
    return sf_binary_writer_write_fix_str(w, s ? s : "", n, 0x20);
}

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static sf_result_t read_part(sf_binary_reader_t *r, sf_acparts4_version_t v,
                             sf_acparts4_part_component_t *p) {
    TRY(sf_binary_reader_read_u16(r, &p->part_id));
    TRY(sf_binary_reader_read_u16(r, &p->model_id));
    TRY(sf_binary_reader_read_i32(r, &p->price));
    TRY(sf_binary_reader_read_u16(r, &p->weight));
    TRY(sf_binary_reader_read_u16(r, &p->en_cost));
    TRY(sf_binary_reader_read_u8(r, &p->category));
    TRY(sf_binary_reader_read_u8(r, &p->init_status));
    TRY(sf_binary_reader_read_u16(r, &p->cap_id));
    TRY(read_fix(r, 0x20, p->name, sizeof(p->name)));
    TRY(read_fix(r, 0x20, p->maker_name, sizeof(p->maker_name)));
    TRY(read_fix(r, 0x20, p->sub_category, sizeof(p->sub_category)));
    if (v == SF_ACPARTS4_VERSION_ACFA) {
        TRY(sf_binary_reader_read_u16(r, &p->sub_category_id));
        TRY(sf_binary_reader_read_u16(r, &p->set_id));
        TRY(read_fix(r, 0xFC, p->explain, sizeof(p->explain)));
    } else {
        TRY(read_fix(r, 0x100, p->explain, sizeof(p->explain)));
    }
    return SF_OK;
}

static sf_result_t write_part(sf_binary_writer_t *w, sf_acparts4_version_t v,
                              const sf_acparts4_part_component_t *p) {
    TRY(sf_binary_writer_write_u16(w, p->part_id));
    TRY(sf_binary_writer_write_u16(w, p->model_id));
    TRY(sf_binary_writer_write_i32(w, p->price));
    TRY(sf_binary_writer_write_u16(w, p->weight));
    TRY(sf_binary_writer_write_u16(w, p->en_cost));
    TRY(sf_binary_writer_write_u8(w, p->category));
    TRY(sf_binary_writer_write_u8(w, p->init_status));
    TRY(sf_binary_writer_write_u16(w, p->cap_id));
    TRY(write_fix(w, p->name, 0x20));
    TRY(write_fix(w, p->maker_name, 0x20));
    TRY(write_fix(w, p->sub_category, 0x20));
    if (v == SF_ACPARTS4_VERSION_ACFA) {
        TRY(sf_binary_writer_write_u16(w, p->sub_category_id));
        TRY(sf_binary_writer_write_u16(w, p->set_id));
        TRY(write_fix(w, p->explain, 0xFC));
    } else {
        TRY(write_fix(w, p->explain, 0x100));
    }
    return SF_OK;
}

static sf_result_t read_defense(sf_binary_reader_t *r, sf_acparts4_defense_component_t *c) {
    TRY(sf_binary_reader_read_u16(r, &c->ballistic_defense));
    return sf_binary_reader_read_u16(r, &c->energy_defense);
}
static sf_result_t write_defense(sf_binary_writer_t *w, const sf_acparts4_defense_component_t *c) {
    TRY(sf_binary_writer_write_u16(w, c->ballistic_defense));
    return sf_binary_writer_write_u16(w, c->energy_defense);
}
static sf_result_t read_pa(sf_binary_reader_t *r, sf_acparts4_pa_component_t *c) {
    TRY(sf_binary_reader_read_u16(r, &c->pa_rectification));
    return sf_binary_reader_read_u16(r, &c->pa_durability);
}
static sf_result_t write_pa(sf_binary_writer_t *w, const sf_acparts4_pa_component_t *c) {
    TRY(sf_binary_writer_write_u16(w, c->pa_rectification));
    return sf_binary_writer_write_u16(w, c->pa_durability);
}
static sf_result_t read_frame(sf_binary_reader_t *r, sf_acparts4_frame_component_t *c) {
    TRY(sf_binary_reader_read_u16(r, &c->tune_max_rectification));
    TRY(sf_binary_reader_read_u16(r, &c->tune_efficiency_rectification));
    TRY(sf_binary_reader_read_u16(r, &c->ap));
    TRY(sf_binary_reader_read_u16(r, &c->unk06));
    TRY(sf_binary_reader_read_f32(r, &c->drag_coefficient));
    TRY(sf_binary_reader_read_u16(r, &c->weight_balance_front));
    TRY(sf_binary_reader_read_u16(r, &c->weight_balance_back));
    TRY(sf_binary_reader_read_u16(r, &c->weight_balance_right));
    return sf_binary_reader_read_u16(r, &c->weight_balance_left);
}
static sf_result_t write_frame(sf_binary_writer_t *w, const sf_acparts4_frame_component_t *c) {
    TRY(sf_binary_writer_write_u16(w, c->tune_max_rectification));
    TRY(sf_binary_writer_write_u16(w, c->tune_efficiency_rectification));
    TRY(sf_binary_writer_write_u16(w, c->ap));
    TRY(sf_binary_writer_write_u16(w, c->unk06));
    TRY(sf_binary_writer_write_f32(w, c->drag_coefficient));
    TRY(sf_binary_writer_write_u16(w, c->weight_balance_front));
    TRY(sf_binary_writer_write_u16(w, c->weight_balance_back));
    TRY(sf_binary_writer_write_u16(w, c->weight_balance_right));
    return sf_binary_writer_write_u16(w, c->weight_balance_left);
}
static sf_result_t read_booster(sf_binary_reader_t *r, sf_acparts4_booster_component_t *c) {
    TRY(sf_binary_reader_read_u32(r, &c->thrust));
    TRY(sf_binary_reader_read_u32(r, &c->tune_max_thrust));
    TRY(sf_binary_reader_read_u16(r, &c->tune_efficiency_thrust));
    TRY(sf_binary_reader_read_u16(r, &c->quick_boost_duration));
    return sf_binary_reader_read_u32(r, &c->thrust_en_cost);
}
static sf_result_t write_booster(sf_binary_writer_t *w, const sf_acparts4_booster_component_t *c) {
    TRY(sf_binary_writer_write_u32(w, c->thrust));
    TRY(sf_binary_writer_write_u32(w, c->tune_max_thrust));
    TRY(sf_binary_writer_write_u16(w, c->tune_efficiency_thrust));
    TRY(sf_binary_writer_write_u16(w, c->quick_boost_duration));
    return sf_binary_writer_write_u32(w, c->thrust_en_cost);
}
static sf_result_t read_radar(sf_binary_reader_t *r, sf_acparts4_radar_component_t *c) {
    TRY(sf_binary_reader_read_u16(r, &c->radar_range));
    TRY(sf_binary_reader_read_u16(r, &c->ecm_resistance));
    TRY(sf_binary_reader_read_u16(r, &c->tune_max_ecm_resistance));
    TRY(sf_binary_reader_read_u16(r, &c->tune_efficiency_ecm_resistance));
    TRY(sf_binary_reader_read_u16(r, &c->radar_refresh_rate));
    TRY(sf_binary_reader_read_u16(r, &c->tune_max_radar_refresh_rate));
    return sf_binary_reader_read_u16(r, &c->tune_efficiency_radar_refresh_rate);
}
static sf_result_t write_radar(sf_binary_writer_t *w, const sf_acparts4_radar_component_t *c) {
    TRY(sf_binary_writer_write_u16(w, c->radar_range));
    TRY(sf_binary_writer_write_u16(w, c->ecm_resistance));
    TRY(sf_binary_writer_write_u16(w, c->tune_max_ecm_resistance));
    TRY(sf_binary_writer_write_u16(w, c->tune_efficiency_ecm_resistance));
    TRY(sf_binary_writer_write_u16(w, c->radar_refresh_rate));
    TRY(sf_binary_writer_write_u16(w, c->tune_max_radar_refresh_rate));
    return sf_binary_writer_write_u16(w, c->tune_efficiency_radar_refresh_rate);
}
static sf_result_t read_stabilizer_component(sf_binary_reader_t *r,
                                             sf_acparts4_stabilizer_component_t *c) {
    uint8_t z = 0;
    TRY(sf_binary_reader_read_u8(r, &c->category));
    TRY(sf_binary_reader_read_u8(r, &z)); if (z != 0) return SF_ERR_BAD_MAGIC;
    TRY(sf_binary_reader_read_u8(r, &z)); if (z != 0) return SF_ERR_BAD_MAGIC;
    TRY(sf_binary_reader_read_u8(r, &z)); if (z != 0) return SF_ERR_BAD_MAGIC;
    return sf_binary_reader_read_f32(r, &c->control_calibration);
}
static sf_result_t write_stabilizer_component(sf_binary_writer_t *w,
                                              const sf_acparts4_stabilizer_component_t *c) {
    TRY(sf_binary_writer_write_u8(w, c->category));
    TRY(sf_binary_writer_write_u8(w, 0));
    TRY(sf_binary_writer_write_u8(w, 0));
    TRY(sf_binary_writer_write_u8(w, 0));
    return sf_binary_writer_write_f32(w, c->control_calibration);
}
static sf_result_t read_weapon_booster(sf_binary_reader_t *r,
                                       sf_acparts4_weapon_booster_component_t *c) {
    TRY(sf_binary_reader_read_u32(r, &c->horizontal_thrust));
    TRY(sf_binary_reader_read_u32(r, &c->vertical_thrust));
    TRY(sf_binary_reader_read_u32(r, &c->quick_boost));
    TRY(sf_binary_reader_read_u32(r, &c->unk0c_thrust));
    TRY(sf_binary_reader_read_u32(r, &c->horizontal_en_cost));
    TRY(sf_binary_reader_read_u32(r, &c->vertical_en_cost));
    TRY(sf_binary_reader_read_u32(r, &c->quick_boost_en_cost));
    return sf_binary_reader_read_u32(r, &c->unk0c_en_cost);
}
static sf_result_t write_weapon_booster(sf_binary_writer_t *w,
                                        const sf_acparts4_weapon_booster_component_t *c) {
    TRY(sf_binary_writer_write_u32(w, c->horizontal_thrust));
    TRY(sf_binary_writer_write_u32(w, c->vertical_thrust));
    TRY(sf_binary_writer_write_u32(w, c->quick_boost));
    TRY(sf_binary_writer_write_u32(w, c->unk0c_thrust));
    TRY(sf_binary_writer_write_u32(w, c->horizontal_en_cost));
    TRY(sf_binary_writer_write_u32(w, c->vertical_en_cost));
    TRY(sf_binary_writer_write_u32(w, c->quick_boost_en_cost));
    return sf_binary_writer_write_u32(w, c->unk0c_en_cost);
}
static sf_result_t read_weapon(sf_binary_reader_t *r, sf_acparts4_weapon_component_t *c) {
    TRY(sf_binary_reader_read_u8(r, &c->weapon_firing_mode));
    TRY(sf_binary_reader_read_bool(r, &c->can_lock_on));
    TRY(sf_binary_reader_read_u8(r, &c->missile_lock_time));
    TRY(sf_binary_reader_read_bool(r, &c->unk03));
    TRY(sf_binary_reader_read_u16(r, &c->firing_range));
    TRY(sf_binary_reader_read_u16(r, &c->melee_ability));
    TRY(sf_binary_reader_read_u32(r, &c->bullet_id));
    TRY(sf_binary_reader_read_u32(r, &c->sfx_id));
    TRY(sf_binary_reader_read_u32(r, &c->hit_effect_id));
    TRY(sf_binary_reader_read_f32(r, &c->ballistics_velocity));
    TRY(sf_binary_reader_read_f32(r, &c->en_cost));
    TRY(sf_binary_reader_read_bool(r, &c->multi_proc));
    TRY(sf_binary_reader_read_u8(r, &c->projectile_count));
    TRY(sf_binary_reader_read_u8(r, &c->continuous_fire_count));
    TRY(sf_binary_reader_read_u8(r, &c->unk1f));
    TRY(sf_binary_reader_read_u16(r, &c->unk20));
    TRY(sf_binary_reader_read_u16(r, &c->auto_interval));
    TRY(sf_binary_reader_read_u16(r, &c->fire_rate));
    TRY(sf_binary_reader_read_u16(r, &c->recoil));
    TRY(sf_binary_reader_read_u16(r, &c->cost_per_round));
    TRY(sf_binary_reader_read_u16(r, &c->shot_precision));
    TRY(sf_binary_reader_read_u16(r, &c->number_of_magazines));
    TRY(sf_binary_reader_read_u16(r, &c->magazine_capacity));
    TRY(sf_binary_reader_read_u16(r, &c->magazine_reload_time));
    TRY(sf_binary_reader_read_u16(r, &c->weapon_type));
    TRY(sf_binary_reader_read_u16(r, &c->charge_time));
    TRY(sf_binary_reader_read_u16(r, &c->kp_charge_cost));
    TRY(sf_binary_reader_read_f32(r, &c->kojima_max_damage_rate));
    TRY(sf_binary_reader_read_u16(r, &c->attack_latency));
    TRY(sf_binary_reader_read_u16(r, &c->unk3e));
    TRY(sf_binary_reader_read_u8(r, &c->damage_type));
    TRY(sf_binary_reader_read_bool(r, &c->damage_pierce));
    TRY(sf_binary_reader_read_bool(r, &c->unk41));
    TRY(sf_binary_reader_read_bool(r, &c->damage_radial));
    TRY(sf_binary_reader_read_f32(r, &c->attack_power));
    TRY(sf_binary_reader_read_f32(r, &c->impact_force));
    TRY(sf_binary_reader_read_f32(r, &c->pa_attentuation));
    return sf_binary_reader_read_f32(r, &c->pa_penetration);
}
static sf_result_t write_weapon(sf_binary_writer_t *w, const sf_acparts4_weapon_component_t *c) {
    TRY(sf_binary_writer_write_u8(w, c->weapon_firing_mode));
    TRY(sf_binary_writer_write_bool(w, c->can_lock_on));
    TRY(sf_binary_writer_write_u8(w, c->missile_lock_time));
    TRY(sf_binary_writer_write_bool(w, c->unk03));
    TRY(sf_binary_writer_write_u16(w, c->firing_range));
    TRY(sf_binary_writer_write_u16(w, c->melee_ability));
    TRY(sf_binary_writer_write_u32(w, c->bullet_id));
    TRY(sf_binary_writer_write_u32(w, c->sfx_id));
    TRY(sf_binary_writer_write_u32(w, c->hit_effect_id));
    TRY(sf_binary_writer_write_f32(w, c->ballistics_velocity));
    TRY(sf_binary_writer_write_f32(w, c->en_cost));
    TRY(sf_binary_writer_write_bool(w, c->multi_proc));
    TRY(sf_binary_writer_write_u8(w, c->projectile_count));
    TRY(sf_binary_writer_write_u8(w, c->continuous_fire_count));
    TRY(sf_binary_writer_write_u8(w, c->unk1f));
    TRY(sf_binary_writer_write_u16(w, c->unk20));
    TRY(sf_binary_writer_write_u16(w, c->auto_interval));
    TRY(sf_binary_writer_write_u16(w, c->fire_rate));
    TRY(sf_binary_writer_write_u16(w, c->recoil));
    TRY(sf_binary_writer_write_u16(w, c->cost_per_round));
    TRY(sf_binary_writer_write_u16(w, c->shot_precision));
    TRY(sf_binary_writer_write_u16(w, c->number_of_magazines));
    TRY(sf_binary_writer_write_u16(w, c->magazine_capacity));
    TRY(sf_binary_writer_write_u16(w, c->magazine_reload_time));
    TRY(sf_binary_writer_write_u16(w, c->weapon_type));
    TRY(sf_binary_writer_write_u16(w, c->charge_time));
    TRY(sf_binary_writer_write_u16(w, c->kp_charge_cost));
    TRY(sf_binary_writer_write_f32(w, c->kojima_max_damage_rate));
    TRY(sf_binary_writer_write_u16(w, c->attack_latency));
    TRY(sf_binary_writer_write_u16(w, c->unk3e));
    TRY(sf_binary_writer_write_u8(w, c->damage_type));
    TRY(sf_binary_writer_write_bool(w, c->damage_pierce));
    TRY(sf_binary_writer_write_bool(w, c->unk41));
    TRY(sf_binary_writer_write_bool(w, c->damage_radial));
    TRY(sf_binary_writer_write_f32(w, c->attack_power));
    TRY(sf_binary_writer_write_f32(w, c->impact_force));
    TRY(sf_binary_writer_write_f32(w, c->pa_attentuation));
    return sf_binary_writer_write_f32(w, c->pa_penetration);
}

static sf_result_t read_head(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_head_t *p) {
    TRY(read_part(r, v, &p->part)); TRY(read_defense(r, &p->defense)); TRY(read_pa(r, &p->pa)); TRY(read_frame(r, &p->frame));
    TRY(sf_binary_reader_read_u16(r, &p->stability)); TRY(sf_binary_reader_read_u16(r, &p->sfx_monoeye)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_stability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_stability));
    if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_reader_read_u16(r, &p->camera_functionality)); TRY(sf_binary_reader_read_u16(r, &p->system_recovery)); TRY(sf_binary_reader_read_u16(r, &p->unk28)); TRY(sf_binary_reader_read_u16(r, &p->unk2a)); }
    TRY(sf_binary_reader_read_i16(r, &p->stabilizer_top_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_top_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_side_x)); return sf_binary_reader_read_i16(r, &p->stabilizer_side_y);
}
static sf_result_t write_head(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_head_t *p) {
    TRY(write_part(w, v, &p->part)); TRY(write_defense(w, &p->defense)); TRY(write_pa(w, &p->pa)); TRY(write_frame(w, &p->frame));
    TRY(sf_binary_writer_write_u16(w, p->stability)); TRY(sf_binary_writer_write_u16(w, p->sfx_monoeye)); TRY(sf_binary_writer_write_u16(w, p->tune_max_stability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_stability));
    if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_writer_write_u16(w, p->camera_functionality)); TRY(sf_binary_writer_write_u16(w, p->system_recovery)); TRY(sf_binary_writer_write_u16(w, p->unk28)); TRY(sf_binary_writer_write_u16(w, p->unk2a)); }
    TRY(sf_binary_writer_write_i16(w, p->stabilizer_top_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_top_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_side_x)); return sf_binary_writer_write_i16(w, p->stabilizer_side_y);
}
static sf_result_t read_core(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_core_t *p) {
    TRY(read_part(r, v, &p->part)); TRY(read_defense(r, &p->defense)); TRY(read_pa(r, &p->pa)); TRY(read_frame(r, &p->frame));
    if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_reader_read_u32(r, &p->hunger_unit)); TRY(sf_binary_reader_read_u32(r, &p->unk20)); } else { TRY(sf_binary_reader_read_u16(r, &p->unk1c)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_unk1c)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_unk1c)); TRY(sf_binary_reader_read_u16(r, &p->stability)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_stability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_stability)); }
    TRY(sf_binary_reader_read_i16(r, &p->stabilizer_up_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_up_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_low_x)); return sf_binary_reader_read_i16(r, &p->stabilizer_low_y);
}
static sf_result_t write_core(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_core_t *p) {
    TRY(write_part(w, v, &p->part)); TRY(write_defense(w, &p->defense)); TRY(write_pa(w, &p->pa)); TRY(write_frame(w, &p->frame));
    if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_writer_write_u32(w, p->hunger_unit)); TRY(sf_binary_writer_write_u32(w, p->unk20)); } else { TRY(sf_binary_writer_write_u16(w, p->unk1c)); TRY(sf_binary_writer_write_u16(w, p->tune_max_unk1c)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_unk1c)); TRY(sf_binary_writer_write_u16(w, p->stability)); TRY(sf_binary_writer_write_u16(w, p->tune_max_stability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_stability)); }
    TRY(sf_binary_writer_write_i16(w, p->stabilizer_up_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_up_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_low_x)); return sf_binary_writer_write_i16(w, p->stabilizer_low_y);
}
static sf_result_t read_arm(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_arm_t *p) {
    TRY(read_part(r, v, &p->part)); TRY(read_defense(r, &p->defense)); TRY(read_pa(r, &p->pa)); TRY(read_frame(r, &p->frame));
    TRY(sf_binary_reader_read_bool(r, &p->is_weapon_arm)); TRY(sf_binary_reader_read_u8(r, &p->unk1d)); TRY(sf_binary_reader_read_u16(r, &p->firing_stability)); TRY(sf_binary_reader_read_u16(r, &p->energy_weapon_skill)); TRY(sf_binary_reader_read_u16(r, &p->weapon_maneuverability)); TRY(sf_binary_reader_read_u16(r, &p->aim_precision)); TRY(sf_binary_reader_read_u8(r, &p->unk26)); TRY(sf_binary_reader_read_u8(r, &p->unk27)); TRY(sf_binary_reader_read_u16(r, &p->unk28)); TRY(sf_binary_reader_read_u16(r, &p->unk2a));
    TRY(sf_binary_reader_read_u16(r, &p->tune_max_firing_stability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_firing_stability)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_energy_weapon_skill)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_energy_weapon_skill)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_weapon_maneuverability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_weapon_maneuverability)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_aim_precision)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_aim_precision));
    TRY(sf_binary_reader_read_u8(r, &p->display_type)); TRY(sf_binary_reader_read_u8(r, &p->unk3d)); TRY(sf_binary_reader_read_u8(r, &p->unk3e)); TRY(sf_binary_reader_read_u8(r, &p->unk3f)); TRY(read_fix(r, 0x10, p->aim_type, sizeof(p->aim_type))); TRY(read_weapon(r, &p->weapon)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_x)); return sf_binary_reader_read_i16(r, &p->stabilizer_y);
}
static sf_result_t write_arm(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_arm_t *p) {
    TRY(write_part(w, v, &p->part)); TRY(write_defense(w, &p->defense)); TRY(write_pa(w, &p->pa)); TRY(write_frame(w, &p->frame));
    TRY(sf_binary_writer_write_bool(w, p->is_weapon_arm)); TRY(sf_binary_writer_write_u8(w, p->unk1d)); TRY(sf_binary_writer_write_u16(w, p->firing_stability)); TRY(sf_binary_writer_write_u16(w, p->energy_weapon_skill)); TRY(sf_binary_writer_write_u16(w, p->weapon_maneuverability)); TRY(sf_binary_writer_write_u16(w, p->aim_precision)); TRY(sf_binary_writer_write_u8(w, p->unk26)); TRY(sf_binary_writer_write_u8(w, p->unk27)); TRY(sf_binary_writer_write_u16(w, p->unk28)); TRY(sf_binary_writer_write_u16(w, p->unk2a));
    TRY(sf_binary_writer_write_u16(w, p->tune_max_firing_stability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_firing_stability)); TRY(sf_binary_writer_write_u16(w, p->tune_max_energy_weapon_skill)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_energy_weapon_skill)); TRY(sf_binary_writer_write_u16(w, p->tune_max_weapon_maneuverability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_weapon_maneuverability)); TRY(sf_binary_writer_write_u16(w, p->tune_max_aim_precision)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_aim_precision));
    TRY(sf_binary_writer_write_u8(w, p->display_type)); TRY(sf_binary_writer_write_u8(w, p->unk3d)); TRY(sf_binary_writer_write_u8(w, p->unk3e)); TRY(sf_binary_writer_write_u8(w, p->unk3f)); TRY(write_fix(w, p->aim_type, 0x10)); TRY(write_weapon(w, &p->weapon)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_x)); return sf_binary_writer_write_i16(w, p->stabilizer_y);
}

static sf_result_t read_leg(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_leg_t *p) {
    TRY(read_part(r, v, &p->part)); TRY(read_defense(r, &p->defense)); TRY(read_pa(r, &p->pa)); TRY(read_frame(r, &p->frame));
    TRY(sf_binary_reader_read_u8(r, &p->type)); TRY(sf_binary_reader_read_u8(r, &p->motion)); TRY(sf_binary_reader_read_u16(r, &p->unk1e)); TRY(sf_binary_reader_read_u16(r, &p->max_load)); TRY(sf_binary_reader_read_u16(r, &p->load)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_load)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_load));
    TRY(sf_binary_reader_read_i32(r, &p->back_unit_angle)); TRY(sf_binary_reader_read_i32(r, &p->movement_ability)); TRY(sf_binary_reader_read_i32(r, &p->turning_ability)); TRY(sf_binary_reader_read_i32(r, &p->braking_ability)); TRY(sf_binary_reader_read_i32(r, &p->jumping_ability)); TRY(sf_binary_reader_read_i32(r, &p->landing_stability)); TRY(sf_binary_reader_read_i32(r, &p->hit_stability)); TRY(sf_binary_reader_read_i32(r, &p->shoot_stability));
    TRY(sf_binary_reader_read_i32(r, &p->tune_max_movement_ability)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_turning_ability)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_braking_ability)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_jumping_ability)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_landing_stability)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_hit_stability)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_shoot_stability));
    TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_movement_ability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_turning_ability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_braking_ability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_jumping_ability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_landing_stability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_hit_stability)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_shoot_stability)); TRY(sf_binary_reader_read_u16(r, &p->unk70));
    TRY(read_booster(r, &p->horizontal_boost)); TRY(read_booster(r, &p->vertical_boost)); TRY(read_booster(r, &p->quick_boost));
    if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_reader_read_u8(r, &p->unk_a4)); TRY(sf_binary_reader_read_u8(r, &p->unk_a5)); TRY(sf_binary_reader_read_i16(r, &p->unk_a6)); }
    TRY(sf_binary_reader_read_i16(r, &p->stabilizer_back_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_back_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_up_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_up_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_mid_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_mid_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_low_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_low_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_up_right_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_up_right_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_mid_right_x)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_mid_right_y)); TRY(sf_binary_reader_read_i16(r, &p->stabilizer_low_right_x)); return sf_binary_reader_read_i16(r, &p->stabilizer_low_right_y);
}
static sf_result_t write_leg(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_leg_t *p) {
    TRY(write_part(w, v, &p->part)); TRY(write_defense(w, &p->defense)); TRY(write_pa(w, &p->pa)); TRY(write_frame(w, &p->frame));
    TRY(sf_binary_writer_write_u8(w, p->type)); TRY(sf_binary_writer_write_u8(w, p->motion)); TRY(sf_binary_writer_write_u16(w, p->unk1e)); TRY(sf_binary_writer_write_u16(w, p->max_load)); TRY(sf_binary_writer_write_u16(w, p->load)); TRY(sf_binary_writer_write_u16(w, p->tune_max_load)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_load));
    TRY(sf_binary_writer_write_i32(w, p->back_unit_angle)); TRY(sf_binary_writer_write_i32(w, p->movement_ability)); TRY(sf_binary_writer_write_i32(w, p->turning_ability)); TRY(sf_binary_writer_write_i32(w, p->braking_ability)); TRY(sf_binary_writer_write_i32(w, p->jumping_ability)); TRY(sf_binary_writer_write_i32(w, p->landing_stability)); TRY(sf_binary_writer_write_i32(w, p->hit_stability)); TRY(sf_binary_writer_write_i32(w, p->shoot_stability));
    TRY(sf_binary_writer_write_i32(w, p->tune_max_movement_ability)); TRY(sf_binary_writer_write_i32(w, p->tune_max_turning_ability)); TRY(sf_binary_writer_write_i32(w, p->tune_max_braking_ability)); TRY(sf_binary_writer_write_i32(w, p->tune_max_jumping_ability)); TRY(sf_binary_writer_write_i32(w, p->tune_max_landing_stability)); TRY(sf_binary_writer_write_i32(w, p->tune_max_hit_stability)); TRY(sf_binary_writer_write_i32(w, p->tune_max_shoot_stability));
    TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_movement_ability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_turning_ability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_braking_ability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_jumping_ability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_landing_stability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_hit_stability)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_shoot_stability)); TRY(sf_binary_writer_write_u16(w, p->unk70));
    TRY(write_booster(w, &p->horizontal_boost)); TRY(write_booster(w, &p->vertical_boost)); TRY(write_booster(w, &p->quick_boost));
    if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_writer_write_u8(w, p->unk_a4)); TRY(sf_binary_writer_write_u8(w, p->unk_a5)); TRY(sf_binary_writer_write_i16(w, p->unk_a6)); }
    TRY(sf_binary_writer_write_i16(w, p->stabilizer_back_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_back_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_up_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_up_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_mid_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_mid_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_low_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_low_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_up_right_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_up_right_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_mid_right_x)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_mid_right_y)); TRY(sf_binary_writer_write_i16(w, p->stabilizer_low_right_x)); return sf_binary_writer_write_i16(w, p->stabilizer_low_right_y);
}

static sf_result_t read_fcs(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_fcs_t *p) {(void)v; TRY(read_part(r, v, &p->part)); TRY(sf_binary_reader_read_u8(r, &p->deflect)); TRY(sf_binary_reader_read_u8(r, &p->lock_target_max)); TRY(sf_binary_reader_read_u16(r, &p->blade_lock_distance)); TRY(sf_binary_reader_read_u16(r, &p->parallel_processing)); TRY(sf_binary_reader_read_u16(r, &p->visibility)); TRY(sf_binary_reader_read_u16(r, &p->lock_distance)); TRY(sf_binary_reader_read_u16(r, &p->lock_box_height)); TRY(sf_binary_reader_read_u16(r, &p->lock_box_width)); TRY(sf_binary_reader_read_u16(r, &p->unk_lock_range4)); TRY(sf_binary_reader_read_u16(r, &p->second_lock_time)); TRY(sf_binary_reader_read_u16(r, &p->missile_lock_speed)); TRY(sf_binary_reader_read_bool(r, &p->multi_lock)); TRY(sf_binary_reader_read_u8(r, &p->unk15)); TRY(sf_binary_reader_read_u16(r, &p->unk16)); TRY(sf_binary_reader_read_u16(r, &p->zoom_range)); TRY(read_radar(r, &p->radar)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_second_lock_time)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_second_lock_time)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_missile_lock_speed)); return sf_binary_reader_read_u16(r, &p->tune_efficiency_missile_lock_speed);}
static sf_result_t write_fcs(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_fcs_t *p) {TRY(write_part(w, v, &p->part)); TRY(sf_binary_writer_write_u8(w, p->deflect)); TRY(sf_binary_writer_write_u8(w, p->lock_target_max)); TRY(sf_binary_writer_write_u16(w, p->blade_lock_distance)); TRY(sf_binary_writer_write_u16(w, p->parallel_processing)); TRY(sf_binary_writer_write_u16(w, p->visibility)); TRY(sf_binary_writer_write_u16(w, p->lock_distance)); TRY(sf_binary_writer_write_u16(w, p->lock_box_height)); TRY(sf_binary_writer_write_u16(w, p->lock_box_width)); TRY(sf_binary_writer_write_u16(w, p->unk_lock_range4)); TRY(sf_binary_writer_write_u16(w, p->second_lock_time)); TRY(sf_binary_writer_write_u16(w, p->missile_lock_speed)); TRY(sf_binary_writer_write_bool(w, p->multi_lock)); TRY(sf_binary_writer_write_u8(w, p->unk15)); TRY(sf_binary_writer_write_u16(w, p->unk16)); TRY(sf_binary_writer_write_u16(w, p->zoom_range)); TRY(write_radar(w, &p->radar)); TRY(sf_binary_writer_write_u16(w, p->tune_max_second_lock_time)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_second_lock_time)); TRY(sf_binary_writer_write_u16(w, p->tune_max_missile_lock_speed)); return sf_binary_writer_write_u16(w, p->tune_efficiency_missile_lock_speed);}
static sf_result_t read_generator(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_generator_t *p) {(void)v; TRY(read_part(r, v, &p->part)); TRY(sf_binary_reader_read_i32(r, &p->energy_capacity)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_energy_capacity)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_energy_capacity)); TRY(sf_binary_reader_read_u16(r, &p->unk0a)); TRY(sf_binary_reader_read_i32(r, &p->energy_output_soft_limit)); TRY(sf_binary_reader_read_i32(r, &p->energy_output)); TRY(sf_binary_reader_read_i32(r, &p->tune_max_energy_output)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_energy_output)); TRY(sf_binary_reader_read_u16(r, &p->kp_output)); TRY(sf_binary_reader_read_u16(r, &p->tune_max_kp_output)); TRY(sf_binary_reader_read_u16(r, &p->tune_efficiency_kp_output)); TRY(sf_binary_reader_read_u16(r, &p->active_se)); return sf_binary_reader_read_u16(r, &p->unk22);}
static sf_result_t write_generator(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_generator_t *p) {TRY(write_part(w, v, &p->part)); TRY(sf_binary_writer_write_i32(w, p->energy_capacity)); TRY(sf_binary_writer_write_i32(w, p->tune_max_energy_capacity)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_energy_capacity)); TRY(sf_binary_writer_write_u16(w, p->unk0a)); TRY(sf_binary_writer_write_i32(w, p->energy_output_soft_limit)); TRY(sf_binary_writer_write_i32(w, p->energy_output)); TRY(sf_binary_writer_write_i32(w, p->tune_max_energy_output)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_energy_output)); TRY(sf_binary_writer_write_u16(w, p->kp_output)); TRY(sf_binary_writer_write_u16(w, p->tune_max_kp_output)); TRY(sf_binary_writer_write_u16(w, p->tune_efficiency_kp_output)); TRY(sf_binary_writer_write_u16(w, p->active_se)); return sf_binary_writer_write_u16(w, p->unk22);}
static sf_result_t read_main_booster(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_main_booster_t *p) {TRY(read_part(r, v, &p->part)); TRY(read_booster(r, &p->horizontal_boost)); TRY(read_booster(r, &p->vertical_boost)); TRY(read_booster(r, &p->quick_boost)); if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_reader_read_u8(r, &p->quick_reload_time)); TRY(sf_binary_reader_read_u8(r, &p->unk31)); TRY(sf_binary_reader_read_u16(r, &p->unk32)); } return SF_OK;}
static sf_result_t write_main_booster(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_main_booster_t *p) {TRY(write_part(w, v, &p->part)); TRY(write_booster(w, &p->horizontal_boost)); TRY(write_booster(w, &p->vertical_boost)); TRY(write_booster(w, &p->quick_boost)); if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_writer_write_u8(w, p->quick_reload_time)); TRY(sf_binary_writer_write_u8(w, p->unk31)); TRY(sf_binary_writer_write_u16(w, p->unk32)); } return SF_OK;}
static sf_result_t read_back_booster(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_back_booster_t *p) {TRY(read_part(r, v, &p->part)); TRY(read_booster(r, &p->horizontal_boost)); TRY(read_booster(r, &p->quick_boost)); if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_reader_read_u8(r, &p->quick_reload_time)); TRY(sf_binary_reader_read_u8(r, &p->unk31)); TRY(sf_binary_reader_read_u16(r, &p->unk32)); } return SF_OK;}
static sf_result_t write_back_booster(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_back_booster_t *p) {TRY(write_part(w, v, &p->part)); TRY(write_booster(w, &p->horizontal_boost)); TRY(write_booster(w, &p->quick_boost)); if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(sf_binary_writer_write_u8(w, p->quick_reload_time)); TRY(sf_binary_writer_write_u8(w, p->unk31)); TRY(sf_binary_writer_write_u16(w, p->unk32)); } return SF_OK;}
static sf_result_t read_overed_booster(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_overed_booster_t *p) {TRY(read_part(r, v, &p->part)); TRY(read_booster(r, &p->horizontal_boost)); TRY(sf_binary_reader_read_u16(r, &p->overed_boost_kp_cost)); TRY(sf_binary_reader_read_u16(r, &p->unk12)); TRY(sf_binary_reader_read_u32(r, &p->prepare_en_cost)); TRY(sf_binary_reader_read_u32(r, &p->prepare_kp_cost)); TRY(sf_binary_reader_read_u32(r, &p->ob_activation_thrust)); TRY(sf_binary_reader_read_u32(r, &p->ob_activation_en_cost)); TRY(sf_binary_reader_read_u32(r, &p->ob_activation_kp_cost)); TRY(sf_binary_reader_read_u32(r, &p->ob_activation_limit)); if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_reader_read_u16(r, &p->sfx_overboost_charge)); TRY(sf_binary_reader_read_u16(r, &p->sfx_overboost_launch)); } else { TRY(sf_binary_reader_read_u32(r, &p->unk2c)); TRY(sf_binary_reader_read_u16(r, &p->assault_armor_attack_power)); TRY(sf_binary_reader_read_u16(r, &p->assault_armor_range)); TRY(sf_binary_reader_read_u32(r, &p->unk34)); TRY(sf_binary_reader_read_u32(r, &p->unk38)); TRY(sf_binary_reader_read_u32(r, &p->unk3c)); } return SF_OK;}
static sf_result_t write_overed_booster(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_overed_booster_t *p) {TRY(write_part(w, v, &p->part)); TRY(write_booster(w, &p->horizontal_boost)); TRY(sf_binary_writer_write_u16(w, p->overed_boost_kp_cost)); TRY(sf_binary_writer_write_u16(w, p->unk12)); TRY(sf_binary_writer_write_u32(w, p->prepare_en_cost)); TRY(sf_binary_writer_write_u32(w, p->prepare_kp_cost)); TRY(sf_binary_writer_write_u32(w, p->ob_activation_thrust)); TRY(sf_binary_writer_write_u32(w, p->ob_activation_en_cost)); TRY(sf_binary_writer_write_u32(w, p->ob_activation_kp_cost)); TRY(sf_binary_writer_write_u32(w, p->ob_activation_limit)); if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_writer_write_u16(w, p->sfx_overboost_charge)); TRY(sf_binary_writer_write_u16(w, p->sfx_overboost_launch)); } else { TRY(sf_binary_writer_write_u32(w, p->unk2c)); TRY(sf_binary_writer_write_u16(w, p->assault_armor_attack_power)); TRY(sf_binary_writer_write_u16(w, p->assault_armor_range)); TRY(sf_binary_writer_write_u32(w, p->unk34)); TRY(sf_binary_writer_write_u32(w, p->unk38)); TRY(sf_binary_writer_write_u32(w, p->unk3c)); } return SF_OK;}
static sf_result_t read_arm_unit(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_arm_unit_t *p) {TRY(read_part(r, v, &p->part)); TRY(read_weapon(r, &p->weapon)); TRY(sf_binary_reader_read_u8(r, &p->hanger_requirement)); TRY(sf_binary_reader_read_u8(r, &p->display_type)); return sf_binary_reader_read_u16(r, &p->unk56);}
static sf_result_t write_arm_unit(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_arm_unit_t *p) {TRY(write_part(w, v, &p->part)); TRY(write_weapon(w, &p->weapon)); TRY(sf_binary_writer_write_u8(w, p->hanger_requirement)); TRY(sf_binary_writer_write_u8(w, p->display_type)); return sf_binary_writer_write_u16(w, p->unk56);}
static sf_result_t read_back_unit(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_back_unit_t *p) {TRY(read_part(r, v, &p->part)); TRY(read_weapon(r, &p->weapon)); TRY(sf_binary_reader_read_u8(r, &p->unk54)); TRY(sf_binary_reader_read_u8(r, &p->unk55)); TRY(sf_binary_reader_read_u16(r, &p->unk56)); TRY(sf_binary_reader_read_u16(r, &p->unk58)); TRY(read_radar(r, &p->radar)); if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(read_weapon_booster(r, &p->weapon_booster)); TRY(sf_binary_reader_read_u16(r, &p->assault_cannon_attack_power)); TRY(sf_binary_reader_read_u16(r, &p->unk8a)); TRY(sf_binary_reader_read_u16(r, &p->assault_cannon_impact)); TRY(sf_binary_reader_read_u16(r, &p->assault_cannon_attentuation)); TRY(sf_binary_reader_read_u16(r, &p->assault_cannon_penetration)); TRY(sf_binary_reader_read_u16(r, &p->unk92)); } TRY(sf_binary_reader_read_u8(r, &p->type)); TRY(sf_binary_reader_read_u8(r, &p->display_type)); if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_reader_read_u8(r, &p->unk6a)); TRY(sf_binary_reader_read_u8(r, &p->unk6b)); } else { TRY(sf_binary_reader_read_bool(r, &p->takes_both_slots)); TRY(sf_binary_reader_read_u8(r, &p->unk97)); } return read_pa(r, &p->pa);}
static sf_result_t write_back_unit(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_back_unit_t *p) {TRY(write_part(w, v, &p->part)); TRY(write_weapon(w, &p->weapon)); TRY(sf_binary_writer_write_u8(w, p->unk54)); TRY(sf_binary_writer_write_u8(w, p->unk55)); TRY(sf_binary_writer_write_u16(w, p->unk56)); TRY(sf_binary_writer_write_u16(w, p->unk58)); TRY(write_radar(w, &p->radar)); if (v == SF_ACPARTS4_VERSION_ACFA) { TRY(write_weapon_booster(w, &p->weapon_booster)); TRY(sf_binary_writer_write_u16(w, p->assault_cannon_attack_power)); TRY(sf_binary_writer_write_u16(w, p->unk8a)); TRY(sf_binary_writer_write_u16(w, p->assault_cannon_impact)); TRY(sf_binary_writer_write_u16(w, p->assault_cannon_attentuation)); TRY(sf_binary_writer_write_u16(w, p->assault_cannon_penetration)); TRY(sf_binary_writer_write_u16(w, p->unk92)); } TRY(sf_binary_writer_write_u8(w, p->type)); TRY(sf_binary_writer_write_u8(w, p->display_type)); if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_writer_write_u8(w, p->unk6a)); TRY(sf_binary_writer_write_u8(w, p->unk6b)); } else { TRY(sf_binary_writer_write_bool(w, p->takes_both_slots)); TRY(sf_binary_writer_write_u8(w, p->unk97)); } return write_pa(w, &p->pa);}
static sf_result_t read_shoulder_unit(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_shoulder_unit_t *p) {TRY(read_part(r, v, &p->part)); TRY(sf_binary_reader_read_u8(r, &p->type)); TRY(sf_binary_reader_read_bool(r, &p->is_weapon)); TRY(sf_binary_reader_read_u8(r, &p->display_type)); TRY(sf_binary_reader_read_u8(r, &p->unk03)); TRY(read_pa(r, &p->pa)); TRY(read_fix(r, 0x10, p->device_name, sizeof(p->device_name))); TRY(sf_binary_reader_read_u16(r, &p->use_count)); TRY(sf_binary_reader_read_u16(r, &p->effect_duration)); TRY(sf_binary_reader_read_u16(r, &p->reload_frame)); TRY(sf_binary_reader_read_u16(r, &p->unk1e)); if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_reader_read_f32(r, &p->effect_param_0)); TRY(sf_binary_reader_read_f32(r, &p->effect_param_1)); } else { TRY(sf_binary_reader_read_f32(r, &p->aa_attack_power)); TRY(sf_binary_reader_read_f32(r, &p->aa_range_boost)); } TRY(read_weapon(r, &p->weapon)); if (v == SF_ACPARTS4_VERSION_ACFA) TRY(read_weapon_booster(r, &p->weapon_booster)); return SF_OK;}
static sf_result_t write_shoulder_unit(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_shoulder_unit_t *p) {TRY(write_part(w, v, &p->part)); TRY(sf_binary_writer_write_u8(w, p->type)); TRY(sf_binary_writer_write_bool(w, p->is_weapon)); TRY(sf_binary_writer_write_u8(w, p->display_type)); TRY(sf_binary_writer_write_u8(w, p->unk03)); TRY(write_pa(w, &p->pa)); TRY(write_fix(w, p->device_name, 0x10)); TRY(sf_binary_writer_write_u16(w, p->use_count)); TRY(sf_binary_writer_write_u16(w, p->effect_duration)); TRY(sf_binary_writer_write_u16(w, p->reload_frame)); TRY(sf_binary_writer_write_u16(w, p->unk1e)); if (v == SF_ACPARTS4_VERSION_AC4) { TRY(sf_binary_writer_write_f32(w, p->effect_param_0)); TRY(sf_binary_writer_write_f32(w, p->effect_param_1)); } else { TRY(sf_binary_writer_write_f32(w, p->aa_attack_power)); TRY(sf_binary_writer_write_f32(w, p->aa_range_boost)); } TRY(write_weapon(w, &p->weapon)); if (v == SF_ACPARTS4_VERSION_ACFA) TRY(write_weapon_booster(w, &p->weapon_booster)); return SF_OK;}
static sf_result_t read_stabilizer_part(sf_binary_reader_t *r, sf_acparts4_version_t v, sf_acparts4_stabilizer_part_t *p) {TRY(read_part(r, v, &p->part)); return read_stabilizer_component(r, &p->stabilizer);}
static sf_result_t write_stabilizer_part(sf_binary_writer_t *w, sf_acparts4_version_t v, const sf_acparts4_stabilizer_part_t *p) {TRY(write_part(w, v, &p->part)); return write_stabilizer_component(w, &p->stabilizer);}

static sf_result_t set_array(const sf_allocator_t *a, void **ptr, size_t *old_count,
                             size_t count, size_t elem_size) {
    if (count > UINT16_MAX) return SF_ERR_OUT_OF_RANGE;
    sf_xfree(a, *ptr);
    *ptr = NULL;
    *old_count = 0;
    if (count == 0) return SF_OK;
    if (elem_size != 0 && count > ((size_t)-1) / elem_size) return SF_ERR_OUT_OF_RANGE;
    void *p = sf_xalloc(a, count * elem_size);
    if (!p) return SF_ERR_OOM;
    memset(p, 0, count * elem_size);
    *ptr = p;
    *old_count = count;
    return SF_OK;
}

sf_result_t sf_acparts4_create(sf_acparts4_t **out, sf_acparts4_version_t version,
                               const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(version == SF_ACPARTS4_VERSION_AC4 || version == SF_ACPARTS4_VERSION_ACFA);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_acparts4_t *a = (sf_acparts4_t *)sf_xalloc(alloc, sizeof(*a));
    if (!a) return SF_ERR_OOM;
    memset(a, 0, sizeof(*a));
    a->alloc = alloc;
    a->version = version;
    *out = a;
    return SF_OK;
}

void sf_acparts4_destroy(sf_acparts4_t *a) {
    if (!a) return;
#define FREE_LIST(name) sf_xfree(a->alloc, a->name)
    FREE_LIST(head); FREE_LIST(core); FREE_LIST(arm); FREE_LIST(leg); FREE_LIST(fcs);
    FREE_LIST(generator); FREE_LIST(main_booster); FREE_LIST(back_booster); FREE_LIST(side_booster);
    FREE_LIST(overed_booster); FREE_LIST(arm_unit); FREE_LIST(back_unit); FREE_LIST(shoulder_unit);
    FREE_LIST(head_top_stabilizer); FREE_LIST(head_side_stabilizer); FREE_LIST(core_upper_side_stabilizer);
    FREE_LIST(core_lower_side_stabilizer); FREE_LIST(arm_stabilizer); FREE_LIST(leg_back_stabilizer);
    FREE_LIST(leg_upper_stabilizer); FREE_LIST(leg_middle_stabilizer); FREE_LIST(leg_lower_stabilizer);
#undef FREE_LIST
    sf_xfree(a->alloc, a);
}

#define DEFINE_LIST_API(name, ctype)                                                         \
    size_t sf_acparts4_##name##_count(const sf_acparts4_t *a) { return a ? a->name##_count : 0u; } \
    const ctype *sf_acparts4_##name##_data(const sf_acparts4_t *a) { return a ? a->name : NULL; } \
    ctype *sf_acparts4_##name##_mutable_data(sf_acparts4_t *a) { return a ? a->name : NULL; } \
    sf_result_t sf_acparts4_set_##name##_count(sf_acparts4_t *a, size_t count) {             \
        SF_CHECK_ARG(a != NULL);                                                            \
        return set_array(a->alloc, (void **)&a->name, &a->name##_count, count, sizeof(ctype)); \
    }
DEFINE_LIST_API(head, sf_acparts4_head_t)
DEFINE_LIST_API(core, sf_acparts4_core_t)
DEFINE_LIST_API(arm, sf_acparts4_arm_t)
DEFINE_LIST_API(leg, sf_acparts4_leg_t)
DEFINE_LIST_API(fcs, sf_acparts4_fcs_t)
DEFINE_LIST_API(generator, sf_acparts4_generator_t)
DEFINE_LIST_API(main_booster, sf_acparts4_main_booster_t)
DEFINE_LIST_API(back_booster, sf_acparts4_back_booster_t)
DEFINE_LIST_API(side_booster, sf_acparts4_side_booster_t)
DEFINE_LIST_API(overed_booster, sf_acparts4_overed_booster_t)
DEFINE_LIST_API(arm_unit, sf_acparts4_arm_unit_t)
DEFINE_LIST_API(back_unit, sf_acparts4_back_unit_t)
DEFINE_LIST_API(shoulder_unit, sf_acparts4_shoulder_unit_t)
DEFINE_LIST_API(head_top_stabilizer, sf_acparts4_head_top_stabilizer_t)
DEFINE_LIST_API(head_side_stabilizer, sf_acparts4_head_side_stabilizer_t)
DEFINE_LIST_API(core_upper_side_stabilizer, sf_acparts4_core_upper_side_stabilizer_t)
DEFINE_LIST_API(core_lower_side_stabilizer, sf_acparts4_core_lower_side_stabilizer_t)
DEFINE_LIST_API(arm_stabilizer, sf_acparts4_arm_stabilizer_t)
DEFINE_LIST_API(leg_back_stabilizer, sf_acparts4_leg_back_stabilizer_t)
DEFINE_LIST_API(leg_upper_stabilizer, sf_acparts4_leg_upper_stabilizer_t)
DEFINE_LIST_API(leg_middle_stabilizer, sf_acparts4_leg_middle_stabilizer_t)
DEFINE_LIST_API(leg_lower_stabilizer, sf_acparts4_leg_lower_stabilizer_t)
#undef DEFINE_LIST_API

sf_acparts4_version_t sf_acparts4_version(const sf_acparts4_t *a) { return a ? a->version : SF_ACPARTS4_VERSION_AC4; }
size_t sf_acparts4_count(const sf_acparts4_t *a) {
    if (!a) return 0;
    return a->head_count + a->core_count + a->arm_count + a->leg_count + a->fcs_count +
           a->generator_count + a->main_booster_count + a->back_booster_count +
           a->side_booster_count + a->overed_booster_count + a->arm_unit_count +
           a->back_unit_count + a->shoulder_unit_count + a->head_top_stabilizer_count +
           a->head_side_stabilizer_count + a->core_upper_side_stabilizer_count +
           a->core_lower_side_stabilizer_count + a->arm_stabilizer_count +
           a->leg_back_stabilizer_count + a->leg_upper_stabilizer_count +
           a->leg_middle_stabilizer_count + a->leg_lower_stabilizer_count;
}

static sf_result_t read_counts_and_payload(sf_binary_reader_t *r, sf_acparts4_t *a) {
    uint32_t zero = 0;
    uint16_t c[22];
    TRY(sf_binary_reader_read_u32(r, &zero));
    if (zero != 0) return SF_ERR_BAD_MAGIC;
    for (size_t i = 0; i < 22; i++) TRY(sf_binary_reader_read_u16(r, &c[i]));
#define SET_FROM_COUNT(idx, name) TRY(sf_acparts4_set_##name##_count(a, c[idx]))
    SET_FROM_COUNT(0, head); SET_FROM_COUNT(1, core); SET_FROM_COUNT(2, arm); SET_FROM_COUNT(3, leg);
    SET_FROM_COUNT(4, fcs); SET_FROM_COUNT(5, generator); SET_FROM_COUNT(6, main_booster);
    SET_FROM_COUNT(7, back_booster); SET_FROM_COUNT(8, side_booster); SET_FROM_COUNT(9, overed_booster);
    SET_FROM_COUNT(10, arm_unit); SET_FROM_COUNT(11, back_unit); SET_FROM_COUNT(12, shoulder_unit);
    SET_FROM_COUNT(13, head_top_stabilizer); SET_FROM_COUNT(14, head_side_stabilizer);
    SET_FROM_COUNT(15, core_upper_side_stabilizer); SET_FROM_COUNT(16, core_lower_side_stabilizer);
    SET_FROM_COUNT(17, arm_stabilizer); SET_FROM_COUNT(18, leg_back_stabilizer);
    SET_FROM_COUNT(19, leg_upper_stabilizer); SET_FROM_COUNT(20, leg_middle_stabilizer);
    SET_FROM_COUNT(21, leg_lower_stabilizer);
#undef SET_FROM_COUNT
#define READ_LIST(name, fn) for (size_t i = 0; i < a->name##_count; i++) TRY(fn(r, a->version, &a->name[i]))
    READ_LIST(head, read_head); READ_LIST(core, read_core); READ_LIST(arm, read_arm); READ_LIST(leg, read_leg);
    READ_LIST(fcs, read_fcs); READ_LIST(generator, read_generator); READ_LIST(main_booster, read_main_booster);
    READ_LIST(back_booster, read_back_booster); READ_LIST(side_booster, read_back_booster); READ_LIST(overed_booster, read_overed_booster);
    READ_LIST(arm_unit, read_arm_unit); READ_LIST(back_unit, read_back_unit); READ_LIST(shoulder_unit, read_shoulder_unit);
    READ_LIST(head_top_stabilizer, read_stabilizer_part); READ_LIST(head_side_stabilizer, read_stabilizer_part);
    READ_LIST(core_upper_side_stabilizer, read_stabilizer_part); READ_LIST(core_lower_side_stabilizer, read_stabilizer_part);
    READ_LIST(arm_stabilizer, read_stabilizer_part); READ_LIST(leg_back_stabilizer, read_stabilizer_part);
    READ_LIST(leg_upper_stabilizer, read_stabilizer_part); READ_LIST(leg_middle_stabilizer, read_stabilizer_part);
    READ_LIST(leg_lower_stabilizer, read_stabilizer_part);
#undef READ_LIST
    return SF_OK;
}

sf_result_t sf_acparts4_read_from_memory(sf_acparts4_t **out, const void *bytes, size_t size,
                                         sf_acparts4_version_t version,
                                         const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    sf_acparts4_t *a = NULL;
    TRY(sf_acparts4_create(&a, version, alloc));
    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_result_t e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e == SF_OK) e = sf_binary_reader_create(&r, s, true, alloc);
    if (e == SF_OK) e = read_counts_and_payload(r, a);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_acparts4_destroy(a); return e; }
    *out = a;
    return SF_OK;
}

static sf_result_t write_count(sf_binary_writer_t *w, size_t count) {
    if (count > UINT16_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_write_u16(w, (uint16_t)count);
}

static sf_result_t write_payload(sf_binary_writer_t *w, const sf_acparts4_t *a) {
    TRY(sf_binary_writer_write_u32(w, 0));
#define WCOUNT(name) TRY(write_count(w, a->name##_count))
    WCOUNT(head); WCOUNT(core); WCOUNT(arm); WCOUNT(leg); WCOUNT(fcs); WCOUNT(generator);
    WCOUNT(main_booster); WCOUNT(back_booster); WCOUNT(side_booster); WCOUNT(overed_booster);
    WCOUNT(arm_unit); WCOUNT(back_unit); WCOUNT(shoulder_unit); WCOUNT(head_top_stabilizer);
    WCOUNT(head_side_stabilizer); WCOUNT(core_upper_side_stabilizer); WCOUNT(core_lower_side_stabilizer);
    WCOUNT(arm_stabilizer); WCOUNT(leg_back_stabilizer); WCOUNT(leg_upper_stabilizer);
    WCOUNT(leg_middle_stabilizer); WCOUNT(leg_lower_stabilizer);
#undef WCOUNT
#define WRITE_LIST(name, fn) for (size_t i = 0; i < a->name##_count; i++) TRY(fn(w, a->version, &a->name[i]))
    WRITE_LIST(head, write_head); WRITE_LIST(core, write_core); WRITE_LIST(arm, write_arm); WRITE_LIST(leg, write_leg);
    WRITE_LIST(fcs, write_fcs); WRITE_LIST(generator, write_generator); WRITE_LIST(main_booster, write_main_booster);
    WRITE_LIST(back_booster, write_back_booster); WRITE_LIST(side_booster, write_back_booster); WRITE_LIST(overed_booster, write_overed_booster);
    WRITE_LIST(arm_unit, write_arm_unit); WRITE_LIST(back_unit, write_back_unit); WRITE_LIST(shoulder_unit, write_shoulder_unit);
    WRITE_LIST(head_top_stabilizer, write_stabilizer_part); WRITE_LIST(head_side_stabilizer, write_stabilizer_part);
    WRITE_LIST(core_upper_side_stabilizer, write_stabilizer_part); WRITE_LIST(core_lower_side_stabilizer, write_stabilizer_part);
    WRITE_LIST(arm_stabilizer, write_stabilizer_part); WRITE_LIST(leg_back_stabilizer, write_stabilizer_part);
    WRITE_LIST(leg_upper_stabilizer, write_stabilizer_part); WRITE_LIST(leg_middle_stabilizer, write_stabilizer_part);
    WRITE_LIST(leg_lower_stabilizer, write_stabilizer_part);
#undef WRITE_LIST
    return SF_OK;
}

sf_result_t sf_acparts4_write_to_memory(const sf_acparts4_t *a, void **out_bytes,
                                        size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(a != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e == SF_OK) e = sf_binary_writer_create(&w, s, true, alloc);
    if (e == SF_OK) e = write_payload(w, a);
    if (e == SF_OK) e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}

#undef TRY
