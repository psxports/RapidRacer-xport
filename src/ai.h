#ifndef RR_AI_H
#define RR_AI_H

#include "psx.h"

struct RACE_VIEW;
#include "route.h"

struct BOAT;

typedef struct AI_CONTROL_CFG
{
    uint8 lookahead;
    uint16 max_speed, target_speed;
} AI_CONTROL_CFG;

extern AI_CONTROL_CFG ai_control_cfg;

sint32 ai_update_trailing_pool(const struct RACE_VIEW *entry);
sint32 ai_place(struct BOAT *boat, sint16 segment);
void ai_sort_racers_by_rank(const struct RACE_VIEW *first, const struct RACE_VIEW *second);
sint32 ai_update_racer(const struct RACE_VIEW *entry);
sint32 ai_random_mode(struct BOAT *boat);
sint32 ai_choose_route_target(struct BOAT *boat);
sint32 ai_config_template_desc(sint32 index, uint32 variation);
sint32 ai_init_race(struct BOAT *boat, sint16 segment, sint32 cfg_idx);

sint32 ai_assign_ctrl_states(const struct RACE_VIEW *group, const struct RACE_VIEW *second_group);

void ai_fn_800304fc(void);

#endif /* RR_AI_H */
